#pragma once
#include "pddi/quest/MRRoomGeometry.h"
#include <openxr/openxr.h>
namespace questxr {
enum class MRRoomState {Off,Unavailable,Idle,Loading,Scanning,Localizing,Ready,Cancelled,Missing,Error};
// Read-only consumption of the system Scene Model. Never creates or saves a
// window anchor and never constructs a multiroom gameplay environment.
class QuestRoomScene {
public:
    void Initialize(XrInstance instance,XrSession session,bool supported,bool captureSupported);
    void Shutdown();
    void SetEnabled(bool value);
    void Request(bool rescan);
    void Resume(){phaseTime=0;geometry.ready=false;if(state==MRRoomState::Ready)state=MRRoomState::Localizing;}
    void Tick(XrSpace reference,XrTime time,const std::array<float,3>& head);
    void HandleEvent(const XrEventDataBuffer& event);
    bool Available()const{return available;}
    MRRoomState State()const{return state;}
    const MRRoomGeometry& Geometry()const{return geometry;}
private:
    struct Candidate{XrSpace space=XR_NULL_HANDLE;XrUuidEXT floor{};std::vector<XrUuidEXT> walls;};
    struct Node{XrSpace space=XR_NULL_HANDLE;XrUuidEXT uuid{};MRRoomSurface surface;bool locateRequested=false;};
    enum class QueryKind{Rooms,Floors,Contents};
    bool StartQuery(QueryKind kind,const std::vector<XrUuidEXT>& ids={});
    bool StartCapture();
    bool ReadNode(XrSpace space,const XrUuidEXT& uuid,Node& node,bool floor);
    void LocateNodes(XrSpace reference,XrTime time);
    void ChooseRoom(const std::array<float,3>& head);
    void FinishQuery();
    void ClearData();
    void Fail(const char* operation,XrResult result);
    void Missing();
    XrSession session=XR_NULL_HANDLE;
    bool available=false,enabled=false,requested=false,rescan=false,discard=false,captureAttempted=false;
    bool stageQueued=false;
    XrAsyncRequestIdFB queryRequest=0,captureRequest=0;
    XrTime phaseTime=0,currentTime=0;
    QueryKind queryKind=QueryKind::Rooms;
    QueryKind nextKind=QueryKind::Rooms;
    std::vector<XrUuidEXT> nextIds;
    MRRoomState state=MRRoomState::Off;
    std::vector<Candidate> candidates;
    std::vector<Node> nodes;
    std::vector<XrUuidEXT> requiredWalls;
    XrUuidEXT selectedFloor{};
    MRRoomGeometry geometry;
    PFN_xrQuerySpacesFB query=nullptr;
    PFN_xrRetrieveSpaceQueryResultsFB retrieve=nullptr;
    PFN_xrGetSpaceRoomLayoutFB layout=nullptr;
    PFN_xrGetSpaceContainerFB container=nullptr;
    PFN_xrEnumerateSpaceSupportedComponentsFB components=nullptr;
    PFN_xrGetSpaceComponentStatusFB getStatus=nullptr;
    PFN_xrSetSpaceComponentStatusFB setStatus=nullptr;
    PFN_xrGetSpaceSemanticLabelsFB labels=nullptr;
    PFN_xrGetSpaceBoundary2DFB boundary=nullptr;
    PFN_xrGetSpaceBoundingBox2DFB box2=nullptr;
    PFN_xrGetSpaceBoundingBox3DFB box3=nullptr;
    PFN_xrRequestSceneCaptureFB capture=nullptr;
};
}
