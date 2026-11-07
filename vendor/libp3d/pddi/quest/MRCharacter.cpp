#include "pddi/quest/MRCharacter.h"
#include "pddi/quest/MRTransferMath.h"
namespace questxr {
namespace {
using V=std::array<float,3>;
V Point(const Mat4& m,V p){V r{};for(int i=0;i<3;++i)r[i]=m(i,0)*p[0]+m(i,1)*p[1]+m(i,2)*p[2]+m(i,3);return r;}
V Position(const Mat4& m){return {m(0,3),m(1,3),m(2,3)};}
float Length(V p){return std::sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);}
float Yaw(const Mat4& pose){return std::atan2(pose(0,2),pose(2,2));}
Mat4 Rotation(float yaw){return PoseMatrix(0,std::sin(yaw*.5f),0,std::cos(yaw*.5f),0,0,0);}
Mat4 Mix(const Mat4& a,const Mat4& b,float t){Mat4 m;for(int i=0;i<16;++i)m.m[i]=a.m[i]+(b.m[i]-a.m[i])*t;return m;}
bool Aperture(const Mat4& p){return std::fabs(p(0,3))<=kPortalHalfWidth&&std::fabs(p(1,3))<=kPortalHalfHeight;}
}
void MRCharacter::SetTarget(const MRCharacterTarget& value){if(Active()&&(!value.valid||value.identity!=snapshot.identity))Cancel();target=value;}
void MRCharacter::Invalidate(){Cancel();target={};}
void MRCharacter::Cancel(){state=MRCharacterState::InGame;owner=-1;consumed=extracted=false;crossing=true;ready={};previousDown={};visuals={};age=elapsed=returnTime=settleTime=fallSpeed=0;}
bool MRCharacter::PickPortal(const MRPointer& hand,const PortalPlacement& window,const V& eye,V& hit)const{
    if(!target.valid||!hand.valid||hand.source!=1)return false;
    const auto inv=window.ReferenceToLocal();const V e=Point(inv,eye);const auto h=inv*hand.grip;
    const float distance=(h(2,3)+kPortalDepth)*window.scale;
    if(distance>.045f||distance<-.16f||!Aperture(h))return false;
    const float dz=h(2,3)-e[2];if(std::fabs(dz)<1e-6f)return false;
    const float t=(-kPortalDepth-e[2])/dz;if(t<0)return false;
    const float x=e[0]+t*(h(0,3)-e[0]),y=e[1]+t*(h(1,3)-e[1]);
    if(std::fabs(x)>kPortalHalfWidth||std::fabs(y)>kPortalHalfHeight)return false;
    float lowX=1e20f,lowY=1e20f,highX=-1e20f,highY=-1e20f;int visible=0;
    for(int corner=0;corner<8;++corner){
        V p{};for(int d=0;d<3;++d)p[d]=(corner&(1<<d))?target.high[d]:target.low[d];p=Point(target.modelToPortal,p);
        const float z=p[2]-e[2];if(std::fabs(z)<1e-6f)continue;const float q=(-kPortalDepth-e[2])/z;if(q<0||q>1)continue;
        const float px=e[0]+q*(p[0]-e[0]),py=e[1]+q*(p[1]-e[1]);lowX=std::min(lowX,px);highX=std::max(highX,px);lowY=std::min(lowY,py);highY=std::max(highY,py);++visible;
    }
    const float pad=.015f/window.scale;
    if(!visible||x<lowX-pad||x>highX+pad||y<lowY-pad||y>highY+pad)return false;
    hit=Point(window.LocalToReference(),{x,y,-kPortalDepth});return true;
}
bool MRCharacter::PickRoom(const MRPointer& hand)const{
    if(!hand.valid||hand.source!=1)return false;
    V centre{};for(int d=0;d<3;++d)centre[d]=(snapshot.standingLow[d]+snapshot.standingHigh[d])*.5f;
    const auto p=Point(transform,centre),h=Position(hand.grip);return Length({p[0]-h[0],p[1]-h[1],p[2]-h[2]})<.065f;
}
void MRCharacter::Grab(int hand,const MRPointer& p,const PortalPlacement& window,bool fromGame){
    if(fromGame){snapshot=target;sourceTransform=window.LocalToReference()*target.modelToPortal;transform=sourceTransform;gripPoint=snapshot.grip;extracted=false;
        const float yaw=std::atan2(sourceTransform(0,2)*snapshot.up,sourceTransform(2,2)*snapshot.up);
        canonical=Rotation(yaw)*Scale(.07f/snapshot.standingHeight);if(snapshot.up<0)for(int row=0;row<3;++row){canonical(row,1)*=-1;canonical(row,2)*=-1;}
        elapsed=0;
    }
    if(!fromGame){canonical=transform;canonical.SetTranslation(0,0,0);}
    grabTransform=transform;const auto a=Point(transform,gripPoint),h=Position(p.grip);for(int d=0;d<3;++d)anchorOffset[d]=a[d]-h[d];
    initialHandYaw=Yaw(p.grip);owner=hand;state=MRCharacterState::Held;age=settleTime=fallSpeed=0;consumed=true;contact=p.grip;
}
void MRCharacter::BeginReturn(){returnFrom=transform;returnTime=0;state=MRCharacterState::Returning;owner=-1;crossing=true;}
void MRCharacter::Update(const std::array<MRPointer,2>& p,const PortalPlacement& window,const V& eye,const MRRoomGeometry& room,bool windowBusy,float dt){
    dt=std::clamp(dt,0.f,.05f);consumed=Active();visuals={};elapsed+=dt;
    if(Active()&&!room.ready){Cancel();consumed=true;return;}
    if(state==MRCharacterState::Returning){returnTime+=dt;transform=Mix(returnFrom,sourceTransform,TransferEase(returnTime/.3f));if(returnTime>=.3f){Cancel();consumed=true;return;}}
    else if(state==MRCharacterState::Held){
        const auto& hand=p[owner];if(!hand.valid||hand.source!=1){Cancel();consumed=true;return;}
        age+=dt;contact=hand.grip;const auto local=window.ReferenceToLocal()*hand.grip;const float distance=TransferDistance(window,hand.grip);
        crossing=Aperture(local)&&distance<.06f;
        if(!extracted&&!Aperture(local)&&distance<0){BeginReturn();return;}
        const float amount=crossing?TransferEase((distance+.04f)/.10f):1;
        Mat4 basis=Mix(sourceTransform,Rotation(Yaw(hand.grip)-initialHandYaw)*canonical,amount);
        transform=Mix(grabTransform,basis,TransferEase(age/.18f));
        const auto h=Position(hand.grip);V a{};for(int d=0;d<3;++d)a[d]=h[d]+anchorOffset[d]*(1-TransferEase(age/.25f));
        const auto grip=Point(transform,gripPoint);for(int d=0;d<3;++d)transform(d,3)+=a[d]-grip[d];
        if(distance>=.06f&&age>=.25f)extracted=true;
        if(!hand.down){owner=-1;if(crossing&&distance<0)BeginReturn();else{state=MRCharacterState::Falling;extracted=true;crossing=false;settleTime=0;grabTransform=transform;canonical=Rotation(Yaw(hand.grip)-initialHandYaw)*canonical;}}
    }else if(state==MRCharacterState::Falling||state==MRCharacterState::Grounded){
        if(settleTime<.16f){
            settleTime+=dt;const auto grip=Point(transform,gripPoint);transform=Mix(grabTransform,canonical,TransferEase(settleTime/.16f));const auto shifted=Point(transform,gripPoint);for(int d=0;d<3;++d)transform(d,3)+=grip[d]-shifted[d];
        }else{
            fallSpeed=std::max(fallSpeed-1.4f*dt,-1.f);V foot={(snapshot.standingLow[0]+snapshot.standingHigh[0])*.5f,snapshot.up>0?snapshot.standingLow[1]:snapshot.standingHigh[1],(snapshot.standingLow[2]+snapshot.standingHigh[2])*.5f};foot=Point(transform,foot);
            MRRoomSupport support;
            if(room.Support({foot[0],foot[1]+.007f,foot[2]},-fallSpeed*dt+.009f,.008f,support)){transform(1,3)+=support.point[1]+.001f-foot[1];fallSpeed=0;state=MRCharacterState::Grounded;}
            else{transform(1,3)+=fallSpeed*dt;state=MRCharacterState::Falling;}
            for(const auto& surface:room.surfaces)if(surface.floor&&surface.located&&transform(1,3)<surface.pose(1,3)-.3f){Cancel();consumed=true;return;}
        }
    }
    for(int i=0;i<2;++i){
        const bool valid=p[i].valid&&p[i].source==1;
        if(!valid)ready[i]=false;else if(!p[i].down)ready[i]=true;
        V point{};const bool hovering=!windowBusy&&valid&&room.ready&&(state==MRCharacterState::InGame?PickPortal(p[i],window,eye,point):((state==MRCharacterState::Falling||state==MRCharacterState::Grounded)&&PickRoom(p[i])));
        if(hovering){if(state!=MRCharacterState::InGame)point=Position(p[i].grip);visuals[i]={Position(p[i].grip),point,true,false,true,false};}
        if(hovering&&ready[i]&&p[i].down&&!previousDown[i]){Grab(i,p[i],window,state==MRCharacterState::InGame);visuals[i].active=true;}
        previousDown[i]=p[i].down;
    }
}
}
