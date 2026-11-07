#pragma once
#include "pddi/quest/MRInteraction.h"
namespace questxr {
inline constexpr float kRoomObjectHeight=.07f;
inline float TransferEase(float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
inline float TransferDistance(const PortalPlacement& window,const Mat4& point){
    return ((window.ReferenceToLocal()*point)(2,3)+kPortalDepth)*window.scale;
}
inline float ObjectHalf(const Mat4& m){return std::sqrt(m(0,0)*m(0,0)+m(1,0)*m(1,0)+m(2,0)*m(2,0));}
inline Mat4 ResizeAtPoint(Mat4 m,const std::array<float,3>& pivot,float half){
    const float current=ObjectHalf(m);if(current<.000001f)return m;
    const float ratio=half/current;
    for(int row=0;row<3;++row){for(int column=0;column<3;++column)m(row,column)*=ratio;m(row,3)=pivot[row]+(m(row,3)-pivot[row])*ratio;}
    return m;
}
inline float TransferHalf(float insideHalf,float distance){
    const float amount=TransferEase((distance+.04f)/.10f);
    return insideHalf+(kRoomObjectHeight*.5f-insideHalf)*amount;
}
}
