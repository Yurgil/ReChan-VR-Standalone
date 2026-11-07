#pragma once
namespace questxr {
// Action availability is independent of pose tracking and of the other hand.
// Short runtime profile changes must not pause an otherwise playable level.
class ControllerAvailability {
public:
    void Reset(){mask=0;lostSince=-1;}
    void Update(bool left,bool right,double now){
        mask=(left?1u:0u)|(right?2u:0u);
        if(mask)lostSince=-1;
        else if(lostSince<0)lostSince=now;
    }
    unsigned Mask()const{return mask;}
    bool Blocked(bool editing,double now)const{return editing||(mask==0&&lostSince>=0&&now-lostSince>=.5);}
private:
    unsigned mask=0;
    double lostSince=-1;
};
}
