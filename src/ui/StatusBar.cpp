#include "StatusBar.h"

namespace zenith {

StatusBar::StatusBar() {}

StatusBar::~StatusBar() {
    destroy();
}

bool StatusBar::create(HINSTANCE hInst) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = wndProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"ZenithStatusBar";
        RegisterClassExW(&wc);
        registered = true;
    }

    // A tiny 16x16 window, placed in top-left
    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        L"ZenithStatusBar", L"",
        WS_POPUP,
        10, 10, 16, 16,
        nullptr, nullptr, hInst, this);

    if (!m_hwnd) return false;

    SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);
    
    // Set GWLP_USERDATA for wndProc
    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    initD2D();
    return true;
}

void StatusBar::destroy() {
    releaseD2D();
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void StatusBar::setVisible(bool visible) {
    if (m_hwnd) {
        ShowWindow(m_hwnd, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
        if (visible) render();
    }
}

bool StatusBar::isVisible() const {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

void StatusBar::setColor(uint32_t argb) {
    m_color = argb;
    if (isVisible()) render();
}

void StatusBar::initD2D() {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2dFactory);
    if (m_d2dFactory && m_hwnd) {
        D2D1_SIZE_U size = D2D1::SizeU(16, 16);
        D2D1_RENDER_TARGET_PROPERTIES rtp = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        D2D1_HWND_RENDER_TARGET_PROPERTIES hrtp = D2D1::HwndRenderTargetProperties(m_hwnd, size);
        m_d2dFactory->CreateHwndRenderTarget(rtp, hrtp, &m_rt);
    }
}

void StatusBar::releaseD2D() {
    if (m_rt) { m_rt->Release(); m_rt = nullptr; }
    if (m_d2dFactory) { m_d2dFactory->Release(); m_d2dFactory = nullptr; }
}

void StatusBar::render() {
    if (!m_rt) return;

    m_rt->BeginDraw();
    m_rt->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    float a = ((m_color >> 24) & 0xFF) / 255.0f;
    float r = ((m_color >> 16) & 0xFF) / 255.0f;
    float g = ((m_color >>  8) & 0xFF) / 255.0f;
    float b = ((m_color >>  0) & 0xFF) / 255.0f;

    ID2D1SolidColorBrush* brush = nullptr;
    m_rt->CreateSolidColorBrush(D2D1::ColorF(r, g, b, a), &brush);
    
    if (brush) {
        D2D1_ELLIPSE ell = D2D1::Ellipse(D2D1::Point2F(8.0f, 8.0f), 6.0f, 6.0f);
        m_rt->FillEllipse(ell, brush);
        brush->Release();
    }

    m_rt->EndDraw();
}

LRESULT CALLBACK StatusBar::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    StatusBar* self = reinterpret_cast<StatusBar*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_PAINT && self) {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd, &ps);
        self->render();
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace zenith
