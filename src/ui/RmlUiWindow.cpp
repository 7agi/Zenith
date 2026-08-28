#include "RmlUiWindow.h"

// RmlUi headers
#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Debugger.h>
#include <RmlUi_Backend.h>       // Backend::Initialize / ProcessEvents / etc.

#include <windows.h>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

namespace zenith {

// ---------------------------------------------------------------------------
// Statics
// ---------------------------------------------------------------------------
std::string RmlUiWindow::s_assetPath;

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
        case VK_NUMPAD0: return "Num0";
        case VK_NUMPAD1: return "Num1";
        case VK_NUMPAD2: return "Num2";
        case VK_NUMPAD3: return "Num3";
        case VK_NUMPAD4: return "Num4";
        case VK_NUMPAD5: return "Num5";
        case VK_NUMPAD6: return "Num6";
        case VK_NUMPAD7: return "Num7";
        case VK_NUMPAD8: return "Num8";
        case VK_NUMPAD9: return "Num9";
        default: {
            char name[64] = {};
            UINT scanCode = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
            if (GetKeyNameTextA((LONG)(scanCode << 16), name, sizeof(name)) > 0)
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
    return s;
}

// ---------------------------------------------------------------------------
// ZenithEventListener – simple adapter so we can attach lambdas to RmlUi
// ---------------------------------------------------------------------------
class ZenithEventListener : public Rml::EventListener {
public:
    std::function<void(Rml::Event&)> handler;
    void ProcessEvent(Rml::Event& event) override { if (handler) handler(event); }
    void OnDetach(Rml::Element*) override { delete this; }
};

// ---------------------------------------------------------------------------
// Static DOM helpers
// ---------------------------------------------------------------------------
static Rml::Element* el(Rml::ElementDocument* doc, const std::string& id) {
    return doc ? doc->GetElementById(id) : nullptr;
}

static std::string getVal(Rml::ElementDocument* doc, const std::string& id) {
    auto* e = el(doc, id);
    return e ? e->GetAttribute<Rml::String>("value", "") : "";
}

static void setVal(Rml::ElementDocument* doc, const std::string& id, const std::string& val) {
    auto* e = el(doc, id);
    if (e) e->SetAttribute("value", val);
}

static std::string getText(Rml::ElementDocument* doc, const std::string& id) {
    auto* e = el(doc, id);
    return e ? e->GetInnerRML() : "";
}

static void setText(Rml::ElementDocument* doc, const std::string& id, const std::string& text) {
    auto* e = el(doc, id);
    if (e) e->SetInnerRML(text);
}

static bool getChecked(Rml::ElementDocument* doc, const std::string& id) {
    auto* e = el(doc, id);
    if (!e) return false;
    return e->GetAttribute<Rml::String>("checked", "") == "checked";
}

static void setChecked(Rml::ElementDocument* doc, const std::string& id, bool checked) {
    auto* e = el(doc, id);
    if (!e) return;
    if (checked) e->SetAttribute("checked", "checked");
    else         e->RemoveAttribute("checked");
}

static int getSelectIndex(Rml::ElementDocument* doc, const std::string& id) {
    auto* e = el(doc, id);
    if (!e) return 0;
    auto val = e->GetAttribute<Rml::String>("value", "0");
    try { return std::stoi(val); } catch (...) { return 0; }
}

static void setSelectIndex(Rml::ElementDocument* doc, const std::string& id, int idx) {
    auto* e = el(doc, id);
    if (e) e->SetAttribute("value", std::to_string(idx));
}

// ---------------------------------------------------------------------------
// Global init / shutdown (call once per process)
// ---------------------------------------------------------------------------
bool RmlUiWindow::initRml(HINSTANCE /*hInst*/, const std::string& assetBasePath) {
    s_assetPath = assetBasePath;
    return true;
}

void RmlUiWindow::shutdownRml() {}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
RmlUiWindow::RmlUiWindow()  = default;
RmlUiWindow::~RmlUiWindow() { destroy(); }

// ---------------------------------------------------------------------------
// create – stores settings, actual init happens in threadFunc()
// ---------------------------------------------------------------------------
bool RmlUiWindow::create(HINSTANCE hInst, const Config& cfg) {
    m_hInst = hInst;
    m_cfg   = cfg;
    return true;
}

void RmlUiWindow::destroy() {
    hide();
    if (m_thread.joinable()) m_thread.join();
}

// ---------------------------------------------------------------------------
// show / hide
// ---------------------------------------------------------------------------
void RmlUiWindow::show() {
    if (m_running.load()) return;
    m_running = true;
    m_thread  = std::thread([this]() { threadFunc(); });
}

void RmlUiWindow::hide() {
    if (!m_running.load()) return;
    Backend::RequestExit();
}

bool RmlUiWindow::isVisible() const {
    return m_running.load();
}

// ---------------------------------------------------------------------------
// loadConfig – update stored config (and DOM if window is open)
// ---------------------------------------------------------------------------
void RmlUiWindow::loadConfig(const Config& cfg) {
    m_cfg = cfg;
    // DOM will be populated on next show() or immediately if window thread
    // calls populateDom() – nothing more needed here.
}

// ---------------------------------------------------------------------------
// setStatus – thread-safe status badge update
// ---------------------------------------------------------------------------
void RmlUiWindow::setStatus(const std::string& statusClass, const std::string& statusText) {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_pendingStatus.cls   = statusClass;
    m_pendingStatus.text  = statusText;
    m_pendingStatus.dirty = true;
}

// Apply pending status on window thread (called each frame)
void RmlUiWindow::applyPendingStatus() {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    if (!m_pendingStatus.dirty || !m_doc) return;
    m_pendingStatus.dirty = false;

    auto* badge = m_doc->GetElementById("status-indicator");
    if (!badge) return;
    badge->SetClass("status-idle",       m_pendingStatus.cls == "status-idle");
    badge->SetClass("status-buffering",  m_pendingStatus.cls == "status-buffering");
    badge->SetClass("status-recording",  m_pendingStatus.cls == "status-recording");
    badge->SetInnerRML(m_pendingStatus.text);
}

// ---------------------------------------------------------------------------
// threadFunc – owns the Backend lifetime + event/render loop
// ---------------------------------------------------------------------------
void RmlUiWindow::threadFunc() {
    // Backend::Initialize creates the Win32 window + DX11 device + swap chain.
    if (!Backend::Initialize("Zenith Settings", m_width, m_height, /*allow_resize=*/false)) {
        m_running = false;
        return;
    }

    Rml::SetSystemInterface(Backend::GetSystemInterface());
    Rml::SetRenderInterface(Backend::GetRenderInterface());

    if (!Rml::Initialise()) {
        Backend::Shutdown();
        m_running = false;
        return;
    }

    // Load fonts (Inter; fall back gracefully if files missing)
    std::string fontsDir = s_assetPath + "/fonts/";
    Rml::LoadFontFace(fontsDir + "Inter-Regular.ttf");
    Rml::LoadFontFace(fontsDir + "Inter-Bold.ttf",    true);
    Rml::LoadFontFace(fontsDir + "Inter-Italic.ttf");
    // Fallback system font so UI is never blank
    Rml::LoadFontFace("C:/Windows/Fonts/segoeui.ttf");

    m_rmlCtx = Rml::CreateContext("main", Rml::Vector2i(m_width, m_height));
    if (!m_rmlCtx) {
        Rml::Shutdown();
        Backend::Shutdown();
        m_running = false;
        return;
    }

    std::string docPath = s_assetPath + "/settings.rml";
    m_doc = m_rmlCtx->LoadDocument(docPath);
    if (m_doc) {
        populateDom();
        installEventListeners();
        m_doc->Show();
    }

    // ----- Main event + render loop -----
    while (Backend::ProcessEvents(m_rmlCtx)) {
        applyPendingStatus();

        Backend::BeginFrame();
        m_rmlCtx->Update();
        m_rmlCtx->Render();
        Backend::PresentFrame();
    }

    // Cleanup (on this thread)
    m_doc = nullptr;
    Rml::RemoveContext("main");
    m_rmlCtx = nullptr;
    Rml::Shutdown();
    Backend::Shutdown();
    m_running = false;
}

// ---------------------------------------------------------------------------
// populateDom – fill every UI field from m_cfg
// ---------------------------------------------------------------------------
void RmlUiWindow::populateDom() {
    if (!m_doc) return;
    const Config& c = m_cfg;

    // --- Capture tab ---
    setSelectIndex(m_doc, "capture-method",  (int)c.captureMethod);
    setVal(m_doc,         "monitor-index",   std::to_string(c.monitorIndex));
    setVal(m_doc,         "target-exe",      c.gameCaptureExe);

    // --- Recording tab ---
    setSelectIndex(m_doc, "record-mode",     (int)c.recordingMode);
    setVal(m_doc,         "clip-duration",   std::to_string(c.clipDurationSecs));
    setVal(m_doc,         "output-folder",   c.outputDirectory);

    // --- Video tab ---
    setVal(m_doc, "video-width",   std::to_string(c.video.width));
    setVal(m_doc, "video-height",  std::to_string(c.video.height));
    setVal(m_doc, "video-fps",     std::to_string(c.video.fps));
    setVal(m_doc, "video-bitrate", std::to_string(c.video.bitrateKbps));
    setVal(m_doc, "encoder",       c.video.encoder);
    setVal(m_doc, "audio-bitrate", std::to_string(c.audio.bitrateKbps));
    setChecked(m_doc, "record-audio", c.audio.captureDesktop);

    // --- Overlays tab ---
    setChecked(m_doc,     "camera-enabled",  c.camera.enabled);
    setVal(m_doc,         "camera-width",    std::to_string((int)c.camera.width));
    setVal(m_doc,         "camera-height",   std::to_string((int)c.camera.height));
    setChecked(m_doc,     "camera-flip-h",   c.camera.flipH);
    setChecked(m_doc,     "keys-enabled",    c.keyOverlay.enabled);
    setVal(m_doc,         "key-size",        std::to_string((int)c.keyOverlay.keySize));

    // --- Hotkeys tab ---
    setText(m_doc, "hk-clip-save",      vkeysToString(c.hotkeys.clipSave.vkeys));
    setText(m_doc, "hk-record-toggle",  vkeysToString(c.hotkeys.recordToggle.vkeys));
    setText(m_doc, "hk-overlay-toggle", vkeysToString(c.hotkeys.overlayToggle.vkeys));
}

// ---------------------------------------------------------------------------
// readDataModel – read UI fields back into a Config
// ---------------------------------------------------------------------------
Config RmlUiWindow::readDataModel() const {
    Config c = m_cfg; // start from current
    if (!m_doc) return c;

    // Capture
    c.captureMethod = (CaptureMethod)getSelectIndex(m_doc, "capture-method");
    try { c.monitorIndex = std::stoi(getVal(m_doc, "monitor-index")); } catch (...) {}
    c.gameCaptureExe = getVal(m_doc, "target-exe");

    // Recording
    c.recordingMode = (RecordingMode)getSelectIndex(m_doc, "record-mode");
    try { c.clipDurationSecs = std::stoi(getVal(m_doc, "clip-duration")); } catch (...) {}
    c.outputDirectory = getVal(m_doc, "output-folder");

    // Video
    try { c.video.width       = std::stoi(getVal(m_doc, "video-width"));   } catch (...) {}
    try { c.video.height      = std::stoi(getVal(m_doc, "video-height"));  } catch (...) {}
    try { c.video.fps         = std::stoi(getVal(m_doc, "video-fps"));     } catch (...) {}
    try { c.video.bitrateKbps = std::stoi(getVal(m_doc, "video-bitrate")); } catch (...) {}
    c.video.encoder = getVal(m_doc, "encoder");
    try { c.audio.bitrateKbps = std::stoi(getVal(m_doc, "audio-bitrate")); } catch (...) {}
    c.audio.captureDesktop = getChecked(m_doc, "record-audio");

    // Overlays
    c.camera.enabled = getChecked(m_doc, "camera-enabled");
    try { c.camera.width  = (float)std::stoi(getVal(m_doc, "camera-width"));  } catch (...) {}
    try { c.camera.height = (float)std::stoi(getVal(m_doc, "camera-height")); } catch (...) {}
    c.camera.flipH         = getChecked(m_doc, "camera-flip-h");
    c.keyOverlay.enabled   = getChecked(m_doc, "keys-enabled");
    try { c.keyOverlay.keySize = (float)std::stoi(getVal(m_doc, "key-size")); } catch (...) {}

    return c;
}

// ---------------------------------------------------------------------------
// installEventListeners – wire up HTML events
// ---------------------------------------------------------------------------
void RmlUiWindow::installEventListeners() {
    if (!m_doc) return;

    // ---- Tab switching ----
    struct TabDef { const char* btn; const char* panel; };
    TabDef tabs[] = {
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
        std::string panelId = td.panel;
        std::string tabId   = td.btn;
        lis->handler = [this, panelId, tabId](Rml::Event&) {
            // Hide all panels
            for (const char* id : {"panel-capture","panel-recording","panel-video",
                                    "panel-overlays","panel-hotkeys"}) {
                auto* p = m_doc->GetElementById(id);
                if (p) p->SetClass("hidden", true);
            }
            // Deactivate all tabs
            for (const char* id : {"tab-capture","tab-recording","tab-video",
                                    "tab-overlays","tab-hotkeys"}) {
                auto* t = m_doc->GetElementById(id);
                if (t) t->SetClass("active", false);
            }
            // Show selected panel + activate tab
            auto* panel = m_doc->GetElementById(panelId);
            if (panel) panel->SetClass("hidden", false);
            auto* tab = m_doc->GetElementById(tabId);
            if (tab) tab->SetClass("active", true);
        };
        btn->AddEventListener(Rml::EventId::Click, lis);
    }

    // ---- Close button ----
    {
        auto* btn = m_doc->GetElementById("close-btn");
        if (btn) {
            auto* lis = new ZenithEventListener();
            lis->handler = [](Rml::Event&) { Backend::RequestExit(); };
            btn->AddEventListener(Rml::EventId::Click, lis);
        }
    }

    // ---- Save & Apply ----
    {
        Rml::ElementList btns;
        m_doc->GetElementsByClassName(btns, "btn-primary");
        for (auto* b : btns) {
            auto* lis = new ZenithEventListener();
            lis->handler = [this](Rml::Event&) {
                Config cfg = readDataModel();
                if (onApplyConfig) onApplyConfig(cfg);
                Backend::RequestExit();
            };
            b->AddEventListener(Rml::EventId::Click, lis);
        }
    }

    // ---- Discard / Cancel ----
    {
        Rml::ElementList btns;
        m_doc->GetElementsByClassName(btns, "btn-secondary");
        for (auto* b : btns) {
            auto* lis = new ZenithEventListener();
            lis->handler = [](Rml::Event&) { Backend::RequestExit(); };
            b->AddEventListener(Rml::EventId::Click, lis);
        }
    }

    // ---- Hotkey binding boxes ----
    const char* hkIds[] = {"hk-clip-save", "hk-record-toggle", "hk-overlay-toggle"};
    for (const char* hkId : hkIds) {
        auto* e = m_doc->GetElementById(hkId);
        if (!e) continue;
        auto* lis = new ZenithEventListener();
        std::string elemId = hkId;
        lis->handler = [this, elemId](Rml::Event&) { startBinding(elemId); };
        e->AddEventListener(Rml::EventId::Click, lis);
    }
}

// ---------------------------------------------------------------------------
// Hotkey binding
// ---------------------------------------------------------------------------
void RmlUiWindow::startBinding(const std::string& elementId) {
    m_bindingElementId = elementId;
    m_pendingKeys.clear();
    m_isBinding = true;

    // Show visual feedback
    auto* e = el(m_doc, elementId);
    if (e) e->SetInnerRML("Press keys...");
}

void RmlUiWindow::commitBinding() {
    if (!m_isBinding) return;
    m_isBinding = false;

    std::string display = vkeysToString(m_pendingKeys);
    auto* e = el(m_doc, m_bindingElementId);
    if (e) e->SetInnerRML(display.empty() ? "(none)" : display);

    // Store into m_cfg so readDataModel() picks it up
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
