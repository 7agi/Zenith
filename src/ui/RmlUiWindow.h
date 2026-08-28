#pragma once
#include "../config/Config.h"
#include <windows.h>
#include <d3d11.h>
#include <functional>
#include <string>

// Forward-declare RmlUi types to keep this header clean
namespace Rml {
    class Context;
    class ElementDocument;
}

namespace zenith {

// ---------------------------------------------------------------------------
// RmlUiWindow
//
// Owns a Win32 window + DirectX 11 swap chain used to render the RmlUi
// settings document (HTML/CSS). Uses the RmlUi Win32/DX11 backend.
//
// Usage:
//   1. Call RmlUiWindow::initRml() once at startup (loads fonts, etc.)
//   2. Construct an RmlUiWindow, call create().
//   3. Call show() to display / hide() to hide.
//   4. Inject Win32 messages via processMessage() from your message loop.
//   5. Call shutdownRml() at program exit.
// ---------------------------------------------------------------------------
class RmlUiWindow {
public:
    RmlUiWindow();
    ~RmlUiWindow();

    // Not copyable
    RmlUiWindow(const RmlUiWindow&) = delete;
    RmlUiWindow& operator=(const RmlUiWindow&) = delete;

    // ------------------------------------------------------------------
    // Static global init/shutdown – call ONCE per process
    // ------------------------------------------------------------------
    static bool initRml(HINSTANCE hInst, const std::string& assetBasePath);
    static void shutdownRml();

    // ------------------------------------------------------------------
    // Instance lifecycle
    // ------------------------------------------------------------------
    bool create(HINSTANCE hInst, const Config& cfg);
    void destroy();

    void show();
    void hide();
    bool isVisible() const;

    // ------------------------------------------------------------------
    // Feed the current config in (populates UI fields)
    // ------------------------------------------------------------------
    void loadConfig(const Config& cfg);

    // ------------------------------------------------------------------
    // Called from the Win32 message loop. Returns true if handled.
    // ------------------------------------------------------------------
    bool processMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    // ------------------------------------------------------------------
    // Callbacks
    // ------------------------------------------------------------------
    std::function<void(const Config&)> onApplyConfig;

    // Set from the Recorder state so the footer badge updates
    void setStatus(const std::string& statusClass, const std::string& statusText);

private:
    // Win32 / DX11
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    bool createDeviceAndSwapChain();
    void releaseDevice();
    void render();
    void resize(int w, int h);

    HWND                    m_hwnd           = nullptr;
    HINSTANCE               m_hInst          = nullptr;
    ID3D11Device*           m_device         = nullptr;
    ID3D11DeviceContext*    m_context        = nullptr;
    IDXGISwapChain*         m_swapChain      = nullptr;
    ID3D11RenderTargetView* m_rtv            = nullptr;
    int                     m_width          = 760;
    int                     m_height         = 600;

    // RmlUi
    Rml::Context*           m_rmlCtx         = nullptr;
    Rml::ElementDocument*   m_doc            = nullptr;

    // Hotkey binding state
    std::string             m_bindingElementId;
    std::vector<int>        m_pendingKeys;
    bool                    m_isBinding      = false;

    // Helpers
    void bindDataModel(const Config& cfg);
    Config readDataModel() const;
    void updateHotkeyDisplay(const std::string& elementId,
                              const std::vector<int>& vkeys);
    void startBinding(const std::string& elementId);
    void commitBinding();
    void installEventListeners();

    static RmlUiWindow*     s_instance;
    static std::string      s_assetPath;
};

} // namespace zenith
