#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

// Modal dialog that prompts the user to enter a recording name.
// Returns true if OK was clicked, false if cancelled.
class NameDialog {
public:
    static bool Show(HWND hParent, const wchar_t* defaultName, std::wstring& outName);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static NameDialog* s_current_;

    HWND         hParent_;
    HWND         hDlg_;
    std::wstring result_;
    bool         confirmed_;
    HFONT        hFont_;
};
