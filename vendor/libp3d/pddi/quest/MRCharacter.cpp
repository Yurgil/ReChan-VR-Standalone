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
void MRCharacter::Cancel(){inputSources=0;controlsReady=jumpDown=previousPunch=previousKick=previousRoll=false;velocityX=velocityZ=moveSpeed=actionTime=0;action=MRCharacterAction::None;state=MRCharacterState::InGame;owner=-1;consumed=extracted=false;crossing=true;ready={};previousDown={};visuals={};age=elapsed=returnTime=settleTime=fallSpeed=0;}
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
    // Closest point on the scaled body, including when Jackie is life-size.
    const auto h=Position(hand.grip);V nearest{};
    for(int d=0;d<3;++d){
        float dot=0,norm=0;for(int r=0;r<3;++r){dot+=(h[r]-transform(r,3))*transform(r,d);norm+=transform(r,d)*transform(r,d);}
        nearest[d]=std::clamp(dot/std::max(norm,1e-12f),snapshot.standingLow[d],snapshot.standingHigh[d]);
    }
    const auto p=Point(transform,nearest);return Length({p[0]-h[0],p[1]-h[1],p[2]-h[2]})<.045f;
}
void MRCharacter::Grab(int hand,const MRPointer& p,const PortalPlacement& window,bool fromGame){
    if(fromGame){standingHeight=kMRJackieStandingHeight;snapshot=target;sourceTransform=window.LocalToReference()*target.modelToPortal;transform=sourceTransform;gripPoint=snapshot.grip;extracted=false;
        const float yaw=std::atan2(sourceTransform(0,2)*snapshot.up,sourceTransform(2,2)*snapshot.up);
        canonical=Rotation(yaw)*Scale(standingHeight/snapshot.standingHeight);if(snapshot.up<0)for(int row=0;row<3;++row){canonical(row,1)*=-1;canonical(row,2)*=-1;}
        elapsed=0;
    }
    if(!fromGame){canonical=transform;canonical.SetTranslation(0,0,0);}
    grabTransform=transform;const auto a=Point(transform,gripPoint),h=Position(p.grip);for(int d=0;d<3;++d)anchorOffset[d]=a[d]-h[d];
    inputSources=0;controlsReady=jumpDown=previousPunch=previousKick=previousRoll=false;velocityX=velocityZ=moveSpeed=actionTime=0;action=MRCharacterAction::None;
    initialHandYaw=Yaw(p.grip);owner=hand;state=MRCharacterState::Held;age=settleTime=fallSpeed=0;consumed=true;contact=p.grip;
}
void MRCharacter::BeginReturn(){returnFrom=transform;returnTime=0;state=MRCharacterState::Returning;owner=-1;crossing=true;}
namespace {
constexpr float kWalkSpeed=.25f,kBodyRadius=.012f,kStepHeight=.009f,kGravity=2.4f,kJumpSpeed=.55f;
float ActionDuration(MRCharacterAction action){switch(action){case MRCharacterAction::Punch:return .4f;case MRCharacterAction::Kick:return .65f;case MRCharacterAction::Roll:return .6f;default:return 1;}}
bool BodyBlocked(const MRRoomGeometry& room,V foot,float scale){
    const float radius=kBodyRadius*scale;
    if(!room.ContainsFloorProjection(foot,radius))return true;
    for(float height=.015f; height<=.0901f; height+=.015f)
        if(room.BlocksSphere({foot[0],foot[1]+height*scale,foot[2]},radius))return true;
    return false;
}
float Approach(float current,float target,float maxDelta){return current+std::clamp(target-current,-maxDelta,maxDelta);}
}
V MRCharacter::ModelFoot()const{return {(snapshot.standingLow[0]+snapshot.standingHigh[0])*.5f,snapshot.up>0?snapshot.standingLow[1]:snapshot.standingHigh[1],(snapshot.standingLow[2]+snapshot.standingHigh[2])*.5f};}
void MRCharacter::Face(float yaw){
    const auto foot=Point(transform,ModelFoot());
    transform=Rotation(yaw)*Scale(standingHeight/snapshot.standingHeight);
    if(snapshot.up<0)for(int r=0;r<3;++r){transform(r,1)*=-1;transform(r,2)*=-1;}
    const auto shifted=Point(transform,ModelFoot());for(int d=0;d<3;++d)transform(d,3)+=foot[d]-shifted[d];
}
float MRCharacter::ActionProgress()const{return std::clamp(actionTime/ActionDuration(action),0.f,1.f);}
void MRCharacter::UpdateRoom(const MRRoomGeometry& room,float dt,const MRCharacterInput& raw){
    MRCharacterInput input;
    if(inputSources!=raw.controllers){controlsReady=false;velocityX=velocityZ=0;action=MRCharacterAction::None;inputSources=raw.controllers;}
    if(!raw.valid){controlsReady=false;velocityX=velocityZ=0;action=MRCharacterAction::None;}
    else if(!controlsReady){
        if(std::hypot(raw.moveX,raw.moveY)<.18f&&raw.jumpTrigger<.02f&&std::fabs(raw.resize)<.18f&&!raw.punch&&!raw.kick&&!raw.roll)controlsReady=true;
    }
    if(controlsReady)input=raw;
    if(std::fabs(input.resize)>.2f){
        const float amount=std::copysign((std::min(std::fabs(input.resize),1.f)-.2f)/.8f,input.resize);
        const float wanted=std::clamp(standingHeight*std::exp(amount*1.0f*dt),kMRJackieMinHeight,kMRJackieMaxHeight);
        // Grow around the soles; reject growth into a scanned wall/ceiling.
        if(wanted<=standingHeight||!BodyBlocked(room,Point(transform,ModelFoot()),wanted/kMRJackieStandingHeight)){
            standingHeight=wanted;
            Face(std::atan2(transform(0,2)*snapshot.up,transform(2,2)*snapshot.up));
        }
    }
    const float bodyScale=standingHeight/kMRJackieStandingHeight;
    const float motionScale=std::sqrt(bodyScale),stepHeight=kStepHeight*bodyScale;
    const bool jump=input.jumpTrigger>=(jumpDown?.02f:.05f);
    const bool pressedJump=jump&&!jumpDown,pressedPunch=input.punch&&!previousPunch,pressedKick=input.kick&&!previousKick,pressedRoll=input.roll&&!previousRoll;
    jumpDown=jump;previousPunch=input.punch;previousKick=input.kick;previousRoll=input.roll;
    if(action!=MRCharacterAction::None){actionTime+=dt;if(actionTime>=ActionDuration(action)){action=MRCharacterAction::None;actionTime=0;}}
    if(Grounded()&&pressedJump){fallSpeed=kJumpSpeed*motionScale;state=MRCharacterState::Falling;action=MRCharacterAction::None;}
    else if(Grounded()&&action==MRCharacterAction::None){
        if(pressedRoll)action=MRCharacterAction::Roll;else if(pressedPunch)action=MRCharacterAction::Punch;else if(pressedKick)action=MRCharacterAction::Kick;
        actionTime=0;
    }
    float x=input.moveX,y=input.moveY,length=std::hypot(x,y);
    if(length<=.18f)x=y=0;
    else{const float gain=(std::min(1.f,length)-.18f)/(.82f*length);x*=gain;y*=gain;}
    // Head-relative planar movement; looking down at the table never adds Y motion.
    float rightX=input.headPose(0,0),rightZ=input.headPose(2,0),rightLength=std::hypot(rightX,rightZ);
    if(rightLength<.001f){rightX=1;rightZ=0;}else{rightX/=rightLength;rightZ/=rightLength;}
    float wantedX=(rightX*x+rightZ*y)*kWalkSpeed*motionScale,wantedZ=(rightZ*x-rightX*y)*kWalkSpeed*motionScale;
    if(action==MRCharacterAction::Punch||action==MRCharacterAction::Kick)wantedX=wantedZ=0;
    else if(action==MRCharacterAction::Roll){const float yaw=std::atan2(transform(0,2)*snapshot.up,transform(2,2)*snapshot.up);wantedX=std::sin(yaw)*.38f*motionScale;wantedZ=std::cos(yaw)*.38f*motionScale;}
    const int steps=std::max(1,int(std::ceil(dt*180.f)));const float step=dt/steps;
    float moved=0;
    for(int n=0;n<steps;++n){
        velocityX=Approach(velocityX,wantedX,2.2f*motionScale*step);velocityZ=Approach(velocityZ,wantedZ,2.2f*motionScale*step);
        if(std::hypot(velocityX,velocityZ)>.015f&&action!=MRCharacterAction::Roll){
            const float current=std::atan2(transform(0,2)*snapshot.up,transform(2,2)*snapshot.up);
            float delta=std::remainder(std::atan2(velocityX,velocityZ)-current,6.2831853f);
            Face(current+std::clamp(delta,-12.f*step,12.f*step));
        }
        auto foot=Point(transform,ModelFoot());const auto before=foot;
        auto move=[&](float dx,float dz){
            V candidate={foot[0]+dx,foot[1],foot[2]+dz};MRRoomSupport support;
            if(Grounded()&&room.Support({candidate[0],candidate[1]+stepHeight,candidate[2]},stepHeight+.013f,0,support))candidate[1]=support.point[1]+.001f;
            if(BodyBlocked(room,candidate,bodyScale)){
                if(!Grounded())return false;
                candidate[1]+=stepHeight;
                if(BodyBlocked(room,candidate,bodyScale))return false;
            }
            foot=candidate;return true;
        };
        const float dx=velocityX*step,dz=velocityZ*step;
        if(!move(dx,dz)){if(!move(dx,0))velocityX=0;if(!move(0,dz))velocityZ=0;}
        moved+=std::hypot(foot[0]-before[0],foot[2]-before[2]);
        fallSpeed=std::max(fallSpeed-kGravity*motionScale*step,-1.4f*motionScale);
        const float nextY=foot[1]+fallSpeed*step;
        if(fallSpeed>0){
            if(BodyBlocked(room,{foot[0],nextY,foot[2]},bodyScale))fallSpeed=0;else foot[1]=nextY;
            state=MRCharacterState::Falling;
        }else{
            MRRoomSupport support;
            if(room.Support({foot[0],foot[1]+.003f,foot[2]},foot[1]-nextY+.004f,0,support)){
                foot[1]=support.point[1]+.001f;fallSpeed=0;state=MRCharacterState::Grounded;
            }else{foot[1]=nextY;state=MRCharacterState::Falling;}
        }
        for(int d=0;d<3;++d)transform(d,3)+=foot[d]-before[d];
    }
    moveSpeed=dt>0?moved/dt:0;
    if(!Grounded())action=MRCharacterAction::None;
    for(const auto& surface:room.surfaces)if(surface.floor&&surface.located&&Point(transform,ModelFoot())[1]<surface.pose(1,3)-.3f){Cancel();return;}
}
void MRCharacter::Update(const std::array<MRPointer,2>& p,const PortalPlacement& window,const V& eye,const MRRoomGeometry& room,bool windowBusy,float dt,const MRCharacterInput& input){
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
            UpdateRoom(room,dt,input);
            if(!Active()){consumed=true;return;}
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
