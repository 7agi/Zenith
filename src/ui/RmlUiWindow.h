#pragma once
#include "../config/Config.h"
#include <windows.h>
#include <functional>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

// Forward-declare RmlUi types to keep this header clean
namespace Rml {
    class Context;
    class ElementDocument;
}

namespace zenith {

// ---------------------------------------------------------------------------
// RmlUiWindow
//
// Uses the RmlUi Win32+DX11 backend to render the settings UI (HTML/CSS).
// The window runs on a dedicated thread so it never blocks the main loop.
//
// Usage:
//   1. Call RmlUiWindow::initRml() once at startup.
//   2. Call create() to prepare the window (stores config + hInst).
//   3. Call show() to open the settings window (spawns a thread).
//   4. Call hide() or let the user close the window.
//   5. Call shutdownRml() at program exit.
// ---------------------------------------------------------------------------
class RmlUiWindow {
public:
    RmlUiWindow();
    ~RmlUiWindow();

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

    // show() opens the window on a dedicated thread.
    void show();
    // hide() signals the window thread to exit.
    void hide();
    bool isVisible() const;

    // ------------------------------------------------------------------
    // Update displayed config (can be called before show()).
    // ------------------------------------------------------------------
    void loadConfig(const Config& cfg);

    // ------------------------------------------------------------------
    // Update the status badge in the footer (thread-safe).
    // ------------------------------------------------------------------
    void setStatus(const std::string& statusClass, const std::string& statusText);

    // ------------------------------------------------------------------
    // Fired when user clicks "Save & Apply" (called on window thread).
    // ------------------------------------------------------------------
    std::function<void(const Config&)> onApplyConfig;

private:
    // The actual window + event + render loop runs here.
    void threadFunc();

    // DOM helpers (only called from window thread)
    void populateDom();
    void installEventListeners();
    Config readDataModel() const;
    void startBinding(const std::string& elementId);
    void commitBinding();

    // Apply any pending status update (called on window thread).
    void applyPendingStatus();

    HINSTANCE              m_hInst   = nullptr;
    int                    m_width   = 760;
    int                    m_height  = 600;
    Config                 m_cfg;

    Rml::Context*          m_rmlCtx  = nullptr;
    Rml::ElementDocument*  m_doc     = nullptr;

    std::thread            m_thread;
    std::atomic<bool>      m_running { false };

    // Pending status update (written by main thread, read by window thread)
    struct PendingStatus {
        std::string cls;
        std::string text;
        bool dirty = false;
    };
    std::mutex     m_statusMutex;
    PendingStatus  m_pendingStatus;

    // Hotkey binding state (window thread only)
    std::string            m_bindingElementId;
    std::vector<int>       m_pendingKeys;
    bool                   m_isBinding = false;

    static std::string     s_assetPath;
};

} // namespace zenith
