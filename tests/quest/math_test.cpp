#include "pddi/quest/QuestMath.h"
#include "pddi/quest/QuestPortal.h"
#include "pddi/quest/PortalPlacement.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace questxr;
static bool close(float a,float b){return std::fabs(a-b)<0.0001f;}
static void checkIdentity(const Mat4& a){for(int c=0;c<4;++c)for(int r=0;r<4;++r)assert(close(a(r,c),r==c?1.f:0.f));}
static float ndc(const Mat4& a, float x,float y,float z,int row){return (a(row,0)*x+a(row,1)*y+a(row,2)*z+a(row,3))/(a(3,0)*x+a(3,1)*y+a(3,2)*z+a(3,3));}
int main(){
    // Rigid inverse must undo both orientation and translated poses.
    const float half=std::sqrt(0.5f);
    auto pose=PoseMatrix(0,half,0,half,1.2f,1.6f,-0.3f);
    checkIdentity(RigidInverse(pose)*pose);checkIdentity(pose*RigidInverse(pose));
    // A moved/scaled portal has one inverse shared by rendering and hit/clipping.
    PortalPlacement placement;
    for(const float scale:{.2f,1.f,2.f}){
        placement.Reset(pose);placement.SetScale(scale);
        checkIdentity(placement.ReferenceToLocal()*placement.LocalToReference());
        for(const auto local:{std::array<float,3>{-1.6f,-1.2f,-2.4f},{1.6f,1.2f,-2.4f},{0,0,-5}}){
            const auto room=Clip(placement.LocalToReference(),local.data());
            const float p[]={room.x,room.y,room.z};
            const auto restored=Clip(placement.ReferenceToLocal(),p);
            assert(close(restored.x,local[0])&&close(restored.y,local[1])&&close(restored.z,local[2]));
        }
    }
    placement.Reset(Mat4());assert(close(placement.scale,1));checkIdentity(placement.LocalToReference());
    // Changing MR window size cannot move its centre toward/away from the user.
    const Mat4 centre=pose*Translation(0,0,-1.2f);
    const float apertureCentre[]={0,0,-kPortalDepth};
    for(const float scale:{.2f,.3f,.65f}){
        placement.SetCentered(centre,-kPortalDepth,scale);
        const auto p=Clip(placement.LocalToReference(),apertureCentre);
        assert(close(p.x,centre(0,3))&&close(p.y,centre(1,3))&&close(p.z,centre(2,3)));
        checkIdentity(placement.ReferenceToLocal()*placement.LocalToReference());
    }
    assert(close(pose(0,2),1));assert(close(pose(2,0),-1));
    // Both asymmetric frustum edges must land on their respective NDC edge.
    const float l=-0.7f,r=0.9f,d=-0.8f,u=0.6f,n=0.02f,f=200;
    auto p=Projection(l,r,d,u,n,f);
    assert(close(ndc(p,n*std::tan(l),0,-n,0),-1));
    assert(close(ndc(p,n*std::tan(r),0,-n,0),1));
    assert(close(ndc(p,0,n*std::tan(d),-n,1),-1));
    assert(close(ndc(p,0,n*std::tan(u),-n,1),1));
    assert(close(ndc(p,0,0,-n,2),1));assert(close(ndc(p,0,0,-f,2),-1));
    // Real IPD: a central near object has crossed disparity, decreasing with distance.
    auto symmetric=Projection(-0.785398f,0.785398f,-0.785398f,0.785398f);
    auto left=symmetric*Translation(0.032f,0,0),right=symmetric*Translation(-0.032f,0,0);
    const float nearDisparity=ndc(left,0,0,-1,0)-ndc(right,0,0,-1,0);
    const float farDisparity=ndc(left,0,0,-4,0)-ndc(right,0,0,-4,0);
    assert(nearDisparity>0);assert(close(nearDisparity,4*farDisparity));
    // World placement composes translation after scaling, preserving placement in metres.
    auto world=Translation(1,2,-3)*Scale(0.001f);
    assert(close(world(0,0),0.001f));assert(close(world(0,3),1));
    // Union-of-eyes culling must preserve a box visible only to the right eye.
    const float low[]={1.015f,-.005f,-1.001f},high[]={1.025f,.005f,-.999f};
    assert(!BoxInFrustum(left,low,high));assert(BoxInFrustum(right,low,high));
    const float behindLow[]={-.1f,-.1f,1},behindHigh[]={.1f,.1f,2};
    assert(!BoxInFrustum(symmetric,behindLow,behindHigh));
    const float crossingLow[]={-.1f,-.1f,-.1f},crossingHigh[]={.1f,.1f,.1f};
    assert(BoxInFrustum(symmetric,crossingLow,crossingHigh));
    const float frontFace[]={-.1f,-.1f,-1, .1f,-.1f,-1, 0,.1f,-1};
    const float backFace[]={-.1f,-.1f,-1, 0,.1f,-1, .1f,-.1f,-1};
    assert(PolygonInFrustum(symmetric,frontFace,3,true));assert(!PolygonInFrustum(symmetric,backFace,3,true));
    Mat4 mirrored; mirrored.m[0]=-1;
    assert(PolygonInFrustum(symmetric*mirrored,frontFace,3,true,true));
    assert(!PolygonInFrustum(symmetric*mirrored,backFace,3,true,true));
    // Background remains window-clipped, with per-eye parallax at the rim.
    const float eye[]={0,0,0},inside[]={0,0,-5},outside[]={4,0,-4},foreground[]={0,0,-1};
    assert(PortalRayVisible(eye,inside));assert(!PortalRayVisible(eye,outside));
    assert(!PortalRayVisible(eye,foreground)); // never allow unbounded near-eye geometry
    const float eyeL[]={-.032f,0,0},eyeR[]={.032f,0,0},rim[]={2.66f,0,-4};
    assert(PortalRayVisible(eyeL,rim));assert(!PortalRayVisible(eyeR,rim));
    const float movedEye[]={1,0,0},revealed[]={-3,0,-4};
    assert(!PortalRayVisible(eye,revealed));assert(PortalRayVisible(movedEye,revealed));
    const float behindSlate[]={0,0,-3};assert(!PortalRayVisible(behindSlate,inside));
    // All foreground/outside fragments are rejected as in 0.3.0, for both eyes.
    const float outsideFront[]={3,-1.3f,-2},outsideBack[]={3,-1.3f,-3};
    for(const float* viewEye:{eyeL,eyeR,movedEye}) {
        assert(!PortalRayVisible(viewEye,outsideFront));
        assert(!PortalRayVisible(viewEye,outsideBack));
    }
    const float atRim[]={kPortalHalfWidth,0,-kPortalDepth};
    assert(PortalRayVisible(eye,atRim));
    // Original PSX camera intrinsics must fit the physical frame, at multiple depths/zooms.
    for(const float tanY:{.15f,.3f,.55f}) {
        const Mat4 original=PerspectiveReversedZTangents(tanY*4/3,tanY,1,65535);
        const Mat4 optics=CameraOptics(original);
        for(const float depth:{3.f,5.f,12.f}) {
            const float x=.08f*depth,y=.04f*depth;
            const float windowX=kPortalDepth*optics.m[0]*x/depth/kPortalHalfWidth;
            const float windowY=kPortalDepth*optics.m[5]*y/depth/kPortalHalfHeight;
            assert(close(windowX,ndc(original,x,y,-depth,0)));
            assert(close(windowY,ndc(original,x,y,-depth,1)));
        }
    }
    static_assert(sizeof(Mat4)==16*sizeof(float));
    std::puts("Quest math: pose inverse, asymmetric FOV, reversed Z, stereo disparity, placement, binocular culling and diorama aperture OK");
}
