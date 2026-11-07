#pragma once
namespace questxr {
// MR thumbstick clicks are resolved on release so the two-stick chord never
// recentres the window or opens the HUD before its second button arrives.
class MRInteractionMode {
public:
    void Reset(){editing=false;blocked=true;chord=false;pendingLeft=pendingRight=false;previousLeft=previousRight=false;statusUntil=0;changed=recenter=status=false;}
    void Update(bool left,bool right,double now){
        changed=recenter=status=false;
        if(blocked){if(!left&&!right)blocked=false;return;}
        if(left&&right&&!chord){
            editing=!editing;changed=true;chord=true;
            pendingLeft=pendingRight=false;statusUntil=0;
        }
        if(!chord){
            if(left&&!previousLeft)pendingLeft=true;
            if(right&&!previousRight)pendingRight=true;
            if(!left&&previousLeft&&pendingLeft){recenter=!editing;pendingLeft=false;}
            if(!right&&previousRight&&pendingRight){if(!editing)statusUntil=now+.1;pendingRight=false;}
        }
        if(!left&&!right)chord=false;
        previousLeft=left;previousRight=right;
        status=!editing&&now<statusUntil;
    }
    bool Editing()const{return editing;}
    bool Changed()const{return changed;}
    bool Recenter()const{return recenter;}
    bool Status()const{return status;}
private:
    bool editing=false,blocked=true,chord=false;
    bool previousLeft=false,previousRight=false,pendingLeft=false,pendingRight=false;
    bool changed=false,recenter=false,status=false;
    double statusUntil=0;
};
}
