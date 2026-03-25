#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Action.h"

class Recorder {
public:
    Recorder();
    ~Recorder();

    void Start();
    void Stop();
    bool IsRecording() const;
    ActionList GetActions() const;
    int GetActionCount() const;
    void SetNotifyHwnd(HWND hwnd);

private:
    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static DWORD WINAPI HookThreadProc(LPVOID param);

    HANDLE hookThread_;
    DWORD hookThreadId_;
    HHOOK mouseHook_;
    HHOOK keyboardHook_;
    ActionList actions_;
    mutable CRITICAL_SECTION cs_;
    LARGE_INTEGER startTime_;
    LARGE_INTEGER frequency_;
    bool recording_;
    HWND notifyHwnd_;
    int lastMouseX_;
    int lastMouseY_;

    static Recorder* instance_;
};
