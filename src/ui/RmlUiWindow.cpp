#include "RmlUiWindow.h"

// RmlUi headers
#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Debugger.h>
#include <RmlUi_Backend.h>

#include <windows.h>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <condition_variable>

namespace zenith {

// ---------------------------------------------------------------------------
// Statics
// ---------------------------------------------------------------------------
std::string RmlUiWindow::s_assetPath;

// ---------------------------------------------------------------------------
// Returns the directory that contains Zenith.exe (always absolute)
// ---------------------------------------------------------------------------
static std::string getExeDir() {
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring path(buf);
    auto pos = path.rfind(L'\\');
    if (pos != std::wstring::npos) path = path.substr(0, pos);
    // convert wstring -> string (ASCII-safe for typical Windows paths)
    return std::string(path.begin(), path.end());
}

// ---------------------------------------------------------------------------
// VKey → display name
// ---------------------------------------------------------------------------
static std::string vkeyToName(int vk) {
    if (vk >= 'A' && vk <= 'Z') return std::string(1, (char)vk);
    if (vk >= '0' && vk <= '9') return std::string(1, (char)vk);
    if (vk >= VK_F1 && vk <= VK_F12) return "F" + std::to_string(vk - VK_F1 + 1);
    switch (vk) {
        case VK_SPACE:   return "Space";
        case VK_RETURN:  return "Enter";
        case VK_BACK:    return "Backspace";
        case VK_TAB:     return "Tab";
        case VK_ESCAPE:  return "Esc";
        case VK_SHIFT:   return "Shift";
        case VK_CONTROL: return "Ctrl";
        case VK_MENU:    return "Alt";
        case VK_LWIN:
        case VK_RWIN:    return "Win";
        case VK_LEFT:    return "Left";
        case VK_RIGHT:   return "Right";
        case VK_UP:      return "Up";
        case VK_DOWN:    return "Down";
        case VK_INSERT:  return "Insert";
        case VK_DELETE:  return "Delete";
        case VK_HOME:    return "Home";
        case VK_END:     return "End";
        case VK_PRIOR:   return "PgUp";
        case VK_NEXT:    return "PgDn";
        default: {
            char name[64] = {};
            UINT sc = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
            if (GetKeyNameTextA((LONG)(sc << 16), name, sizeof(name)) > 0)
                return std::string(name);
            return "Key" + std::to_string(vk);
        }
    }
}

static std::string vkeysToString(const std::vector<int>& vkeys) {
    std::string s;
    for (int i = 0; i < (int)vkeys.size(); ++i) {
        if (i) s += " + ";
        s += vkeyToName(vkeys[i]);
    }
    return s.empty() ? "(none)" : s;
}

// ---------------------------------------------------------------------------
// ZenithEventListener
// ---------------------------------------------------------------------------
class ZenithEventListener : public Rml::EventListener {
public:
    std::function<void(Rml::Event&)> handler;
    void ProcessEvent(Rml::Event& ev) override { if (handler) handler(ev); }
    void OnDetach(Rml::Element*) override { delete this; }
};

// ---------------------------------------------------------------------------
// DOM helpers
// ---------------------------------------------------------------------------
static Rml::Element* el(Rml::ElementDocument* doc, const std::string& id) {
    return doc ? doc->GetElementById(id) : nullptr;
}
static std::string getVal(Rml::ElementDocument* doc, const std::string& id) {
    auto* e = el(doc, id);
    return e ? e->GetAttribute<Rml::String>("value", "") : "";
}
static void setVal(Rml::ElementDocument* doc, const std::string& id, const std::string& v) {
    auto* e = el(doc, id);
    if (e) e->SetAttribute("value", v);
}
static void setText(Rml::ElementDocument* doc, const std::string& id, const std::string& t) {
    auto* e = el(doc, id);
    if (e) e->SetInnerRML(t);
}
static bool getChecked(Rml::ElementDocument* doc, const std::string& id) {
    auto* e = el(doc, id);
    return e && e->GetAttribute<Rml::String>("checked", "") == "checked";
}
static void setChecked(Rml::ElementDocument* doc, const std::string& id, bool v) {
    auto* e = el(doc, id);
    if (!e) return;
    if (v) e->SetAttribute("checked", "checked");
    else   e->RemoveAttribute("checked");
}
static int getSelectIndex(Rml::ElementDocument* doc, const std::string& id) {
    auto* e = el(doc, id);
    if (!e) return 0;
    try { return std::stoi(e->GetAttribute<Rml::String>("value", "0")); }
    catch (...) { return 0; }
}
static void setSelectIndex(Rml::ElementDocument* doc, const std::string& id, int idx) {
    auto* e = el(doc, id);
    if (e) e->SetAttribute("value", std::to_string(idx));
}

// ---------------------------------------------------------------------------
// initRml – just records the asset base path; real init is done once in
//            the pre-warm thread started by create().
// ---------------------------------------------------------------------------
bool RmlUiWindow::initRml(HINSTANCE /*hInst*/, const std::string& assetBasePath) {
    // If relative, convert to absolute based on exe location
    if (!assetBasePath.empty() && assetBasePath[0] != '/' && assetBasePath[1] != ':') {
        s_assetPath = getExeDir() + "\\" + assetBasePath;
    } else {
        s_assetPath = assetBasePath;
    }
    return true;
}
void RmlUiWindow::shutdownRml() {}

RmlUiWindow::RmlUiWindow()  = default;
RmlUiWindow::~RmlUiWindow() { destroy(); }

// ---------------------------------------------------------------------------
// create – stores config, then starts the pre-warm thread immediately so
//           the window is ready before the user even clicks "Settings".
// ---------------------------------------------------------------------------
bool RmlUiWindow::create(HINSTANCE hInst, const Config& cfg) {
    m_hInst = hInst;
    m_cfg   = cfg;
    m_shouldExit = false;

    // Start the persistent backend thread right away.
    // It will initialise everything, then hide and wait.
    m_thread = std::thread([this]() { threadFunc(); });

    // Give the thread time to fully initialize before returning,
    // so subsequent show() calls are instant.
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        // Wait until initialized (signalled in threadFunc)
        m_cv.wait_for(lock, std::chrono::seconds(10),
                      [this]{ return m_initialized.load(); });
    }
    return true;
}

void RmlUiWindow::destroy() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_shouldExit = true;
    }
    m_cv.notify_all();
    Backend::RequestExit(); // break ProcessEvents if running
    if (m_thread.joinable()) m_thread.join();
}

// ---------------------------------------------------------------------------
// show / hide  (instant – window is pre-created, just toggle visibility)
// ---------------------------------------------------------------------------
void RmlUiWindow::show() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized) return;
        m_shouldShow = true;
        m_shouldHide = false;
    }
    m_cv.notify_one();
}

void RmlUiWindow::hide() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_shouldHide = true;
        m_shouldShow = false;
    }
    m_cv.notify_one();
}

bool RmlUiWindow::isVisible() const {
    return m_visible.load();
}

// ---------------------------------------------------------------------------
// loadConfig  (thread-safe: just updates m_cfg; populateDom on next show)
// ---------------------------------------------------------------------------
void RmlUiWindow::loadConfig(const Config& cfg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cfg = cfg;
    m_cfgDirty = true;
}

// ---------------------------------------------------------------------------
// setStatus  (thread-safe)
// ---------------------------------------------------------------------------
void RmlUiWindow::setStatus(const std::string& statusClass,
                             const std::string& statusText) {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_pendingStatus.cls   = statusClass;
    m_pendingStatus.text  = statusText;
    m_pendingStatus.dirty = true;
}

void RmlUiWindow::applyPendingStatus() {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    if (!m_pendingStatus.dirty || !m_doc) return;
    m_pendingStatus.dirty = false;
    auto* badge = m_doc->GetElementById("status-indicator");
    if (!badge) return;
    badge->SetClass("status-idle",      m_pendingStatus.cls == "status-idle");
    badge->SetClass("status-buffering", m_pendingStatus.cls == "status-buffering");
    badge->SetClass("status-recording", m_pendingStatus.cls == "status-recording");
    badge->SetInnerRML(m_pendingStatus.text);
}

// ---------------------------------------------------------------------------
// threadFunc – persistent lifecycle:
//   1. Initialize Backend + RmlUi + fonts + document (once)
//   2. Hide window, wait for show() signal
//   3. On show: make window visible, run event loop until hide/close
//   4. On hide: hide window, go back to step 2
//   5. On destroy: exit
// ---------------------------------------------------------------------------
void RmlUiWindow::threadFunc() {
    // ---- One-time initialization ----
    if (!Backend::Initialize("Zenith Settings", m_width, m_height, false)) {
        m_initialized = true;
        m_cv.notify_all();
        return;
    }

    Rml::SetSystemInterface(Backend::GetSystemInterface());
    Rml::SetRenderInterface(Backend::GetRenderInterface());

    if (!Rml::Initialise()) {
        Backend::Shutdown();
        m_initialized = true;
        m_cv.notify_all();
        return;
    }

    // Find the HWND the backend created so we can show/hide it
    // The backend window class is "Win32" and title is "Zenith Settings"
    m_backendHwnd = FindWindowW(nullptr, L"Zenith Settings");

    // Hide immediately – window starts hidden, shown only on show()
    if (m_backendHwnd) ShowWindow(m_backendHwnd, SW_HIDE);

    // Load fonts (absolute paths)
    std::string fontsDir = s_assetPath + "\\fonts\\";
    // Custom fonts (user must supply Inter; falls back gracefully)
    Rml::LoadFontFace(fontsDir + "Inter-Regular.ttf");
    Rml::LoadFontFace(fontsDir + "Inter-Bold.ttf", true);
    Rml::LoadFontFace(fontsDir + "Inter-Italic.ttf");
    // Reliable system fallbacks (always present on Windows)
    bool hasFontFallback = false;
    const char* sysFonts[] = {
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\Segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\Arial.ttf",
        "C:\\Windows\\Fonts\\tahoma.ttf",
    };
    for (const char* f : sysFonts) {
        if (Rml::LoadFontFace(f)) { hasFontFallback = true; break; }
    }
    // Last resort: try every ttf in Windows\Fonts
    if (!hasFontFallback) {
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA("C:\\Windows\\Fonts\\*.ttf", &fd);
        if (h != INVALID_HANDLE_VALUE) {
            std::string p = std::string("C:\\Windows\\Fonts\\") + fd.cFileName;
            Rml::LoadFontFace(p, true);
            FindClose(h);
        }
    }

    m_rmlCtx = Rml::CreateContext("main", Rml::Vector2i(m_width, m_height));

    // Load document
    if (m_rmlCtx) {
        std::string docPath = s_assetPath + "\\settings.rml";
        m_doc = m_rmlCtx->LoadDocument(docPath);
        if (m_doc) {
            populateDom();
            installEventListeners();
            // Keep hidden for now
            m_doc->Hide();
        }
    }

    // Signal that we're done initializing
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_initialized = true;
    }
    m_cv.notify_all();

    // ---- Persistent show/hide loop ----
    while (!m_shouldExit.load()) {
        // Wait until we're asked to show (or exit)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]{
                return m_shouldShow.load() || m_shouldExit.load();
            });
        }
        if (m_shouldExit) break;

        // --- SHOW phase ---
        m_shouldShow = false;
        m_visible    = true;

        // Refresh DOM with latest config
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_cfgDirty) { populateDom(); m_cfgDirty = false; }
        }

        if (m_doc)        m_doc->Show();
        if (m_backendHwnd) {
            ShowWindow(m_backendHwnd, SW_SHOW);
            SetForegroundWindow(m_backendHwnd);
        }

        // Event + render loop (runs while window is visible)
        while (!m_shouldExit && !m_shouldHide) {
            // Pump messages without blocking so we can check m_shouldHide
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    m_shouldHide = true;
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if (m_shouldHide) break;

            applyPendingStatus();

            Backend::BeginFrame();
            if (m_rmlCtx) {
                m_rmlCtx->Update();
                m_rmlCtx->Render();
            }
            Backend::PresentFrame();

            // ~60 fps cap when visible
            Sleep(16);
        }

        // --- HIDE phase ---
        m_shouldHide = false;
        m_visible    = false;
        if (m_doc)         m_doc->Hide();
        if (m_backendHwnd) ShowWindow(m_backendHwnd, SW_HIDE);
    }

    // Cleanup
    m_doc    = nullptr;
    if (m_rmlCtx) { Rml::RemoveContext("main"); m_rmlCtx = nullptr; }
    Rml::Shutdown();
    Backend::Shutdown();
}

// ---------------------------------------------------------------------------
// populateDom
// ---------------------------------------------------------------------------
void RmlUiWindow::populateDom() {
    if (!m_doc) return;
    const Config& c = m_cfg;

    setSelectIndex(m_doc, "capture-method", (int)c.captureMethod);
    setVal(m_doc, "monitor-index",  std::to_string(c.monitorIndex));
    setVal(m_doc, "target-exe",     c.gameCaptureExe);

    setSelectIndex(m_doc, "record-mode",  (int)c.recordingMode);
    setVal(m_doc, "clip-duration",  std::to_string(c.clipDurationSecs));
    setVal(m_doc, "output-folder",  c.outputDirectory);

    setVal(m_doc, "video-width",    std::to_string(c.video.width));
    setVal(m_doc, "video-height",   std::to_string(c.video.height));
    setVal(m_doc, "video-fps",      std::to_string(c.video.fps));
    setVal(m_doc, "video-bitrate",  std::to_string(c.video.bitrateKbps));
    setVal(m_doc, "encoder",        c.video.encoder);
    setVal(m_doc, "audio-bitrate",  std::to_string(c.audio.bitrateKbps));
    setChecked(m_doc, "record-audio", c.audio.captureDesktop);

    setChecked(m_doc, "camera-enabled", c.camera.enabled);
    setVal(m_doc, "camera-width",   std::to_string((int)c.camera.width));
    setVal(m_doc, "camera-height",  std::to_string((int)c.camera.height));
    setChecked(m_doc, "camera-flip-h", c.camera.flipH);
    setChecked(m_doc, "keys-enabled",  c.keyOverlay.enabled);
    setVal(m_doc, "key-size",       std::to_string((int)c.keyOverlay.keySize));

    setText(m_doc, "hk-clip-save",      vkeysToString(c.hotkeys.clipSave.vkeys));
    setText(m_doc, "hk-record-toggle",  vkeysToString(c.hotkeys.recordToggle.vkeys));
    setText(m_doc, "hk-overlay-toggle", vkeysToString(c.hotkeys.overlayToggle.vkeys));
}

// ---------------------------------------------------------------------------
// readDataModel
// ---------------------------------------------------------------------------
Config RmlUiWindow::readDataModel() const {
    Config c = m_cfg;
    if (!m_doc) return c;

    c.captureMethod = (CaptureMethod)getSelectIndex(m_doc, "capture-method");
    try { c.monitorIndex = std::stoi(getVal(m_doc, "monitor-index")); } catch (...) {}
    c.gameCaptureExe = getVal(m_doc, "target-exe");

    c.recordingMode = (RecordingMode)getSelectIndex(m_doc, "record-mode");
    try { c.clipDurationSecs = std::stoi(getVal(m_doc, "clip-duration")); } catch (...) {}
    c.outputDirectory = getVal(m_doc, "output-folder");

    try { c.video.width       = std::stoi(getVal(m_doc, "video-width"));   } catch (...) {}
    try { c.video.height      = std::stoi(getVal(m_doc, "video-height"));  } catch (...) {}
    try { c.video.fps         = std::stoi(getVal(m_doc, "video-fps"));     } catch (...) {}
    try { c.video.bitrateKbps = std::stoi(getVal(m_doc, "video-bitrate")); } catch (...) {}
    c.video.encoder = getVal(m_doc, "encoder");
    try { c.audio.bitrateKbps = std::stoi(getVal(m_doc, "audio-bitrate")); } catch (...) {}
    c.audio.captureDesktop = getChecked(m_doc, "record-audio");

    c.camera.enabled = getChecked(m_doc, "camera-enabled");
    try { c.camera.width  = (float)std::stoi(getVal(m_doc, "camera-width"));  } catch (...) {}
    try { c.camera.height = (float)std::stoi(getVal(m_doc, "camera-height")); } catch (...) {}
    c.camera.flipH       = getChecked(m_doc, "camera-flip-h");
    c.keyOverlay.enabled = getChecked(m_doc, "keys-enabled");
    try { c.keyOverlay.keySize = (float)std::stoi(getVal(m_doc, "key-size")); } catch (...) {}

    return c;
}

// ---------------------------------------------------------------------------
// installEventListeners
// ---------------------------------------------------------------------------
void RmlUiWindow::installEventListeners() {
    if (!m_doc) return;

    // Tab switching
    struct TabDef { const char* btn; const char* panel; };
    static const TabDef tabs[] = {
        {"tab-capture",   "panel-capture"},
        {"tab-recording", "panel-recording"},
        {"tab-video",     "panel-video"},
        {"tab-overlays",  "panel-overlays"},
        {"tab-hotkeys",   "panel-hotkeys"},
    };
    for (auto& td : tabs) {
        auto* btn = m_doc->GetElementById(td.btn);
        if (!btn) continue;
        auto* lis = new ZenithEventListener();
        std::string panelId = td.panel, tabId = td.btn;
        lis->handler = [this, panelId, tabId](Rml::Event&) {
            for (const char* id : {"panel-capture","panel-recording","panel-video",
                                    "panel-overlays","panel-hotkeys"})
                if (auto* p = m_doc->GetElementById(id)) p->SetClass("hidden", true);
            for (const char* id : {"tab-capture","tab-recording","tab-video",
                                    "tab-overlays","tab-hotkeys"})
                if (auto* t = m_doc->GetElementById(id)) t->SetClass("active", false);
            if (auto* p = m_doc->GetElementById(panelId)) p->SetClass("hidden", false);
            if (auto* t = m_doc->GetElementById(tabId))   t->SetClass("active", true);
        };
        btn->AddEventListener(Rml::EventId::Click, lis);
    }

    // Close / Discard buttons → hide window
    auto makeHideListener = [this]() -> ZenithEventListener* {
        auto* lis = new ZenithEventListener();
        lis->handler = [this](Rml::Event&) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_shouldHide = true;
        };
        return lis;
    };
    if (auto* btn = m_doc->GetElementById("close-btn"))
        btn->AddEventListener(Rml::EventId::Click, makeHideListener());
    {
        Rml::ElementList btns;
        m_doc->GetElementsByClassName(btns, "btn-secondary");
        for (auto* b : btns) b->AddEventListener(Rml::EventId::Click, makeHideListener());
    }

    // Save & Apply
    {
        Rml::ElementList btns;
        m_doc->GetElementsByClassName(btns, "btn-primary");
        for (auto* b : btns) {
            auto* lis = new ZenithEventListener();
            lis->handler = [this](Rml::Event&) {
                Config cfg = readDataModel();
                if (onApplyConfig) onApplyConfig(cfg);
                std::lock_guard<std::mutex> lock(m_mutex);
                m_shouldHide = true;
            };
            b->AddEventListener(Rml::EventId::Click, lis);
        }
    }

    // Hotkey binding boxes
    for (const char* hkId : {"hk-clip-save","hk-record-toggle","hk-overlay-toggle"}) {
        if (auto* e = m_doc->GetElementById(hkId)) {
            auto* lis = new ZenithEventListener();
            std::string id = hkId;
            lis->handler = [this, id](Rml::Event&) { startBinding(id); };
            e->AddEventListener(Rml::EventId::Click, lis);
        }
    }
}

// ---------------------------------------------------------------------------
// Hotkey binding
// ---------------------------------------------------------------------------
void RmlUiWindow::startBinding(const std::string& elementId) {
    m_bindingElementId = elementId;
    m_pendingKeys.clear();
    m_isBinding = true;
    if (auto* e = el(m_doc, elementId)) e->SetInnerRML("Press keys...");
}

void RmlUiWindow::commitBinding() {
    if (!m_isBinding) return;
    m_isBinding = false;
    std::string display = vkeysToString(m_pendingKeys);
    if (auto* e = el(m_doc, m_bindingElementId)) e->SetInnerRML(display);
    if      (m_bindingElementId == "hk-clip-save")
        m_cfg.hotkeys.clipSave.vkeys = m_pendingKeys;
    else if (m_bindingElementId == "hk-record-toggle")
        m_cfg.hotkeys.recordToggle.vkeys = m_pendingKeys;
    else if (m_bindingElementId == "hk-overlay-toggle")
        m_cfg.hotkeys.overlayToggle.vkeys = m_pendingKeys;
    m_bindingElementId.clear();
    m_pendingKeys.clear();
}

} // namespace zenith
