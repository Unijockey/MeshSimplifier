#pragma once
// qem_decimate.h - QEM edge collapse with per-loop UV, matching Blender exactly
// Fixes all 27 differences found in the comparison.
#include "math_utils.h"
#include "quadric.h"
#include "quadric5.h"
#include <vector>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <cstring>
#include <cfloat>
#include <climits>
#include <cmath>
#include <algorithm>

namespace blender {

// ─── Constants (same as Blender) ────────────────────────
static const float BOUNDARY_PRESERVE_WEIGHT = 100.0f;
static const double OPTIMIZE_EPS = 1e-8;
static const float TOPOLOGY_FALLBACK_EPS = 1e-12f;
static const float COST_INVALID = FLT_MAX;
static const float UV_EPS = 1e-6f;
static const float UV_SEAM_COST_WEIGHT = 25000.0f;
static const float TEX_DEFORMATION_WEIGHT = 4000.0f;
static const float TEX_QUADRIC5_WEIGHT = 0.35f;
static const float TEX_QUADRIC5_UV_BLEND = 0.65f;
static const float TEX_QUALITY_THRESHOLD = 0.1f;
static const float TEX_MIN_QUALITY = 1e-4f;
static const float TEX_EXTRA_TCOORD_WEIGHT = 0.10f;

// ─── Edge key for hash map ──────────────────────────────
struct EdgeKey { int v0, v1; bool operator==(const EdgeKey& o) const { return v0==o.v0 && v1==o.v1; } };
struct EdgeKeyHash {
    size_t operator()(const EdgeKey& k) const {
        return std::hash<int>()(k.v0) ^ (std::hash<int>()(k.v1) * 2654435761u);
    }
};
inline EdgeKey make_ek(int a, int b) { return a<b ? EdgeKey{a,b} : EdgeKey{b,a}; }

// ─── Mesh with per-loop UV ──────────────────────────────
struct QEMMesh {
    // Per-vertex
    std::vector<float> co;     // [x,y,z,...] stride 3
    std::vector<float> no;     // vertex normals
    std::vector<std::set<int>> vert_faces;
    std::vector<std::set<int>> vert_edges;
    std::vector<bool> vert_dead;

    // Per-face
    struct Face { int v[3]; float normal[3]; float center[3]; bool dead; int len; };
    std::vector<Face> faces;
    std::vector<int> face_materials;

    // Per-edge
    struct Edge { int v[2]; bool dead; int other(int vi) const { return v[0]==vi?v[1]:v[0]; } };
    std::vector<Edge> edges;
    std::vector<std::set<int>> edge_faces;
    std::unordered_map<EdgeKey, int, EdgeKeyHash> edge_map;

    // Per-face-vertex UV (matching Blender's per-loop storage)
    // Each face has 3 UV pairs: face_uvs[fi*6 + ki*2 + 0/1]
    std::vector<float> face_uvs; // [u,v, u,v, u,v, ...] stride 2, 3 per face
    bool has_uv;

    // Collapse tracking
    std::vector<int> vert_map; // v_clear -> v_other after collapse

    int n_verts() const { return (int)co.size()/3; }
    int n_faces() const { return (int)faces.size(); }
    int n_edges() const { return (int)edges.size(); }
    float* vert(int i) { return &co[i*3]; }
    const float* vert(int i) const { return &co[i*3]; }
    float* vnormal(int i) { return &no[i*3]; }
    const float* vnormal(int i) const { return &no[i*3]; }

    // UV access: face fi, local index ki
    float* face_uv(int fi, int ki) { return &face_uvs[(fi*3+ki)*2]; }
    const float* face_uv(int fi, int ki) const { return &face_uvs[(fi*3+ki)*2]; }

    static bool uv_equal(const float a[2], const float b[2], float eps=UV_EPS) {
        return fabsf(a[0]-b[0]) <= eps && fabsf(a[1]-b[1]) <= eps;
    }

    struct UVEdgePair {
        float clear_uv[2];
        float other_uv[2];
    };

    int live_edge_faces_into(int ei, int live[3]) const {
        int count = 0;
        if (ei < 0 || ei >= (int)edge_faces.size()) return 0;
        for (int fi : edge_faces[ei]) {
            if (fi >= 0 && fi < n_faces() && !faces[fi].dead) {
                if (count < 3) {
                    live[count] = fi;
                }
                count++;
            }
        }
        return count;
    }

    float edge_uv_seam_error(int ei) const {
        if (!has_uv) return 0.0f;
        int live[3] = {-1, -1, -1};
        if (live_edge_faces_into(ei, live) != 2) return 0.0f;
        const Edge& e = edges[ei];
        float err = 0.0f;
        for (int vi : {e.v[0], e.v[1]}) {
            int k0 = face_local_idx(live[0], vi);
            int k1 = face_local_idx(live[1], vi);
            if (k0 < 0 || k1 < 0) continue;
            const float* uv0 = face_uv(live[0], k0);
            const float* uv1 = face_uv(live[1], k1);
            float du = uv0[0] - uv1[0];
            float dv = uv0[1] - uv1[1];
            err = std::max(err, du*du + dv*dv);
        }
        return err;
    }

    bool edge_crosses_uv_seam(int ei) const {
        return edge_uv_seam_error(ei) > (UV_EPS * UV_EPS);
    }

    bool edge_crosses_material_boundary(int ei) const {
        int live[3] = {-1, -1, -1};
        int count = live_edge_faces_into(ei, live);
        if (count <= 1) return false;
        int mat = -1;
        for (int i=0; i<count && i<3; i++) {
            int fi = live[i];
            if (fi < 0 || fi >= (int)face_materials.size()) continue;
            if (mat == -1) mat = face_materials[fi];
            else if (face_materials[fi] != mat) return true;
        }
        return false;
    }

    bool face_contains_vertex(int fi, int vi) const {
        if (fi < 0 || fi >= n_faces()) return false;
        const Face& f = faces[fi];
        return f.v[0] == vi || f.v[1] == vi || f.v[2] == vi;
    }

    // Find which local index in face fi corresponds to vertex vi
    int face_local_idx(int fi, int vi) const {
        const Face& f = faces[fi];
        for (int k=0;k<3;k++) if (f.v[k]==vi) return k;
        return -1;
    }

    void collect_edge_uv_pairs(int ei, int v_clear, int v_other,
                               std::vector<UVEdgePair>& edge_pairs) const {
        edge_pairs.clear();
        if (!has_uv || ei < 0 || ei >= (int)edge_faces.size()) return;

        int live[3] = {-1, -1, -1};
        int live_count = live_edge_faces_into(ei, live);
        for (int i=0; i<live_count && i<3; i++) {
            int fi = live[i];
            int ki_clear = face_local_idx(fi, v_clear);
            int ki_other = face_local_idx(fi, v_other);
            if (ki_clear < 0 || ki_other < 0) continue;

            const float* uv_clear = face_uv(fi, ki_clear);
            const float* uv_other = face_uv(fi, ki_other);
            UVEdgePair pair;
            pair.clear_uv[0] = uv_clear[0];
            pair.clear_uv[1] = uv_clear[1];
            pair.other_uv[0] = uv_other[0];
            pair.other_uv[1] = uv_other[1];
            edge_pairs.push_back(pair);
        }
    }

    static double triangle_quality_from_points(const float p0[3], const float p1[3], const float p2[3]) {
        float e01[3] = {p1[0]-p0[0], p1[1]-p0[1], p1[2]-p0[2]};
        float e12[3] = {p2[0]-p1[0], p2[1]-p1[1], p2[2]-p1[2]};
        float e20[3] = {p0[0]-p2[0], p0[1]-p2[1], p0[2]-p2[2]};
        double l2sum = (double)dot_v3v3(e01, e01) + (double)dot_v3v3(e12, e12) + (double)dot_v3v3(e20, e20);
        if (l2sum <= 1e-30) return 0.0;
        float e02[3] = {p2[0]-p0[0], p2[1]-p0[1], p2[2]-p0[2]};
        float cr[3];
        cross_v3_v3v3(cr, e01, e02);
        double area2 = sqrt((double)dot_v3v3(cr, cr));
        double q = (2.0 * sqrt(3.0) * area2) / l2sum;
        if (q < 0.0) return 0.0;
        return q > 1.0 ? 1.0 : q;
    }

    double face_mean_edge_len_squared_after_replace(int fi, int replace_vi, const float replacement[3]) const {
        const Face& f = faces[fi];
        float p[3][3];
        for (int k=0; k<3; k++) {
            const float* src = (f.v[k] == replace_vi) ? replacement : vert(f.v[k]);
            p[k][0] = src[0];
            p[k][1] = src[1];
            p[k][2] = src[2];
        }
        double l0 = (double)len_squared_v3v3(p[0], p[1]);
        double l1 = (double)len_squared_v3v3(p[1], p[2]);
        double l2 = (double)len_squared_v3v3(p[2], p[0]);
        return (l0 + l1 + l2) / 3.0;
    }

    double face_quality_after_replace(int fi, int replace_vi, const float replacement[3]) const {
        const Face& f = faces[fi];
        float p[3][3];
        for (int k=0; k<3; k++) {
            const float* src = (f.v[k] == replace_vi) ? replacement : vert(f.v[k]);
            p[k][0] = src[0];
            p[k][1] = src[1];
            p[k][2] = src[2];
        }
        return triangle_quality_from_points(p[0], p[1], p[2]);
    }

    double edge_min_quality_after_collapse(int ei, int v_clear, int v_other, const float opt[3]) const {
        double min_quality = 1.0;
        bool seen = false;

        for (int fi : vert_faces[v_clear]) {
            if (fi < 0 || fi >= n_faces() || faces[fi].dead) continue;
            if (face_contains_vertex(fi, v_other)) continue;
            min_quality = std::min(min_quality, face_quality_after_replace(fi, v_clear, opt));
            seen = true;
        }
        for (int fi : vert_faces[v_other]) {
            if (fi < 0 || fi >= n_faces() || faces[fi].dead) continue;
            if (face_contains_vertex(fi, v_clear)) continue;
            min_quality = std::min(min_quality, face_quality_after_replace(fi, v_other, opt));
            seen = true;
        }
        (void)ei;
        return seen ? min_quality : 1.0;
    }

    static bool collapse_projected_uv(const std::vector<UVEdgePair>& edge_pairs,
                                      const float old_uv[2], bool from_clear,
                                      float t, float new_uv[2]) {
        for (const UVEdgePair& pair : edge_pairs) {
            if (from_clear) {
                if (!uv_equal(old_uv, pair.clear_uv)) continue;
                new_uv[0] = (1.0f - t) * pair.other_uv[0] + t * old_uv[0];
                new_uv[1] = (1.0f - t) * pair.other_uv[1] + t * old_uv[1];
                return true;
            }
            if (!uv_equal(old_uv, pair.other_uv)) continue;
            new_uv[0] = (1.0f - t) * old_uv[0] + t * pair.clear_uv[0];
            new_uv[1] = (1.0f - t) * old_uv[1] + t * pair.clear_uv[1];
            return true;
        }
        return false;
    }

    double edge_texture_deformation_error(int ei, int v_clear, int v_other,
                                          const float opt[3], float t) const {
        if (!has_uv) return 0.0;

        std::vector<UVEdgePair> edge_pairs;
        collect_edge_uv_pairs(ei, v_clear, v_other, edge_pairs);
        if (edge_pairs.empty()) return 0.0;

        auto accum_face = [&](int fi, int vi, bool from_clear) -> double {
            int ki = face_local_idx(fi, vi);
            if (ki < 0) return 0.0;

            const float* old_uv = face_uv(fi, ki);
            float new_uv[2];
            if (!collapse_projected_uv(edge_pairs, old_uv, from_clear, t, new_uv)) {
                return 0.0;
            }

            double du = (double)new_uv[0] - (double)old_uv[0];
            double dv = (double)new_uv[1] - (double)old_uv[1];
            double duv2 = du * du + dv * dv;
            if (duv2 <= (double)(UV_EPS * UV_EPS)) return 0.0;

            double geom_scale = face_mean_edge_len_squared_after_replace(fi, vi, opt);
            if (geom_scale < 1e-20) geom_scale = 1e-20;

            double quality = face_quality_after_replace(fi, vi, opt);
            double quality_scale = 1.0;
            if (quality < (double)TEX_QUALITY_THRESHOLD) {
                quality_scale = (double)TEX_QUALITY_THRESHOLD / std::max(quality, (double)TEX_MIN_QUALITY);
            }
            return duv2 * geom_scale * quality_scale;
        };

        double err = 0.0;
        for (int fi : vert_faces[v_clear]) {
            if (fi < 0 || fi >= n_faces() || faces[fi].dead) continue;
            if (face_contains_vertex(fi, v_other)) continue;
            err += accum_face(fi, v_clear, true);
        }
        for (int fi : vert_faces[v_other]) {
            if (fi < 0 || fi >= n_faces() || faces[fi].dead) continue;
            if (face_contains_vertex(fi, v_clear)) continue;
            err += accum_face(fi, v_other, false);
        }
        return err;
    }

    int distinct_vertex_uv_count(int vi) const {
        if (!has_uv || vi < 0 || vi >= n_verts()) return 1;
        std::vector<float> unique_uvs;
        for (int fi : vert_faces[vi]) {
            if (fi < 0 || fi >= n_faces() || faces[fi].dead) continue;
            int ki = face_local_idx(fi, vi);
            if (ki < 0) continue;
            const float* uv = face_uv(fi, ki);
            bool found = false;
            for (size_t i=0; i+1<unique_uvs.size(); i+=2) {
                float ref[2] = {unique_uvs[i], unique_uvs[i+1]};
                if (uv_equal(uv, ref)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                unique_uvs.push_back(uv[0]);
                unique_uvs.push_back(uv[1]);
            }
        }
        int count = (int)unique_uvs.size() / 2;
        return count > 0 ? count : 1;
    }

    void init(const float* vdata, int nv, const int* fdata, int nf,
              const float* face_uv_data,
              const int* face_material_data=nullptr) {
        co.assign(vdata, vdata+nv*3);
        no.resize(nv*3, 0.0f);
        vert_faces.resize(nv);
        vert_edges.resize(nv);
        vert_dead.resize(nv, false);
        vert_map.resize(nv);
        for (int i=0;i<nv;i++) vert_map[i]=i;

        faces.resize(nf);
        face_materials.resize(nf, -1);
        for (int i=0;i<nf;i++) {
            faces[i].v[0]=fdata[i*3]; faces[i].v[1]=fdata[i*3+1]; faces[i].v[2]=fdata[i*3+2];
            faces[i].dead=false; faces[i].len=3;
            if (face_material_data) face_materials[i] = face_material_data[i];
        }

        // Per-face-vertex UV (3 UV pairs per face)
        has_uv = (face_uv_data != nullptr);
        if (has_uv) {
            face_uvs.assign(face_uv_data, face_uv_data + nf*3*2);
        }

        build_adjacency();
        compute_face_geometry();
        compute_vert_normals();
    }

    void build_adjacency() {
        for (int i=0;i<n_verts();i++) { vert_faces[i].clear(); vert_edges[i].clear(); }
        edge_faces.clear(); edges.clear(); edge_map.clear();

        for (int fi=0;fi<n_faces();fi++) {
            Face& f = faces[fi];
            for (int k=0;k<3;k++) vert_faces[f.v[k]].insert(fi);
            for (int k=0;k<3;k++) {
                int a=f.v[k], b=f.v[(k+1)%3];
                EdgeKey ek = make_ek(a,b);
                auto it = edge_map.find(ek);
                if (it == edge_map.end()) {
                    int ei = (int)edges.size();
                    edges.push_back({{a, b}, false});
                    edge_map[ek] = ei;
                    vert_edges[ek.v0].insert(ei);
                    vert_edges[ek.v1].insert(ei);
                    std::set<int> fs; fs.insert(fi);
                    edge_faces.push_back(fs);
                } else {
                    edge_faces[it->second].insert(fi);
                }
            }
        }
    }

    void compute_face_geometry() {
        for (int fi=0;fi<n_faces();fi++) {
            Face& f = faces[fi];
            if (f.dead) continue;
            const float* v0=vert(f.v[0]); const float* v1=vert(f.v[1]); const float* v2=vert(f.v[2]);
            f.center[0]=(v0[0]+v1[0]+v2[0])/3; f.center[1]=(v0[1]+v1[1]+v2[1])/3; f.center[2]=(v0[2]+v1[2]+v2[2])/3;
            float e1[3]={v1[0]-v0[0],v1[1]-v0[1],v1[2]-v0[2]};
            float e2[3]={v2[0]-v0[0],v2[1]-v0[1],v2[2]-v0[2]};
            cross_v3_v3v3(f.normal, e1, e2);
            normalize_v3(f.normal);
        }
    }

    void compute_vert_normals() {
        for (int vi=0;vi<n_verts();vi++) {
            float* n = vnormal(vi);
            n[0]=n[1]=n[2]=0; int cnt=0;
            for (int fi : vert_faces[vi]) { if (!faces[fi].dead) { n[0]+=faces[fi].normal[0]; n[1]+=faces[fi].normal[1]; n[2]+=faces[fi].normal[2]; cnt++; } }
            if (cnt>0) { n[0]/=cnt; n[1]/=cnt; n[2]/=cnt; normalize_v3(n); }
        }
    }

    // Edge topology queries (matching Blender)
    bool edge_is_manifold(int ei) const {
        if (ei<0 || ei>=(int)edge_faces.size()) return false;
        int cnt=0; for (int fi : edge_faces[ei]) if (!faces[fi].dead) cnt++;
        return cnt==2;
    }
    bool edge_is_boundary(int ei) const {
        if (ei<0 || ei>=(int)edge_faces.size()) return false;
        int cnt=0; for (int fi : edge_faces[ei]) if (!faces[fi].dead) cnt++;
        return cnt==1;
    }
    int edge_face_count(int ei) const {
        if (ei<0 || ei>=(int)edge_faces.size()) return 0;
        int cnt=0; for (int fi : edge_faces[ei]) if (!faces[fi].dead) cnt++;
        return cnt;
    }

    // ─── UV interpolation for edge collapse ─────────────
    // Blender: bm_edge_collapse_loop_customdata
    // Walks the fan of loops around v_clear, interpolating per-loop UV
    // UV interpolation for edge collapse
    // Matches Blender's bm_edge_collapse_loop_customdata.
    // Two parts:
    //   1. Edge faces: interpolate v_other UV toward v_clear UV (factor t)
    //   2. Non-edge faces of v_clear: interpolate v_clear UV toward v_other UV (factor t)
    //      This prevents UV discontinuity when v_clear is replaced by v_other
    void interpolate_uv_collapse(int v_clear, int v_other, int ei, float t) {
        if (!has_uv) return;
        if (ei<0 || ei>=(int)edge_faces.size()) return;

        struct UVPair {
            float clear_uv[2];
            float other_uv[2];
        };
        std::vector<UVPair> edge_pairs;

        int live[3] = {-1, -1, -1};
        int live_count = live_edge_faces_into(ei, live);
        for (int i=0; i<live_count && i<3; i++) {
            int fi = live[i];
            int ki_clear = face_local_idx(fi, v_clear);
            int ki_other = face_local_idx(fi, v_other);
            if (ki_clear < 0 || ki_other < 0) continue;

            const float* uv_clear = face_uv(fi, ki_clear);
            const float* uv_other = face_uv(fi, ki_other);
            UVPair pair;
            pair.clear_uv[0] = uv_clear[0];
            pair.clear_uv[1] = uv_clear[1];
            pair.other_uv[0] = uv_other[0];
            pair.other_uv[1] = uv_other[1];
            edge_pairs.push_back(pair);
        }

        // Part 1: edge faces are removed, but Blender updates their loop data
        // before splicing. Keep this for consistency with the reference values.
        for (int i=0; i<live_count && i<3; i++) {
            int fi = live[i];
            int ki_clear = face_local_idx(fi, v_clear);
            int ki_other = face_local_idx(fi, v_other);
            if (ki_clear < 0 || ki_other < 0) continue;
            float* uv_other = face_uv(fi, ki_other);
            const float* uv_clear = face_uv(fi, ki_clear);
            uv_other[0] = (1.0f-t)*uv_other[0] + t*uv_clear[0];
            uv_other[1] = (1.0f-t)*uv_other[1] + t*uv_clear[1];
        }

        // Part 2a: v_clear fan. This matches Blender side == 0:
        // src = {clear, other}, weights = {t, 1-t}.
        for (int fi : vert_faces[v_clear]) {
            if (faces[fi].dead) continue;
            // Skip edge faces (already handled)
            bool is_edge = false;
            for (int efi : edge_faces[ei]) { if (efi==fi) { is_edge=true; break; } }
            if (is_edge) continue;

            // Skip faces that also contain v_other (would become degenerate)
            Face& f = faces[fi];
            bool has_other = false;
            for (int k=0;k<3;k++) if (f.v[k]==v_other) { has_other=true; break; }
            if (has_other) continue;

            int ki_clear = face_local_idx(fi, v_clear);
            if (ki_clear < 0) continue;

            float* uv_clear = face_uv(fi, ki_clear);
            float ref_uv[2] = {0.0f, 0.0f};
            bool found_ref = false;
            for (const UVPair& pair : edge_pairs) {
                if (uv_equal(uv_clear, pair.clear_uv)) {
                    ref_uv[0] = pair.other_uv[0];
                    ref_uv[1] = pair.other_uv[1];
                    found_ref = true;
                    break;
                }
            }
            if (!found_ref) continue;

            uv_clear[0] = (1.0f-t)*ref_uv[0] + t*uv_clear[0];
            uv_clear[1] = (1.0f-t)*ref_uv[1] + t*uv_clear[1];
        }

        // Part 2b: v_other fan. Blender also runs side == 1 before splicing:
        // src = {other, clear}, weights = {1-t, t}. These faces survive, so
        // missing this step leaves discontinuous UV bands around the kept vertex.
        for (int fi : vert_faces[v_other]) {
            if (faces[fi].dead) continue;
            bool is_edge = false;
            for (int efi : edge_faces[ei]) { if (efi==fi) { is_edge=true; break; } }
            if (is_edge) continue;

            Face& f = faces[fi];
            bool has_clear = false;
            for (int k=0;k<3;k++) if (f.v[k]==v_clear) { has_clear=true; break; }
            if (has_clear) continue;

            int ki_other = face_local_idx(fi, v_other);
            if (ki_other < 0) continue;

            float* uv_other = face_uv(fi, ki_other);
            float ref_uv[2] = {0.0f, 0.0f};
            bool found_ref = false;
            for (const UVPair& pair : edge_pairs) {
                if (uv_equal(uv_other, pair.other_uv)) {
                    ref_uv[0] = pair.clear_uv[0];
                    ref_uv[1] = pair.clear_uv[1];
                    found_ref = true;
                    break;
                }
            }
            if (!found_ref) continue;

            uv_other[0] = (1.0f-t)*uv_other[0] + t*ref_uv[0];
            uv_other[1] = (1.0f-t)*uv_other[1] + t*ref_uv[1];
        }
    }

    // ─── Edge collapse ──────────────────────────────────
    // Matches Blender: collapses e, removes v_clear (v[1]), keeps v_other (v[0])
    bool edge_collapse(int ei, const float opt[3], int v_clear, int *r_faces_removed=nullptr) {
        Edge& e = edges[ei];
        int v_other = e.v[0];
        int faces_removed = 0;

        // Verify v_clear is in this edge
        if (e.v[0] != v_clear && e.v[1] != v_clear) return false;
        v_other = (e.v[0] == v_clear) ? e.v[1] : e.v[0];

        // Safety: verify both vertices are alive
        if (vert_dead[v_clear] || vert_dead[v_other]) return false;
        if (v_clear < 0 || v_clear >= n_verts() || v_other < 0 || v_other >= n_verts()) return false;

        int fc = edge_face_count(ei);
        if (fc != 1 && fc != 2) return false;

        // Interpolation factor t (NOT clamped - matches Blender)
        float t;
        if (!compare_v3v3(vert(v_other), vert(v_clear), FLT_EPSILON)) {
            t = line_point_factor_v3(opt, vert(v_other), vert(v_clear));
        } else {
            t = 0.5f;
        }

        // UV interpolation (per-loop, matching Blender's bm_edge_collapse_loop_customdata)
        interpolate_uv_collapse(v_clear, v_other, ei, t);

        // Weight interpolation (if applicable)
        // v_other_weight = interpf(v_other_weight, v_clear_weight, t)

        // Normal interpolation (USE_VERT_NORMAL_INTERP)
        float v_clear_no[3];
        copy_v3_v3(v_clear_no, vnormal(v_clear));

        // Move v_other to optimize position
        float* co_other = vert(v_other);
        co_other[0]=opt[0]; co_other[1]=opt[1]; co_other[2]=opt[2];

        // Update faces: replace v_clear with v_other
        std::set<int> shared_faces;
        for (int fi : edge_faces[ei]) { if (!faces[fi].dead) shared_faces.insert(fi); }

        for (int fi : vert_faces[v_clear]) {
            if (faces[fi].dead) continue;
            Face& f = faces[fi];
            for (int k=0;k<3;k++) { if (f.v[k]==v_clear) f.v[k]=v_other; }
            // Check degenerate (duplicate vertex)
            if (f.v[0]==f.v[1] || f.v[1]==f.v[2] || f.v[0]==f.v[2]) {
                if (!f.dead) {
                    f.dead=true;
                    faces_removed++;
                }
            } else {
                vert_faces[v_other].insert(fi);
            }
        }
        // Remove shared faces
        for (int fi : shared_faces) {
            if (!faces[fi].dead) {
                faces[fi].dead=true;
                faces_removed++;
            }
        }

        // Transfer edges from v_clear to v_other
        std::vector<int> adj_edges;
        for (int ei2 : vert_edges[v_clear]) {
            if (ei2!=ei && !edges[ei2].dead) adj_edges.push_back(ei2);
        }

        for (int ei2 : adj_edges) {
            Edge& e2 = edges[ei2];
            int ov = (e2.v[0]==v_clear) ? e2.v[1] : e2.v[0];
            if (ov==v_other) {
                // This edge becomes a self-loop - mark as dead
                // (Edge splice: merge with existing edge)
                e2.dead=true;
                continue;
            }
            // Redirect
            if (e2.v[0]==v_clear) e2.v[0]=v_other;
            if (e2.v[1]==v_clear) e2.v[1]=v_other;

            // Update edge_map
            edge_map.erase(make_ek(v_clear, ov));
            EdgeKey new_key = make_ek(v_other, ov);
            auto it2 = edge_map.find(new_key);
            if (it2 != edge_map.end()) {
                // Edge splice: merge duplicate edges
                int existing_ei = it2->second;
                // Transfer faces from e2 to existing
                for (int fi : edge_faces[ei2]) {
                    if (!faces[fi].dead) edge_faces[existing_ei].insert(fi);
                }
                e2.dead=true;
                edge_faces[ei2].clear(); // BUG 9 fix: clean up self-loop
            } else {
                edge_map[new_key] = ei2;
            }

            vert_edges[v_other].insert(ei2);
            // Clean dead faces from edge_faces
            auto& ef = edge_faces[ei2];
            for (auto it3=ef.begin(); it3!=ef.end(); ) {
                if (faces[*it3].dead) it3=ef.erase(it3); else ++it3;
            }
        }

        // Mark v_clear as dead
        vert_dead[v_clear]=true;
        vert_faces[v_clear].clear();
        vert_edges[v_clear].clear();
        vert_map[v_clear]=v_other;

        // Mark collapsed edge as dead
        e.dead=true;
        if (ei < (int)edge_faces.size()) edge_faces[ei].clear();

        // Normal interpolation (USE_VERT_NORMAL_INTERP)
        float* no_other = vnormal(v_other);
        interp_v3_v3v3(no_other, no_other, v_clear_no, t);
        normalize_v3(no_other);

        // Update face geometry around v_other
        for (int fi : vert_faces[v_other]) {
            if (fi < 0 || fi >= (int)faces.size() || faces[fi].dead) continue;
            Face& f = faces[fi];
            if (f.v[0] < 0 || f.v[0] >= n_verts() || f.v[1] < 0 || f.v[1] >= n_verts() ||
                f.v[2] < 0 || f.v[2] >= n_verts()) continue;
            const float* va=vert(f.v[0]); const float* vb=vert(f.v[1]); const float* vc=vert(f.v[2]);
            f.center[0]=(va[0]+vb[0]+vc[0])/3; f.center[1]=(va[1]+vb[1]+vc[1])/3; f.center[2]=(va[2]+vb[2]+vc[2])/3;
            float e1[3]={vb[0]-va[0],vb[1]-va[1],vb[2]-va[2]};
            float e2[3]={vc[0]-va[0],vc[1]-va[1],vc[2]-va[2]};
            cross_v3_v3v3(f.normal, e1, e2); normalize_v3(f.normal);
        }

        if (r_faces_removed) *r_faces_removed = faces_removed;
        return true;
    }

    int live_face_count() const {
        int c=0; for (auto& f : faces) if (!f.dead) c++; return c;
    }

    void collect_result(std::vector<float>& ov, std::vector<int>& of,
                        std::vector<float>& out_face_uvs,
                        std::vector<int>* out_face_materials=nullptr) const {
        std::vector<int> remap(n_verts(), -1);
        int ni=0;
        for (int i=0;i<n_verts();i++) if (!vert_dead[i]) remap[i]=ni++;

        ov.resize(ni*3);
        for (int i=0;i<n_verts();i++) {
            if (remap[i]>=0) {
                ov[remap[i]*3+0]=co[i*3+0];
                ov[remap[i]*3+1]=co[i*3+1];
                ov[remap[i]*3+2]=co[i*3+2];
            }
        }

        of.clear();
        out_face_uvs.clear();
        if (out_face_materials) out_face_materials->clear();

        for (int fi=0;fi<n_faces();fi++) {
            if (faces[fi].dead) continue;
            int a=remap[faces[fi].v[0]], b=remap[faces[fi].v[1]], c=remap[faces[fi].v[2]];
            if (a>=0&&b>=0&&c>=0&&a!=b&&b!=c&&a!=c) {
                of.push_back(a); of.push_back(b); of.push_back(c);

                // Per-face-vertex UVs (preserves UV seams)
                if (has_uv) {
                    for (int ki=0;ki<3;ki++) {
                        const float* uv = face_uv(fi, ki);
                        out_face_uvs.push_back(uv[0]);
                        out_face_uvs.push_back(uv[1]);
                    }
                }
                if (out_face_materials) {
                    out_face_materials->push_back(
                        fi < (int)face_materials.size() ? face_materials[fi] : -1);
                }
            }
        }
    }
};

// ─── Heap item ──────────────────────────────────────────
struct HeapItem {
    float cost; int edge_idx; int version;
    bool operator>(const HeapItem& o) const { return cost > o.cost; }
};

struct VertexTexQuadric {
    float uv[2];
    Quadric5 q;
};

// ─── Simplify entry ─────────────────────────────────────
struct SimplifyParams {
    float target_ratio;
    float *vweights;
    float vweight_factor;
    int symmetry_axis;
    float symmetry_eps;
    SimplifyParams() : target_ratio(0.5f), vweights(nullptr), vweight_factor(1.0f),
                       symmetry_axis(-1), symmetry_eps(2e-5f) {}
};

inline void simplify(QEMMesh& mesh, const SimplifyParams& params,
                     std::vector<float>& out_verts, std::vector<int>& out_faces,
                     std::vector<float>& out_face_uvs) {

    int nv = mesh.n_verts();
    int live_faces = mesh.live_face_count();
    int nf_target = std::max(4, (int)(live_faces * params.target_ratio));

    // Step 1: Build vertex quadrics (bm_decim_build_quadrics)
    std::vector<Quadric> vq(nv);
    for (int i=0;i<nv;i++) quadric_clear(&vq[i]);
    std::vector<std::vector<VertexTexQuadric>> vtq(nv);

    for (int fi=0;fi<mesh.n_faces();fi++) {
        auto& f = mesh.faces[fi];
        if (f.dead) continue;
        double plane[4] = {(double)f.normal[0], (double)f.normal[1], (double)f.normal[2],
                           -((double)f.normal[0]*f.center[0]+(double)f.normal[1]*f.center[1]+(double)f.normal[2]*f.center[2])};
        Quadric q; quadric_from_plane(&q, plane);
        for (int k=0;k<3;k++) quadric_add(&vq[f.v[k]], &q);
    }

    auto tex_quadric_find = [&](int vi, const float uv[2]) -> int {
        if (vi < 0 || vi >= (int)vtq.size()) return -1;
        for (int i=0; i<(int)vtq[vi].size(); i++) {
            if (QEMMesh::uv_equal(vtq[vi][i].uv, uv)) return i;
        }
        return -1;
    };

    auto tex_quadric_ensure = [&](int vi, const float uv[2]) -> int {
        int idx = tex_quadric_find(vi, uv);
        if (idx >= 0) return idx;
        VertexTexQuadric entry;
        entry.uv[0] = uv[0];
        entry.uv[1] = uv[1];
        quadric5_clear(&entry.q);
        quadric5_sum3(&entry.q, &vq[vi], uv[0], uv[1]);
        vtq[vi].push_back(entry);
        return (int)vtq[vi].size() - 1;
    };

    if (mesh.has_uv) {
        for (int fi=0; fi<mesh.n_faces(); fi++) {
            auto& f = mesh.faces[fi];
            if (f.dead) continue;
            double p[3][5];
            for (int k=0; k<3; k++) {
                const float* co = mesh.vert(f.v[k]);
                const float* uv = mesh.face_uv(fi, k);
                p[k][0] = co[0];
                p[k][1] = co[1];
                p[k][2] = co[2];
                p[k][3] = uv[0];
                p[k][4] = uv[1];
                tex_quadric_ensure(f.v[k], uv);
            }

            Quadric5 fq;
            quadric5_from_plane(&fq, p[0], p[1], p[2]);
            for (int k=0; k<3; k++) {
                const float* uv = mesh.face_uv(fi, k);
                int idx = tex_quadric_find(f.v[k], uv);
                if (idx >= 0) quadric5_add(&vtq[f.v[k]][idx].q, &fq);
            }
        }
    }

    // Boundary edge quadrics
    for (auto& ekp : mesh.edge_map) {
        int ei = ekp.second;
        if (mesh.edges[ei].dead || !mesh.edge_is_boundary(ei)) continue;
        auto& e = mesh.edges[ei];
        int fi=-1;
        for (int f : mesh.edge_faces[ei]) { if (!mesh.faces[f].dead) { fi=f; break; } }
        if (fi<0) continue;
        const float* va=mesh.vert(e.v[0]); const float* vb=mesh.vert(e.v[1]);
        float ev[3]={vb[0]-va[0],vb[1]-va[1],vb[2]-va[2]};
        float ep[3]; cross_v3_v3v3(ep, ev, mesh.faces[fi].normal);
        if (normalize_v3(ep) < FLT_EPSILON) continue;
        float mid[3]={(va[0]+vb[0])*0.5f,(va[1]+vb[1])*0.5f,(va[2]+vb[2])*0.5f};
        double plane[4]={(double)ep[0],(double)ep[1],(double)ep[2],-(double)dot_v3v3(ep,mid)};
        Quadric q; quadric_from_plane(&q,plane); quadric_mul(&q,(double)BOUNDARY_PRESERVE_WEIGHT);
        quadric_add(&vq[e.v[0]],&q); quadric_add(&vq[e.v[1]],&q);
    }

    // Step 2: Build edge cost heap
    // Version tracking for stale heap entries
    std::vector<int> edge_version(mesh.n_edges(), 0);
    std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> eheap;

    auto tex_quadric_lookup = [&](int vi, const float uv[2]) -> const Quadric5* {
        int idx = tex_quadric_find(vi, uv);
        if (idx >= 0) return &vtq[vi][idx].q;
        if (vi < 0 || vi >= (int)vtq.size() || vtq[vi].empty()) return nullptr;

        int best = 0;
        double best_dist = DBL_MAX;
        for (int i=0; i<(int)vtq[vi].size(); i++) {
            double du = (double)vtq[vi][i].uv[0] - (double)uv[0];
            double dv = (double)vtq[vi][i].uv[1] - (double)uv[1];
            double dist = du*du + dv*dv;
            if (dist < best_dist) {
                best_dist = dist;
                best = i;
            }
        }
        return &vtq[vi][best].q;
    };

    auto edge_quadric5_cost = [&](int ei, int v_clear, int v_other, const float opt[3]) -> double {
        if (!mesh.has_uv || vtq.empty()) return 0.0;

        std::vector<QEMMesh::UVEdgePair> edge_pairs;
        mesh.collect_edge_uv_pairs(ei, v_clear, v_other, edge_pairs);
        if (edge_pairs.empty()) return 0.0;

        double geo[3] = {(double)opt[0], (double)opt[1], (double)opt[2]};
        double worst = 0.0;
        for (const auto& pair : edge_pairs) {
            const Quadric5* q_other = tex_quadric_lookup(v_other, pair.other_uv);
            const Quadric5* q_clear = tex_quadric_lookup(v_clear, pair.clear_uv);
            if (!q_other || !q_clear) continue;

            Quadric5 qsum;
            quadric5_add_two(&qsum, q_other, q_clear);

            double uv_opt[2] = {
                ((double)pair.other_uv[0] + (double)pair.clear_uv[0]) * 0.5,
                ((double)pair.other_uv[1] + (double)pair.clear_uv[1]) * 0.5,
            };
            double uv_min[2];
            if (quadric5_minimize_uv_with_geo(&qsum, geo, uv_min)) {
                uv_opt[0] = uv_min[0];
                uv_opt[1] = uv_min[1];
                bool source_in_unit =
                    pair.other_uv[0] >= 0.0f && pair.other_uv[0] <= 1.0f &&
                    pair.other_uv[1] >= 0.0f && pair.other_uv[1] <= 1.0f &&
                    pair.clear_uv[0] >= 0.0f && pair.clear_uv[0] <= 1.0f &&
                    pair.clear_uv[1] >= 0.0f && pair.clear_uv[1] <= 1.0f;
                if (source_in_unit) {
                    uv_opt[0] = std::min(1.0, std::max(0.0, uv_opt[0]));
                    uv_opt[1] = std::min(1.0, std::max(0.0, uv_opt[1]));
                }
            }

            double v5[5] = {geo[0], geo[1], geo[2], uv_opt[0], uv_opt[1]};
            double priority = quadric5_evaluate(&qsum, v5);

            double v_other5[5] = {geo[0], geo[1], geo[2], pair.other_uv[0], pair.other_uv[1]};
            double v_clear5[5] = {geo[0], geo[1], geo[2], pair.clear_uv[0], pair.clear_uv[1]};
            double v_mid5[5] = {
                geo[0], geo[1], geo[2],
                ((double)pair.other_uv[0] + (double)pair.clear_uv[0]) * 0.5,
                ((double)pair.other_uv[1] + (double)pair.clear_uv[1]) * 0.5,
            };
            priority = std::min(priority, quadric5_evaluate(&qsum, v_other5));
            priority = std::min(priority, quadric5_evaluate(&qsum, v_clear5));
            priority = std::min(priority, quadric5_evaluate(&qsum, v_mid5));
            worst = std::max(worst, priority);
        }

        int extra_uv_count = (int)vtq[v_other].size() + (int)vtq[v_clear].size() - 2;
        if (extra_uv_count > 0) {
            worst *= 1.0 + (double)TEX_EXTRA_TCOORD_WEIGHT * (double)extra_uv_count;
        }
        return worst;
    };

    auto compute_cost = [&](int ei) -> float {
        auto& e = mesh.edges[ei];
        if (e.dead) return COST_INVALID;
        if (e.v[0] < 0 || e.v[0] >= mesh.n_verts() || e.v[1] < 0 || e.v[1] >= mesh.n_verts())
            return COST_INVALID;
        if (mesh.vert_dead[e.v[0]] || mesh.vert_dead[e.v[1]])
            return COST_INVALID;
        if (params.vweights && (params.vweights[e.v[0]]==0 || params.vweights[e.v[1]]==0))
            return COST_INVALID;
        if (mesh.edge_crosses_material_boundary(ei))
            return COST_INVALID;

        // FIX #1: Triangle-face enforcement
        int live_edge_faces[3] = {-1, -1, -1};
        int fc = mesh.live_edge_faces_into(ei, live_edge_faces);
        if (fc==0) return COST_INVALID;
        if (fc==1) { if (mesh.faces[live_edge_faces[0]].len != 3) return COST_INVALID; }
        else if (fc==2) {
            if (mesh.faces[live_edge_faces[0]].len!=3 || mesh.faces[live_edge_faces[1]].len!=3) return COST_INVALID;
        }
        else return COST_INVALID;

        Quadric qm; quadric_add_two(&qm, &vq[e.v[0]], &vq[e.v[1]]);
        double oc[3];
        if (!quadric_optimize(&qm, oc, OPTIMIZE_EPS)) {
            const float* va=mesh.vert(e.v[0]); const float* vb=mesh.vert(e.v[1]);
            oc[0]=(va[0]+vb[0])*0.5; oc[1]=(va[1]+vb[1])*0.5; oc[2]=(va[2]+vb[2])*0.5;
        }
        double cost = fabs(quadric_evaluate(&qm, oc));
        float opt[3] = {(float)oc[0], (float)oc[1], (float)oc[2]};
        int v_clear = e.v[1];
        int v_other = e.v[0];
        float t;
        if (!compare_v3v3(mesh.vert(v_other), mesh.vert(v_clear), FLT_EPSILON)) {
            t = line_point_factor_v3(opt, mesh.vert(v_other), mesh.vert(v_clear));
        } else {
            t = 0.5f;
        }
        if (mesh.has_uv) {
            double uv_seam_error = (double)mesh.edge_uv_seam_error(ei);
            if (uv_seam_error > (double)(UV_EPS * UV_EPS)) {
                double edge_len = (double)len_v3v3(mesh.vert(e.v[0]), mesh.vert(e.v[1]));
                cost += uv_seam_error * std::max(edge_len, 1e-8) * (double)UV_SEAM_COST_WEIGHT;
            }
            cost += edge_quadric5_cost(ei, v_clear, v_other, opt) *
                    (double)TEX_QUADRIC5_WEIGHT;
            cost += mesh.edge_texture_deformation_error(ei, v_clear, v_other, opt, t) *
                    (double)TEX_DEFORMATION_WEIGHT;

            double min_quality = mesh.edge_min_quality_after_collapse(ei, v_clear, v_other, opt);
            if (min_quality < (double)TEX_QUALITY_THRESHOLD && cost > 1e-15) {
                cost *= (double)TEX_QUALITY_THRESHOLD / std::max(min_quality, (double)TEX_MIN_QUALITY);
            }

            int extra_uv_count = mesh.distinct_vertex_uv_count(e.v[0]) +
                                 mesh.distinct_vertex_uv_count(e.v[1]) - 2;
            if (extra_uv_count > 0) {
                cost *= 1.0 + (double)TEX_EXTRA_TCOORD_WEIGHT * (double)extra_uv_count;
            }
        }

        // USE_TOPOLOGY_FALLBACK
        if (cost < (double)TOPOLOGY_FALLBACK_EPS) {
            const float* n0=mesh.vnormal(e.v[0]); const float* n1=mesh.vnormal(e.v[1]);
            double dn = fabs((double)dot_v3v3(n0,n1));
            if (params.vweights) {
                // FIX #2: Use non-squared length for weighted case
                double el = (double)len_v3v3(mesh.vert(e.v[0]),mesh.vert(e.v[1]));
                if(el<1e-10) el=1e-10;
                cost = -dn/el - cost;
                double ew=(double)(params.vweights[e.v[0]]+params.vweights[e.v[1]]);
                if (ew) cost *= 1.0+ew*params.vweight_factor;
            } else {
                double elsq=(double)len_squared_v3v3(mesh.vert(e.v[0]),mesh.vert(e.v[1]));
                if(elsq<1e-10) elsq=1e-10;
                cost = -dn/elsq - cost;
            }
        } else if (params.vweights) {
            double ew = 2.0-(double)(params.vweights[e.v[0]]+params.vweights[e.v[1]]);
            if (ew) cost += (double)len_v3v3(mesh.vert(e.v[0]),mesh.vert(e.v[1]))*ew*params.vweight_factor;
        }
        return (float)cost;
    };

    for (int ei=0;ei<mesh.n_edges();ei++) {
        if (mesh.edges[ei].dead) continue;
        float c = compute_cost(ei);
        if (c < COST_INVALID) {
            edge_version[ei]++;
            eheap.push({c, ei, edge_version[ei]});
        }
    }

    auto push_invalid_edge = [&](int ei) {
        if (ei < 0 || ei >= mesh.n_edges()) return;
        if (mesh.edges[ei].dead) return;
        edge_version[ei]++;
        eheap.push({COST_INVALID, ei, edge_version[ei]});
    };

    struct PendingTexQuadric {
        float uv[2];
        float blender_uv[2];
        float other_uv[2];
        float clear_uv[2];
        Quadric5 q;
    };

    auto merge_tex_quadric_entry = [&](int vi, const float uv[2], const Quadric5& q) {
        if (!mesh.has_uv || vi < 0 || vi >= (int)vtq.size()) return;
        int idx = tex_quadric_find(vi, uv);
        if (idx >= 0) {
            quadric5_add(&vtq[vi][idx].q, &q);
            return;
        }
        VertexTexQuadric entry;
        entry.uv[0] = uv[0];
        entry.uv[1] = uv[1];
        entry.q = q;
        vtq[vi].push_back(entry);
    };

    auto build_pending_tex_quadrics = [&](int ei, int v_clear, int v_other, const float opt[3], float t) {
        std::vector<PendingTexQuadric> pending;
        if (!mesh.has_uv || vtq.empty()) return pending;

        std::vector<QEMMesh::UVEdgePair> edge_pairs;
        mesh.collect_edge_uv_pairs(ei, v_clear, v_other, edge_pairs);
        double geo[3] = {(double)opt[0], (double)opt[1], (double)opt[2]};
        for (const auto& pair : edge_pairs) {
            const Quadric5* q_other = tex_quadric_lookup(v_other, pair.other_uv);
            const Quadric5* q_clear = tex_quadric_lookup(v_clear, pair.clear_uv);
            if (!q_other || !q_clear) continue;

            PendingTexQuadric item;
            item.other_uv[0] = pair.other_uv[0];
            item.other_uv[1] = pair.other_uv[1];
            item.clear_uv[0] = pair.clear_uv[0];
            item.clear_uv[1] = pair.clear_uv[1];
            item.blender_uv[0] = (1.0f - t) * pair.other_uv[0] + t * pair.clear_uv[0];
            item.blender_uv[1] = (1.0f - t) * pair.other_uv[1] + t * pair.clear_uv[1];
            item.uv[0] = item.blender_uv[0];
            item.uv[1] = item.blender_uv[1];
            quadric5_add_two(&item.q, q_other, q_clear);

            double uv_min[2];
            if (quadric5_minimize_uv_with_geo(&item.q, geo, uv_min)) {
                double du = uv_min[0] - (double)item.blender_uv[0];
                double dv = uv_min[1] - (double)item.blender_uv[1];
                double span_u = std::fabs((double)pair.other_uv[0] - (double)pair.clear_uv[0]);
                double span_v = std::fabs((double)pair.other_uv[1] - (double)pair.clear_uv[1]);
                double max_dist2 = std::max(0.25, 16.0 * (span_u*span_u + span_v*span_v));
                if (du*du + dv*dv <= max_dist2) {
                    item.uv[0] = (1.0f - TEX_QUADRIC5_UV_BLEND) * item.blender_uv[0] +
                                 TEX_QUADRIC5_UV_BLEND * (float)uv_min[0];
                    item.uv[1] = (1.0f - TEX_QUADRIC5_UV_BLEND) * item.blender_uv[1] +
                                 TEX_QUADRIC5_UV_BLEND * (float)uv_min[1];
                    bool source_in_unit =
                        pair.other_uv[0] >= 0.0f && pair.other_uv[0] <= 1.0f &&
                        pair.other_uv[1] >= 0.0f && pair.other_uv[1] <= 1.0f &&
                        pair.clear_uv[0] >= 0.0f && pair.clear_uv[0] <= 1.0f &&
                        pair.clear_uv[1] >= 0.0f && pair.clear_uv[1] <= 1.0f;
                    if (source_in_unit) {
                        item.uv[0] = std::min(1.0f, std::max(0.0f, item.uv[0]));
                        item.uv[1] = std::min(1.0f, std::max(0.0f, item.uv[1]));
                    }
                }
            }
            pending.push_back(item);
        }
        return pending;
    };

    auto apply_tex_quadric_merge = [&](int v_clear, int v_other, const std::vector<PendingTexQuadric>& pending) {
        if (!mesh.has_uv || vtq.empty()) return;
        for (const auto& entry : vtq[v_clear]) {
            merge_tex_quadric_entry(v_other, entry.uv, entry.q);
        }
        for (const auto& entry : pending) {
            merge_tex_quadric_entry(v_other, entry.uv, entry.q);
        }
        vtq[v_clear].clear();
    };

    auto apply_tex_uv_after_collapse = [&](int v_other, const std::vector<PendingTexQuadric>& pending) {
        if (!mesh.has_uv || pending.empty()) return;
        for (int fi : mesh.vert_faces[v_other]) {
            if (fi < 0 || fi >= mesh.n_faces() || mesh.faces[fi].dead) continue;
            int ki = mesh.face_local_idx(fi, v_other);
            if (ki < 0) continue;
            float* uv = mesh.face_uv(fi, ki);
            for (const auto& item : pending) {
                if (QEMMesh::uv_equal(uv, item.blender_uv, UV_EPS * 8.0f) ||
                    QEMMesh::uv_equal(uv, item.other_uv, UV_EPS * 8.0f) ||
                    QEMMesh::uv_equal(uv, item.clear_uv, UV_EPS * 8.0f)) {
                    uv[0] = item.uv[0];
                    uv[1] = item.uv[1];
                    break;
                }
            }
        }
    };

    // Step 3: Main loop (matching Blender exactly)
    int collapsed=0;
    std::vector<int> topo_face_tag(mesh.n_faces(), 0);
    std::vector<int> topo_vert_tag(mesh.n_verts(), 0);
    int topo_mark = 1;

    while (live_faces > nf_target && !eheap.empty()) {
        // FIX #22: Early termination on COST_INVALID
        if (eheap.top().cost >= COST_INVALID) break;

        HeapItem top = eheap.top(); eheap.pop();

        // FIX #10: Validate heap entry is not stale
        if (top.version != edge_version[top.edge_idx]) continue;

        int ei = top.edge_idx;
        if (mesh.edges[ei].dead) continue;

        // Recompute cost to validate (Blender trusts heap, but we check for stale)
        float nc = compute_cost(ei);
        if (nc >= COST_INVALID) continue;

        // Compute optimize position
        auto& e = mesh.edges[ei];
        Quadric qm; quadric_add_two(&qm, &vq[e.v[0]], &vq[e.v[1]]);
        double oc[3];
        if (!quadric_optimize(&qm, oc, OPTIMIZE_EPS)) {
            const float* va=mesh.vert(e.v[0]); const float* vb=mesh.vert(e.v[1]);
            oc[0]=(va[0]+vb[0])*0.5; oc[1]=(va[1]+vb[1])*0.5; oc[2]=(va[2]+vb[2])*0.5;
        }
        float opt[3]={(float)oc[0],(float)oc[1],(float)oc[2]};

        // FIX #5: Topology check BEFORE flip check (matches Blender order)
        // Topology degeneration check (disk-walk style, matching Blender)
        auto check_topo = [&](int edge_i) -> bool {
            auto& ed = mesh.edges[edge_i];
            for (int vi : {ed.v[0], ed.v[1]}) {
                for (int ei2 : mesh.vert_edges[vi]) {
                    if (ei2 < 0 || ei2 >= (int)mesh.edges.size() || mesh.edges[ei2].dead) continue;
                    int fc = mesh.edge_face_count(ei2);
                    if (fc != 1 && fc != 2) return true;
                }
            }

            int shared_mark = topo_mark++;
            int v0_face_mark = topo_mark++;
            int v0_neighbor_mark = topo_mark++;
            if (topo_mark > INT_MAX - 8) {
                std::fill(topo_face_tag.begin(), topo_face_tag.end(), 0);
                std::fill(topo_vert_tag.begin(), topo_vert_tag.end(), 0);
                topo_mark = 1;
                shared_mark = topo_mark++;
                v0_face_mark = topo_mark++;
                v0_neighbor_mark = topo_mark++;
            }

            for (int fi : mesh.edge_faces[edge_i]) {
                if (fi < 0 || fi >= mesh.n_faces() || mesh.faces[fi].dead) continue;
                topo_face_tag[fi] = shared_mark;
                for (int k=0; k<3; k++) {
                    int vi = mesh.faces[fi].v[k];
                    if (vi >= 0 && vi < mesh.n_verts()) topo_vert_tag[vi] = shared_mark;
                }
            }

            for (int fi : mesh.vert_faces[ed.v[0]]) {
                if (fi < 0 || fi >= mesh.n_faces() || mesh.faces[fi].dead) continue;
                if (topo_face_tag[fi] != shared_mark) topo_face_tag[fi] = v0_face_mark;
            }
            for (int fi : mesh.vert_faces[ed.v[1]]) {
                if (fi < 0 || fi >= mesh.n_faces() || mesh.faces[fi].dead) continue;
                if (topo_face_tag[fi] != shared_mark && topo_face_tag[fi] == v0_face_mark) {
                    return true;
                }
            }

            for (int ei2 : mesh.vert_edges[ed.v[0]]) {
                if (ei2!=edge_i && ei2<(int)mesh.edges.size() && !mesh.edges[ei2].dead) {
                    int ov = mesh.edges[ei2].other(ed.v[0]);
                    if (ov >= 0 && ov < mesh.n_verts() && !mesh.vert_dead[ov] &&
                        ov != ed.v[1] && topo_vert_tag[ov] != shared_mark) {
                        topo_vert_tag[ov] = v0_neighbor_mark;
                    }
                }
            }
            for (int ei2 : mesh.vert_edges[ed.v[1]]) {
                if (ei2!=edge_i && ei2<(int)mesh.edges.size() && !mesh.edges[ei2].dead) {
                    int ov = mesh.edges[ei2].other(ed.v[1]);
                    if (ov >= 0 && ov < mesh.n_verts() && !mesh.vert_dead[ov] &&
                        ov != ed.v[0] && topo_vert_tag[ov] != shared_mark &&
                        topo_vert_tag[ov] == v0_neighbor_mark) {
                        return true;
                    }
                }
            }
            return false;
        };
        if (check_topo(ei)) {
            push_invalid_edge(ei);
            continue;
        }

        // Flip check
        auto check_flip = [&](int edge_i, const float opt_c[3]) -> bool {
            auto& ed = mesh.edges[edge_i];
            for (int si=0;si<2;si++) {
                int vi = ed.v[si];
                for (int fi : mesh.vert_faces[vi]) {
                    if (mesh.faces[fi].dead) continue;
                    auto& f = mesh.faces[fi];
                    int others[2]; int cnt=0;
                    for (int k=0;k<3;k++) if (f.v[k]!=ed.v[0] && f.v[k]!=ed.v[1] && cnt<2) others[cnt++]=f.v[k];
                    if (cnt<2) continue;
                    const float* pp=mesh.vert(others[0]); const float* pn=mesh.vert(others[1]);
                    float d[3]={pp[0]-pn[0],pp[1]-pn[1],pp[2]-pn[2]};
                    float ve[3]={pp[0]-mesh.vert(vi)[0],pp[1]-mesh.vert(vi)[1],pp[2]-mesh.vert(vi)[2]};
                    float vo[3]={pp[0]-opt_c[0],pp[1]-opt_c[1],pp[2]-opt_c[2]};
                    float ce[3],co[3]; cross_v3_v3v3(ce,d,ve); cross_v3_v3v3(co,d,vo);
                    float deo=dot_v3v3(ce,co);
                    float th=(dot_v3v3(ce,ce)+dot_v3v3(co,co))*0.01f;
                    if (deo<=th) return true;
                }
            }
            return false;
        };
        if (check_flip(ei, opt)) {
            push_invalid_edge(ei);
            continue;
        }

        // Collapse (v_clear = e.v[1], v_other = e.v[0])
        int v_clear = e.v[1];
        int v_other = e.v[0];
        float collapse_t;
        if (!compare_v3v3(mesh.vert(v_other), mesh.vert(v_clear), FLT_EPSILON)) {
            collapse_t = line_point_factor_v3(opt, mesh.vert(v_other), mesh.vert(v_clear));
        } else {
            collapse_t = 0.5f;
        }
        std::vector<PendingTexQuadric> pending_tex =
            build_pending_tex_quadrics(ei, v_clear, v_other, opt, collapse_t);

        // Quadric accumulate
        quadric_add(&vq[v_other], &vq[v_clear]);

        int faces_removed = 0;
        if (mesh.edge_collapse(ei, opt, v_clear, &faces_removed)) {
            collapsed++;
            live_faces -= faces_removed;
            apply_tex_uv_after_collapse(v_other, pending_tex);
            apply_tex_quadric_merge(v_clear, v_other, pending_tex);

            // Weight interpolation (BUG 5 fix: use correct t, not always 0.5)
            if (params.vweights) {
                float t;
                if (!compare_v3v3(mesh.vert(v_other), mesh.vert(v_clear), FLT_EPSILON)) {
                    t = line_point_factor_v3(opt, mesh.vert(v_other), mesh.vert(v_clear));
                } else {
                    t = 0.5f;
                }
                float w = interpf(params.vweights[v_other], params.vweights[v_clear], t);
                if(w<0) w=0; if(w>1) w=1;
                params.vweights[v_other] = w;
            }

            // Incremental update: edges around v_other
            for (int ei2 : mesh.vert_edges[v_other]) {
                if (ei2 >= 0 && ei2 < (int)mesh.edges.size() && !mesh.edges[ei2].dead) {
                    float c = compute_cost(ei2);
                    if (c < COST_INVALID) {
                        edge_version[ei2]++;
                        eheap.push({c, ei2, edge_version[ei2]});
                    }
                }
            }
            // Fan outer edges
            for (int fi : mesh.vert_faces[v_other]) {
                if (fi < 0 || fi >= (int)mesh.faces.size() || mesh.faces[fi].dead) continue;
                auto& f = mesh.faces[fi];
                int loc=-1;
                for (int k=0;k<3;k++) if (f.v[k]==v_other) { loc=k; break; }
                if (loc<0) continue;
                int ov0=f.v[(loc+1)%3], ov1=f.v[(loc+2)%3];
                if (ov0 < 0 || ov0 >= mesh.n_verts() || ov1 < 0 || ov1 >= mesh.n_verts()) continue;
                if (mesh.vert_dead[ov0] || mesh.vert_dead[ov1]) continue;
                EdgeKey ok2=make_ek(ov0,ov1);
                auto it2=mesh.edge_map.find(ok2);
                if (it2!=mesh.edge_map.end() && it2->second >= 0 && it2->second < (int)mesh.edges.size()) {
                    float c=compute_cost(it2->second);
                    if(c<COST_INVALID) {
                        edge_version[it2->second]++;
                        eheap.push({c, it2->second, edge_version[it2->second]});
                    }
                }
            }
        }
        else {
            push_invalid_edge(ei);
        }
    }

    mesh.collect_result(out_verts, out_faces, out_face_uvs);
}

} // namespace blender
