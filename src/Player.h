#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include "Action.h"

// WM_PLAYER_PROGRESS and WM_PLAYER_COMPLETE are defined in resource.h

class Player {
public:
    Player();
    ~Player();

    // repeatCount = -1 for infinite
    void Play(const ActionList& actions, double speed, int repeatCount, HWND notifyHwnd);
    void Pause();
    void Resume();
    void Stop();

    bool IsPlaying() const;
    bool IsPaused() const;
    int  GetCurrentRepeat() const;
    int  GetCurrentActionIndex() const;

private:
    static DWORD WINAPI PlayThreadProc(LPVOID param);
    void PlayLoop();
    void ExecuteAction(const Action& a);
    void InterruptibleSleep(double seconds);

    ActionList            actions_;
    double                speed_;
    int                   repeatCount_;
    HWND                  notifyHwnd_;

    HANDLE                thread_;
    HANDLE                stopEvent_;
    HANDLE                resumeEvent_;   // signaled = playing, non-signaled = paused

    std::atomic<bool>     playing_;
    std::atomic<bool>     paused_;
    std::atomic<int>      currentRepeat_;
    std::atomic<int>      currentActionIndex_;
};
