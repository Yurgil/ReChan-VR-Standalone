#pragma once
#include "p3d/matrix.h"
#include <cmath>

namespace questxr {
// Runtime poses are right-handed metres. Game view is already GL right-handed.
inline Mat4 PoseMatrix(float x, float y, float z, float w, float tx, float ty, float tz) {
    Mat4 r;
    r.m[0]=1-2*(y*y+z*z); r.m[1]=2*(x*y+z*w); r.m[2]=2*(x*z-y*w);
    r.m[4]=2*(x*y-z*w); r.m[5]=1-2*(x*x+z*z); r.m[6]=2*(y*z+x*w);
    r.m[8]=2*(x*z+y*w); r.m[9]=2*(y*z-x*w); r.m[10]=1-2*(x*x+y*y);
    r.SetTranslation(tx,ty,tz);
    return r;
}
inline Mat4 RigidInverse(const Mat4& a) {
    Mat4 r;
    for(int c=0;c<3;++c)for(int row=0;row<3;++row)r(row,c)=a(c,row);
    for(int row=0;row<3;++row)r(row,3)=-(r(row,0)*a(0,3)+r(row,1)*a(1,3)+r(row,2)*a(2,3));
    return r;
}
// OpenGL [-1,+1] depth, reversed Z (clear=0, GEQUAL), asymmetric runtime FOV.
inline Mat4 Projection(float left, float right, float down, float up, float nearZ=0.02f, float farZ=200.0f) {
    const float l=std::tan(left),r=std::tan(right),d=std::tan(down),u=std::tan(up);
    Mat4 p;
    for(auto& v:p.m)v=0;
    p.m[0]=2/(r-l);p.m[5]=2/(u-d);p.m[8]=(r+l)/(r-l);p.m[9]=(u+d)/(u-d);
    p.m[10]=(farZ+nearZ)/(farZ-nearZ);p.m[11]=-1;p.m[14]=2*farZ*nearZ/(farZ-nearZ);
    return p;
}
inline Mat4 Scale(float s) { Mat4 r;r.m[0]=r.m[5]=r.m[10]=s;return r; }
inline Mat4 Translation(float x,float y,float z) { Mat4 r;r.SetTranslation(x,y,z);return r; }
struct ClipPoint { float x,y,z,w; };
inline ClipPoint Clip(const Mat4& m,const float* point) {
    ClipPoint r;
    r.x=m(0,0)*point[0]+m(0,1)*point[1]+m(0,2)*point[2]+m(0,3);
    r.y=m(1,0)*point[0]+m(1,1)*point[1]+m(1,2)*point[2]+m(1,3);
    r.z=m(2,0)*point[0]+m(2,1)*point[1]+m(2,2)*point[2]+m(2,3);
    r.w=m(3,0)*point[0]+m(3,1)*point[1]+m(3,2)*point[2]+m(3,3);
    return r;
}
inline unsigned ClipCode(const ClipPoint& p) {
    return (p.x < -p.w ? 1u:0u)|(p.x > p.w ? 2u:0u)|
           (p.y < -p.w ? 4u:0u)|(p.y > p.w ? 8u:0u)|
           (p.z < -p.w ? 16u:0u)|(p.z > p.w ? 32u:0u);
}
// Conservative homogeneous plane test: intersecting/camera-straddling boxes stay visible.
inline bool BoxInFrustum(const Mat4& m,const float* low,const float* high) {
    unsigned common=63;
    for(int corner=0;corner<8;++corner){
        const float point[]={corner&1?high[0]:low[0],corner&2?high[1]:low[1],corner&4?high[2]:low[2]};
        common &= ClipCode(Clip(m,point));
    }
    return common==0;
}
inline bool PolygonInFrustum(const Mat4& m,const float* xyz,int count,bool cullBackface,bool mirrored=false) {
    unsigned common=63;ClipPoint projected[4];bool crossesEye=false;
    for(int i=0;i<count;++i){projected[i]=Clip(m,xyz+3*i);common&=ClipCode(projected[i]);crossesEye|=projected[i].w<=0.02f;}
    if(common)return false;
    if(!cullBackface||crossesEye)return true;
    const float ax=projected[1].x/projected[1].w-projected[0].x/projected[0].w;
    const float ay=projected[1].y/projected[1].w-projected[0].y/projected[0].w;
    const float bx=projected[2].x/projected[2].w-projected[0].x/projected[0].w;
    const float by=projected[2].y/projected[2].w-projected[0].y/projected[0].w;
    // PSX's retained face has negative screen-Y-down area, hence positive GL NDC area.
    const float area=ax*by-ay*bx;
    return mirrored ? area<=0 : area>=0;
}

}
