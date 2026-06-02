#pragma once
// quadric.h - Quadric Error Metric (from BLI_quadric.h + quadric.cc)
#include "math_utils.h"
#include <cstring>

namespace blender {

struct Quadric {
    double a2, ab, ac, ad;
    double      b2, bc, bd;
    double           c2, cd;
    double                d2;
};

inline void quadric_clear(Quadric *q) { memset(q, 0, sizeof(Quadric)); }

inline void quadric_from_plane(Quadric *q, const double v[4]) {
    q->a2=v[0]*v[0]; q->b2=v[1]*v[1]; q->c2=v[2]*v[2];
    q->ab=v[0]*v[1]; q->ac=v[0]*v[2]; q->bc=v[1]*v[2];
    q->ad=v[0]*v[3]; q->bd=v[1]*v[3]; q->cd=v[2]*v[3];
    q->d2=v[3]*v[3];
}

inline void quadric_to_vector_v3(const Quadric *q, double v[3]) { v[0]=q->ad; v[1]=q->bd; v[2]=q->cd; }
inline void quadric_add(Quadric *a, const Quadric *b) { add_vn_vn_d((double*)a, (const double*)b, 10); }
inline void quadric_add_two(Quadric *r, const Quadric *a, const Quadric *b) { add_vn_vnvn_d((double*)r, (const double*)a, (const double*)b, 10); }
inline void quadric_mul(Quadric *a, double s) { mul_vn_db((double*)a, 10, s); }

inline double quadric_evaluate(const Quadric *q, const double v[3]) {
    double x=v[0],y=v[1],z=v[2];
    return (q->a2*x*x + q->ab*2*x*y + q->ac*2*x*z + q->ad*2*x +
            q->b2*y*y + q->bc*2*y*z + q->bd*2*y +
            q->c2*z*z + q->cd*2*z + q->d2);
}

inline bool quadric_optimize(const Quadric *q, double v[3], double epsilon) {
    double det = (q->a2*(q->b2*q->c2 - q->bc*q->bc) -
                  q->ab*(q->ab*q->c2 - q->ac*q->bc) +
                  q->ac*(q->ab*q->bc - q->ac*q->b2));
    if (fabs(det) <= epsilon) return false;
    double invdet = 1.0 / det;
    double m[3][3];
    m[0][0]=(q->b2*q->c2 - q->bc*q->bc)*invdet; m[1][0]=(q->bc*q->ac - q->ab*q->c2)*invdet; m[2][0]=(q->ab*q->bc - q->b2*q->ac)*invdet;
    m[0][1]=(q->ac*q->bc - q->ab*q->c2)*invdet; m[1][1]=(q->a2*q->c2 - q->ac*q->ac)*invdet; m[2][1]=(q->ab*q->ac - q->a2*q->bc)*invdet;
    m[0][2]=(q->ab*q->bc - q->ac*q->b2)*invdet; m[1][2]=(q->ac*q->ab - q->a2*q->bc)*invdet; m[2][2]=(q->a2*q->b2 - q->ab*q->ab)*invdet;
    quadric_to_vector_v3(q, v);
    m3_v3_mul(v, m, v);
    d3_negate(v);
    return true;
}

} // namespace blender
