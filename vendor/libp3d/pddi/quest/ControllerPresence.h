#pragma once
#include <array>
namespace questxr {
// A controller can remain position-tracked on a table. Pose validity alone
// therefore cannot decide whether it should own the user's interaction hand.
class ControllerPresence {
public:
    bool Update(int hand,bool contact,bool input,double now){
        if(contact||input)until[hand]=now+1.25;
        return now<until[hand];
    }
    void Reset(){until={};}
private:
    std::array<double,2> until{};
};
}
