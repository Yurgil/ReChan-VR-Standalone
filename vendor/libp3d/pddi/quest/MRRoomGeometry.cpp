#include "pddi/quest/MRRoomGeometry.h"
#include <algorithm>
namespace questxr {
namespace {
using V=std::array<float,3>;
V Point(const Mat4& m,V p){V r{};for(int i=0;i<3;++i)r[i]=m(i,0)*p[0]+m(i,1)*p[1]+m(i,2)*p[2]+m(i,3);return r;}
V Direction(const Mat4& m,V p){V r{};for(int i=0;i<3;++i)r[i]=m(i,0)*p[0]+m(i,1)*p[1]+m(i,2)*p[2];return r;}
}
bool MRRoomGeometry::Contains(const std::vector<std::array<float,2>>& p,float x,float y,float margin){
    if(p.size()<3)return false;bool inside=false;float closest=1e20f;
    for(size_t i=0,j=p.size()-1;i<p.size();j=i++){
        const auto a=p[j],b=p[i];const float dx=b[0]-a[0],dy=b[1]-a[1],length=dx*dx+dy*dy;
        const float t=length>1e-10f?std::clamp(((x-a[0])*dx+(y-a[1])*dy)/length,0.f,1.f):0;
        const float ex=x-a[0]-t*dx,ey=y-a[1]-t*dy;closest=std::min(closest,ex*ex+ey*ey);
        if((a[1]>y)!=(b[1]>y)&&x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0])inside=!inside;
    }
    return (inside||closest<1e-10f)&&closest+1e-10f>=margin*margin;
}
bool MRRoomGeometry::Support(const V& origin,float maxDrop,float radius,MRRoomSupport& result)const{
    if(!ready||maxDrop<0)return false;bool found=false;float best=maxDrop;
    for(size_t index=0;index<surfaces.size();++index){
        const auto& s=surfaces[index];if(!s.located||s.wall)continue;
        const auto inv=RigidInverse(s.pose);const auto o=Point(inv,origin),d=Direction(inv,{0,-1,0});
        float t=-1;V n{};
        if(s.volume){
            float enter=-1e20f,exit=1e20f;int face=-1;float sign=0;
            for(int axis=0;axis<3;++axis){
                if(std::fabs(d[axis])<1e-7f){if(o[axis]<s.low[axis]||o[axis]>s.low[axis]+s.extent[axis]){exit=-1e20f;break;}continue;}
                float near=(s.low[axis]-o[axis])/d[axis],far=(s.low[axis]+s.extent[axis]-o[axis])/d[axis],normal=-1;
                if(near>far){std::swap(near,far);normal=1;}
                if(near>enter){enter=near;face=axis;sign=normal;}exit=std::min(exit,far);
            }
            if(face<0||enter<-.0001f||enter>exit)continue;t=std::max(0.f,enter);n[face]=sign;
            // Keep support away from edges, so a miniature does not balance on
            // a point outside a scanned tabletop.
            for(int axis=0;axis<3;++axis)if(axis!=face){const float hit=o[axis]+t*d[axis];if(hit<s.low[axis]+radius||hit>s.low[axis]+s.extent[axis]-radius)t=-1;}
        }else{
            if(std::fabs(d[2])<1e-7f)continue;t=-o[2]/d[2];n={0,0,s.pose(1,2)>=0?1.f:-1.f};
            if(!Contains(s.boundary,o[0]+t*d[0],o[1]+t*d[1],radius))continue;
        }
        if(t<-.0001f||t>best)continue;n=Direction(s.pose,n);if(n[1]<.7f)continue;
        best=std::max(0.f,t);result={V{origin[0],origin[1]-best,origin[2]},n,best,int(index)};found=true;
    }
    return found;
}
bool MRRoomGeometry::BlocksSphere(const V& centre,float radius)const{
    if(!ready)return false;
    for(const auto& s:surfaces){
        if(!s.located)continue;const auto p=Point(RigidInverse(s.pose),centre);
        if(s.volume){float distance=0;for(int axis=0;axis<3;++axis){const float delta=p[axis]-std::clamp(p[axis],s.low[axis],s.low[axis]+s.extent[axis]);distance+=delta*delta;}if(distance<radius*radius)return true;}
        else if(s.wall&&std::fabs(p[2])<radius+.015f&&Contains(s.boundary,p[0],p[1]))return true;
    }
    return false;
}
}
