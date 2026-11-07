#include "pc/inputaction.h"
#include "gen/control.h"
#include "p3d/input.h"
#include "pddi/null/nullrender.h"
#include <cassert>
#include <cstdio>

class TouchSnapshot final : public nullGamepad {
public:
    unsigned buttons=0;
    float axes[6]={0,0,0,0,-1,-1};
    bool IsConnected() const override {return true;}
    bool IsButtonDown(int b) const override {return (buttons&(1u<<b))!=0;}
    float GetAxis(int axis) const override {return axes[axis];}
    void Trigger(int axis,float travel){axes[axis]=2*travel-1;}
};
int main(){
    nullDisplay display;TouchSnapshot pad;PlatformInput platform;ActionInput actions;
    platform.SetDisplay(&display);platform.SetGamepad(&pad);
    auto poll=[&](){platform.ServiceInput();actions.Update(&platform);return actions.GetPadButtons(false);};
    assert(poll()==0);
    pad.Trigger(GamepadAxis::RightTrigger,.01f);assert(poll()==0);
    pad.Trigger(GamepadAxis::RightTrigger,.049f);assert(poll()==0);
    pad.Trigger(GamepadAxis::RightTrigger,.051f);assert(poll()==0x0040); // jump at 5%, not 55%/75%
    assert(actions.JustPressed(ACTION_JUMP));
    assert(actions.GetPadButtons(true)==0); // squeezing jump does not confirm a menu
    pad.Trigger(GamepadAxis::RightTrigger,.04f);assert(poll()==0x0040); // hysteresis suppresses chatter
    assert(!actions.JustPressed(ACTION_JUMP));
    pad.Trigger(GamepadAxis::RightTrigger,.019f);assert(poll()==0);
    assert(actions.JustReleased(ACTION_JUMP));
    pad.Trigger(GamepadAxis::RightTrigger,0);
    pad.Trigger(GamepadAxis::LeftTrigger,.051f);assert(poll()==0x0020); // grab/doors at 5%
    pad.Trigger(GamepadAxis::LeftTrigger,0);assert(poll()==0);
    pad.buttons=1u<<GamepadButton::RightBumper;assert(poll()==0x0008);
    pad.buttons=1u<<GamepadButton::LeftBumper;assert(poll()==0x0002);
    pad.buttons=1u<<GamepadButton::RightThumb;assert(poll()==0x0001);
    pad.buttons=1u<<GamepadButton::X;assert(poll()==0); // lower left no longer counters
    pad.buttons=1u<<GamepadButton::Y;assert(poll()==0x0004); // upper left = counter
    assert(actions.IsHeld(ACTION_MENU_CLEAR));
    assert((actions.GetPadButtons(true)&0x0004)==0); // clear in menus without counter
    pad.buttons=1u<<GamepadButton::A;assert(poll()==0x0080); // lower right = punch
    assert(actions.IsHeld(ACTION_MENU_CONFIRM));
    assert(actions.GetPadButtons(true)==0x0040); // menu confirm without punch side effect
    pad.buttons=1u<<GamepadButton::B;assert(poll()==0x0010); // upper right = kick
    assert(actions.IsHeld(ACTION_MENU_BACK));
    assert(actions.GetPadButtons(true)==PsxPad::MenuBack);
    pad.buttons=1u<<GamepadButton::LeftThumb;assert(poll()==0);
    pad.buttons=0;assert(poll()==0);
    std::puts("Touch: A/B attacks, early triggers with release hysteresis, grips/stick/Y and menu isolation OK");
}
