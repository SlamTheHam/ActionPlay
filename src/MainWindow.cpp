#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <cwchar>
#include "MainWindow.h"
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")

// ---------------------------------------------------------------------------
// Static member definition
// ---------------------------------------------------------------------------
MainWindow* MainWindow::instance_ = nullptr;

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
MainWindow::MainWindow()
    : hwnd_(nullptr)
    , hInstance_(nullptr)
    , hFont_(nullptr)
    , hFontBold_(nullptr)
    , statusColor_(RGB(50, 200, 50))
{
}

MainWindow::~MainWindow()
{
    if (hFont_)     { DeleteObject(hFont_);     hFont_     = nullptr; }
    if (hFontBold_) { DeleteObject(hFontBold_); hFontBold_ = nullptr; }
}

// ---------------------------------------------------------------------------
// Create & Show
// ---------------------------------------------------------------------------
bool MainWindow::Create(HINSTANCE hInstance)
{
    hInstance_ = hInstance;

    // Initialise common controls (trackbar, progress bar)
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_BAR_CLASSES | ICC_PROGRESS_CLASS | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    // Register window class
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance_;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"ActionPlay";
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassExW(&wc))
        return false;

    // Set instance_ before CreateWindowEx so WM_CREATE can find us
    instance_ = this;

    hwnd_ = CreateWindowExW(
        0,
        L"ActionPlay",
        L"ActionPlay",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 620,
        nullptr, nullptr,
        hInstance_, nullptr);

    return hwnd_ != nullptr;
}

void MainWindow::Show(int nCmdShow)
{
    if (hwnd_)
    {
        ShowWindow(hwnd_, nCmdShow);
        UpdateWindow(hwnd_);
    }
}

// ---------------------------------------------------------------------------
// WndProc
// ---------------------------------------------------------------------------
LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    MainWindow* self = instance_;

    switch (msg)
    {
    case WM_CREATE:
        if (self)
        {
            self->hwnd_ = hwnd;
            self->OnCreate(hwnd);
        }
        return 0;

    case WM_COMMAND:
        if (self) self->OnCommand(wParam, lParam);
        return 0;

    case WM_HSCROLL:
    {
        // Speed slider scroll
        HWND hSlider = GetDlgItem(hwnd, IDC_SPEED_SLIDER);
        if (self && (HWND)lParam == hSlider)
        {
            int pos = (int)SendMessageW(hSlider, TBM_GETPOS, 0, 0);
            double speed = pos / 10.0;
            wchar_t buf[32];
            swprintf_s(buf, L"%.1fx", speed);
            SetWindowTextW(GetDlgItem(hwnd, IDC_SPEED_LABEL), buf);
        }
        return 0;
    }

    case WM_HOTKEY:
        if (self) self->OnHotKey((int)wParam);
        return 0;

    case WM_RECORDER_UPDATE:
        if (self) self->OnRecorderUpdate((int)wParam);
        return 0;

    case WM_PLAYER_PROGRESS:
        if (self)
        {
            int actionIdx    = (int)wParam;
            int repeat       = (int)LOWORD(lParam);
            int totalRepeats = (int)HIWORD(lParam);
            self->OnPlayerProgress(actionIdx, repeat, totalRepeats);
        }
        return 0;

    case WM_PLAYER_COMPLETE:
        if (self) self->OnPlayerComplete();
        return 0;

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;
        // Apply coloured text to the status label only
        if (self && GetDlgCtrlID(hCtrl) == IDC_STATUS)
        {
            SetTextColor(hdc, self->statusColor_);
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }
        // All other statics: use the default dialog background
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)(HBRUSH)(COLOR_BTNFACE + 1);
    }

    case WM_DESTROY:
        UnregisterHotKey(hwnd, HOTKEY_START_REC);
        UnregisterHotKey(hwnd, HOTKEY_STOP_REC);
        UnregisterHotKey(hwnd, HOTKEY_PLAY);
        UnregisterHotKey(hwnd, HOTKEY_PAUSE);
        UnregisterHotKey(hwnd, HOTKEY_STOP_PLAY);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// OnCreate — build all child controls
// ---------------------------------------------------------------------------
void MainWindow::OnCreate(HWND hwnd)
{
    // --- Fonts -----------------------------------------------------------
    hFont_ = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
        L"Segoe UI");

    hFontBold_ = CreateFontW(
        -14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
        L"Segoe UI");

    // Helper lambda: create a control and set its font
    auto MakeCtrl = [&](LPCWSTR cls, LPCWSTR text, DWORD style, int x, int y, int w, int h, int id) -> HWND
    {
        HWND hCtrl = CreateWindowExW(
            0, cls, text,
            WS_CHILD | WS_VISIBLE | style,
            x, y, w, h,
            hwnd, (HMENU)(UINT_PTR)id, hInstance_, nullptr);
        SendMessageW(hCtrl, WM_SETFONT, (WPARAM)hFont_, TRUE);
        return hCtrl;
    };

    // -----------------------------------------------------------------------
    // Status bar (top)
    // -----------------------------------------------------------------------
    HWND hStatus = MakeCtrl(L"STATIC", L"\u25cf Ready",
        SS_LEFT, 10, 5, 464, 28, IDC_STATUS);
    SendMessageW(hStatus, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    // -----------------------------------------------------------------------
    // Group: Recording  (y=38, h=65)
    // -----------------------------------------------------------------------
    MakeCtrl(L"BUTTON", L"Recording",
        BS_GROUPBOX, 10, 38, 464, 65, 0);

    MakeCtrl(L"STATIC", L"Actions recorded: 0",
        SS_LEFT, 20, 58, 160, 18, IDC_ACTION_COUNT);

    MakeCtrl(L"BUTTON", L"Start Recording (F9)",
        BS_PUSHBUTTON, 190, 55, 130, 24, IDC_BTN_START_REC);

    MakeCtrl(L"BUTTON", L"Stop Recording (F10)",
        BS_PUSHBUTTON, 330, 55, 130, 24, IDC_BTN_STOP_REC);

    // -----------------------------------------------------------------------
    // Group: Playback  (y=110, h=120)
    // -----------------------------------------------------------------------
    MakeCtrl(L"BUTTON", L"Playback",
        BS_GROUPBOX, 10, 110, 464, 120, 0);

    // Row 1: speed
    MakeCtrl(L"STATIC", L"Speed:",
        SS_LEFT, 20, 133, 45, 18, 0);

    HWND hSlider = MakeCtrl(TRACKBAR_CLASSW, L"",
        TBS_HORZ | TBS_NOTICKS, 68, 130, 300, 24, IDC_SPEED_SLIDER);
    SendMessageW(hSlider, TBM_SETRANGE, TRUE, MAKELPARAM(1, 100));
    SendMessageW(hSlider, TBM_SETPOS,   TRUE, 10);

    MakeCtrl(L"STATIC", L"1.0x",
        SS_LEFT, 372, 133, 50, 18, IDC_SPEED_LABEL);

    // Row 2: repeat
    MakeCtrl(L"STATIC", L"Repeat:",
        SS_LEFT, 20, 163, 50, 18, 0);

    HWND hRadioFinite = MakeCtrl(L"BUTTON", L"Finite",
        BS_AUTORADIOBUTTON | WS_GROUP, 75, 161, 60, 18, IDC_RADIO_FINITE);
    SendMessageW(hRadioFinite, BM_SETCHECK, BST_CHECKED, 0);

    MakeCtrl(L"BUTTON", L"Infinite",
        BS_AUTORADIOBUTTON, 140, 161, 65, 18, IDC_RADIO_INFINITE);

    MakeCtrl(L"EDIT", L"1",
        ES_NUMBER | WS_BORDER, 210, 160, 40, 20, IDC_REPEAT_COUNT);

    MakeCtrl(L"STATIC", L"times",
        SS_LEFT, 255, 163, 40, 18, 0);

    // Row 3: playback buttons
    MakeCtrl(L"BUTTON", L"Play (F5)",
        BS_PUSHBUTTON, 20, 192, 100, 26, IDC_BTN_PLAY);

    MakeCtrl(L"BUTTON", L"Pause (F6)",
        BS_PUSHBUTTON, 130, 192, 100, 26, IDC_BTN_PAUSE);

    MakeCtrl(L"BUTTON", L"Stop (F7)",
        BS_PUSHBUTTON, 240, 192, 100, 26, IDC_BTN_STOP_PLAY);

    // -----------------------------------------------------------------------
    // Group: Progress  (y=237, h=60)
    // -----------------------------------------------------------------------
    MakeCtrl(L"BUTTON", L"Progress",
        BS_GROUPBOX, 10, 237, 464, 60, 0);

    HWND hProg = MakeCtrl(PROGRESS_CLASSW, L"",
        PBS_SMOOTH, 20, 255, 444, 16, IDC_PROGRESSBAR);
    SendMessageW(hProg, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(hProg, PBM_SETPOS, 0, 0);

    MakeCtrl(L"STATIC", L"Action 0 / 0",
        SS_LEFT, 20, 276, 200, 16, IDC_ACTION_LABEL);

    MakeCtrl(L"STATIC", L"Repeat 0 / 0",
        SS_LEFT, 300, 276, 150, 16, IDC_REPEAT_LABEL);

    // -----------------------------------------------------------------------
    // Group: Hotkeys  (y=302, h=40)
    // -----------------------------------------------------------------------
    MakeCtrl(L"BUTTON", L"Hotkeys",
        BS_GROUPBOX, 10, 302, 464, 40, 0);

    MakeCtrl(L"STATIC",
        L"F9 Start Rec  F10 Stop Rec  F5 Play  F6 Pause/Resume  F7 Stop",
        SS_LEFT, 20, 318, 440, 18, 0);

    // -----------------------------------------------------------------------
    // Group: Log  (y=347, h=220)
    // -----------------------------------------------------------------------
    MakeCtrl(L"BUTTON", L"Log",
        BS_GROUPBOX, 10, 347, 464, 220, 0);

    MakeCtrl(L"LISTBOX", L"",
        LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
        18, 365, 448, 194, IDC_LOG);

    // -----------------------------------------------------------------------
    // Register global hotkeys
    // -----------------------------------------------------------------------
    RegisterHotKey(hwnd, HOTKEY_START_REC,  0, VK_F9);
    RegisterHotKey(hwnd, HOTKEY_STOP_REC,   0, VK_F10);
    RegisterHotKey(hwnd, HOTKEY_PLAY,       0, VK_F5);
    RegisterHotKey(hwnd, HOTKEY_PAUSE,      0, VK_F6);
    RegisterHotKey(hwnd, HOTKEY_STOP_PLAY,  0, VK_F7);

    // -----------------------------------------------------------------------
    // Hook recorder notifications to this window
    // -----------------------------------------------------------------------
    recorder_.SetNotifyHwnd(hwnd);

    // -----------------------------------------------------------------------
    // Initial UI state
    // -----------------------------------------------------------------------
    SetButtonStates(false, false, false);
    UpdateStatus(L"\u25cf Ready", RGB(50, 200, 50));
}

// ---------------------------------------------------------------------------
// WM_COMMAND
// ---------------------------------------------------------------------------
void MainWindow::OnCommand(WPARAM wParam, LPARAM lParam)
{
    int id      = LOWORD(wParam);
    int notify  = HIWORD(wParam);

    switch (id)
    {
    case IDC_BTN_START_REC:
        StartRecording();
        break;

    case IDC_BTN_STOP_REC:
        StopRecording();
        break;

    case IDC_BTN_PLAY:
        PlayActions();
        break;

    case IDC_BTN_PAUSE:
        PausePlayback();
        break;

    case IDC_BTN_STOP_PLAY:
        StopPlayback();
        break;

    case IDC_RADIO_FINITE:
        if (notify == BN_CLICKED)
            EnableWindow(GetDlgItem(hwnd_, IDC_REPEAT_COUNT), TRUE);
        break;

    case IDC_RADIO_INFINITE:
        if (notify == BN_CLICKED)
            EnableWindow(GetDlgItem(hwnd_, IDC_REPEAT_COUNT), FALSE);
        break;
    }

    (void)lParam;
}

// ---------------------------------------------------------------------------
// WM_HOTKEY
// ---------------------------------------------------------------------------
void MainWindow::OnHotKey(int id)
{
    switch (id)
    {
    case HOTKEY_START_REC:  StartRecording();  break;
    case HOTKEY_STOP_REC:   StopRecording();   break;
    case HOTKEY_PLAY:       PlayActions();     break;
    case HOTKEY_PAUSE:      PausePlayback();   break;
    case HOTKEY_STOP_PLAY:  StopPlayback();    break;
    }
}

// ---------------------------------------------------------------------------
// Recorder update
// ---------------------------------------------------------------------------
void MainWindow::OnRecorderUpdate(int count)
{
    wchar_t buf[64];
    swprintf_s(buf, L"Actions recorded: %d", count);
    SetWindowTextW(GetDlgItem(hwnd_, IDC_ACTION_COUNT), buf);
}

// ---------------------------------------------------------------------------
// Player progress
// ---------------------------------------------------------------------------
void MainWindow::OnPlayerProgress(int actionIdx, int repeat, int totalRepeats)
{
    int totalActions = recorder_.GetActionCount();

    // Progress bar: fraction through current repeat
    int pct = 0;
    if (totalActions > 0)
        pct = actionIdx * 100 / totalActions;
    SendMessageW(GetDlgItem(hwnd_, IDC_PROGRESSBAR), PBM_SETPOS, (WPARAM)pct, 0);

    // "Action X / Y"
    wchar_t aBuf[64];
    swprintf_s(aBuf, L"Action %d / %d", actionIdx + 1, totalActions);
    SetWindowTextW(GetDlgItem(hwnd_, IDC_ACTION_LABEL), aBuf);

    // "Repeat X / Y"  — totalRepeats == 0xFFFF means infinite
    wchar_t rBuf[64];
    if (totalRepeats == 0xFFFF)
        swprintf_s(rBuf, L"Repeat %d / \u221E", repeat + 1);
    else
        swprintf_s(rBuf, L"Repeat %d / %d", repeat + 1, totalRepeats);
    SetWindowTextW(GetDlgItem(hwnd_, IDC_REPEAT_LABEL), rBuf);
}

// ---------------------------------------------------------------------------
// Player complete
// ---------------------------------------------------------------------------
void MainWindow::OnPlayerComplete()
{
    UpdateStatus(L"\u25cf Ready", RGB(50, 200, 50));
    SetButtonStates(false, false, false);
    AppendLog(L"Playback finished.");

    // Reset progress indicators
    SendMessageW(GetDlgItem(hwnd_, IDC_PROGRESSBAR), PBM_SETPOS, 0, 0);
    SetWindowTextW(GetDlgItem(hwnd_, IDC_ACTION_LABEL), L"Action 0 / 0");
    SetWindowTextW(GetDlgItem(hwnd_, IDC_REPEAT_LABEL), L"Repeat 0 / 0");
}

// ---------------------------------------------------------------------------
// StartRecording
// ---------------------------------------------------------------------------
void MainWindow::StartRecording()
{
    if (player_.IsPlaying())
        return;

    recorder_.Start();
    AppendLog(L"Recording started.");
    UpdateStatus(L"\u25cf Recording...", RGB(220, 50, 50));
    SetButtonStates(true, false, false);
}

// ---------------------------------------------------------------------------
// StopRecording
// ---------------------------------------------------------------------------
void MainWindow::StopRecording()
{
    recorder_.Stop();
    int n = recorder_.GetActionCount();

    wchar_t buf[128];
    swprintf_s(buf, L"Recording stopped. %d action(s) captured.", n);
    AppendLog(buf);

    UpdateStatus(L"\u25cf Ready", RGB(50, 200, 50));
    SetButtonStates(false, false, false);

    wchar_t countBuf[64];
    swprintf_s(countBuf, L"Actions recorded: %d", n);
    SetWindowTextW(GetDlgItem(hwnd_, IDC_ACTION_COUNT), countBuf);
}

// ---------------------------------------------------------------------------
// PlayActions
// ---------------------------------------------------------------------------
void MainWindow::PlayActions()
{
    if (recorder_.GetActionCount() == 0)
    {
        AppendLog(L"No actions recorded.");
        return;
    }

    // If paused, resume instead of starting fresh
    if (player_.IsPlaying() && player_.IsPaused())
    {
        player_.Resume();
        UpdateStatus(L"\u25cf Playing...", RGB(50, 100, 220));
        SetButtonStates(false, true, false);
        return;
    }

    double speed   = GetSpeed();
    int    repeats = GetRepeatCount();

    player_.Play(recorder_.GetActions(), speed, repeats, hwnd_);
    UpdateStatus(L"\u25cf Playing...", RGB(50, 100, 220));
    SetButtonStates(false, true, false);
    AppendLog(L"Playback started.");
}

// ---------------------------------------------------------------------------
// PausePlayback
// ---------------------------------------------------------------------------
void MainWindow::PausePlayback()
{
    if (player_.IsPlaying() && !player_.IsPaused())
    {
        player_.Pause();
        UpdateStatus(L"\u25cf Paused", RGB(220, 180, 50));
        SetButtonStates(false, true, true);
        AppendLog(L"Paused.");
    }
    else if (player_.IsPlaying() && player_.IsPaused())
    {
        // Toggle: resume
        PlayActions();
    }
}

// ---------------------------------------------------------------------------
// StopPlayback
// ---------------------------------------------------------------------------
void MainWindow::StopPlayback()
{
    player_.Stop();
    UpdateStatus(L"\u25cf Ready", RGB(50, 200, 50));
    SetButtonStates(false, false, false);
    AppendLog(L"Stopped.");

    // Reset progress indicators
    SendMessageW(GetDlgItem(hwnd_, IDC_PROGRESSBAR), PBM_SETPOS, 0, 0);
    SetWindowTextW(GetDlgItem(hwnd_, IDC_ACTION_LABEL), L"Action 0 / 0");
    SetWindowTextW(GetDlgItem(hwnd_, IDC_REPEAT_LABEL), L"Repeat 0 / 0");
}

// ---------------------------------------------------------------------------
// UpdateStatus
// ---------------------------------------------------------------------------
void MainWindow::UpdateStatus(const wchar_t* text, COLORREF color)
{
    statusColor_ = color;
    HWND hStatus = GetDlgItem(hwnd_, IDC_STATUS);
    SetWindowTextW(hStatus, text);
    InvalidateRect(hStatus, nullptr, TRUE);
}

// ---------------------------------------------------------------------------
// AppendLog
// ---------------------------------------------------------------------------
void MainWindow::AppendLog(const wchar_t* text)
{
    HWND hLog = GetDlgItem(hwnd_, IDC_LOG);

    // Keep the log to a maximum of 100 items
    int count = (int)SendMessageW(hLog, LB_GETCOUNT, 0, 0);
    while (count >= 100)
    {
        SendMessageW(hLog, LB_DELETESTRING, 0, 0);
        count = (int)SendMessageW(hLog, LB_GETCOUNT, 0, 0);
    }

    int idx = (int)SendMessageW(hLog, LB_ADDSTRING, 0, (LPARAM)text);
    SendMessageW(hLog, LB_SETCURSEL, (WPARAM)idx, 0);
}

// ---------------------------------------------------------------------------
// SetButtonStates
// ---------------------------------------------------------------------------
void MainWindow::SetButtonStates(bool recording, bool playing, bool paused)
{
    // Start Rec: only when not recording and not playing
    EnableWindow(GetDlgItem(hwnd_, IDC_BTN_START_REC),
        !recording && !playing ? TRUE : FALSE);

    // Stop Rec: only when recording
    EnableWindow(GetDlgItem(hwnd_, IDC_BTN_STOP_REC),
        recording ? TRUE : FALSE);

    // Play: available when not recording; can also be used to resume
    EnableWindow(GetDlgItem(hwnd_, IDC_BTN_PLAY),
        !recording ? TRUE : FALSE);

    // Pause: only when playing and not already paused
    EnableWindow(GetDlgItem(hwnd_, IDC_BTN_PAUSE),
        playing ? TRUE : FALSE);

    // Stop Play: only when playing
    EnableWindow(GetDlgItem(hwnd_, IDC_BTN_STOP_PLAY),
        playing ? TRUE : FALSE);

    // Speed & repeat controls: not during recording or active (unpaused) play
    BOOL settingsEnabled = (!recording && (!playing || paused)) ? TRUE : FALSE;
    EnableWindow(GetDlgItem(hwnd_, IDC_SPEED_SLIDER), settingsEnabled);

    // Repeat count edit only if finite mode is selected
    BOOL finiteChecked = (SendMessageW(GetDlgItem(hwnd_, IDC_RADIO_FINITE),
                              BM_GETCHECK, 0, 0) == BST_CHECKED);
    EnableWindow(GetDlgItem(hwnd_, IDC_REPEAT_COUNT),
        settingsEnabled && finiteChecked ? TRUE : FALSE);

    EnableWindow(GetDlgItem(hwnd_, IDC_RADIO_FINITE),    settingsEnabled);
    EnableWindow(GetDlgItem(hwnd_, IDC_RADIO_INFINITE),  settingsEnabled);

    (void)paused;  // already used above
}

// ---------------------------------------------------------------------------
// GetSpeed
// ---------------------------------------------------------------------------
double MainWindow::GetSpeed()
{
    int pos = (int)SendMessageW(GetDlgItem(hwnd_, IDC_SPEED_SLIDER),
                                TBM_GETPOS, 0, 0);
    return pos / 10.0;
}

// ---------------------------------------------------------------------------
// GetRepeatCount  (-1 = infinite)
// ---------------------------------------------------------------------------
int MainWindow::GetRepeatCount()
{
    HWND hInfinite = GetDlgItem(hwnd_, IDC_RADIO_INFINITE);
    if (SendMessageW(hInfinite, BM_GETCHECK, 0, 0) == BST_CHECKED)
        return -1;

    wchar_t buf[32] = {};
    GetWindowTextW(GetDlgItem(hwnd_, IDC_REPEAT_COUNT), buf, 32);

    int val = _wtoi(buf);
    if (val < 1) val = 1;
    return val;
}
