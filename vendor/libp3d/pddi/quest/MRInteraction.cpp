#include "pddi/quest/MRInteraction.h"

namespace questxr {
namespace {
using V=std::array<float,3>;
V Position(const Mat4& m){return {m(0,3),m(1,3),m(2,3)};}
V Sub(V a,V b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
V Mid(V a,V b){return {(a[0]+b[0])*.5f,(a[1]+b[1])*.5f,(a[2]+b[2])*.5f};}
float Length(V a){return std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);}
Mat4 T(V a){return Translation(a[0],a[1],a[2]);}
Mat4 Align(V a,V b){
    const float al=Length(a),bl=Length(b);if(al<.0001f||bl<.0001f)return Mat4();
    for(int i=0;i<3;++i){a[i]/=al;b[i]/=bl;}
    float dot=std::clamp(a[0]*b[0]+a[1]*b[1]+a[2]*b[2],-1.f,1.f);
    V cross={a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
    if(dot<-.9999f){V axis=std::fabs(a[0])<.8f?V{1,0,0}:V{0,1,0};cross={a[1]*axis[2]-a[2]*axis[1],a[2]*axis[0]-a[0]*axis[2],a[0]*axis[1]-a[1]*axis[0]};const float l=Length(cross);for(float& v:cross)v/=l;return PoseMatrix(cross[0],cross[1],cross[2],0,0,0,0);}
    const float w=1+dot,l=std::sqrt(w*w+Length(cross)*Length(cross));
    return PoseMatrix(cross[0]/l,cross[1]/l,cross[2]/l,w/l,0,0,0);
}
}
bool MRInteraction::Hit(const MRPointer& p,const PortalPlacement& w,Grip& g)const{
    const Mat4 inv=w.ReferenceToLocal();
    const Mat4 direct=inv*p.grip;
    float x=direct(0,3),y=direct(1,3);
    const float pad=.025f/w.scale;
    auto onFrame=[&](){return std::fabs(x)<=kSlateHalfWidth+pad&&y>=kSlateBottom-pad&&y<=kSlateTop+pad&&!(std::fabs(x)<kPortalHalfWidth-pad&&std::fabs(y)<kPortalHalfHeight-pad);};
    if(std::fabs(direct(2,3)+kPortalDepth)*w.scale<.08f&&onFrame()){
        g.direct=true;g.distance=0;
    }else{
        const Mat4 ray=inv*p.aim;const float dz=-ray(2,2);
        if(std::fabs(dz)<.00001f)return false;
        g.distance=(-kPortalDepth-ray(2,3))/dz;
        if(g.distance<.05f||g.distance>4.f)return false;
        x=ray(0,3)-g.distance*ray(0,2);y=ray(1,3)-g.distance*ray(1,2);
        if(!onFrame())return false;g.direct=false;
    }
    g.resize=std::fabs(x)>kPortalHalfWidth-.06f/w.scale&&(y>kPortalHalfHeight-.06f/w.scale||y< -kPortalHalfHeight+.06f/w.scale);
    g.source=p.source;return true;
}
Mat4 MRInteraction::GripPose(const MRPointer& p,const Grip& g)const{return g.direct?p.grip:p.aim*Translation(0,0,-g.distance);}
void MRInteraction::Cancel(){mask=0;ready={false,false};wasDown={false,false};previousSource={-1,-1};visuals={};consumed=false;}
void MRInteraction::Update(const std::array<MRPointer,2>& p,PortalPlacement& w,const V& head){
    const unsigned old=mask;consumed=old!=0;visuals={};
    for(int i=0;i<2;++i){
        if(!p[i].valid||((mask&(1u<<i))&&p[i].source!=grips[i].source)||!p[i].down)mask&=~(1u<<i);
        if(!p[i].valid||p[i].source!=previousSource[i])ready[i]=false;
        if(p[i].valid&&!p[i].down)ready[i]=true;
        Grip hit;
        const bool hovered=p[i].valid&&Hit(p[i],w,hit);
        const bool pressed=ready[i]&&p[i].down&&!wasDown[i];
        if(!(mask&(1u<<i))&&pressed&&hovered){grips[i]=hit;mask|=1u<<i;}
        if(hovered||(mask&(1u<<i))){
            const Grip& g=(mask&(1u<<i))?grips[i]:hit;
            visuals[i]={Position(g.direct?p[i].grip:p[i].aim),Position(GripPose(p[i],g)),true,(mask&(1u<<i))!=0,g.direct,g.resize};
        }
        wasDown[i]=p[i].down;previousSource[i]=p[i].source;
    }
    consumed|=mask!=0;
    if(mask!=old){baseline=w;for(int i=0;i<2;++i)if(mask&(1u<<i))startPose[i]=GripPose(p[i],grips[i]);return;}
    if(!mask)return;
    if(mask==3){
        const V a=Position(startPose[0]),b=Position(startPose[1]);
        const V c=Position(GripPose(p[0],grips[0])),d=Position(GripPose(p[1],grips[1]));
        const float initial=Length(Sub(b,a)),now=Length(Sub(d,c));
        if(initial<.08f||now<.06f)return;
        const float newScale=std::clamp(baseline.scale*now/initial,.2f,.65f);
        const Mat4 transform=T(Mid(c,d))*Align(Sub(b,a),Sub(d,c))*Scale(newScale/baseline.scale)*T(Sub(V{},Mid(a,b)))*baseline.LocalToReference();
        w.scale=newScale;w.pose=transform*Scale(1/newScale);
    }else{
        const int i=(mask&1)?0:1;const Mat4 current=GripPose(p[i],grips[i]);
        if(grips[i].resize){
            const Mat4 centre=baseline.pose*Translation(0,0,-kPortalDepth*baseline.scale);
            const float initial=Length(Sub(Position(startPose[i]),Position(centre)));
            if(initial>.05f)w.SetCentered(centre,-kPortalDepth,std::clamp(baseline.scale*Length(Sub(Position(current),Position(centre)))/initial,.2f,.65f));
        }else{w.pose=current*RigidInverse(startPose[i])*baseline.pose;w.scale=baseline.scale;}
    }
    // Keep the frame centre at a usable distance; recenter remains a reset.
    const auto centre=Position(w.pose*Translation(0,0,-kPortalDepth*w.scale));
    const auto delta=Sub(centre,head);const float distance=Length(delta);
    if(distance>.0001f&&(distance<.45f||distance>3.f)){
        const float factor=std::clamp(distance,.45f,3.f)/distance-1;
        w.pose.SetTranslation(w.pose(0,3)+factor*delta[0],w.pose(1,3)+factor*delta[1],w.pose(2,3)+factor*delta[2]);
    }
}
}
