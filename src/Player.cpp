#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include "Action.h"
#include "Player.h"

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

Player::Player()
    : speed_(1.0)
    , repeatCount_(1)
    , notifyHwnd_(nullptr)
    , thread_(nullptr)
    , stopEvent_(nullptr)
    , resumeEvent_(nullptr)
    , playing_(false)
    , paused_(false)
    , currentRepeat_(0)
    , currentActionIndex_(0)
{
    // Manual-reset, initially non-signaled  → stop is not requested yet
    stopEvent_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    // Manual-reset, initially signaled      → not paused (free to run)
    resumeEvent_ = CreateEvent(nullptr, TRUE, TRUE, nullptr);
}

Player::~Player()
{
    Stop();

    if (stopEvent_)  { CloseHandle(stopEvent_);  stopEvent_  = nullptr; }
    if (resumeEvent_){ CloseHandle(resumeEvent_); resumeEvent_ = nullptr; }
}

// ---------------------------------------------------------------------------
// Public control interface
// ---------------------------------------------------------------------------

void Player::Play(const ActionList& actions, double speed, int repeatCount, HWND notifyHwnd)
{
    if (playing_.load()) return;

    actions_     = actions;
    speed_       = (speed > 0.0) ? speed : 1.0;
    repeatCount_ = repeatCount;
    notifyHwnd_  = notifyHwnd;

    currentRepeat_.store(0);
    currentActionIndex_.store(0);

    // Reset events to clean state
    ResetEvent(stopEvent_);   // clear any previous stop signal
    SetEvent(resumeEvent_);   // ensure we start in a non-paused state

    playing_.store(true);
    paused_.store(false);

    thread_ = CreateThread(nullptr, 0, PlayThreadProc, this, 0, nullptr);
}

void Player::Pause()
{
    if (!playing_.load() || paused_.load()) return;
    paused_.store(true);
    ResetEvent(resumeEvent_);   // block the play loop at next InterruptibleSleep
}

void Player::Resume()
{
    if (!playing_.load() || !paused_.load()) return;
    paused_.store(false);
    SetEvent(resumeEvent_);     // unblock the play loop
}

void Player::Stop()
{
    if (!playing_.load() && thread_ == nullptr) return;

    SetEvent(stopEvent_);   // signal the thread to stop
    SetEvent(resumeEvent_); // unblock if paused so the thread can see stopEvent_

    if (thread_)
    {
        WaitForSingleObject(thread_, INFINITE);
        CloseHandle(thread_);
        thread_ = nullptr;
    }

    playing_.store(false);
    paused_.store(false);
}

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------

bool Player::IsPlaying() const          { return playing_.load();            }
bool Player::IsPaused()  const          { return paused_.load();             }
int  Player::GetCurrentRepeat() const   { return currentRepeat_.load();      }
int  Player::GetCurrentActionIndex() const { return currentActionIndex_.load(); }

// ---------------------------------------------------------------------------
// Thread entry point
// ---------------------------------------------------------------------------

DWORD WINAPI Player::PlayThreadProc(LPVOID param)
{
    Player* self = static_cast<Player*>(param);
    self->PlayLoop();
    return 0;
}

// ---------------------------------------------------------------------------
// Main playback loop
// ---------------------------------------------------------------------------

void Player::PlayLoop()
{
    const bool infinite = (repeatCount_ == -1);
    const int  total    = infinite ? 0 : repeatCount_;

    for (int repeat = 0; infinite || repeat < total; ++repeat)
    {
        currentRepeat_.store(repeat);

        for (int i = 0; i < static_cast<int>(actions_.size()); ++i)
        {
            // ---- Check for stop ----------------------------------------
            if (WaitForSingleObject(stopEvent_, 0) == WAIT_OBJECT_0)
                goto done;

            // ---- Inter-action delay ------------------------------------
            if (i > 0)
            {
                double delay = (actions_[i].timestamp - actions_[i - 1].timestamp) / speed_;
                if (delay > 0.0)
                    InterruptibleSleep(delay);

                // Re-check stop after sleeping
                if (WaitForSingleObject(stopEvent_, 0) == WAIT_OBJECT_0)
                    goto done;
            }

            // ---- Post progress notification ----------------------------
            currentActionIndex_.store(i);

            if (notifyHwnd_)
            {
                WORD totalRepeatsWord = infinite
                    ? 0xFFFF
                    : static_cast<WORD>(total & 0xFFFF);

                PostMessage(
                    notifyHwnd_,
                    WM_PLAYER_PROGRESS,
                    static_cast<WPARAM>(i),
                    MAKELPARAM(static_cast<WORD>(repeat), totalRepeatsWord)
                );
            }

            // ---- Execute the action ------------------------------------
            ExecuteAction(actions_[i]);
        }
    }

done:
    if (notifyHwnd_)
        PostMessage(notifyHwnd_, WM_PLAYER_COMPLETE, 0, 0);

    playing_.store(false);
    paused_.store(false);
}

// ---------------------------------------------------------------------------
// Interruptible sleep
// ---------------------------------------------------------------------------

void Player::InterruptibleSleep(double seconds)
{
    // First, block here if paused (wait until resumeEvent_ is signaled)
    // Also wake up if stopEvent_ fires.
    HANDLE events[2] = { stopEvent_, resumeEvent_ };

    // Block while paused; exit immediately if stop is requested.
    // resumeEvent_ starts signaled (not paused) so this returns at once normally.
    DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);
    if (waitResult == WAIT_OBJECT_0)   // stopEvent_ fired
        return;

    // Sleep in 10 ms chunks, checking for stop between each chunk.
    DWORD totalMs    = static_cast<DWORD>(seconds * 1000.0 + 0.5);
    DWORD elapsed    = 0;
    const DWORD chunk = 10;

    while (elapsed < totalMs)
    {
        if (WaitForSingleObject(stopEvent_, 0) == WAIT_OBJECT_0)
            return;

        // Also re-block if paused mid-sleep
        waitResult = WaitForMultipleObjects(2, events, FALSE, 0);
        if (waitResult == WAIT_OBJECT_0)   // stopEvent_
            return;
        if (waitResult == WAIT_TIMEOUT)    // resumeEvent_ not signaled → paused
        {
            // Block until unpaused or stopped
            waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);
            if (waitResult == WAIT_OBJECT_0)
                return;
        }

        DWORD sleepTime = (totalMs - elapsed < chunk) ? (totalMs - elapsed) : chunk;
        Sleep(sleepTime);
        elapsed += sleepTime;
    }
}

// ---------------------------------------------------------------------------
// ExecuteAction — translates an Action to a SendInput call
// ---------------------------------------------------------------------------

void Player::ExecuteAction(const Action& a)
{
    INPUT input;
    ZeroMemory(&input, sizeof(input));

    switch (a.type)
    {
    // ------------------------------------------------------------------
    case ActionType::MouseMove:
    {
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);

        input.type           = INPUT_MOUSE;
        input.mi.dx          = (screenW > 0) ? MulDiv(a.x, 65535, screenW - 1) : 0;
        input.mi.dy          = (screenH > 0) ? MulDiv(a.y, 65535, screenH - 1) : 0;
        input.mi.dwFlags     = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
        SendInput(1, &input, sizeof(INPUT));
        break;
    }

    // ------------------------------------------------------------------
    case ActionType::MouseClick:
    {
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);

        LONG dx = (screenW > 0) ? MulDiv(a.x, 65535, screenW - 1) : 0;
        LONG dy = (screenH > 0) ? MulDiv(a.y, 65535, screenH - 1) : 0;

        DWORD downFlag = 0;
        DWORD upFlag   = 0;

        switch (a.mouseButton)
        {
        case 0: downFlag = MOUSEEVENTF_LEFTDOWN;   upFlag = MOUSEEVENTF_LEFTUP;   break;
        case 1: downFlag = MOUSEEVENTF_RIGHTDOWN;  upFlag = MOUSEEVENTF_RIGHTUP;  break;
        case 2: downFlag = MOUSEEVENTF_MIDDLEDOWN; upFlag = MOUSEEVENTF_MIDDLEUP; break;
        default: return;
        }

        DWORD clickFlag = a.pressed ? downFlag : upFlag;

        input.type       = INPUT_MOUSE;
        input.mi.dx      = dx;
        input.mi.dy      = dy;
        input.mi.dwFlags = clickFlag | MOUSEEVENTF_ABSOLUTE;
        SendInput(1, &input, sizeof(INPUT));
        break;
    }

    // ------------------------------------------------------------------
    case ActionType::MouseScroll:
    {
        input.type           = INPUT_MOUSE;
        input.mi.mouseData   = static_cast<DWORD>(a.scrollDelta);
        input.mi.dwFlags     = MOUSEEVENTF_WHEEL;
        SendInput(1, &input, sizeof(INPUT));
        break;
    }

    // ------------------------------------------------------------------
    case ActionType::KeyPress:
    case ActionType::KeyRelease:
    {
        input.type         = INPUT_KEYBOARD;
        input.ki.wVk       = static_cast<WORD>(a.vkCode);
        input.ki.wScan     = a.scanCode;
        input.ki.dwFlags   = 0;

        if (!a.pressed)
            input.ki.dwFlags |= KEYEVENTF_KEYUP;

        if (a.extendedKey)
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;

        SendInput(1, &input, sizeof(INPUT));
        break;
    }

    default:
        break;
    }
}
