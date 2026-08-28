#pragma once
#include "../config/Config.h"
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <vector>
#include <unordered_map>
#include <functional>
#include <string>
#include <mutex>
#include <atomic>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace zenith {

// ---------------------------------------------------------------------------
// KeyOverlayWindow
//
// A transparent, click-through, always-on-top Win32 window that renders
// animated key buttons using Direct2D.
//
// The window is NOT a preview of the recording — it IS the overlay that
// appears on-screen (and therefore in the recording via screen capture).
//
// Interaction:
//   - Notified of key state changes via `onKeyStateChanged(vk, pressed)`.
//   - Moveable by dragging (while holding Alt, to avoid interfering with
//     game inputs).
//   - Position/size saved to config.
// ---------------------------------------------------------------------------

class KeyOverlayWindow {
public:
    KeyOverlayWindow();
    ~KeyOverlayWindow();

    // Create the window. Call from the UI thread.
    bool create(HINSTANCE hInst, const KeyOverlayConfig& cfg);
    void destroy();

    // Call from HotkeyManager callback when a tracked key changes state.
    void onKeyStateChanged(int vk, bool pressed);

    // Show / hide the overlay.
    void setVisible(bool visible);
    bool isVisible() const;

    // Apply new config (keys list, colors, position, size).
    void applyConfig(const KeyOverlayConfig& cfg);

    // Called by WndProc — returns true if handled.
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    // Callback: called when user drags the overlay to a new position.
    std::function<void(float x, float y)> onPositionChanged;

private:
    void render();
    void initD2D();
    void releaseD2D();
    void drawKey(ID2D1RenderTarget* rt, IDWriteTextFormat* tf,
                 float x, float y, float size,
                 const std::wstring& label, bool pressed, float opacity);

    HWND                    m_hwnd      = nullptr;
    ID2D1Factory*           m_d2dFactory = nullptr;
    ID2D1HwndRenderTarget*  m_rt        = nullptr;
    IDWriteFactory*         m_dwFactory = nullptr;
    IDWriteTextFormat*      m_textFmt   = nullptr;

    KeyOverlayConfig        m_cfg;
    std::unordered_map<int, bool> m_keyState;  // vk -> pressed
    mutable std::mutex      m_mutex;

    // Drag support
    bool  m_dragging  = false;
    POINT m_dragStart = {};
    POINT m_winStart  = {};

    static KeyOverlayWindow* s_fromHwnd(HWND hwnd);
};

} // namespace zenith
