#include "pddi/quest/MRPortalSurface.h"
namespace questxr {
void MRPortalSurface::Reset(){ripples={};contacts={};handValid={};time=0;next=0;}
void MRPortalSurface::Emit(float x,float y,int source,float strength){
    if(std::fabs(x)>kPortalHalfWidth||std::fabs(y)>kPortalHalfHeight)return;
    ripples[next]={x,y,0,strength,source};next=(next+1)%ripples.size();
}
void MRPortalSurface::Sample(Contact& old,std::array<float,3> p,bool valid,float scale,int source,float strength){
    if(!valid){old.valid=false;old.lastTime=-10;return;}
    const float distance=(p[2]+kPortalDepth)*scale;
    const bool near=std::fabs(distance)<.015f;
    bool crossing=false;float x=p[0],y=p[1];
    if(old.valid){
        const float before=(old.local[2]+kPortalDepth)*scale;
        if(before*distance<0&&std::fabs(before-distance)<.25f){
            const float t=before/(before-distance);x=old.local[0]+(p[0]-old.local[0])*t;y=old.local[1]+(p[1]-old.local[1])*t;crossing=true;
        }
    }
    const float dx=(x-old.lastPoint[0])*scale,dy=(y-old.lastPoint[1])*scale;
    if((crossing||(near&&(!old.valid||dx*dx+dy*dy>.0001f)))&&time-old.lastTime>=.18f){
        Emit(x,y,source,strength);old.lastTime=time;old.lastPoint={x,y};
    }
    old.valid=true;old.local=p;
}
void MRPortalSurface::Update(const std::array<MRHandVisual,2>& hands,const PortalPlacement& window,const MRTestObject& object,float dt,const Mat4* characterContact,bool characterHeld){
    dt=std::clamp(dt,0.f,.05f);time+=dt;
    for(auto& ripple:ripples)if(ripple.strength>0){ripple.age+=dt;if(ripple.age>=1.2f)ripple.strength=0;}
    const auto inv=window.ReferenceToLocal();
    for(int hand=0;hand<2;++hand){
        if(handValid[hand]&&!hands[hand].valid)for(auto& ripple:ripples)if(ripple.source==hand)ripple.strength=0;
        handValid[hand]=hands[hand].valid;
        for(int point=0;point<2;++point){
            const auto& joint=hands[hand].joints[point?0:10];
            const auto local=inv*Translation(joint.position[0],joint.position[1],joint.position[2]);
            Sample(contacts[hand*2+point],{local(0,3),local(1,3),local(2,3)},hands[hand].valid&&joint.valid,window.scale,hand,point?.6f:.9f);
        }
    }
    if(!object.Visible()&&!characterContact)for(auto& ripple:ripples)if(ripple.source==2)ripple.strength=0;
    const auto local=inv*(characterContact?*characterContact:object.Transform());
    Sample(contacts[4],{local(0,3),local(1,3),local(2,3)},characterContact?characterHeld:(object.Visible()&&object.Held()),window.scale,2,1.f);
}
}
