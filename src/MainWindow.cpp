#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <shlobj.h>
#include <cstdio>
#include <cwchar>
#include <string>
#include "MainWindow.h"
#include "NameDialog.h"
#include "RecordingStore.h"
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shell32.lib")

// ---------------------------------------------------------------------------
// Static member
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
    , trayAdded_(false)
{
    ZeroMemory(&trayNid_, sizeof(trayNid_));
}

MainWindow::~MainWindow()
{
    RemoveTrayIcon();
    if (hFont_)     { DeleteObject(hFont_);     hFont_     = nullptr; }
    if (hFontBold_) { DeleteObject(hFontBold_); hFontBold_ = nullptr; }
}

// ---------------------------------------------------------------------------
// Create & Show
// ---------------------------------------------------------------------------
bool MainWindow::Create(HINSTANCE hInstance)
{
    hInstance_ = hInstance;

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_BAR_CLASSES | ICC_PROGRESS_CLASS | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

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

    instance_ = this;

    hwnd_ = CreateWindowExW(
        0,
        L"ActionPlay",
        L"ActionPlay",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        520, 740,
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
        if (self) { self->hwnd_ = hwnd; self->OnCreate(hwnd); }
        return 0;

    case WM_COMMAND:
        if (self) self->OnCommand(wParam, lParam);
        return 0;

    case WM_HSCROLL:
    {
        HWND hSlider = GetDlgItem(hwnd, IDC_SPEED_SLIDER);
        if (self && (HWND)lParam == hSlider)
        {
            int pos = (int)SendMessageW(hSlider, TBM_GETPOS, 0, 0);
            wchar_t buf[32];
            swprintf_s(buf, L"%.1fx", pos / 10.0);
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
            self->OnPlayerProgress((int)wParam,
                                   (int)LOWORD(lParam),
                                   (int)HIWORD(lParam));
        return 0;

    case WM_PLAYER_COMPLETE:
        if (self) self->OnPlayerComplete();
        return 0;

    // -----------------------------------------------------------------------
    // Fix for status-label text ghosting: return an opaque brush so the
    // control properly erases its background before painting new text.
    // -----------------------------------------------------------------------
    case WM_CTLCOLORSTATIC:
    {
        HDC  hdc   = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;
        if (self && GetDlgCtrlID(hCtrl) == IDC_STATUS)
        {
            SetTextColor(hdc, self->statusColor_);
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, GetSysColor(COLOR_BTNFACE));
        }
        else
        {
            SetBkMode(hdc, TRANSPARENT);
        }
        return (LRESULT)(HBRUSH)(COLOR_BTNFACE + 1);
    }

    case WM_DESTROY:
        if (self) self->OnDestroy();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// OnCreate
// ---------------------------------------------------------------------------
void MainWindow::OnCreate(HWND hwnd)
{
    hFont_ = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    hFontBold_ = CreateFontW(
        -14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    auto MakeCtrl = [&](LPCWSTR cls, LPCWSTR text, DWORD style,
                        int x, int y, int w, int h, int id) -> HWND
    {
        HWND hCtrl = CreateWindowExW(
            0, cls, text, WS_CHILD | WS_VISIBLE | style,
            x, y, w, h, hwnd, (HMENU)(UINT_PTR)id, hInstance_, nullptr);
        SendMessageW(hCtrl, WM_SETFONT, (WPARAM)hFont_, TRUE);
        return hCtrl;
    };

    // ── Status bar ─────────────────────────────────────────────────────────
    HWND hStatus = MakeCtrl(L"STATIC", L"\u25cf Ready",
        SS_LEFT, 10, 6, 484, 24, IDC_STATUS);
    SendMessageW(hStatus, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    // ── Group: Recording  (y=36, h=65) ────────────────────────────────────
    MakeCtrl(L"BUTTON", L"Recording",   BS_GROUPBOX,   10,  36, 484,  65, 0);
    MakeCtrl(L"STATIC", L"Actions recorded: 0",
        SS_LEFT,                                        20,  57, 160,  18, IDC_ACTION_COUNT);
    MakeCtrl(L"BUTTON", L"Start Recording (F9)",
        BS_PUSHBUTTON,                                 200,  54, 140,  24, IDC_BTN_START_REC);
    MakeCtrl(L"BUTTON", L"Stop Recording (F10)",
        BS_PUSHBUTTON,                                 348,  54, 136,  24, IDC_BTN_STOP_REC);

    // ── Group: Playback  (y=107, h=120) ───────────────────────────────────
    MakeCtrl(L"BUTTON", L"Playback",    BS_GROUPBOX,   10, 107, 484, 120, 0);

    MakeCtrl(L"STATIC", L"Speed:",      SS_LEFT,       20, 130,  45,  18, 0);
    HWND hSlider = MakeCtrl(TRACKBAR_CLASSW, L"",
        TBS_HORZ | TBS_NOTICKS,                        68, 127, 308,  24, IDC_SPEED_SLIDER);
    SendMessageW(hSlider, TBM_SETRANGE, TRUE, MAKELPARAM(1, 100));
    SendMessageW(hSlider, TBM_SETPOS,   TRUE, 10);
    MakeCtrl(L"STATIC", L"1.0x",        SS_LEFT,      380, 130,  50,  18, IDC_SPEED_LABEL);

    MakeCtrl(L"STATIC", L"Repeat:",     SS_LEFT,       20, 160,  50,  18, 0);
    HWND hRadioFinite = MakeCtrl(L"BUTTON", L"Finite",
        BS_AUTORADIOBUTTON | WS_GROUP,                 75, 158,  60,  18, IDC_RADIO_FINITE);
    SendMessageW(hRadioFinite, BM_SETCHECK, BST_CHECKED, 0);
    MakeCtrl(L"BUTTON", L"Infinite",
        BS_AUTORADIOBUTTON,                           140, 158,  65,  18, IDC_RADIO_INFINITE);
    MakeCtrl(L"EDIT",   L"1",
        ES_NUMBER | WS_BORDER,                        210, 157,  40,  20, IDC_REPEAT_COUNT);
    MakeCtrl(L"STATIC", L"times",       SS_LEFT,      255, 160,  40,  18, 0);

    MakeCtrl(L"BUTTON", L"Play (F5)",   BS_PUSHBUTTON, 20, 190, 100,  26, IDC_BTN_PLAY);
    MakeCtrl(L"BUTTON", L"Pause (F6)",  BS_PUSHBUTTON,130, 190, 100,  26, IDC_BTN_PAUSE);
    MakeCtrl(L"BUTTON", L"Stop (F7)",   BS_PUSHBUTTON,240, 190, 100,  26, IDC_BTN_STOP_PLAY);

    // ── Group: Progress  (y=233, h=60) ────────────────────────────────────
    MakeCtrl(L"BUTTON", L"Progress",    BS_GROUPBOX,   10, 233, 484,  60, 0);
    HWND hProg = MakeCtrl(PROGRESS_CLASSW, L"",
        PBS_SMOOTH,                                    20, 251, 464,  16, IDC_PROGRESSBAR);
    SendMessageW(hProg, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    MakeCtrl(L"STATIC", L"Action 0 / 0",  SS_LEFT,    20, 272, 200,  16, IDC_ACTION_LABEL);
    MakeCtrl(L"STATIC", L"Repeat 0 / 0",  SS_LEFT,   310, 272, 174,  16, IDC_REPEAT_LABEL);

    // ── Group: Saved Recordings  (y=299, h=155) ───────────────────────────
    MakeCtrl(L"BUTTON", L"Saved Recordings", BS_GROUPBOX, 10, 299, 484, 155, 0);
    MakeCtrl(L"LISTBOX", L"",
        LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
        20, 317, 464, 100, IDC_LIST_RECORDINGS);
    MakeCtrl(L"BUTTON", L"Load & Play",  BS_PUSHBUTTON, 20, 423, 110,  24, IDC_BTN_LOAD_REC);
    MakeCtrl(L"BUTTON", L"Delete",       BS_PUSHBUTTON,140, 423,  80,  24, IDC_BTN_DELETE_REC);

    // ── Group: Hotkeys  (y=460, h=38) ─────────────────────────────────────
    MakeCtrl(L"BUTTON", L"Hotkeys",     BS_GROUPBOX,   10, 460, 484,  38, 0);
    MakeCtrl(L"STATIC",
        L"F9 Start Rec  \u2502  F10 Stop Rec  \u2502  F5 Play  \u2502  F6 Pause/Resume  \u2502  F7 Stop",
        SS_LEFT,                                       20, 476, 460,  16, 0);

    // ── Group: Log  (y=504, h=165) ────────────────────────────────────────
    MakeCtrl(L"BUTTON", L"Log",         BS_GROUPBOX,   10, 504, 484, 165, 0);
    MakeCtrl(L"LISTBOX", L"",
        LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
        18, 520, 468, 142, IDC_LOG);

    // ── Register hotkeys ──────────────────────────────────────────────────
    RegisterHotKey(hwnd, HOTKEY_START_REC,  0, VK_F9);
    RegisterHotKey(hwnd, HOTKEY_STOP_REC,   0, VK_F10);
    RegisterHotKey(hwnd, HOTKEY_PLAY,       0, VK_F5);
    RegisterHotKey(hwnd, HOTKEY_PAUSE,      0, VK_F6);
    RegisterHotKey(hwnd, HOTKEY_STOP_PLAY,  0, VK_F7);

    recorder_.SetNotifyHwnd(hwnd);

    SetupTrayIcon();
    SetButtonStates(false, false, false);
    UpdateStatus(L"\u25cf Ready", RGB(50, 200, 50));
    RefreshRecordingsList();
}

// ---------------------------------------------------------------------------
// OnDestroy
// ---------------------------------------------------------------------------
void MainWindow::OnDestroy()
{
    UnregisterHotKey(hwnd_, HOTKEY_START_REC);
    UnregisterHotKey(hwnd_, HOTKEY_STOP_REC);
    UnregisterHotKey(hwnd_, HOTKEY_PLAY);
    UnregisterHotKey(hwnd_, HOTKEY_PAUSE);
    UnregisterHotKey(hwnd_, HOTKEY_STOP_PLAY);
    RemoveTrayIcon();
}

// ---------------------------------------------------------------------------
// WM_COMMAND
// ---------------------------------------------------------------------------
void MainWindow::OnCommand(WPARAM wParam, LPARAM lParam)
{
    int id     = LOWORD(wParam);
    int notify = HIWORD(wParam);

    switch (id)
    {
    case IDC_BTN_START_REC:   StartRecording();         break;
    case IDC_BTN_STOP_REC:    StopRecording();          break;
    case IDC_BTN_PLAY:        PlayActions();             break;
    case IDC_BTN_PAUSE:       PausePlayback();          break;
    case IDC_BTN_STOP_PLAY:   StopPlayback();           break;
    case IDC_BTN_LOAD_REC:    LoadSelectedRecording();  break;
    case IDC_BTN_DELETE_REC:  DeleteSelectedRecording();break;

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
// Recorder update (action-count changed)
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
    int total = (int)currentActions_.size();

    int pct = (total > 0) ? (actionIdx * 100 / total) : 0;
    SendMessageW(GetDlgItem(hwnd_, IDC_PROGRESSBAR), PBM_SETPOS, (WPARAM)pct, 0);

    wchar_t aBuf[64];
    swprintf_s(aBuf, L"Action %d / %d", actionIdx + 1, total);
    SetWindowTextW(GetDlgItem(hwnd_, IDC_ACTION_LABEL), aBuf);

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
    SendMessageW(GetDlgItem(hwnd_, IDC_PROGRESSBAR), PBM_SETPOS, 0, 0);
    SetWindowTextW(GetDlgItem(hwnd_, IDC_ACTION_LABEL), L"Action 0 / 0");
    SetWindowTextW(GetDlgItem(hwnd_, IDC_REPEAT_LABEL), L"Repeat 0 / 0");
}

// ---------------------------------------------------------------------------
// StartRecording
// ---------------------------------------------------------------------------
void MainWindow::StartRecording()
{
    if (player_.IsPlaying()) return;

    recorder_.Start();
    AppendLog(L"Recording started.");
    UpdateStatus(L"\u25cf Recording...", RGB(220, 50, 50));
    SetButtonStates(true, false, false);
    ShowTrayNotification(L"ActionPlay", L"Recording started (F10 to stop)");
}

// ---------------------------------------------------------------------------
// StopRecording
// ---------------------------------------------------------------------------
void MainWindow::StopRecording()
{
    if (!recorder_.IsRecording()) return;

    recorder_.Stop();
    currentActions_ = recorder_.GetActions();
    int n = (int)currentActions_.size();

    wchar_t buf[128];
    swprintf_s(buf, L"Recording stopped. %d action(s) captured.", n);
    AppendLog(buf);
    UpdateStatus(L"\u25cf Ready", RGB(50, 200, 50));
    SetButtonStates(false, false, false);

    wchar_t countBuf[64];
    swprintf_s(countBuf, L"Actions recorded: %d", n);
    SetWindowTextW(GetDlgItem(hwnd_, IDC_ACTION_COUNT), countBuf);

    ShowTrayNotification(L"ActionPlay", L"Recording stopped.");

    // Bring app to front and prompt to save
    SetForegroundWindow(hwnd_);
    BringWindowToTop(hwnd_);

    if (n > 0)
        PromptSaveRecording();
}

// ---------------------------------------------------------------------------
// PromptSaveRecording  – "Save recording?" dialog flow
// ---------------------------------------------------------------------------
bool MainWindow::PromptSaveRecording()
{
    int n = (int)currentActions_.size();
    wchar_t question[128];
    swprintf_s(question, L"Save this recording?\n%d action(s) captured.", n);

    int choice = MessageBoxW(hwnd_, question, L"Save Recording?",
                             MB_YESNO | MB_ICONQUESTION);
    if (choice != IDYES) return false;

    // Generate a default name like "Recording 1", "Recording 2", ...
    auto existing = RecordingStore::ListAll();
    std::wstring defaultName = L"Recording ";
    defaultName += std::to_wstring((int)existing.size() + 1);

    std::wstring name;
    if (!PromptRecordingName(defaultName, name)) return false;
    if (name.empty()) return false;

    if (!RecordingStore::Save(name, currentActions_))
    {
        MessageBoxW(hwnd_, L"Failed to save recording.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    wchar_t logMsg[128];
    swprintf_s(logMsg, L"Saved recording: \"%ls\"", name.c_str());
    AppendLog(logMsg);
    RefreshRecordingsList();
    return true;
}

// ---------------------------------------------------------------------------
// PromptRecordingName  – shows NameDialog
// ---------------------------------------------------------------------------
bool MainWindow::PromptRecordingName(const std::wstring& defaultName, std::wstring& outName)
{
    return NameDialog::Show(hwnd_, defaultName.c_str(), outName);
}

// ---------------------------------------------------------------------------
// PlayActions
// ---------------------------------------------------------------------------
void MainWindow::PlayActions()
{
    if (currentActions_.empty())
    {
        AppendLog(L"No actions to play. Record something or load a saved recording.");
        return;
    }

    if (player_.IsPlaying() && player_.IsPaused())
    {
        player_.Resume();
        UpdateStatus(L"\u25cf Playing...", RGB(50, 100, 220));
        SetButtonStates(false, true, false);
        AppendLog(L"Resumed.");
        return;
    }

    if (player_.IsPlaying()) return;

    double speed   = GetSpeed();
    int    repeats = GetRepeatCount();

    player_.Play(currentActions_, speed, repeats, hwnd_);
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
        PlayActions();  // toggle: resume
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
    SendMessageW(GetDlgItem(hwnd_, IDC_PROGRESSBAR), PBM_SETPOS, 0, 0);
    SetWindowTextW(GetDlgItem(hwnd_, IDC_ACTION_LABEL), L"Action 0 / 0");
    SetWindowTextW(GetDlgItem(hwnd_, IDC_REPEAT_LABEL), L"Repeat 0 / 0");
}

// ---------------------------------------------------------------------------
// RefreshRecordingsList
// ---------------------------------------------------------------------------
void MainWindow::RefreshRecordingsList()
{
    HWND hList = GetDlgItem(hwnd_, IDC_LIST_RECORDINGS);
    SendMessageW(hList, LB_RESETCONTENT, 0, 0);

    auto recs = RecordingStore::ListAll();
    for (auto& r : recs)
        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)r.name.c_str());
}

// ---------------------------------------------------------------------------
// LoadSelectedRecording
// ---------------------------------------------------------------------------
void MainWindow::LoadSelectedRecording()
{
    HWND hList = GetDlgItem(hwnd_, IDC_LIST_RECORDINGS);
    int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR)
    {
        AppendLog(L"No recording selected.");
        return;
    }

    auto recs = RecordingStore::ListAll();
    if (sel >= (int)recs.size()) return;

    ActionList actions;
    if (!RecordingStore::LoadActions(recs[sel].filePath, actions))
    {
        AppendLog(L"Failed to load recording.");
        return;
    }

    currentActions_ = actions;

    wchar_t buf[128];
    swprintf_s(buf, L"Loaded \"%ls\" (%d actions). Press F5 to play.",
               recs[sel].name.c_str(), (int)actions.size());
    AppendLog(buf);

    wchar_t countBuf[64];
    swprintf_s(countBuf, L"Actions recorded: %d", (int)actions.size());
    SetWindowTextW(GetDlgItem(hwnd_, IDC_ACTION_COUNT), countBuf);

    // Start playback immediately
    PlayActions();
}

// ---------------------------------------------------------------------------
// DeleteSelectedRecording
// ---------------------------------------------------------------------------
void MainWindow::DeleteSelectedRecording()
{
    HWND hList = GetDlgItem(hwnd_, IDC_LIST_RECORDINGS);
    int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR)
    {
        AppendLog(L"No recording selected.");
        return;
    }

    auto recs = RecordingStore::ListAll();
    if (sel >= (int)recs.size()) return;

    wchar_t question[256];
    swprintf_s(question, L"Delete recording \"%ls\"?", recs[sel].name.c_str());
    if (MessageBoxW(hwnd_, question, L"Delete Recording",
                    MB_YESNO | MB_ICONWARNING) != IDYES) return;

    if (RecordingStore::Delete(recs[sel].filePath))
    {
        wchar_t buf[128];
        swprintf_s(buf, L"Deleted \"%ls\".", recs[sel].name.c_str());
        AppendLog(buf);
        RefreshRecordingsList();
    }
    else
        AppendLog(L"Failed to delete recording.");
}

// ---------------------------------------------------------------------------
// Tray icon helpers
// ---------------------------------------------------------------------------
void MainWindow::SetupTrayIcon()
{
    ZeroMemory(&trayNid_, sizeof(trayNid_));
    trayNid_.cbSize           = sizeof(trayNid_);
    trayNid_.hWnd             = hwnd_;
    trayNid_.uID              = TRAY_ICON_ID;
    trayNid_.uFlags           = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    trayNid_.uCallbackMessage = WM_APP + 10;
    trayNid_.hIcon            = LoadIcon(nullptr, IDI_APPLICATION);
    wcsncpy_s(trayNid_.szTip, L"ActionPlay", ARRAYSIZE(trayNid_.szTip));

    if (Shell_NotifyIconW(NIM_ADD, &trayNid_))
        trayAdded_ = true;
}

void MainWindow::RemoveTrayIcon()
{
    if (trayAdded_)
    {
        Shell_NotifyIconW(NIM_DELETE, &trayNid_);
        trayAdded_ = false;
    }
}

void MainWindow::ShowTrayNotification(const wchar_t* title, const wchar_t* msg)
{
    if (!trayAdded_) return;

    NOTIFYICONDATA nid  = trayNid_;
    nid.uFlags          = NIF_INFO;
    nid.dwInfoFlags     = NIIF_INFO;
    nid.uTimeout        = 3000;
    wcsncpy_s(nid.szInfoTitle, title, ARRAYSIZE(nid.szInfoTitle));
    wcsncpy_s(nid.szInfo,      msg,   ARRAYSIZE(nid.szInfo));
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

// ---------------------------------------------------------------------------
// UpdateStatus  – sets text and colour; uses opaque background to prevent
//                 the old text from ghosting through after updates.
// ---------------------------------------------------------------------------
void MainWindow::UpdateStatus(const wchar_t* text, COLORREF color)
{
    statusColor_ = color;
    HWND hStatus = GetDlgItem(hwnd_, IDC_STATUS);
    SetWindowTextW(hStatus, text);
    // Force a full erase + repaint so no ghost text remains
    RedrawWindow(hStatus, nullptr, nullptr,
                 RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
}

// ---------------------------------------------------------------------------
// AppendLog
// ---------------------------------------------------------------------------
void MainWindow::AppendLog(const wchar_t* text)
{
    HWND hLog = GetDlgItem(hwnd_, IDC_LOG);
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
    EnableWindow(GetDlgItem(hwnd_, IDC_BTN_START_REC),
        !recording && !playing ? TRUE : FALSE);
    EnableWindow(GetDlgItem(hwnd_, IDC_BTN_STOP_REC),
        recording ? TRUE : FALSE);
    EnableWindow(GetDlgItem(hwnd_, IDC_BTN_PLAY),
        !recording ? TRUE : FALSE);
    EnableWindow(GetDlgItem(hwnd_, IDC_BTN_PAUSE),
        playing ? TRUE : FALSE);
    EnableWindow(GetDlgItem(hwnd_, IDC_BTN_STOP_PLAY),
        playing ? TRUE : FALSE);

    BOOL settingsEnabled = (!recording && (!playing || paused)) ? TRUE : FALSE;
    EnableWindow(GetDlgItem(hwnd_, IDC_SPEED_SLIDER),   settingsEnabled);
    EnableWindow(GetDlgItem(hwnd_, IDC_RADIO_FINITE),   settingsEnabled);
    EnableWindow(GetDlgItem(hwnd_, IDC_RADIO_INFINITE), settingsEnabled);

    BOOL finiteChecked = (SendMessageW(GetDlgItem(hwnd_, IDC_RADIO_FINITE),
                              BM_GETCHECK, 0, 0) == BST_CHECKED);
    EnableWindow(GetDlgItem(hwnd_, IDC_REPEAT_COUNT),
        settingsEnabled && finiteChecked ? TRUE : FALSE);

    // Load/Delete buttons only when not recording or playing
    EnableWindow(GetDlgItem(hwnd_, IDC_BTN_LOAD_REC),
        !recording && !playing ? TRUE : FALSE);
    EnableWindow(GetDlgItem(hwnd_, IDC_BTN_DELETE_REC),
        !recording && !playing ? TRUE : FALSE);
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
    if (SendMessageW(GetDlgItem(hwnd_, IDC_RADIO_INFINITE),
                     BM_GETCHECK, 0, 0) == BST_CHECKED)
        return -1;

    wchar_t buf[32] = {};
    GetWindowTextW(GetDlgItem(hwnd_, IDC_REPEAT_COUNT), buf, 32);
    int val = _wtoi(buf);
    return (val < 1) ? 1 : val;
}
