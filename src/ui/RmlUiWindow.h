#pragma once
#include "../config/Config.h"
#include <windows.h>
#include <functional>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace Rml {
    class Context;
    class ElementDocument;
}

namespace zenith {

// ---------------------------------------------------------------------------
// RmlUiWindow
//
// Renders the settings UI (HTML/CSS) via RmlUi + Win32 + DX11.
//
// Design: A persistent background thread initialises the backend once at
// create() time. show()/hide() simply toggle window visibility, making
// opening the settings panel instant after the first call to create().
// ---------------------------------------------------------------------------
class RmlUiWindow {
public:
    RmlUiWindow();
    ~RmlUiWindow();

    RmlUiWindow(const RmlUiWindow&) = delete;
    RmlUiWindow& operator=(const RmlUiWindow&) = delete;

    // Call once at startup. Records the asset directory (converted to absolute).
    static bool initRml(HINSTANCE hInst, const std::string& assetBasePath);
    static void shutdownRml();

    // Starts the background thread and pre-warms the backend.
    // Blocks until initialization is complete (up to 10 s).
    bool create(HINSTANCE hInst, const Config& cfg);

    // Signals the background thread to shut down and joins it.
    void destroy();

    // Show/hide the settings window instantly (no re-initialization).
    void show();
    void hide();
    bool isVisible() const;

    // Update stored config (used on next show). Thread-safe.
    void loadConfig(const Config& cfg);

    // Update status badge in footer. Thread-safe, callable from any thread.
    void setStatus(const std::string& statusClass, const std::string& statusText);

    // Fired after "Save & Apply" (called on the window thread).
    std::function<void(const Config&)> onApplyConfig;

private:
    void threadFunc();
    void populateDom();
    void installEventListeners();
    Config readDataModel() const;
    void startBinding(const std::string& elementId);
    void commitBinding();
    void applyPendingStatus();

    HINSTANCE              m_hInst      = nullptr;
    int                    m_width      = 760;
    int                    m_height     = 600;
    Config                 m_cfg;
    HWND                   m_backendHwnd = nullptr;

    Rml::Context*          m_rmlCtx  = nullptr;
    Rml::ElementDocument*  m_doc     = nullptr;

    std::thread            m_thread;
    std::mutex             m_mutex;
    std::condition_variable m_cv;

    // Signals from main thread → window thread
    std::atomic<bool>      m_initialized  { false };
    std::atomic<bool>      m_shouldShow   { false };
    std::atomic<bool>      m_shouldHide   { false };
    std::atomic<bool>      m_shouldExit   { false };
    std::atomic<bool>      m_visible      { false };
    bool                   m_cfgDirty     = false;

    // Pending status update
    struct PendingStatus { std::string cls, text; bool dirty = false; };
    std::mutex    m_statusMutex;
    PendingStatus m_pendingStatus;

    // Hotkey binding state (window thread only)
    std::string   m_bindingElementId;
    std::vector<int> m_pendingKeys;
    bool          m_isBinding = false;

    static std::string s_assetPath;
};

} // namespace zenith
