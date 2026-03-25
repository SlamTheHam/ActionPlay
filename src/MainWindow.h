#pragma once
#include <windows.h>
#include <commctrl.h>
#include "Action.h"
#include "Recorder.h"
#include "Player.h"

class MainWindow {
public:
    MainWindow();
    ~MainWindow();
    bool Create(HINSTANCE hInstance);
    void Show(int nCmdShow);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    void OnCreate(HWND hwnd);
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnHotKey(int id);
    void OnRecorderUpdate(int count);
    void OnPlayerProgress(int actionIdx, int repeat, int totalRepeats);
    void OnPlayerComplete();
    void StartRecording();
    void StopRecording();
    void PlayActions();
    void PausePlayback();
    void StopPlayback();
    void UpdateStatus(const wchar_t* text, COLORREF color);
    void AppendLog(const wchar_t* text);
    void SetButtonStates(bool recording, bool playing, bool paused);
    double GetSpeed();
    int GetRepeatCount();  // -1 if infinite

    HWND      hwnd_;
    HINSTANCE hInstance_;
    Recorder  recorder_;
    Player    player_;
    HFONT     hFont_;
    HFONT     hFontBold_;
    COLORREF  statusColor_;

    static MainWindow* instance_;
};
