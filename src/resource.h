#pragma once

// ---------------------------------------------------------------------------
// Control IDs
// ---------------------------------------------------------------------------
#define IDC_STATUS              1001
#define IDC_ACTION_COUNT        1002
#define IDC_BTN_START_REC       1003
#define IDC_BTN_STOP_REC        1004
#define IDC_SPEED_SLIDER        1005
#define IDC_SPEED_LABEL         1006
#define IDC_RADIO_FINITE        1007
#define IDC_RADIO_INFINITE      1008
#define IDC_REPEAT_COUNT        1009
#define IDC_BTN_PLAY            1010
#define IDC_BTN_PAUSE           1011
#define IDC_BTN_STOP_PLAY       1012
#define IDC_PROGRESSBAR         1013
#define IDC_ACTION_LABEL        1014
#define IDC_REPEAT_LABEL        1015
#define IDC_LOG                 1016

// Saved-recordings panel
#define IDC_LIST_RECORDINGS     1017
#define IDC_BTN_LOAD_REC        1018
#define IDC_BTN_DELETE_REC      1019

// ---------------------------------------------------------------------------
// Hotkey IDs (used with RegisterHotKey)
// ---------------------------------------------------------------------------
#define HOTKEY_START_REC        1
#define HOTKEY_STOP_REC         2
#define HOTKEY_PLAY             3
#define HOTKEY_PAUSE            4
#define HOTKEY_STOP_PLAY        5

// ---------------------------------------------------------------------------
// Custom window messages
// ---------------------------------------------------------------------------
#define WM_RECORDER_UPDATE  (WM_APP + 1)   // wParam = current action count
#define WM_PLAYER_PROGRESS  (WM_APP + 2)   // wParam = actionIdx, lParam = MAKELPARAM(repeat, totalRepeats) [0xFFFF=infinite]
#define WM_PLAYER_COMPLETE  (WM_APP + 3)   // playback finished or stopped

// ---------------------------------------------------------------------------
// Tray icon ID
// ---------------------------------------------------------------------------
#define TRAY_ICON_ID            1
