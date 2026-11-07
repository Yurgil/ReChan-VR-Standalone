#pragma once
#include <array>
#include <cstdint>
namespace questxr {
// Suppress only controls held across an editing/focus transition. A held
// trigger or drifting stick must never lock every other gameplay control.
class GameplayInputGate {
public:
    void Reset(){buttons=~uint32_t(0);triggers={true,true};}
    uint32_t FilterButtons(uint32_t raw,bool blocked){
        if(blocked){buttons=raw;return 0;}
        buttons&=raw;return raw&~buttons;
    }
    float FilterTrigger(int hand,float raw,bool blocked){
        if(blocked){triggers[hand]=raw>=.02f;return 0;}
        if(raw<.02f)triggers[hand]=false;
        return triggers[hand]?0:raw;
    }
private:
    uint32_t buttons=~uint32_t(0);
    std::array<bool,2> triggers{true,true};
};
}
