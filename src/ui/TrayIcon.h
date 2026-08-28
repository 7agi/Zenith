#pragma once
#include <windows.h>
#include <string>
#include <functional>

namespace zenith {

// ---------------------------------------------------------------------------
// TrayIcon
//
// Simple Win32 Notification Area (System Tray) icon.
// Provides a context menu (Settings, Exit) and double-click to open settings.
// ---------------------------------------------------------------------------

class TrayIcon {
public:
    TrayIcon();
    ~TrayIcon();

    bool init(HWND hwndMessage, UINT wmCallbackMessage, HICON hIcon);
    void shutdown();

    void showNotification(const std::string& title, const std::string& msg);

    // Called when the tray icon receives a callback message in the parent WndProc
    void onMessage(WPARAM wp, LPARAM lp);

    // Callbacks
    std::function<void()> onSettingsClicked;
    std::function<void()> onExitClicked;

private:
    void showContextMenu();

    HWND  m_hwnd = nullptr;
    UINT  m_msg  = 0;
    bool  m_added = false;
};

} // namespace zenith
