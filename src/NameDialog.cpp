#include "NameDialog.h"
#include <windows.h>
#include <string>

#define IDC_NAME_EDIT   101

NameDialog* NameDialog::s_current_ = nullptr;

bool NameDialog::Show(HWND hParent, const wchar_t* defaultName, std::wstring& outName)
{
    NameDialog dlg;
    dlg.hParent_    = hParent;
    dlg.hDlg_       = nullptr;
    dlg.confirmed_  = false;
    dlg.hFont_      = nullptr;

    s_current_ = &dlg;

    // Register window class (once per process lifetime is fine; ignore ERROR_CLASS_ALREADY_EXISTS)
    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = NameDialog::WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"NameDialogClass";
    RegisterClassExW(&wc); // ignore return; may already be registered

    // Create the popup window
    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"NameDialogClass",
        L"Name Recording",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0,          // position set below after centering
        340, 130,
        hParent,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );

    if (!hwnd)
    {
        s_current_ = nullptr;
        return false;
    }

    dlg.hDlg_ = hwnd;

    // Pre-fill the edit control with the default name
    if (defaultName)
        SetWindowTextW(GetDlgItem(hwnd, IDC_NAME_EDIT), defaultName);

    // Center over parent
    RECT rcParent = {};
    if (hParent && GetWindowRect(hParent, &rcParent))
    {
        RECT rcDlg;
        GetWindowRect(hwnd, &rcDlg);
        int dlgW = rcDlg.right  - rcDlg.left;
        int dlgH = rcDlg.bottom - rcDlg.top;
        int parentW = rcParent.right  - rcParent.left;
        int parentH = rcParent.bottom - rcParent.top;
        int x = rcParent.left + (parentW - dlgW) / 2;
        int y = rcParent.top  + (parentH - dlgH) / 2;
        SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }

    // Disable parent to simulate modal behaviour
    if (hParent)
        EnableWindow(hParent, FALSE);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Modal message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Re-enable parent and bring it to the foreground
    if (hParent)
    {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }

    s_current_ = nullptr;

    if (dlg.confirmed_)
    {
        outName = dlg.result_;
        return true;
    }
    return false;
}

LRESULT CALLBACK NameDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    NameDialog* self = s_current_;

    switch (msg)
    {
    case WM_CREATE:
    {
        // Create Segoe UI 14px font
        HFONT hFont = CreateFontW(
            -14, 0, 0, 0,
            FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS,
            L"Segoe UI"
        );

        if (self)
            self->hFont_ = hFont;

        auto applyFont = [&](HWND hCtrl) {
            SendMessageW(hCtrl, WM_SETFONT, (WPARAM)hFont, TRUE);
        };

        // Static label
        HWND hLabel = CreateWindowExW(
            0, L"STATIC", L"Recording name:",
            WS_CHILD | WS_VISIBLE,
            12, 14, 140, 18,
            hwnd, nullptr, GetModuleHandleW(nullptr), nullptr
        );
        applyFont(hLabel);

        // Edit control
        HWND hEdit = CreateWindowExW(
            0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            12, 36, 308, 22,
            hwnd, (HMENU)(INT_PTR)IDC_NAME_EDIT, GetModuleHandleW(nullptr), nullptr
        );
        applyFont(hEdit);
        SetFocus(hEdit);

        // OK button
        HWND hOK = CreateWindowExW(
            0, L"BUTTON", L"OK",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            148, 72, 80, 26,
            hwnd, (HMENU)(INT_PTR)IDOK, GetModuleHandleW(nullptr), nullptr
        );
        applyFont(hOK);

        // Cancel button
        HWND hCancel = CreateWindowExW(
            0, L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            236, 72, 80, 26,
            hwnd, (HMENU)(INT_PTR)IDCANCEL, GetModuleHandleW(nullptr), nullptr
        );
        applyFont(hCancel);

        return 0;
    }

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id == IDOK)
        {
            if (self)
            {
                HWND hEdit = GetDlgItem(hwnd, IDC_NAME_EDIT);
                int len = GetWindowTextLengthW(hEdit);
                if (len > 0)
                {
                    self->result_.resize(static_cast<size_t>(len));
                    GetWindowTextW(hEdit, &self->result_[0], len + 1);
                }
                else
                {
                    self->result_.clear();
                }
                self->confirmed_ = true;
            }
            DestroyWindow(hwnd);
            return 0;
        }
        else if (id == IDCANCEL)
        {
            if (self)
                self->confirmed_ = false;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }

    case WM_CLOSE:
    {
        if (self)
            self->confirmed_ = false;
        DestroyWindow(hwnd);
        return 0;
    }

    case WM_DESTROY:
    {
        if (self && self->hFont_)
        {
            DeleteObject(self->hFont_);
            self->hFont_ = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
