#pragma once
// obj_io.h - OBJ file I/O
// KEY DESIGN: UVs are stored per-face-vertex throughout the entire pipeline.
// OBJ format: f v1/vt1 v2/vt2 v3/vt3 -- each face vertex has its own UV.
// This preserves UV seams correctly.
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <filesystem>

namespace blender {

struct ObjData {
    std::vector<float> verts;       // [x,y,z, ...] per vertex
    std::vector<int> faces;         // [v0,v1,v2, ...] per face (triangulated)
    std::vector<float> face_uvs;    // [u,v, u,v, u,v, ...] per face vertex (3 per face)
    std::vector<int> face_materials;
    std::vector<std::string> material_names;
    std::string mtllib;
    bool has_uvs;
    ObjData() : has_uvs(false) {}
};

inline ObjData load_obj(const char *path) {
    ObjData obj;
    std::ifstream f(path);
    if (!f.is_open()) return obj;

    std::vector<float> raw_v, raw_vt;
    struct FV { int vi, vti; };
    std::vector<std::vector<FV>> raw_faces;
    std::vector<int> raw_face_materials;
    std::unordered_map<std::string, int> material_map;
    int current_material = -1;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0]=='#') continue;
        std::istringstream iss(line); std::string key; iss >> key;
        if (key=="v") {
            float x,y,z; iss>>x>>y>>z;
            raw_v.push_back(x); raw_v.push_back(y); raw_v.push_back(z);
        }
        else if (key=="mtllib") {
            std::string rest;
            std::getline(iss, rest);
            size_t start = rest.find_first_not_of(" \t\r\n\"");
            size_t end = rest.find_last_not_of(" \t\r\n\"");
            if (start != std::string::npos && end != std::string::npos && end >= start) {
                obj.mtllib = rest.substr(start, end - start + 1);
            }
        }
        else if (key=="usemtl") {
            std::string name;
            iss >> name;
            auto it = material_map.find(name);
            if (it == material_map.end()) {
                current_material = (int)obj.material_names.size();
                material_map[name] = current_material;
                obj.material_names.push_back(name);
            }
            else {
                current_material = it->second;
            }
        }
        else if (key=="vt") {
            float u,v; iss>>u>>v;
            raw_vt.push_back(u); raw_vt.push_back(v);
        }
        else if (key=="vn") { /* skip */ }
        else if (key=="f") {
            std::vector<FV> face; std::string tok;
            while (iss>>tok) {
                FV fv; fv.vi=-1; fv.vti=-1;
                size_t p1=tok.find('/');
                if (p1==std::string::npos) {
                    int idx = std::stoi(tok);
                    fv.vi = idx > 0 ? idx-1 : (int)(raw_v.size()/3) + idx;
                } else {
                    int idx = std::stoi(tok.substr(0,p1));
                    fv.vi = idx > 0 ? idx-1 : (int)(raw_v.size()/3) + idx;
                    if (p1+1<tok.size() && tok[p1+1]!='/') {
                        size_t p2=tok.find('/',p1+1);
                        if (p2==std::string::npos) {
                            int tidx = std::stoi(tok.substr(p1+1));
                            fv.vti = tidx > 0 ? tidx-1 : (int)(raw_vt.size()/2) + tidx;
                        } else if (p2>p1+1) {
                            int tidx = std::stoi(tok.substr(p1+1,p2-p1-1));
                            fv.vti = tidx > 0 ? tidx-1 : (int)(raw_vt.size()/2) + tidx;
                        }
                    }
                }
                face.push_back(fv);
            }
            raw_faces.push_back(face);
            raw_face_materials.push_back(current_material);
        }
    }

    int nv = (int)raw_v.size()/3;
    int nvt = (int)raw_vt.size()/2;
    obj.verts = raw_v;
    obj.has_uvs = (nvt > 0);

    // Fan triangulation + per-face-vertex UV collection
    for (size_t face_i=0; face_i<raw_faces.size(); face_i++) {
        auto& face = raw_faces[face_i];
        if (face.size() < 3) continue;

        for (size_t i=1; i+1<face.size(); i++) {
            FV tri[3] = {face[0], face[i], face[i+1]};

            // Add face vertices
            for (int k=0; k<3; k++) {
                obj.faces.push_back(tri[k].vi);
            }

            // Add per-face-vertex UVs (preserves UV seams!)
            if (obj.has_uvs) {
                for (int k=0; k<3; k++) {
                    if (tri[k].vti >= 0 && tri[k].vti < nvt) {
                        obj.face_uvs.push_back(raw_vt[tri[k].vti*2]);
                        obj.face_uvs.push_back(raw_vt[tri[k].vti*2+1]);
                    } else {
                        obj.face_uvs.push_back(0.0f);
                        obj.face_uvs.push_back(0.0f);
                    }
                }
            }
            obj.face_materials.push_back(raw_face_materials[face_i]);
        }
    }
    return obj;
}

inline void save_obj(const char *path, const float *verts, int nv,
                     const int *faces, int nf,
                     const float *face_uvs,
                     const int *face_materials=nullptr,
                     const std::vector<std::string> *material_names=nullptr,
                     const std::string *mtllib=nullptr) {
    std::ofstream f(path);
    f << "# QEM Simplifier output\n";
    if (mtllib && !mtllib->empty()) {
        f << "mtllib " << *mtllib << "\n";
    }

    for (int i=0; i<nv; i++)
        f << "v " << verts[i*3] << " " << verts[i*3+1] << " " << verts[i*3+2] << "\n";
    f << "\n";

    if (face_uvs) {
        // Write per-face-vertex UVs as vt entries
        int total_fv = nf * 3;
        for (int i=0; i<total_fv; i++)
            f << "vt " << face_uvs[i*2] << " " << face_uvs[i*2+1] << "\n";
        f << "\n";

        // Faces reference per-face-vertex UVs
        int vt_idx = 1;
        int current_material = -2;
        for (int i=0; i<nf; i++) {
            if (face_materials && material_names && face_materials[i] >= 0 &&
                face_materials[i] < (int)material_names->size() &&
                face_materials[i] != current_material) {
                current_material = face_materials[i];
                f << "usemtl " << (*material_names)[current_material] << "\n";
            }
            int a=faces[i*3]+1, b=faces[i*3+1]+1, c=faces[i*3+2]+1;
            f << "f " << a << "/" << vt_idx << " "
              << b << "/" << (vt_idx+1) << " "
              << c << "/" << (vt_idx+2) << "\n";
            vt_idx += 3;
        }
    } else {
        int current_material = -2;
        for (int i=0; i<nf; i++) {
            if (face_materials && material_names && face_materials[i] >= 0 &&
                face_materials[i] < (int)material_names->size() &&
                face_materials[i] != current_material) {
                current_material = face_materials[i];
                f << "usemtl " << (*material_names)[current_material] << "\n";
            }
            int a=faces[i*3]+1, b=faces[i*3+1]+1, c=faces[i*3+2]+1;
            f << "f " << a << " " << b << " " << c << "\n";
        }
    }
}

inline void copy_obj_mtl(const std::string& input_obj, const std::string& output_obj, const std::string& mtllib) {
    if (mtllib.empty()) return;
    try {
        std::filesystem::path src = std::filesystem::path(input_obj).parent_path() / mtllib;
        std::filesystem::path dst = std::filesystem::path(output_obj).parent_path() / mtllib;
        if (std::filesystem::exists(src) && src != dst) {
            std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing);
        }
        if (std::filesystem::exists(src)) {
            std::ifstream f(src);
            std::string line;
            while (std::getline(f, line)) {
                std::istringstream iss(line);
                std::string key;
                iss >> key;
                if (key != "map_Kd") continue;
                std::string token, tex_name;
                while (iss >> token) {
                    tex_name = token;
                }
                if (tex_name.empty()) continue;
                std::filesystem::path tex_src = src.parent_path() / tex_name;
                std::filesystem::path tex_dst = dst.parent_path() / tex_name;
                if (std::filesystem::exists(tex_src) && tex_src != tex_dst) {
                    std::filesystem::copy_file(tex_src, tex_dst, std::filesystem::copy_options::overwrite_existing);
                }
            }
        }
    }
    catch (...) {
        // Non-critical: geometry and UV output are still valid without copying the MTL.
    }
}

} // namespace blender
