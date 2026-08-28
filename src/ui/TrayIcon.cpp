#include "TrayIcon.h"
#include <shellapi.h>

#define ID_TRAY_SETTINGS 1001
#define ID_TRAY_EXIT     1002

namespace zenith {

TrayIcon::TrayIcon() {}

TrayIcon::~TrayIcon() {
    shutdown();
}

bool TrayIcon::init(HWND hwndMessage, UINT wmCallbackMessage, HICON hIcon) {
    m_hwnd = hwndMessage;
    m_msg  = wmCallbackMessage;

    NOTIFYICONDATAW nid = {};
    nid.cbSize           = sizeof(nid);
    nid.hWnd             = m_hwnd;
    nid.uID              = 1;
    nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = m_msg;
    nid.hIcon            = hIcon;
    wcscpy_s(nid.szTip, L"Zenith Recorder");

    m_added = Shell_NotifyIconW(NIM_ADD, &nid);
    return m_added;
}

void TrayIcon::shutdown() {
    if (m_added) {
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd   = m_hwnd;
        nid.uID    = 1;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        m_added = false;
    }
}

void TrayIcon::showNotification(const std::string& title, const std::string& msg) {
    if (!m_added) return;

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = m_hwnd;
    nid.uID    = 1;
    nid.uFlags = NIF_INFO;
    
    MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, nid.szInfoTitle, 64);
    MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, nid.szInfo, 256);
    nid.dwInfoFlags = NIIF_INFO;

    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayIcon::onMessage(WPARAM wp, LPARAM lp) {
    if (LOWORD(lp) == WM_RBUTTONUP) {
        showContextMenu();
    } else if (LOWORD(lp) == WM_LBUTTONDBLCLK) {
        if (onSettingsClicked) onSettingsClicked();
    }
}

void TrayIcon::showContextMenu() {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_SETTINGS, L"Settings");
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Exit");

    // Required for the menu to disappear when clicking outside
    SetForegroundWindow(m_hwnd);

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_BOTTOMALIGN | TPM_RIGHTALIGN,
                             pt.x, pt.y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd == ID_TRAY_SETTINGS && onSettingsClicked) onSettingsClicked();
    if (cmd == ID_TRAY_EXIT && onExitClicked) onExitClicked();
}

} // namespace zenith
