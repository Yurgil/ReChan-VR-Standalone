#include "pddi/quest/QuestRoomScene.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <set>
#include <map>
using namespace questxr;
namespace {
int Id(XrSpace s){return int(reinterpret_cast<uintptr_t>(s));}
XrSpace Space(int id){return reinterpret_cast<XrSpace>(uintptr_t(id));}
XrUuidEXT UUID(int id){XrUuidEXT u{};u.data[0]=id;return u;}
std::set<int> alive,locatable;
std::vector<int> results,containerRooms;
std::vector<std::vector<int>> queries;
XrAsyncRequestIdFB next=1,queryId=0,captureId=0,completed=0;
bool noRooms=false,failQuery=false,locations=true;
int captures=0;
XrTime now=1000000000;
XrResult XRAPI_CALL Query(XrSession,const XrSpaceQueryInfoBaseHeaderFB* base,XrAsyncRequestIdFB* out){
    if(failQuery)return XR_ERROR_RUNTIME_FAILURE;
    const auto* info=reinterpret_cast<const XrSpaceQueryInfoFB*>(base);assert(info->queryAction==XR_SPACE_QUERY_ACTION_LOAD_FB);
    const auto* filter=reinterpret_cast<const XrBaseInStructure*>(info->filter);
    const auto* storage=reinterpret_cast<const XrSpaceStorageLocationFilterInfoFB*>(filter->next);assert(storage&&storage->type==XR_TYPE_SPACE_STORAGE_LOCATION_FILTER_INFO_FB&&storage->location==XR_SPACE_STORAGE_LOCATION_LOCAL_FB&&!storage->next);results.clear();
    if(filter->type==XR_TYPE_SPACE_COMPONENT_FILTER_INFO_FB){assert(reinterpret_cast<const XrSpaceComponentFilterInfoFB*>(filter)->componentType==XR_SPACE_COMPONENT_TYPE_ROOM_LAYOUT_FB);if(!noRooms)results={1,2};}
    else{const auto* uuids=reinterpret_cast<const XrSpaceUuidFilterInfoFB*>(filter);for(uint32_t i=0;i<uuids->uuidCount;++i)results.push_back(uuids->uuids[i].data[0]);}
    queries.push_back(results);*out=queryId=next++;return XR_SUCCESS;
}
XrResult XRAPI_CALL Retrieve(XrSession,XrAsyncRequestIdFB id,XrSpaceQueryResultsFB* out){assert(id==queryId);out->resultCountOutput=results.size();if(out->resultCapacityInput){assert(out->resultCapacityInput>=results.size());for(size_t i=0;i<results.size();++i){alive.insert(results[i]);out->results[i]={Space(results[i]),UUID(results[i])};}}return XR_SUCCESS;}
XrResult XRAPI_CALL Layout(XrSession,XrSpace space,XrRoomLayoutFB* out){const int room=Id(space);assert(room==1||room==2);out->floorUuid=UUID(0);out->ceilingUuid=UUID(0);out->wallUuidCountOutput=4;if(out->wallUuidCapacityInput){out->floorUuid=UUID(room==1?11:12);for(int i=0;i<4;++i)out->wallUuids[i]=UUID((room==1?21:41)+i);}return XR_SUCCESS;}
XrResult XRAPI_CALL Container(XrSession,XrSpace space,XrSpaceContainerFB* out){const int room=Id(space);containerRooms.push_back(room);assert(room==1);const int ids[]={11,21,22,23,24,31};out->uuidCountOutput=6;if(out->uuidCapacityInput)for(int i=0;i<6;++i)out->uuids[i]=UUID(ids[i]);return XR_SUCCESS;}
XrResult XRAPI_CALL Components(XrSpace space,uint32_t capacity,uint32_t* count,XrSpaceComponentTypeFB* out){std::vector<XrSpaceComponentTypeFB> values={XR_SPACE_COMPONENT_TYPE_LOCATABLE_FB,XR_SPACE_COMPONENT_TYPE_SEMANTIC_LABELS_FB};values.push_back(Id(space)==31?XR_SPACE_COMPONENT_TYPE_BOUNDED_3D_FB:XR_SPACE_COMPONENT_TYPE_BOUNDED_2D_FB);*count=values.size();if(capacity)std::copy(values.begin(),values.end(),out);return XR_SUCCESS;}
XrResult XRAPI_CALL GetStatus(XrSpace space,XrSpaceComponentTypeFB type,XrSpaceComponentStatusFB* out){assert(type==XR_SPACE_COMPONENT_TYPE_LOCATABLE_FB);out->enabled=locatable.contains(Id(space));return XR_SUCCESS;}
XrResult XRAPI_CALL SetStatus(XrSpace space,const XrSpaceComponentStatusSetInfoFB* info,XrAsyncRequestIdFB* out){assert(info->componentType==XR_SPACE_COMPONENT_TYPE_LOCATABLE_FB&&info->enabled);locatable.insert(Id(space));*out=next++;return XR_SUCCESS;}
XrResult XRAPI_CALL Labels(XrSession,XrSpace space,XrSemanticLabelsFB* out){const int id=Id(space);const char* value=id==31?"TABLE":id==11||id==12?"FLOOR":"WALL_FACE";out->bufferCountOutput=std::strlen(value)+1;if(out->bufferCapacityInput)std::memcpy(out->buffer,value,out->bufferCountOutput);return XR_SUCCESS;}
XrResult XRAPI_CALL Boundary(XrSession,XrSpace,XrBoundary2DFB* out){out->vertexCountOutput=4;if(out->vertexCapacityInput){out->vertices[0]={-2,-2};out->vertices[1]={2,-2};out->vertices[2]={2,2};out->vertices[3]={-2,2};}return XR_SUCCESS;}
XrResult XRAPI_CALL Box2(XrSession,XrSpace,XrRect2Df* out){*out={{-2,-2},{4,4}};return XR_SUCCESS;}
XrResult XRAPI_CALL Box3(XrSession,XrSpace,XrRect3DfFB* out){*out={{-.5f,0,-.4f},{1,.75f,.8f}};return XR_SUCCESS;}
XrResult XRAPI_CALL Capture(XrSession,const XrSceneCaptureRequestInfoFB* info,XrAsyncRequestIdFB* out){assert(!info->request&&info->requestByteCount==0);++captures;*out=captureId=next++;return XR_SUCCESS;}
template<class T>void Event(QuestRoomScene& scene,const T& event){XrEventDataBuffer buffer{XR_TYPE_EVENT_DATA_BUFFER};static_assert(sizeof(event)<sizeof(buffer));std::memcpy(&buffer,&event,sizeof(event));scene.HandleEvent(buffer);}
void Complete(QuestRoomScene& scene){XrEventDataSpaceQueryResultsAvailableFB result{XR_TYPE_EVENT_DATA_SPACE_QUERY_RESULTS_AVAILABLE_FB};result.requestId=queryId;Event(scene,result);XrEventDataSpaceQueryCompleteFB done{XR_TYPE_EVENT_DATA_SPACE_QUERY_COMPLETE_FB};done.requestId=queryId;done.result=XR_SUCCESS;Event(scene,done);completed=queryId;}
void CaptureComplete(QuestRoomScene& scene,XrResult value){XrEventDataSceneCaptureCompleteFB done{XR_TYPE_EVENT_DATA_SCENE_CAPTURE_COMPLETE_FB};done.requestId=captureId;done.result=value;Event(scene,done);}
void Pump(QuestRoomScene& scene,int count=16){for(int i=0;i<count;++i){now+=14000000;scene.Tick(XR_NULL_HANDLE,now,{0,1.7f,0});if(queryId!=completed)Complete(scene);}}
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance,const char* name,PFN_xrVoidFunction* out){
#define PROC(n,f) if(std::strcmp(name,#n)==0){*out=reinterpret_cast<PFN_xrVoidFunction>(f);return XR_SUCCESS;}
    PROC(xrQuerySpacesFB,Query);PROC(xrRetrieveSpaceQueryResultsFB,Retrieve);PROC(xrGetSpaceRoomLayoutFB,Layout);PROC(xrGetSpaceContainerFB,Container);PROC(xrEnumerateSpaceSupportedComponentsFB,Components);PROC(xrGetSpaceComponentStatusFB,GetStatus);PROC(xrSetSpaceComponentStatusFB,SetStatus);PROC(xrGetSpaceSemanticLabelsFB,Labels);PROC(xrGetSpaceBoundary2DFB,Boundary);PROC(xrGetSpaceBoundingBox2DFB,Box2);PROC(xrGetSpaceBoundingBox3DFB,Box3);PROC(xrRequestSceneCaptureFB,Capture);
#undef PROC
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrDestroySpace(XrSpace space){assert(alive.erase(Id(space))==1);return XR_SUCCESS;}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrLocateSpace(XrSpace space,XrSpace,XrTime,XrSpaceLocation* out){
    out->locationFlags=locations?XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT:0;out->pose.orientation.w=1;
    const int id=Id(space);if(id==11||id==12){const float q=std::sqrt(.5f);out->pose.orientation={-q,0,0,q};out->pose.position.x=id==11?0:8;}
    else if(id!=31){out->pose.position.y=1;out->pose.position.z=-2;}
    return XR_SUCCESS;
}
int main(){
    QuestRoomScene scene;scene.Initialize(XR_NULL_HANDLE,XR_NULL_HANDLE,false,false);scene.SetEnabled(true);scene.Request(false);Pump(scene);assert(!scene.Available()&&queries.empty());scene.Shutdown();
    scene.Initialize(XR_NULL_HANDLE,XR_NULL_HANDLE,true,true);Pump(scene);assert(queries.empty());scene.SetEnabled(true);Pump(scene);assert(queries.empty());
    scene.Request(false);Pump(scene);assert(scene.State()==MRRoomState::Ready&&scene.Geometry().ready&&scene.Geometry().surfaces.size()==6&&captures==0);
    assert(queries.size()==3);for(int id:queries.back())assert(id!=12&&id<40);for(int id:containerRooms)assert(id==1);
    MRRoomSupport hit;assert(scene.Geometry().Support({0,2,0},3,.03f,hit)&&std::fabs(hit.point[1]-.75f)<.0001f);
    locations=false;Pump(scene,1);assert(!scene.Geometry().ready&&scene.State()==MRRoomState::Localizing);locations=true;Pump(scene,1);assert(scene.Geometry().ready&&captures==0);
    scene.Resume();assert(!scene.Geometry().ready);now+=60000000000LL;Pump(scene,1);assert(scene.Geometry().ready&&captures==0);
    scene.SetEnabled(false);assert(alive.empty()&&scene.State()==MRRoomState::Off);
    // Late query events after disable must be drained and all returned handles freed.
    scene.SetEnabled(true);scene.Request(false);now+=14000000;scene.Tick(XR_NULL_HANDLE,now,{0,1.7f,0});scene.SetEnabled(false);Complete(scene);assert(alive.empty()&&scene.State()==MRRoomState::Off);
    // Missing room starts the system scan once; cancel never loops a prompt.
    noRooms=true;scene.SetEnabled(true);scene.Request(false);Pump(scene);assert(scene.State()==MRRoomState::Scanning&&captures==1);CaptureComplete(scene,XR_ERROR_RUNTIME_FAILURE);Pump(scene);assert(scene.State()==MRRoomState::Cancelled&&captures==1);
    scene.Request(false);Pump(scene);assert(scene.State()==MRRoomState::Missing&&captures==1);
    scene.Request(false);Pump(scene);assert(scene.State()==MRRoomState::Missing&&captures==1);
    // Explicit retry, successful scan, and reuse of only the containing room.
    scene.Request(true);Pump(scene);assert(captures==2);noRooms=false;CaptureComplete(scene,XR_SUCCESS);Pump(scene);assert(scene.Geometry().ready);
    scene.SetEnabled(false);assert(alive.empty());scene.SetEnabled(true);scene.Request(false);now+=14000000;scene.Tick(XR_NULL_HANDLE,now,{0,1.7f,0});now+=13000000000LL;scene.Tick(XR_NULL_HANDLE,now,{0,1.7f,0});Complete(scene);assert(scene.State()==MRRoomState::Error&&alive.empty());
    scene.Request(false);failQuery=true;Pump(scene);assert(scene.State()==MRRoomState::Error);scene.Shutdown();assert(alive.empty());
    std::cout<<"System room: opt-in only, one containing room, saved-scan reuse, surfaces, loss/recovery, scan/cancel/retry, late events and timeout cleanup OK\n";
}
