#include "KeyOverlay.h"
#include <algorithm>
#include <cmath>
#include <cassert>
#include <cwchar>

// ---------------------------------------------------------------------------
// HWND -> KeyOverlayWindow map (needed for WndProc)
// ---------------------------------------------------------------------------
static std::unordered_map<HWND, zenith::KeyOverlayWindow*> s_hwndMap;

namespace zenith {

KeyOverlayWindow::KeyOverlayWindow() {}

KeyOverlayWindow::~KeyOverlayWindow() {
    destroy();
}

// ---------------------------------------------------------------------------
bool KeyOverlayWindow::create(HINSTANCE hInst, const KeyOverlayConfig& cfg) {
    m_cfg = cfg;

    // Initialize key states
    for (int vk : cfg.keys) m_keyState[vk] = false;

    // Register window class
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = wndProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;  // we paint everything ourselves
        wc.lpszClassName = L"ZenithKeyOverlay";
        RegisterClassExW(&wc);
        registered = true;
    }

    // Calculate window size based on keys
    int nKeys     = (int)cfg.keys.size();
    int winWidth  = (int)(nKeys * cfg.keySize + (nKeys - 1) * cfg.keySpacing + 16);
    int winHeight = (int)(cfg.keySize + 16);

    DWORD exStyle = WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW;
    DWORD style   = WS_POPUP;

    m_hwnd = CreateWindowExW(
        exStyle, L"ZenithKeyOverlay", L"",
        style,
        (int)cfg.posX, (int)cfg.posY, winWidth, winHeight,
        nullptr, nullptr, hInst, nullptr);

    if (!m_hwnd) return false;

    s_hwndMap[m_hwnd] = this;

    // Allow dragging by removing WS_EX_TRANSPARENT temporarily via Alt+drag
    // (see wndProc for toggle logic)
    SetLayeredWindowAttributes(m_hwnd, 0,
        (BYTE)(cfg.opacity * 255.0f), LWA_ALPHA);

    initD2D();

    if (cfg.enabled) {
        ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
        render();
    }

    return true;
}

void KeyOverlayWindow::destroy() {
    releaseD2D();
    if (m_hwnd) {
        s_hwndMap.erase(m_hwnd);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

// ---------------------------------------------------------------------------
void KeyOverlayWindow::initD2D() {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2dFactory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&m_dwFactory));

    RECT rc{};
    GetClientRect(m_hwnd, &rc);

    D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);
    D2D1_RENDER_TARGET_PROPERTIES rtp = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    D2D1_HWND_RENDER_TARGET_PROPERTIES hrtp = D2D1::HwndRenderTargetProperties(m_hwnd, size);
    m_d2dFactory->CreateHwndRenderTarget(rtp, hrtp, &m_rt);

    if (m_dwFactory) {
        m_dwFactory->CreateTextFormat(
            L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            m_cfg.keySize * 0.4f, L"en-US", &m_textFmt);
        if (m_textFmt) {
            m_textFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_textFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    }
}

void KeyOverlayWindow::releaseD2D() {
    if (m_textFmt) { m_textFmt->Release(); m_textFmt = nullptr; }
    if (m_rt)      { m_rt->Release();      m_rt      = nullptr; }
    if (m_dwFactory) { m_dwFactory->Release(); m_dwFactory = nullptr; }
    if (m_d2dFactory) { m_d2dFactory->Release(); m_d2dFactory = nullptr; }
}

// ---------------------------------------------------------------------------
void KeyOverlayWindow::setVisible(bool visible) {
    if (!m_hwnd) return;
    ShowWindow(m_hwnd, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
    if (visible) render();
}

bool KeyOverlayWindow::isVisible() const {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

// ---------------------------------------------------------------------------
void KeyOverlayWindow::onKeyStateChanged(int vk, bool pressed) {
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_keyState.find(vk) != m_keyState.end()) {
            m_keyState[vk] = pressed;
        }
    }
    if (m_hwnd) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
        render();
    }
}

// ---------------------------------------------------------------------------
void KeyOverlayWindow::applyConfig(const KeyOverlayConfig& cfg) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_cfg = cfg;
    m_keyState.clear();
    for (int vk : cfg.keys) m_keyState[vk] = false;
}

// ---------------------------------------------------------------------------
static D2D1_COLOR_F argbToColor(uint32_t argb) {
    float a = ((argb >> 24) & 0xFF) / 255.0f;
    float r = ((argb >> 16) & 0xFF) / 255.0f;
    float g = ((argb >>  8) & 0xFF) / 255.0f;
    float b = ((argb >>  0) & 0xFF) / 255.0f;
    return D2D1::ColorF(r, g, b, a);
}

void KeyOverlayWindow::render() {
    if (!m_rt) return;

    m_rt->BeginDraw();
    m_rt->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));  // transparent background

    KeyOverlayConfig cfg;
    std::unordered_map<int, bool> states;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        cfg    = m_cfg;
        states = m_keyState;
    }

    float x = 8.0f;
    float y = 8.0f;

    for (int vk : cfg.keys) {
        bool pressed = states.count(vk) ? states.at(vk) : false;

        // Key label: get name from VK
        wchar_t label[8]{};
        if (vk >= 'A' && vk <= 'Z') {
            label[0] = (wchar_t)vk;
        } else {
            UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
            GetKeyNameTextW((LONG)(sc << 16), label, 8);
        }

        drawKey(m_rt, m_textFmt, x, y, cfg.keySize, label, pressed, cfg.opacity);
        x += cfg.keySize + cfg.keySpacing;
    }

    m_rt->EndDraw();
}

void KeyOverlayWindow::drawKey(ID2D1RenderTarget* rt, IDWriteTextFormat* tf,
                                float x, float y, float size,
                                const std::wstring& label, bool pressed, float /*opacity*/) {
    if (!rt) return;

    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
        D2D1::RectF(x, y, x + size, y + size), 8.0f, 8.0f);

    // Fill
    D2D1_COLOR_F fillColor = argbToColor(pressed ? m_cfg.colorActive : m_cfg.colorIdle);
    // Animate: when pressed, add a slight glow / brighten
    if (pressed) {
        fillColor.r = std::min(1.0f, fillColor.r + 0.2f);
        fillColor.g = std::min(1.0f, fillColor.g + 0.2f);
        fillColor.b = std::min(1.0f, fillColor.b + 0.2f);
    }

    ID2D1SolidColorBrush* fillBrush = nullptr;
    rt->CreateSolidColorBrush(fillColor, &fillBrush);
    if (fillBrush) {
        rt->FillRoundedRectangle(rr, fillBrush);
        fillBrush->Release();
    }

    // Border
    ID2D1SolidColorBrush* borderBrush = nullptr;
    D2D1_COLOR_F borderColor = pressed
        ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.9f)
        : D2D1::ColorF(0.6f, 0.6f, 0.6f, 0.5f);
    rt->CreateSolidColorBrush(borderColor, &borderBrush);
    if (borderBrush) {
        rt->DrawRoundedRectangle(rr, borderBrush, pressed ? 2.0f : 1.0f);
        borderBrush->Release();
    }

    // Label
    if (tf && m_cfg.showLabels && !label.empty()) {
        ID2D1SolidColorBrush* textBrush = nullptr;
        rt->CreateSolidColorBrush(argbToColor(m_cfg.colorText), &textBrush);
        if (textBrush) {
            D2D1_RECT_F textRect = D2D1::RectF(x, y, x + size, y + size);
            rt->DrawTextW(label.c_str(), (UINT32)label.size(), tf, textRect, textBrush);
            textBrush->Release();
        }
    }
}

// ---------------------------------------------------------------------------
// WndProc
// ---------------------------------------------------------------------------

KeyOverlayWindow* KeyOverlayWindow::s_fromHwnd(HWND hwnd) {
    auto it = s_hwndMap.find(hwnd);
    return it != s_hwndMap.end() ? it->second : nullptr;
}

LRESULT CALLBACK KeyOverlayWindow::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = s_fromHwnd(hwnd);

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(hwnd, &ps);
            if (self) self->render();
            EndPaint(hwnd, &ps);
            return 0;
        }

        // Alt+drag to reposition (temporarily make window clickable)
        case WM_NCHITTEST: {
            if (GetAsyncKeyState(VK_MENU) & 0x8000) {
                // Remove transparent flag temporarily
                LONG ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
                SetWindowLongW(hwnd, GWL_EXSTYLE, ex & ~WS_EX_TRANSPARENT);
                return HTCAPTION;
            }
            // Restore transparent so mouse passes through
            LONG ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
            SetWindowLongW(hwnd, GWL_EXSTYLE, ex | WS_EX_TRANSPARENT);
            return HTTRANSPARENT;
        }

        case WM_MOVE: {
            if (self && self->onPositionChanged) {
                RECT rc{};
                GetWindowRect(hwnd, &rc);
                self->onPositionChanged((float)rc.left, (float)rc.top);
            }
            return 0;
        }

        case WM_SIZE: {
            if (self && self->m_rt) {
                RECT rc{};
                GetClientRect(hwnd, &rc);
                D2D1_SIZE_U sz = D2D1::SizeU(rc.right, rc.bottom);
                self->m_rt->Resize(sz);
            }
            return 0;
        }

        case WM_DESTROY:
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace zenith
