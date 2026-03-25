#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include "Recorder.h"
#include "Action.h"
#include "resource.h"

// ----------------------------------------------------------------------------
// Static member definition
// ----------------------------------------------------------------------------
Recorder* Recorder::instance_ = nullptr;

// ----------------------------------------------------------------------------
// Constructor / Destructor
// ----------------------------------------------------------------------------
Recorder::Recorder()
    : hookThread_(nullptr)
    , hookThreadId_(0)
    , mouseHook_(nullptr)
    , keyboardHook_(nullptr)
    , recording_(false)
    , notifyHwnd_(nullptr)
    , lastMouseX_(0)
    , lastMouseY_(0)
{
    InitializeCriticalSection(&cs_);
    startTime_.QuadPart = 0;
    frequency_.QuadPart = 0;
}

Recorder::~Recorder()
{
    if (recording_) {
        Stop();
    }
    DeleteCriticalSection(&cs_);
}

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------
void Recorder::Start()
{
    if (recording_) {
        return;
    }

    // Clear any previous recording data
    EnterCriticalSection(&cs_);
    actions_.clear();
    LeaveCriticalSection(&cs_);

    // Set up performance counter baseline
    QueryPerformanceFrequency(&frequency_);
    QueryPerformanceCounter(&startTime_);

    lastMouseX_ = 0;
    lastMouseY_ = 0;

    // Expose this instance to the static callbacks
    instance_ = this;
    recording_ = true;

    // Spawn the hook thread; hooks must be installed on the thread that runs
    // the message loop so that the system can deliver hook notifications to it
    hookThread_ = CreateThread(
        nullptr,            // default security
        0,                  // default stack size
        HookThreadProc,
        this,               // pass Recorder* as parameter
        0,                  // start immediately
        &hookThreadId_
    );
}

void Recorder::Stop()
{
    if (!recording_) {
        return;
    }

    recording_ = false;

    // Ask the hook thread's message loop to exit
    if (hookThreadId_ != 0) {
        PostThreadMessage(hookThreadId_, WM_QUIT, 0, 0);
    }

    // Wait for the hook thread to finish (give it a reasonable timeout)
    if (hookThread_ != nullptr) {
        WaitForSingleObject(hookThread_, 5000);
        CloseHandle(hookThread_);
        hookThread_ = nullptr;
    }

    hookThreadId_ = 0;
    instance_ = nullptr;
}

bool Recorder::IsRecording() const
{
    return recording_;
}

ActionList Recorder::GetActions() const
{
    EnterCriticalSection(&cs_);
    ActionList copy = actions_;
    LeaveCriticalSection(&cs_);
    return copy;
}

int Recorder::GetActionCount() const
{
    EnterCriticalSection(&cs_);
    int count = static_cast<int>(actions_.size());
    LeaveCriticalSection(&cs_);
    return count;
}

void Recorder::SetNotifyHwnd(HWND hwnd)
{
    notifyHwnd_ = hwnd;
}

// ----------------------------------------------------------------------------
// Hook thread
// ----------------------------------------------------------------------------
DWORD WINAPI Recorder::HookThreadProc(LPVOID param)
{
    Recorder* self = static_cast<Recorder*>(param);

    // Install low-level hooks on this thread
    self->mouseHook_ = SetWindowsHookEx(
        WH_MOUSE_LL,
        MouseProc,
        nullptr,    // LL hooks: hMod must be NULL
        0           // LL hooks: dwThreadId must be 0
    );

    self->keyboardHook_ = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        KeyboardProc,
        nullptr,
        0
    );

    // Standard message loop — keeps the thread alive and delivers hook events
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Clean up hooks before exiting
    if (self->mouseHook_ != nullptr) {
        UnhookWindowsHookEx(self->mouseHook_);
        self->mouseHook_ = nullptr;
    }

    if (self->keyboardHook_ != nullptr) {
        UnhookWindowsHookEx(self->keyboardHook_);
        self->keyboardHook_ = nullptr;
    }

    return 0;
}

// ----------------------------------------------------------------------------
// Helper: compute timestamp in seconds from the recording start
// ----------------------------------------------------------------------------
static double ComputeTimestamp(const LARGE_INTEGER& startTime,
                               const LARGE_INTEGER& frequency)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return static_cast<double>(now.QuadPart - startTime.QuadPart)
         / static_cast<double>(frequency.QuadPart);
}

// ----------------------------------------------------------------------------
// Mouse hook
// ----------------------------------------------------------------------------
LRESULT CALLBACK Recorder::MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0 || instance_ == nullptr || !instance_->recording_) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    const MSLLHOOKSTRUCT* ms = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
    Recorder* self = instance_;

    Action action{};
    bool shouldRecord = false;

    switch (wParam) {
        case WM_MOUSEMOVE: {
            int dx = ms->pt.x - self->lastMouseX_;
            int dy = ms->pt.y - self->lastMouseY_;
            // Throttle: only record if the cursor moved at least 5 pixels
            if ((dx * dx + dy * dy) >= 25) {
                action.type      = ActionType::MouseMove;
                action.timestamp = ComputeTimestamp(self->startTime_, self->frequency_);
                action.x         = ms->pt.x;
                action.y         = ms->pt.y;
                self->lastMouseX_ = ms->pt.x;
                self->lastMouseY_ = ms->pt.y;
                shouldRecord = true;
            }
            break;
        }

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            action.type        = ActionType::MouseClick;
            action.timestamp   = ComputeTimestamp(self->startTime_, self->frequency_);
            action.x           = ms->pt.x;
            action.y           = ms->pt.y;
            action.mouseButton = 0;
            action.pressed     = (wParam == WM_LBUTTONDOWN);
            shouldRecord = true;
            break;

        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            action.type        = ActionType::MouseClick;
            action.timestamp   = ComputeTimestamp(self->startTime_, self->frequency_);
            action.x           = ms->pt.x;
            action.y           = ms->pt.y;
            action.mouseButton = 1;
            action.pressed     = (wParam == WM_RBUTTONDOWN);
            shouldRecord = true;
            break;

        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            action.type        = ActionType::MouseClick;
            action.timestamp   = ComputeTimestamp(self->startTime_, self->frequency_);
            action.x           = ms->pt.x;
            action.y           = ms->pt.y;
            action.mouseButton = 2;
            action.pressed     = (wParam == WM_MBUTTONDOWN);
            shouldRecord = true;
            break;

        case WM_MOUSEWHEEL: {
            // The high-order word of mouseData is the signed wheel delta
            SHORT delta = static_cast<SHORT>(HIWORD(ms->mouseData));
            action.type        = ActionType::MouseScroll;
            action.timestamp   = ComputeTimestamp(self->startTime_, self->frequency_);
            action.x           = ms->pt.x;
            action.y           = ms->pt.y;
            action.scrollDelta = static_cast<int>(delta);
            shouldRecord = true;
            break;
        }

        default:
            break;
    }

    if (shouldRecord) {
        EnterCriticalSection(&self->cs_);
        self->actions_.push_back(action);
        int count = static_cast<int>(self->actions_.size());
        LeaveCriticalSection(&self->cs_);

        if (self->notifyHwnd_ != nullptr) {
            PostMessage(self->notifyHwnd_, WM_RECORDER_UPDATE,
                        static_cast<WPARAM>(count), 0);
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// ----------------------------------------------------------------------------
// Keyboard hook
// ----------------------------------------------------------------------------
LRESULT CALLBACK Recorder::KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0 || instance_ == nullptr || !instance_->recording_) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    const KBDLLHOOKSTRUCT* kb = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
    Recorder* self = instance_;

    // Exclude hotkeys used to control the recorder itself
    const DWORD vk = kb->vkCode;
    if (vk == VK_F5  || vk == VK_F6 || vk == VK_F7 ||
        vk == VK_F9  || vk == VK_F10)
    {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    bool isKeyUp   = (wParam == WM_KEYUP   || wParam == WM_SYSKEYUP);

    if (!isKeyDown && !isKeyUp) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    Action action{};
    action.type        = isKeyDown ? ActionType::KeyPress : ActionType::KeyRelease;
    action.timestamp   = ComputeTimestamp(self->startTime_, self->frequency_);
    action.pressed     = isKeyDown;
    action.vkCode      = kb->vkCode;
    action.scanCode    = static_cast<WORD>(kb->scanCode);
    // Bit 0 of flags indicates an extended key
    action.extendedKey = (kb->flags & LLKHF_EXTENDED) != 0;

    EnterCriticalSection(&self->cs_);
    self->actions_.push_back(action);
    int count = static_cast<int>(self->actions_.size());
    LeaveCriticalSection(&self->cs_);

    if (self->notifyHwnd_ != nullptr) {
        PostMessage(self->notifyHwnd_, WM_RECORDER_UPDATE,
                    static_cast<WPARAM>(count), 0);
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
