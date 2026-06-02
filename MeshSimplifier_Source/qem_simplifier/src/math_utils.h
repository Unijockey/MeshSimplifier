#pragma once
// math_utils.h - Inline math (replaces BLI_math)
#include <cmath>
#include <cstring>

namespace blender {

inline void copy_v3_v3(float dst[3], const float src[3]) { dst[0]=src[0]; dst[1]=src[1]; dst[2]=src[2]; }
inline float dot_v3v3(const float a[3], const float b[3]) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
inline float len_v3v3(const float a[3], const float b[3]) {
    float d[3]={a[0]-b[0],a[1]-b[1],a[2]-b[2]}; return sqrtf(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
}
inline float len_squared_v3v3(const float a[3], const float b[3]) {
    float d[3]={a[0]-b[0],a[1]-b[1],a[2]-b[2]}; return d[0]*d[0]+d[1]*d[1]+d[2]*d[2];
}
inline void cross_v3_v3v3(float r[3], const float a[3], const float b[3]) {
    r[0]=a[1]*b[2]-a[2]*b[1]; r[1]=a[2]*b[0]-a[0]*b[2]; r[2]=a[0]*b[1]-a[1]*b[0];
}
inline float normalize_v3(float v[3]) {
    float l=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if(l>1e-20f){float inv=1.0f/l;v[0]*=inv;v[1]*=inv;v[2]*=inv;}
    else {v[0]=0;v[1]=0;v[2]=1;} // safe fallback
    return l;
}
inline void interp_v3_v3v3(float r[3], const float a[3], const float b[3], float t) {
    float s=1.0f-t; r[0]=s*a[0]+t*b[0]; r[1]=s*a[1]+t*b[1]; r[2]=s*a[2]+t*b[2];
}
inline float interpf(float a, float b, float t) { return a+(b-a)*t; }
inline float line_point_factor_v3(const float p[3], const float a[3], const float b[3]) {
    float d[3]={b[0]-a[0],b[1]-a[1],b[2]-a[2]};
    float lsq=d[0]*d[0]+d[1]*d[1]+d[2]*d[2];
    if(lsq<1e-20f) return 0.5f;
    float result = ((p[0]-a[0])*d[0]+(p[1]-a[1])*d[1]+(p[2]-a[2])*d[2])/lsq;
    // Don't clamp - matches Blender behavior
    return result;
}
inline bool compare_v3v3(const float a[3], const float b[3], float eps) {
    return (fabsf(a[0]-b[0])<=eps && fabsf(a[1]-b[1])<=eps && fabsf(a[2]-b[2])<=eps);
}

// double ops for Quadric
inline void d3_negate(double v[3]) { v[0]=-v[0]; v[1]=-v[1]; v[2]=-v[2]; }
inline void m3_v3_mul(double r[3], const double m[3][3], const double v[3]) {
    double x=v[0],y=v[1],z=v[2];
    r[0]=m[0][0]*x+m[1][0]*y+m[2][0]*z;
    r[1]=m[0][1]*x+m[1][1]*y+m[2][1]*z;
    r[2]=m[0][2]*x+m[1][2]*y+m[2][2]*z;
}
inline void add_vn_vn_d(double *dst, const double *src, int n) { for(int i=0;i<n;i++) dst[i]+=src[i]; }
inline void add_vn_vnvn_d(double *r, const double *a, const double *b, int n) { for(int i=0;i<n;i++) r[i]=a[i]+b[i]; }
inline void mul_vn_db(double *v, int n, double s) { for(int i=0;i<n;i++) v[i]*=s; }

} // namespace blender
