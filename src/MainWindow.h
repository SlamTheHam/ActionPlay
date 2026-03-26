#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <string>
#include "Action.h"
#include "Recorder.h"
#include "Player.h"
#include "RecordingStore.h"

class MainWindow {
public:
    MainWindow();
    ~MainWindow();
    bool Create(HINSTANCE hInstance);
    void Show(int nCmdShow);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    // Message handlers
    void OnCreate(HWND hwnd);
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnHotKey(int id);
    void OnRecorderUpdate(int count);
    void OnPlayerProgress(int actionIdx, int repeat, int totalRepeats);
    void OnPlayerComplete();
    void OnDestroy();

    // Recording
    void StartRecording();
    void StopRecording();           // stops, then triggers save prompt
    bool PromptSaveRecording();     // shows "Save recording?" flow; returns true if saved
    bool PromptRecordingName(const std::wstring& defaultName, std::wstring& outName);

    // Playback
    void PlayActions();
    void PausePlayback();
    void StopPlayback();

    // Saved-recordings panel
    void RefreshRecordingsList();
    void LoadSelectedRecording();
    void DeleteSelectedRecording();

    // Tray notifications
    void SetupTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayNotification(const wchar_t* title, const wchar_t* msg);

    // UI helpers
    void UpdateStatus(const wchar_t* text, COLORREF color);
    void AppendLog(const wchar_t* text);
    void SetButtonStates(bool recording, bool playing, bool paused);
    double GetSpeed();
    int    GetRepeatCount();        // -1 = infinite

    // -----------------------------------------------------------------------
    HWND        hwnd_;
    HINSTANCE   hInstance_;
    Recorder    recorder_;
    Player      player_;
    HFONT       hFont_;
    HFONT       hFontBold_;
    COLORREF    statusColor_;

    // Currently active action list (from recorder or loaded from file)
    ActionList  currentActions_;

    // Tray icon state
    NOTIFYICONDATA  trayNid_;
    bool            trayAdded_;

    static MainWindow* instance_;
};
