#include "pddi/gles/AndroidPlatform.h"
#include "pddi/pddidev.h"
#include <cassert>
#include <cstdio>

int main() {
    using namespace androidbridge;
    for (int activity=0; activity<5; ++activity) {
        BeginActivity();
        assert(!ExitRequested());
        assert(!PeekCurrentWindow());
        assert(!TakePendingWindow());
        const auto input=LoadPadSnapshot();
        assert(input.heldButtons==0);
        assert(input.axes[GamepadAxis::LeftTrigger]==-1);
        assert(input.axes[GamepadAxis::RightTrigger]==-1);
        assert(input.axes[GamepadAxis::LeftX]==0);
        // Reproduce a destroyed activity while Android retains the process.
        int window=activity;
        SetNativeWindow(&window);
        PostGamepadButton(GamepadButton::Y,true);
        PostGamepadAxis(GamepadAxis::RightTrigger,1);
        RequestExit();
        assert(ExitRequested());
    }
    std::puts("Android lifecycle: repeated activity creation clears stale exit, windows and held input OK");
}
