#include "pddi/quest/QuestRoomScene.h"
#include <android/log.h>
#include <algorithm>
#include <cstring>
namespace questxr {
namespace {
bool Equal(const XrUuidEXT& a,const XrUuidEXT& b){return std::memcmp(a.data,b.data,16)==0;}
bool Nonzero(const XrUuidEXT& id){for(auto b:id.data)if(b)return true;return false;}
void Add(std::vector<XrUuidEXT>& ids,const XrUuidEXT& id){if(Nonzero(id)&&std::none_of(ids.begin(),ids.end(),[&](const auto& other){return Equal(id,other);}))ids.push_back(id);}
bool Label(const std::string& labels,const char* value){size_t start=0;while(start<labels.size()){const auto end=labels.find(',',start);if(labels.substr(start,end==std::string::npos?end:end-start)==value)return true;if(end==std::string::npos)break;start=end+1;}return false;}
Mat4 Matrix(const XrPosef& p){return PoseMatrix(p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w,p.position.x,p.position.y,p.position.z);}
}
void QuestRoomScene::Initialize(XrInstance instance,XrSession s,bool supported,bool captureSupported){
    Shutdown();session=s;if(!supported){state=MRRoomState::Unavailable;return;}
#define GET(n,f) if(XR_FAILED(xrGetInstanceProcAddr(instance,#n,reinterpret_cast<PFN_xrVoidFunction*>(&f)))||!f){state=MRRoomState::Unavailable;return;}
    GET(xrQuerySpacesFB,query);GET(xrRetrieveSpaceQueryResultsFB,retrieve);GET(xrGetSpaceRoomLayoutFB,layout);GET(xrGetSpaceContainerFB,container);
    GET(xrEnumerateSpaceSupportedComponentsFB,components);GET(xrGetSpaceComponentStatusFB,getStatus);GET(xrSetSpaceComponentStatusFB,setStatus);
    GET(xrGetSpaceSemanticLabelsFB,labels);GET(xrGetSpaceBoundary2DFB,boundary);GET(xrGetSpaceBoundingBox2DFB,box2);GET(xrGetSpaceBoundingBox3DFB,box3);
#undef GET
    if(captureSupported)xrGetInstanceProcAddr(instance,"xrRequestSceneCaptureFB",reinterpret_cast<PFN_xrVoidFunction*>(&capture));
    available=true;state=MRRoomState::Off;
    __android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR room: system scene read/query ready; capture=%d",capture!=nullptr);
}
void QuestRoomScene::ClearData(){
    for(const auto& candidate:candidates)if(candidate.space)xrDestroySpace(candidate.space);candidates.clear();
    for(const auto& node:nodes)if(node.space)xrDestroySpace(node.space);nodes.clear();
    geometry={};requiredWalls.clear();selectedFloor={};
}
void QuestRoomScene::Shutdown(){ClearData();session=XR_NULL_HANDLE;queryRequest=captureRequest=0;available=enabled=requested=rescan=discard=captureAttempted=stageQueued=false;phaseTime=currentTime=0;nextIds.clear();capture=nullptr;state=MRRoomState::Off;}
void QuestRoomScene::SetEnabled(bool value){
    if(value==enabled)return;enabled=value;ClearData();requested=stageQueued=false;discard=queryRequest||captureRequest;
    if(value)captureAttempted=false;
    state=value?(available?MRRoomState::Idle:MRRoomState::Unavailable):MRRoomState::Off;
}
void QuestRoomScene::Request(bool force){if(!enabled||!available)return;requested=true;rescan=force;stageQueued=false;if(force)captureAttempted=false;if(queryRequest||captureRequest)discard=true;state=MRRoomState::Loading;geometry.ready=false;}
void QuestRoomScene::Fail(const char* op,XrResult result){
    geometry.ready=false;state=MRRoomState::Error;stageQueued=false;
    __android_log_print(ANDROID_LOG_WARN,"ReChanXR","MR room: %s failed %d; basic MR remains available",op,result);
}
bool QuestRoomScene::StartQuery(QueryKind kind,const std::vector<XrUuidEXT>& ids){
    XrSpaceStorageLocationFilterInfoFB storage{XR_TYPE_SPACE_STORAGE_LOCATION_FILTER_INFO_FB};storage.location=XR_SPACE_STORAGE_LOCATION_LOCAL_FB;
    XrSpaceComponentFilterInfoFB component{XR_TYPE_SPACE_COMPONENT_FILTER_INFO_FB};component.componentType=XR_SPACE_COMPONENT_TYPE_ROOM_LAYOUT_FB;
    XrSpaceUuidFilterInfoFB filter{XR_TYPE_SPACE_UUID_FILTER_INFO_FB};filter.uuidCount=ids.size();filter.uuids=const_cast<XrUuidEXT*>(ids.data());
    component.next=&storage;filter.next=&storage;
    XrSpaceQueryInfoFB info{XR_TYPE_SPACE_QUERY_INFO_FB};info.queryAction=XR_SPACE_QUERY_ACTION_LOAD_FB;info.maxResultCount=256;info.timeout=8000000000LL;info.filter=kind==QueryKind::Rooms?reinterpret_cast<XrSpaceFilterInfoBaseHeaderFB*>(&component):reinterpret_cast<XrSpaceFilterInfoBaseHeaderFB*>(&filter);
    const auto result=query(session,reinterpret_cast<const XrSpaceQueryInfoBaseHeaderFB*>(&info),&queryRequest);
    if(XR_FAILED(result)){queryRequest=0;Fail("scene query",result);return false;}
    queryKind=kind;phaseTime=currentTime;state=MRRoomState::Loading;
    __android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR room: query stage %d (%zu UUIDs)",int(kind),ids.size());return true;
}
bool QuestRoomScene::StartCapture(){
    if(!capture){state=MRRoomState::Missing;return false;}
    ClearData();captureAttempted=true;
    XrSceneCaptureRequestInfoFB info{XR_TYPE_SCENE_CAPTURE_REQUEST_INFO_FB};
    const auto result=capture(session,&info,&captureRequest);
    if(XR_FAILED(result)){captureRequest=0;Fail("system capture request",result);return false;}
    state=MRRoomState::Scanning;__android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR room: requested system Space Setup");return true;
}
void QuestRoomScene::Missing(){
    ClearData();state=MRRoomState::Missing;
    if(!captureAttempted&&capture){requested=true;rescan=true;}
    __android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR room: no matching room; capture queued=%d",requested);
}
bool QuestRoomScene::ReadNode(XrSpace space,const XrUuidEXT& uuid,Node& n,bool isFloor){
    uint32_t count=0;if(XR_FAILED(components(space,0,&count,nullptr))||count>32)return false;
    std::vector<XrSpaceComponentTypeFB> types(count);if(XR_FAILED(components(space,count,&count,types.data())))return false;
    auto has=[&](XrSpaceComponentTypeFB type){return std::find(types.begin(),types.end(),type)!=types.end();};
    if(!has(XR_SPACE_COMPONENT_TYPE_LOCATABLE_FB))return false;
    n.space=space;n.uuid=uuid;n.surface.floor=isFloor||Equal(uuid,selectedFloor);
    n.surface.wall=std::any_of(requiredWalls.begin(),requiredWalls.end(),[&](const auto& id){return Equal(id,uuid);});
    if(has(XR_SPACE_COMPONENT_TYPE_SEMANTIC_LABELS_FB)){
        XrSemanticLabelsFB text{XR_TYPE_SEMANTIC_LABELS_FB};
        if(XR_SUCCEEDED(labels(session,space,&text))&&text.bufferCountOutput>0&&text.bufferCountOutput<4096){
            std::vector<char> bytes(text.bufferCountOutput+1,0);text.bufferCapacityInput=text.bufferCountOutput;text.buffer=bytes.data();
            if(XR_SUCCEEDED(labels(session,space,&text)))n.surface.labels=bytes.data();
        }
    }
    n.surface.floor|=Label(n.surface.labels,"FLOOR");n.surface.wall|=Label(n.surface.labels,"WALL_FACE")||Label(n.surface.labels,"INVISIBLE_WALL_FACE");
    if(has(XR_SPACE_COMPONENT_TYPE_BOUNDED_2D_FB)){
        XrBoundary2DFB polygon{XR_TYPE_BOUNDARY_2D_FB};
        if(XR_SUCCEEDED(boundary(session,space,&polygon))&&polygon.vertexCountOutput>=3&&polygon.vertexCountOutput<=2048){
            std::vector<XrVector2f> points(polygon.vertexCountOutput);polygon.vertexCapacityInput=points.size();polygon.vertices=points.data();
            if(XR_SUCCEEDED(boundary(session,space,&polygon)))for(uint32_t i=0;i<polygon.vertexCountOutput;++i)if(std::isfinite(points[i].x)&&std::isfinite(points[i].y))n.surface.boundary.push_back({points[i].x,points[i].y});
        }
        if(n.surface.boundary.size()<3){XrRect2Df rect{};if(XR_SUCCEEDED(box2(session,space,&rect))&&rect.extent.width>0&&rect.extent.height>0)n.surface.boundary={{rect.offset.x,rect.offset.y},{rect.offset.x+rect.extent.width,rect.offset.y},{rect.offset.x+rect.extent.width,rect.offset.y+rect.extent.height},{rect.offset.x,rect.offset.y+rect.extent.height}};}
    }
    if(has(XR_SPACE_COMPONENT_TYPE_BOUNDED_3D_FB)){
        XrRect3DfFB box{};if(XR_SUCCEEDED(box3(session,space,&box))){
            n.surface.low={box.offset.x,box.offset.y,box.offset.z};n.surface.extent={box.extent.width,box.extent.height,box.extent.depth};n.surface.volume=true;
            for(int i=0;i<3;++i)if(!std::isfinite(n.surface.low[i])||!std::isfinite(n.surface.extent[i])||n.surface.extent[i]<=0||n.surface.extent[i]>50)n.surface.volume=false;
        }
    }
    return n.surface.volume||n.surface.boundary.size()>=3;
}
void QuestRoomScene::LocateNodes(XrSpace reference,XrTime time){
    for(auto& node:nodes){
        node.surface.located=false;XrSpaceComponentStatusFB status{XR_TYPE_SPACE_COMPONENT_STATUS_FB};
        if(XR_FAILED(getStatus(node.space,XR_SPACE_COMPONENT_TYPE_LOCATABLE_FB,&status)))continue;
        if(!status.enabled){
            if(!node.locateRequested&&!status.changePending){XrSpaceComponentStatusSetInfoFB info{XR_TYPE_SPACE_COMPONENT_STATUS_SET_INFO_FB};info.componentType=XR_SPACE_COMPONENT_TYPE_LOCATABLE_FB;info.enabled=XR_TRUE;info.timeout=5000000000LL;XrAsyncRequestIdFB request;const auto r=setStatus(node.space,&info,&request);node.locateRequested=XR_SUCCEEDED(r)||r==XR_ERROR_SPACE_COMPONENT_STATUS_PENDING_FB||r==XR_ERROR_SPACE_COMPONENT_STATUS_ALREADY_SET_FB;}
            continue;
        }
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};const auto valid=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        if(XR_SUCCEEDED(xrLocateSpace(node.space,reference,time,&location))&&(location.locationFlags&valid)==valid){node.surface.pose=Matrix(location.pose);node.surface.located=true;}
    }
}
void QuestRoomScene::ChooseRoom(const std::array<float,3>& head){
    int chosen=-1;float best=1e20f;
    for(size_t i=0;i<candidates.size();++i)for(const auto& node:nodes)if(node.surface.located&&Equal(node.uuid,candidates[i].floor)){
        const auto p=RigidInverse(node.surface.pose)*Translation(head[0],head[1],head[2]);const float height=std::fabs(p(2,3));
        if(height>.1f&&height<3.5f&&height<best&&MRRoomGeometry::Contains(node.surface.boundary,p(0,3),p(1,3))){best=height;chosen=i;}
    }
    if(chosen<0)return;
    selectedFloor=candidates[chosen].floor;requiredWalls=candidates[chosen].walls;
    std::vector<XrUuidEXT> ids;for(const auto& wall:requiredWalls)Add(ids,wall);
    XrSpaceContainerFB content{XR_TYPE_SPACE_CONTAINER_FB};
    if(XR_SUCCEEDED(container(session,candidates[chosen].space,&content))&&content.uuidCountOutput<=256){
        std::vector<XrUuidEXT> all(content.uuidCountOutput);content.uuidCapacityInput=all.size();content.uuids=all.data();
        if(XR_SUCCEEDED(container(session,candidates[chosen].space,&content)))for(uint32_t i=0;i<content.uuidCountOutput;++i)if(!Equal(all[i],selectedFloor))Add(ids,all[i]);
    }
    for(auto it=nodes.begin();it!=nodes.end();)if(!Equal(it->uuid,selectedFloor)){xrDestroySpace(it->space);it=nodes.erase(it);}else ++it;
    for(const auto& candidate:candidates)xrDestroySpace(candidate.space);candidates.clear();
    nextIds=ids;nextKind=QueryKind::Contents;stageQueued=true;state=MRRoomState::Loading;
    __android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR room: selected one containing room; %zu walls, %zu contents",requiredWalls.size(),ids.size());
}
void QuestRoomScene::FinishQuery(){
    if(queryKind==QueryKind::Rooms){
        if(candidates.empty()){Missing();return;}
        nextIds.clear();for(const auto& room:candidates)Add(nextIds,room.floor);nextKind=QueryKind::Floors;stageQueued=true;
    }else if(queryKind==QueryKind::Floors){state=MRRoomState::Localizing;phaseTime=0;if(nodes.empty())Missing();}
    else{state=MRRoomState::Localizing;phaseTime=0;}
}
void QuestRoomScene::Tick(XrSpace reference,XrTime time,const std::array<float,3>& head){
    currentTime=time;if(!enabled||!available)return;
    if(queryRequest){if(!phaseTime)phaseTime=time;if(state!=MRRoomState::Error&&time-phaseTime>12000000000LL){discard=true;Fail("scene query timeout",XR_TIMEOUT_EXPIRED);}return;}
    if(captureRequest)return;
    if(requested){requested=false;ClearData();discard=false;if(rescan){rescan=false;StartCapture();return;}StartQuery(QueryKind::Rooms);return;}
    if(stageQueued){stageQueued=false;if(nextKind!=QueryKind::Rooms&&nextIds.empty()){Missing();return;}StartQuery(nextKind,nextIds);return;}
    if(state!=MRRoomState::Localizing&&state!=MRRoomState::Ready)return;
    if(!phaseTime)phaseTime=time;
    LocateNodes(reference,time);
    if(!candidates.empty()){
        ChooseRoom(head);if(!candidates.empty()&&time-phaseTime>10000000000LL)Missing();return;
    }
    bool complete=Nonzero(selectedFloor)&&!requiredWalls.empty();
    auto located=[&](const XrUuidEXT& id){return std::any_of(nodes.begin(),nodes.end(),[&](const auto& n){return Equal(n.uuid,id)&&n.surface.located;});};
    complete&=located(selectedFloor);for(const auto& wall:requiredWalls)complete&=located(wall);
    geometry.surfaces.clear();for(const auto& node:nodes)geometry.surfaces.push_back(node.surface);geometry.ready=complete;
    const auto next=complete?MRRoomState::Ready:MRRoomState::Localizing;
    if(next!=state){__android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR room: %s; %zu surfaces",complete?"READY":"tracking lost; collisions disabled",nodes.size());if(!complete)phaseTime=time;}
    state=next;if(!complete&&time-phaseTime>10000000000LL)Fail("incomplete or unlocalized scene",XR_TIMEOUT_EXPIRED);
}
void QuestRoomScene::HandleEvent(const XrEventDataBuffer& event){
    if(event.type==XR_TYPE_EVENT_DATA_SCENE_CAPTURE_COMPLETE_FB){
        const auto& e=*reinterpret_cast<const XrEventDataSceneCaptureCompleteFB*>(&event);if(!captureRequest||captureRequest!=e.requestId)return;captureRequest=0;
        if(discard||!enabled){discard=false;return;}
        if(XR_FAILED(e.result)){state=MRRoomState::Cancelled;__android_log_print(ANDROID_LOG_WARN,"ReChanXR","MR room: system capture cancelled/failed %d",e.result);return;}
        nextIds.clear();nextKind=QueryKind::Rooms;stageQueued=true;state=MRRoomState::Loading;return;
    }
    if(event.type==XR_TYPE_EVENT_DATA_SPACE_QUERY_RESULTS_AVAILABLE_FB){
        const auto& e=*reinterpret_cast<const XrEventDataSpaceQueryResultsAvailableFB*>(&event);if(!queryRequest||e.requestId!=queryRequest)return;
        XrSpaceQueryResultsFB results{XR_TYPE_SPACE_QUERY_RESULTS_FB};auto r=retrieve(session,queryRequest,&results);
        if(XR_FAILED(r)||results.resultCountOutput>256){discard=true;Fail("query results",XR_FAILED(r)?r:XR_ERROR_LIMIT_REACHED);return;}
        std::vector<XrSpaceQueryResultFB> entries(results.resultCountOutput);results.resultCapacityInput=entries.size();results.results=entries.data();
        r=retrieve(session,queryRequest,&results);if(XR_FAILED(r)){discard=true;Fail("query results",r);return;}
        __android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR room: query stage %d returned %u spaces",int(queryKind),results.resultCountOutput);
        for(uint32_t i=0;i<results.resultCountOutput;++i){
            const auto& result=entries[i];bool keep=false;
            if(enabled&&!discard){
                if(queryKind==QueryKind::Rooms){
                    if(std::any_of(candidates.begin(),candidates.end(),[&](const auto& room){return room.space==result.space;}))continue;
                    XrRoomLayoutFB room{XR_TYPE_ROOM_LAYOUT_FB};
                    const auto countResult=layout(session,result.space,&room);
                    if(XR_SUCCEEDED(countResult)&&room.wallUuidCountOutput<=256){
                        // floorUuid/ceilingUuid are unspecified on the capacity
                        // query. Always perform the data call before using them.
                        Candidate candidate;candidate.space=result.space;candidate.walls.resize(std::max(1u,room.wallUuidCountOutput));room.wallUuidCapacityInput=candidate.walls.size();room.wallUuids=candidate.walls.data();
                        const auto dataResult=layout(session,result.space,&room);
                        __android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR room: layout data result=%d floor=%d walls=%u",dataResult,Nonzero(room.floorUuid),room.wallUuidCountOutput);
                        if(XR_SUCCEEDED(dataResult)&&Nonzero(room.floorUuid)&&room.wallUuidCountOutput<=candidate.walls.size()){candidate.floor=room.floorUuid;candidate.walls.resize(room.wallUuidCountOutput);candidates.push_back(candidate);keep=true;}
                    }else{
                        __android_log_print(ANDROID_LOG_WARN,"ReChanXR","MR room: layout capacity result=%d walls=%u",countResult,room.wallUuidCountOutput);
                    }
                }else{
                    auto existing=std::find_if(nodes.begin(),nodes.end(),[&](const auto& node){return Equal(node.uuid,result.uuid);});
                    if(existing!=nodes.end())keep=existing->space==result.space;
                    else{Node node;if(ReadNode(result.space,result.uuid,node,queryKind==QueryKind::Floors)){nodes.push_back(node);keep=true;}}
                }
            }
            if(!keep)xrDestroySpace(result.space);
        }
    }else if(event.type==XR_TYPE_EVENT_DATA_SPACE_QUERY_COMPLETE_FB){
        const auto& e=*reinterpret_cast<const XrEventDataSpaceQueryCompleteFB*>(&event);if(!queryRequest||e.requestId!=queryRequest)return;queryRequest=0;
        if(discard||!enabled){discard=false;return;}
        if(XR_FAILED(e.result)){Fail("query complete",e.result);return;}FinishQuery();
    }
}
}
