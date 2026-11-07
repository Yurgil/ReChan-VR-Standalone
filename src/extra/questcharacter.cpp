#include "extra/questcharacter.h"
#if defined(RC_PLATFORM_QUEST)
#include "pddi/quest/QuestXR.h"
#include "pddi/quest/MRTransferMath.h"
#include "gen/model.h"
#include "gen/animstruct.h"
#include "gen/charmgr.h"
#include "gen/game.h"
#include "gen/lights.h"
#include "gen/envmgr.h"
#include "ai/player.h"
#include "p3d/context.h"
#include "p3d/skeleton.h"
#include "pddi/pddidev.h"
#include "pc/log.h"
#include <GLES3/gl3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>
namespace questcharacter {
namespace {
using V=std::array<float,3>;
SModel* source=nullptr;
SModel* clone=nullptr;
OriginalSTree* asset=nullptr;
questxr::MRCharacterTarget reference;
std::vector<STreeJoint> captured;
int torso=-1,animation=-1;
float animationTime=0,lastElapsed=0;
bool wasActive=false,wasGrounded=false;

V Point(const Mat4& m,V p){V v;Mat4TransformPoint(m,p[0],p[1],p[2],v[0],v[1],v[2]);return v;}
STreeData* Skeleton(const SModel* model){auto* t=GetActiveSTree(model?model->drawable:nullptr);return t?t->skeleton:nullptr;}
bool Bounds(const SModel* model,V& low,V& high,V* chest=nullptr){
    auto* skel=Skeleton(model);auto* original=model&&model->drawable?model->drawable->GetOriginalSTree():nullptr;
    auto* skin=original?original->skinData:nullptr;
    if(!skel||!skel->numJoints||!skin||!skin->numVerts)return false;
    std::vector<Mat4> matrices(skel->numJoints);skel->ComputeWorldMatrices(matrices.data());
    low={1e20f,1e20f,1e20f};high={-1e20f,-1e20f,-1e20f};
    for(u32 i=0;i<skin->numVerts;++i){const auto& v=skin->verts[i];if(v.jointIdx>=matrices.size())continue;const auto p=Point(matrices[v.jointIdx],{v.lx,v.ly,v.lz});for(int d=0;d<3;++d){low[d]=std::min(low[d],p[d]);high[d]=std::max(high[d],p[d]);}}
    if(chest){if(torso>=0&&torso<int(matrices.size()))*chest=Point(matrices[torso],{0,0,0});else for(int d=0;d<3;++d)(*chest)[d]=(low[d]+high[d])*.5f;}
    return high[1]>low[1]&&std::isfinite(high[1]-low[1]);
}
void Destroy(){
    auto* old=clone;source=nullptr;clone=nullptr;asset=nullptr;captured.clear();torso=animation=-1;wasActive=wasGrounded=false;
    questxr::InvalidateCharacter();questxr::SetCharacterDrawCallback(nullptr);
    delete old;
}
void SetAnimation(int id){
    if(animation==id)return;
    clone->ApplyAnimToModel(0,id,ANIM_LOOP,0,0);animation=id;animationTime=0;
}
void SamplePose(AnimStructure* anim,s32 frame){
    // Match the game's sequence sampler without advancing Player/AI callbacks.
    if(anim->sequence&&anim->flip){
        s32 local=0;auto* part=anim->sequence->ResolvePart(frame>>16,local);
        if(part){if(anim->flip->anim!=part){anim->flip->anim=part;anim->flip->dirty=1;}frame=(local<<16)|(frame&0xFFFF);}
    }
    anim->ApplyFlipPose(frame);
}
void BlendCaptured(float weight){
    auto* skel=Skeleton(clone);if(!skel||captured.size()!=skel->numJoints)return;
    for(u32 i=0;i<skel->numJoints;++i){auto& j=skel->joints[i];const auto& c=captured[i];
        j.translationX+=s32((c.translationX-j.translationX)*weight);j.translationY+=s32((c.translationY-j.translationY)*weight);j.translationZ+=s32((c.translationZ-j.translationZ)*weight);
        j.rotationX+=s16(s16(c.rotationX-j.rotationX)*weight);j.rotationY+=s16(s16(c.rotationY-j.rotationY)*weight);j.rotationZ+=s16(s16(c.rotationZ-j.rotationZ)*weight);
    }
}
void Draw(){
    const auto& actor=questxr::Character();if(!clone||!source||!actor.Active())return;
    const bool first=!wasActive||actor.Elapsed()<lastElapsed;
    if(first){auto* skel=Skeleton(source);captured.assign(skel->joints,skel->joints+skel->numJoints);animation=-1;wasGrounded=false;}
    const float dt=first?0.f:std::clamp(actor.Elapsed()-lastElapsed,0.f,.05f);lastElapsed=actor.Elapsed();wasActive=true;
    if(!actor.Grounded()){SetAnimation(39);wasGrounded=false;}
    else{
        if(!wasGrounded)SetAnimation(40);
        if(actor.Action()==questxr::MRCharacterAction::Punch)SetAnimation(57);
        else if(actor.Action()==questxr::MRCharacterAction::Kick)SetAnimation(53);
        else if(actor.Action()==questxr::MRCharacterAction::Roll)SetAnimation(24);
        else if(actor.Moving())SetAnimation(2);
        else if(animation!=40)SetAnimation(22);
        wasGrounded=true;
    }
    auto* anim=static_cast<AnimStructure*>(clone->animStructure);if(!anim)return;
    const float strideScale=actor.StandingHeight()/questxr::kMRJackieStandingHeight;
    animationTime+=dt*(animation==2?std::clamp(actor.MoveSpeed()/(.25f*strideScale),.2f,1.5f):1.f);const float duration=float(anim->endFrame+FIX16_ONE)/FIX16_ONE/30.f;
    if(animation==40&&animationTime>=duration){SetAnimation(22);anim=static_cast<AnimStructure*>(clone->animStructure);}
    const bool actionPose=actor.Grounded()&&actor.Action()!=questxr::MRCharacterAction::None;
    const float frame=actionPose?actor.ActionProgress()*float(anim->endFrame)/FIX16_ONE:animation==40?std::min(animationTime*30.f,float(anim->endFrame)/FIX16_ONE):std::fmod(animationTime*30.f,std::max(1.f,float(anim->endFrame+FIX16_ONE)/FIX16_ONE));
    SamplePose(anim,s32(frame*FIX16_ONE));
    if(actor.Returning())BlendCaptured(questxr::TransferEase(actor.ReturnProgress()));
    else if(actor.Elapsed()<.15f)BlendCaptured(1-questxr::TransferEase(actor.Elapsed()/.15f));
    V low,high,chest;if(!Bounds(clone,low,high,&chest))return;
    const auto& snapshot=actor.Snapshot();V offset;for(int d=0;d<3;++d)offset[d]=snapshot.grip[d]-chest[d];
    if(actor.InRoom())offset[1]=snapshot.up>0?snapshot.standingLow[1]-low[1]:snapshot.standingHigh[1]-high[1];
    const auto oldWorld=p3d::context->GetWorldMatrix();const bool oldMirror=p3d::context->GetWorldMirror();
    p3d::context->SetWorldMatrix(questxr::Translation(offset[0],offset[1],offset[2]));
    p3d::context->SetWorldMirror(false);p3d::context->SetCullMode(PDDI_CULL_NONE);p3d::context->SetTexture(nullptr);
    p3d::context->EnableZBuffer(true);p3d::context->SetBlendMode(PDDI_BLEND_NONE);p3d::context->SetReceiveShadows(false);p3d::context->SetPolygonOffset(false);
    // End-of-frame game UI may have left GL state different from PDDI's cache.
    glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_GEQUAL);glDepthMask(GL_TRUE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    if(source->ambientLight)static_cast<AmbientLight*>(source->ambientLight)->SetPortToLight();
    bool added[3]={};
    if(g_environmentManager&&source->hwLights){auto* lights=static_cast<HardwareLight*>(source->hwLights);for(int i=0;i<std::min(3,source->hwLightCount);++i)if(lights[i].active){LVector dir={lights[i].directionX,lights[i].directionY,lights[i].directionZ};g_environmentManager->lighting.AddLightToPort(i,&dir,lights[i].colour);added[i]=true;}}
    clone->drawable->Display(0);
    RestoreWorldAmbientLightToPort();if(g_environmentManager)for(int i=0;i<3;++i)if(added[i])g_environmentManager->lighting.RemoveLightFromPort(i);
    p3d::context->SetWorldMatrix(oldWorld);p3d::context->SetWorldMirror(oldMirror);
}
bool Prepare(SModel* model){
    auto* original=model->drawable->GetOriginalSTree();if(!original||!original->skeleton||!original->skinData)return false;
    if(source==model&&asset==original&&clone)return true;
    Destroy();
    if(!g_characterManager)return false;
    // Player::AS_DIVE_ROLL uses24;27 is the guard-release transition.
    // Canonical unarmed move IDs: player combo root maps punch to57 and kick to53.
    for(int id:{22,39,40,2,24,53,57}){if(!g_characterManager->GetAnimation(0,id))g_characterManager->LoadAnimation(0,id,1,nullptr);if(!g_characterManager->GetAnimation(0,id))return false;}
    source=model;asset=original;clone=new SModel;clone->SetOriginalSTree(original);
    if(Skeleton(clone)==Skeleton(source)){LOG("[MR Jackie] cannot isolate skeleton");Destroy();return false;}
    auto* skel=Skeleton(clone);
    for(u32 i=0;i<skel->numJoints;++i)if(std::strstr(skel->joints[i].name,"Spine2"))torso=int(i);
    SetAnimation(22);static_cast<AnimStructure*>(clone->animStructure)->ApplyFlipPose(0);
    if(!Bounds(clone,reference.standingLow,reference.standingHigh)){Destroy();return false;}
    reference.standingHeight=reference.standingHigh[1]-reference.standingLow[1];reference.up=1;
    std::vector<Mat4> matrices(skel->numJoints);skel->ComputeWorldMatrices(matrices.data());
    for(u32 i=0;i<skel->numJoints;++i)if(std::strstr(skel->joints[i].name,"Head")){reference.up=matrices[i](1,3)>(reference.standingLow[1]+reference.standingHigh[1])*.5f?1.f:-1.f;break;}
    reference.identity=reinterpret_cast<uintptr_t>(model);reference.valid=true;
    questxr::SetCharacterDrawCallback(Draw);
    LOG("[MR Jackie] isolated skeleton ready: joints=%u standing=%.2f up=%.0f torso=%d",skel->numJoints,reference.standingHeight,reference.up,torso);
    return true;
}
}
void Capture(SModel* model,const Mat4& world){
    if(!questxr::JackieInteractionRequested()||!questxr::MixedRealityRequested()||!g_game||g_game->GetState()!=GameState::Play||g_directorActive||model->backPtr!=Player::s_player||!Player::s_player||Player::s_player->health==0)return;
    if(!Prepare(model))return;
    auto target=reference;if(!Bounds(source,target.low,target.high,&target.grip))return;
    questxr::PublishCharacter(target,p3d::context->GetViewMatrix(),world);
    if(!questxr::CharacterActive())wasActive=false;
}
bool IsDetached(const Thing* thing){return thing&&thing==Player::s_player&&questxr::CharacterActive();}
bool Hide(const Model* model){return model==source&&questxr::CharacterActive();}
void ModelDestroyed(const Model* model){if(model==source)Destroy();}
void TreeDestroyed(const OriginalSTree* tree){if(tree==asset)Destroy();}
}
#endif
