#pragma once
#include <windows.h>
#include <d2d1.h>
#include <string>

namespace zenith {

// ---------------------------------------------------------------------------
// StatusBar
//
// A small, borderless, always-on-top window that displays a colored dot
// to indicate recording state (e.g. red for recording, yellow for buffering).
// ---------------------------------------------------------------------------

class StatusBar {
public:
    StatusBar();
    ~StatusBar();

    bool create(HINSTANCE hInst);
    void destroy();

    void setVisible(bool visible);
    bool isVisible() const;

    // Set the color (ARGB)
    void setColor(uint32_t argb);

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void render();
    void initD2D();
    void releaseD2D();

    HWND m_hwnd = nullptr;
    ID2D1Factory* m_d2dFactory = nullptr;
    ID2D1HwndRenderTarget* m_rt = nullptr;

    uint32_t m_color = 0xFFFF0000; // default red
};

} // namespace zenith
