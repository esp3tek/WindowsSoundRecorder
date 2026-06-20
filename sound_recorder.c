/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 esp3tek
 */
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include "audio_capture.h"
#include "audio_util.h"

#define IDC_PATH    101
#define IDC_BROWSE  102
#define IDC_RECORD  103
#define IDC_STATUS  104
#define IDC_FMT_WAV   105
#define IDC_FMT_MP3   106
#define IDC_AUTOSTART 107
#define ID_TIMER    1

#define WM_TRAYICON (WM_APP + 1)
#define IDM_SHOW    2001
#define IDM_EXIT    2002

static const wchar_t CLASS_NAME[]   = L"WindowsSoundRecorderWnd";
static const wchar_t WINDOW_TITLE[] = L"Windows Sound Recorder";
static const wchar_t MUTEX_NAME[]   = L"WindowsSoundRecorder_SingleInstance_{9F2C}";
static NOTIFYICONDATA g_nid = {0};
static BOOL g_tray_added = FALSE;

static HWND  g_path_edit  = NULL;
static HWND  g_record_btn = NULL;
static HWND  g_status     = NULL;
static BOOL  g_recording  = FALSE;
static DWORD g_elapsed    = 0;   /* segundos */
static CaptureSession *g_session = NULL;
static HWND g_wav_radio = NULL, g_mp3_radio = NULL, g_autostart = NULL;
static BOOL g_autostart_on = FALSE;

/* Rellena `dst` con la ruta del Escritorio del usuario. */
static void get_desktop_path(wchar_t *dst, size_t len) {
    wchar_t tmp[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, tmp))) {
        wcsncpy(dst, tmp, len - 1);
        dst[len - 1] = L'\0';
    } else {
        dst[0] = L'\0';
    }
}

/* Abre un diálogo para elegir carpeta; devuelve TRUE y escribe en `dst`. */
static BOOL browse_folder(HWND owner, wchar_t *dst, size_t len) {
    BROWSEINFO bi = {0};
    bi.hwndOwner = owner;
    bi.lpszTitle = L"Choose the folder to save recordings";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (!pidl) return FALSE;
    wchar_t tmp[MAX_PATH] = {0};
    BOOL ok = SHGetPathFromIDList(pidl, tmp);
    CoTaskMemFree(pidl);
    if (ok) {
        wcsncpy(dst, tmp, len - 1);
        dst[len - 1] = L'\0';
    }
    return ok;
}

static void update_timer_label(HWND hwnd) {
    wchar_t buf[64];
    swprintf(buf, 64, L"%02lu:%02lu", g_elapsed / 60, g_elapsed % 60);
    SetWindowText(g_status, buf);
    (void)hwnd;
}

static AudioFormat current_format(void) {
    return (SendMessage(g_mp3_radio, BM_GETCHECK, 0, 0) == BST_CHECKED) ? FORMAT_MP3 : FORMAT_WAV;
}

static void set_idle_controls(HWND hwnd, BOOL idle) {
    EnableWindow(g_path_edit, idle);
    EnableWindow(g_wav_radio, idle);
    EnableWindow(g_mp3_radio, idle);
    HWND browse = GetDlgItem(hwnd, IDC_BROWSE);
    if (browse) EnableWindow(browse, idle);
}

static void start_recording(HWND hwnd) {
    wchar_t folder[MAX_PATH];
    GetWindowText(g_path_edit, folder, MAX_PATH);
    if (folder[0] == L'\0') {
        MessageBox(hwnd, L"Please choose a destination folder.", L"Windows Sound Recorder",
                   MB_OK | MB_ICONWARNING);
        return;
    }
    wchar_t err[256] = {0};
    g_session = capture_start(NULL, CAPTURE_MANUAL, current_format(), folder, err, 256);
    if (!g_session) {
        MessageBox(hwnd, err[0] ? err : L"Could not start recording.",
                   L"Windows Sound Recorder", MB_OK | MB_ICONERROR);
        return;
    }
    g_recording = TRUE;
    g_elapsed = 0;
    update_timer_label(hwnd);
    SetWindowText(g_record_btn, L"Stop");
    set_idle_controls(hwnd, FALSE);
    EnableWindow(g_autostart, FALSE);
    SetTimer(hwnd, ID_TIMER, 1000, NULL);
}

static void stop_recording(HWND hwnd) {
    if (g_session) { capture_stop(g_session); g_session = NULL; }
    g_recording = FALSE;
    KillTimer(hwnd, ID_TIMER);
    SetWindowText(g_record_btn, L"Record");
    set_idle_controls(hwnd, TRUE);
    EnableWindow(g_autostart, TRUE);
    SetWindowText(g_status, L"Saved");
}

static void arm_autostart(HWND hwnd) {
    wchar_t folder[MAX_PATH];
    GetWindowText(g_path_edit, folder, MAX_PATH);
    if (folder[0] == L'\0') {
        MessageBox(hwnd, L"Please choose a destination folder.", L"Windows Sound Recorder",
                   MB_OK | MB_ICONWARNING);
        SendMessage(g_autostart, BM_SETCHECK, BST_UNCHECKED, 0);
        return;
    }
    wchar_t err[256] = {0};
    g_session = capture_start(hwnd, CAPTURE_AUTOSTART, current_format(), folder, err, 256);
    if (!g_session) {
        MessageBox(hwnd, err[0] ? err : L"Could not start autostart.",
                   L"Windows Sound Recorder", MB_OK | MB_ICONERROR);
        SendMessage(g_autostart, BM_SETCHECK, BST_UNCHECKED, 0);
        return;
    }
    g_autostart_on = TRUE;
    set_idle_controls(hwnd, FALSE);
    EnableWindow(g_record_btn, FALSE);
    SetWindowText(g_record_btn, L"Waiting for sound...");
    SetWindowText(g_status, L"Waiting for sound...");
}

static void disarm_autostart(HWND hwnd) {
    if (g_session) { capture_stop(g_session); g_session = NULL; }
    g_autostart_on = FALSE;
    g_recording = FALSE;
    KillTimer(hwnd, ID_TIMER);
    set_idle_controls(hwnd, TRUE);
    EnableWindow(g_record_btn, TRUE);
    SetWindowText(g_record_btn, L"Record");
    SetWindowText(g_status, L"Ready");
}

static void create_controls(HWND hwnd, HINSTANCE hInst) {
    CreateWindow(L"STATIC", L"Save folder:",
        WS_CHILD | WS_VISIBLE, 12, 12, 200, 18, hwnd, NULL, hInst, NULL);

    g_path_edit = CreateWindow(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        12, 32, 290, 24, hwnd, (HMENU)IDC_PATH, hInst, NULL);

    CreateWindow(L"BUTTON", L"Browse...",
        WS_CHILD | WS_VISIBLE, 310, 32, 90, 24, hwnd, (HMENU)IDC_BROWSE, hInst, NULL);

    CreateWindow(L"STATIC", L"Format:",
        WS_CHILD | WS_VISIBLE, 12, 70, 60, 18, hwnd, NULL, hInst, NULL);

    g_wav_radio = CreateWindow(L"BUTTON", L"WAV",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        78, 68, 70, 22, hwnd, (HMENU)IDC_FMT_WAV, hInst, NULL);

    g_mp3_radio = CreateWindow(L"BUTTON", L"MP3 320 kbps",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        158, 68, 130, 22, hwnd, (HMENU)IDC_FMT_MP3, hInst, NULL);

    SendMessage(g_wav_radio, BM_SETCHECK, BST_CHECKED, 0);

    g_autostart = CreateWindow(L"BUTTON", L"Autostart (record on sound; stop after 5 s silence)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        12, 98, 410, 22, hwnd, (HMENU)IDC_AUTOSTART, hInst, NULL);

    g_record_btn = CreateWindow(L"BUTTON", L"Record",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        12, 128, 410, 40, hwnd, (HMENU)IDC_RECORD, hInst, NULL);

    g_status = CreateWindow(L"STATIC", L"Ready",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        12, 178, 410, 22, hwnd, (HMENU)IDC_STATUS, hInst, NULL);

    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(g_path_edit,  WM_SETFONT, (WPARAM)font, TRUE);
    SendMessage(g_wav_radio,  WM_SETFONT, (WPARAM)font, TRUE);
    SendMessage(g_mp3_radio,  WM_SETFONT, (WPARAM)font, TRUE);
    SendMessage(g_autostart,  WM_SETFONT, (WPARAM)font, TRUE);
    SendMessage(g_record_btn, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessage(g_status,     WM_SETFONT, (WPARAM)font, TRUE);

    wchar_t desktop[MAX_PATH];
    get_desktop_path(desktop, MAX_PATH);
    SetWindowText(g_path_edit, desktop);
}

static void tray_add(HWND hwnd, HINSTANCE hInst) {
    if (g_tray_added) return;
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd   = hwnd;
    g_nid.uID    = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon  = LoadIcon(hInst, MAKEINTRESOURCE(1));
    wcscpy(g_nid.szTip, L"Windows Sound Recorder");
    Shell_NotifyIcon(NIM_ADD, &g_nid);
    g_tray_added = TRUE;
}

static void tray_remove(void) {
    if (!g_tray_added) return;
    Shell_NotifyIcon(NIM_DELETE, &g_nid);
    g_tray_added = FALSE;
}

static void tray_show_menu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU menu = CreatePopupMenu();
    AppendMenu(menu, MF_STRING, IDM_SHOW, L"Show");
    AppendMenu(menu, MF_SEPARATOR, 0, NULL);
    AppendMenu(menu, MF_STRING, IDM_EXIT, L"Exit");
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(menu);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        create_controls(hwnd, ((LPCREATESTRUCT)lp)->hInstance);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BROWSE: {
            wchar_t folder[MAX_PATH];
            if (browse_folder(hwnd, folder, MAX_PATH))
                SetWindowText(g_path_edit, folder);
            return 0;
        }
        case IDC_RECORD:
            if (!g_recording) start_recording(hwnd);
            else              stop_recording(hwnd);
            return 0;
        case IDC_AUTOSTART:
            if (SendMessage(g_autostart, BM_GETCHECK, 0, 0) == BST_CHECKED)
                arm_autostart(hwnd);
            else
                disarm_autostart(hwnd);
            return 0;
        case IDM_SHOW:
            ShowWindow(hwnd, SW_SHOW);
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
            tray_remove();
            return 0;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            return 0;
        }
        return 0;

    case WM_SIZE:
        if (wp == SIZE_MINIMIZED) {
            tray_add(hwnd, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE));
            ShowWindow(hwnd, SW_HIDE);
        }
        return 0;

    case WM_TRAYICON:
        if (LOWORD(lp) == WM_LBUTTONUP || LOWORD(lp) == WM_LBUTTONDBLCLK) {
            ShowWindow(hwnd, SW_SHOW);
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
            tray_remove();
        } else if (LOWORD(lp) == WM_RBUTTONUP) {
            tray_show_menu(hwnd);
        }
        return 0;

    case WM_TIMER:
        if (wp == ID_TIMER) {
            g_elapsed++;
            update_timer_label(hwnd);
        }
        return 0;

    case WM_APP_STATE:
        if (wp == STATE_RECORDING) {
            g_elapsed = 0;
            update_timer_label(hwnd);
            SetWindowText(g_record_btn, L"Recording");
            SetWindowText(g_status, L"Recording");
            SetTimer(hwnd, ID_TIMER, 1000, NULL);
        } else { /* STATE_WAITING */
            KillTimer(hwnd, ID_TIMER);
            SetWindowText(g_record_btn, L"Waiting for sound...");
            SetWindowText(g_status, L"Waiting for sound...");
        }
        return 0;

    case WM_DESTROY:
        if (g_session) { capture_stop(g_session); g_session = NULL; }
        tray_remove();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int show) {
    (void)hPrev; (void)cmd;

    HANDLE mutex = CreateMutex(NULL, TRUE, MUTEX_NAME);
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindow(CLASS_NAME, NULL);
        if (existing) {
            ShowWindow(existing, SW_SHOW);
            ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        return 0;
    }

    WNDCLASS wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon         = LoadIcon(hInst, MAKEINTRESOURCE(1));
    RegisterClass(&wc);

    HWND hwnd = CreateWindow(CLASS_NAME, WINDOW_TITLE,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 448, 258,
        NULL, NULL, hInst, NULL);
    if (!hwnd) return 1;

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessage(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    return 0;
}
