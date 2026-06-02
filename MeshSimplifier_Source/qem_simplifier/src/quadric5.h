#pragma once
// quadric5.h - Small 5D quadric used for textured edge-collapse ranking.
#include "quadric.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace blender {

struct Quadric5 {
    double a[15];
    double b[5];
    double c;
};

inline void quadric5_clear(Quadric5 *q) {
    std::memset(q, 0, sizeof(Quadric5));
}

inline double dot_v5v5(const double a[5], const double b[5]) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3] + a[4]*b[4];
}

inline bool normalize_v5(double v[5]) {
    double len = std::sqrt(dot_v5v5(v, v));
    if (len <= 1e-20 || !std::isfinite(len)) return false;
    double inv = 1.0 / len;
    for (int i=0; i<5; i++) v[i] *= inv;
    return true;
}

inline void quadric5_add(Quadric5 *a, const Quadric5 *b) {
    for (int i=0; i<15; i++) a->a[i] += b->a[i];
    for (int i=0; i<5; i++) a->b[i] += b->b[i];
    a->c += b->c;
}

inline void quadric5_add_two(Quadric5 *r, const Quadric5 *a, const Quadric5 *b) {
    for (int i=0; i<15; i++) r->a[i] = a->a[i] + b->a[i];
    for (int i=0; i<5; i++) r->b[i] = a->b[i] + b->b[i];
    r->c = a->c + b->c;
}

inline void quadric5_scale(Quadric5 *q, double s) {
    for (int i=0; i<15; i++) q->a[i] *= s;
    for (int i=0; i<5; i++) q->b[i] *= s;
    q->c *= s;
}

inline void quadric5_from_plane(Quadric5 *q, const double p0[5], const double p1[5], const double p2[5]) {
    quadric5_clear(q);

    double e1[5], e2[5], diff[5];
    for (int i=0; i<5; i++) {
        e1[i] = p1[i] - p0[i];
        diff[i] = p2[i] - p0[i];
    }
    if (!normalize_v5(e1)) return;

    double proj = dot_v5v5(diff, e1);
    for (int i=0; i<5; i++) e2[i] = diff[i] - proj * e1[i];
    if (!normalize_v5(e2)) return;

    double full[5][5];
    for (int i=0; i<5; i++) {
        for (int j=0; j<5; j++) {
            full[i][j] = (i == j ? 1.0 : 0.0) - e1[i]*e1[j] - e2[i]*e2[j];
        }
    }

    q->a[0] = full[0][0];
    q->a[1] = full[0][1];
    q->a[2] = full[0][2];
    q->a[3] = full[0][3];
    q->a[4] = full[0][4];
    q->a[5] = full[1][1];
    q->a[6] = full[1][2];
    q->a[7] = full[1][3];
    q->a[8] = full[1][4];
    q->a[9] = full[2][2];
    q->a[10] = full[2][3];
    q->a[11] = full[2][4];
    q->a[12] = full[3][3];
    q->a[13] = full[3][4];
    q->a[14] = full[4][4];

    double pe1 = dot_v5v5(p0, e1);
    double pe2 = dot_v5v5(p0, e2);
    for (int i=0; i<5; i++) {
        q->b[i] = pe1 * e1[i] + pe2 * e2[i] - p0[i];
    }
    q->c = dot_v5v5(p0, p0) - pe1*pe1 - pe2*pe2;
    if (q->c < 0.0 && q->c > -1e-8) q->c = 0.0;
}

inline void quadric5_sum3(Quadric5 *q5, const Quadric *q3, double u, double v) {
    q5->a[0] += q3->a2;
    q5->a[1] += q3->ab;
    q5->a[2] += q3->ac;
    q5->a[5] += q3->b2;
    q5->a[6] += q3->bc;
    q5->a[9] += q3->c2;

    q5->a[12] += 1.0;
    q5->a[14] += 1.0;

    q5->b[0] += q3->ad;
    q5->b[1] += q3->bd;
    q5->b[2] += q3->cd;
    q5->b[3] -= u;
    q5->b[4] -= v;

    q5->c += q3->d2 + u*u + v*v;
}

inline double quadric5_evaluate(const Quadric5 *q, const double v[5]) {
    double x0=v[0], x1=v[1], x2=v[2], x3=v[3], x4=v[4];
    double value =
        q->a[0]*x0*x0 + 2.0*q->a[1]*x0*x1 + 2.0*q->a[2]*x0*x2 + 2.0*q->a[3]*x0*x3 + 2.0*q->a[4]*x0*x4 +
        q->a[5]*x1*x1 + 2.0*q->a[6]*x1*x2 + 2.0*q->a[7]*x1*x3 + 2.0*q->a[8]*x1*x4 +
        q->a[9]*x2*x2 + 2.0*q->a[10]*x2*x3 + 2.0*q->a[11]*x2*x4 +
        q->a[12]*x3*x3 + 2.0*q->a[13]*x3*x4 +
        q->a[14]*x4*x4 +
        2.0*(q->b[0]*x0 + q->b[1]*x1 + q->b[2]*x2 + q->b[3]*x3 + q->b[4]*x4) +
        q->c;
    return std::fabs(value);
}

inline bool quadric5_minimize_uv_with_geo(const Quadric5 *q, const double geo[3], double uv[2]) {
    double c3 = -(q->b[3] + geo[0]*q->a[3] + geo[1]*q->a[7] + geo[2]*q->a[10]);
    double c4 = -(q->b[4] + geo[0]*q->a[4] + geo[1]*q->a[8] + geo[2]*q->a[11]);
    double det = q->a[12]*q->a[14] - q->a[13]*q->a[13];
    if (std::fabs(det) <= 1e-12) return false;
    uv[0] = (c3*q->a[14] - c4*q->a[13]) / det;
    uv[1] = (q->a[12]*c4 - q->a[13]*c3) / det;
    return std::isfinite(uv[0]) && std::isfinite(uv[1]);
}

} // namespace blender
