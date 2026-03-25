#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>

enum class ActionType : BYTE {
    MouseMove,
    MouseClick,
    MouseScroll,
    KeyPress,
    KeyRelease
};

struct Action {
    ActionType type;
    double     timestamp;    // seconds since recording started
    int        x, y;         // mouse screen position
    int        mouseButton;  // 0=left, 1=right, 2=middle  (MouseClick)
    bool       pressed;      // true=press/down             (MouseClick, Key*)
    int        scrollDelta;  // scroll amount               (MouseScroll)
    DWORD      vkCode;       // virtual key code            (Key*)
    WORD       scanCode;     // OEM scan code               (Key*)
    bool       extendedKey;  // extended-key flag           (Key*)
};

using ActionList = std::vector<Action>;
