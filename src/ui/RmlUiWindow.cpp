#include "RmlUiWindow.h"

// RmlUi headers
#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Debugger.h>
#include <RmlUi_Backend.h>        // RmlUi win32+dx11 backend (from FetchContent)

#include <windowsx.h>
#include <string>
#include <sstream>
#include <algorithm>

// Link DX11
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace zenith {

// ---------------------------------------------------------------------------
// Statics
// ---------------------------------------------------------------------------
RmlUiWindow* RmlUiWindow::s_instance = nullptr;
std::string  RmlUiWindow::s_assetPath;

// ---------------------------------------------------------------------------
// VKey → display name helper
// ---------------------------------------------------------------------------
static std::string vkeyToName(int vk) {
    if (vk >= 'A' && vk <= 'Z') return std::string(1, (char)vk);
    if (vk >= '0' && vk <= '9') return std::string(1, (char)vk);
    switch (vk) {
        case VK_SPACE:   return "Space";
        case VK_RETURN:  return "Enter";
        case VK_BACK:    return "Backspace";
        case VK_TAB:     return "Tab";
        case VK_ESCAPE:  return "Esc";
        case VK_SHIFT:   return "Shift";
        case VK_CONTROL: return "Ctrl";
        case VK_MENU:    return "Alt";
        case VK_LSHIFT:  return "LShift";
        case VK_RSHIFT:  return "RShift";
        case VK_LCONTROL:return "LCtrl";
        case VK_RCONTROL:return "RCtrl";
        case VK_LMENU:   return "LAlt";
        case VK_RMENU:   return "RAlt";
        case VK_F1:      return "F1";
        case VK_F2:      return "F2";
        case VK_F3:      return "F3";
        case VK_F4:      return "F4";
        case VK_F5:      return "F5";
        case VK_F6:      return "F6";
        case VK_F7:      return "F7";
        case VK_F8:      return "F8";
        case VK_F9:      return "F9";
        case VK_F10:     return "F10";
        case VK_F11:     return "F11";
        case VK_F12:     return "F12";
        case VK_INSERT:  return "Ins";
        case VK_DELETE:  return "Del";
        case VK_HOME:    return "Home";
        case VK_END:     return "End";
        case VK_PRIOR:   return "PgUp";
        case VK_NEXT:    return "PgDn";
        case VK_LEFT:    return "Left";
        case VK_RIGHT:   return "Right";
        case VK_UP:      return "Up";
        case VK_DOWN:    return "Down";
        default: {
            char buf[32];
            snprintf(buf, sizeof(buf), "VK(%d)", vk);
            return buf;
        }
    }
}

static std::string vkeysToString(const std::vector<int>& vkeys) {
    if (vkeys.empty()) return "(not set)";
    std::string result;
    for (size_t i = 0; i < vkeys.size(); ++i) {
        if (i > 0) result += " + ";
        result += vkeyToName(vkeys[i]);
    }
    return result;
}

// Parse "Z, X, SPACE" → vector of vkeys
static std::vector<int> parseKeyNames(const std::string& text) {
    std::vector<int> out;
    std::istringstream ss(text);
    std::string token;
    while (std::getline(ss, token, ',')) {
        // trim
        while (!token.empty() && isspace((unsigned char)token.front())) token.erase(token.begin());
        while (!token.empty() && isspace((unsigned char)token.back()))  token.pop_back();
        // uppercase
        for (auto& c : token) c = (char)toupper((unsigned char)c);
        if (token.size() == 1 && token[0] >= 'A' && token[0] <= 'Z') {
            out.push_back((int)token[0]);
        } else if (token == "SPACE") {
            out.push_back(VK_SPACE);
        } else if (token == "ENTER") {
            out.push_back(VK_RETURN);
        } else if (token.size() >= 2 && token[0] == 'F') {
            int n = atoi(token.c_str() + 1);
            if (n >= 1 && n <= 12) out.push_back(VK_F1 + n - 1);
        }
    }
    return out;
}

static std::string vkeysToKeyNames(const std::vector<int>& vkeys) {
    std::string result;
    for (size_t i = 0; i < vkeys.size(); ++i) {
        if (i > 0) result += ", ";
        result += vkeyToName(vkeys[i]);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Helper: get element by ID
// ---------------------------------------------------------------------------
static Rml::Element* el(Rml::ElementDocument* doc, const std::string& id) {
    return doc ? doc->GetElementById(id) : nullptr;
}

static std::string getVal(Rml::ElementDocument* doc, const std::string& id) {
    auto* e = el(doc, id);
    if (!e) return {};
    return e->GetAttribute<Rml::String>("value", "");
}

static void setVal(Rml::ElementDocument* doc, const std::string& id, const std::string& val) {
    auto* e = el(doc, id);
    if (!e) return;
    e->SetAttribute("value", val);
}

static void setText(Rml::ElementDocument* doc, const std::string& id, const std::string& text) {
    auto* e = el(doc, id);
    if (!e) return;
    e->SetInnerRML(text);
}

static bool getChecked(Rml::ElementDocument* doc, const std::string& id) {
    auto* e = el(doc, id);
    if (!e) return false;
    return e->HasAttribute("checked");
}

static void setChecked(Rml::ElementDocument* doc, const std::string& id, bool checked) {
    auto* e = el(doc, id);
    if (!e) return;
    if (checked) e->SetAttribute("checked", "");
    else e->RemoveAttribute("checked");
}

static void setSelectIndex(Rml::ElementDocument* doc, const std::string& id, int idx) {
    auto* e = el(doc, id);
    if (!e) return;
    // Walk option children and set "selected"
    int ci = 0;
    for (int i = 0; i < (int)e->GetNumChildren(); ++i) {
        auto* child = e->GetChild(i);
        if (!child) continue;
        if (ci == idx) child->SetAttribute("selected", "");
        else child->RemoveAttribute("selected");
        ci++;
    }
}

static int getSelectIndex(Rml::ElementDocument* doc, const std::string& id) {
    auto* e = el(doc, id);
    if (!e) return 0;
    int ci = 0;
    for (int i = 0; i < (int)e->GetNumChildren(); ++i) {
        auto* child = e->GetChild(i);
        if (!child) continue;
        if (child->HasAttribute("selected")) return ci;
        ci++;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Global init/shutdown
// ---------------------------------------------------------------------------
bool RmlUiWindow::initRml(HINSTANCE hInst, const std::string& assetBasePath) {
    s_assetPath = assetBasePath;

    // RmlUi backend initializes its own Win32 window class and DX11 device
    // We pass nullptr for the window since we create our own below.
    if (!Backend::Initialize("Zenith UI", 760, 600, false))
        return false;

    Rml::SetSystemInterface(Backend::GetSystemInterface());
    Rml::SetRenderInterface(Backend::GetRenderInterface());
    Rml::Initialise();

    // Load font
    std::string fontPath = assetBasePath + "/fonts/Inter-Regular.ttf";
    Rml::LoadFontFace(fontPath);
    fontPath = assetBasePath + "/fonts/Inter-Bold.ttf";
    Rml::LoadFontFace(fontPath, true);

    return true;
}

void RmlUiWindow::shutdownRml() {
    Rml::Shutdown();
    Backend::Shutdown();
}

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------
RmlUiWindow::RmlUiWindow() {}

RmlUiWindow::~RmlUiWindow() {
    destroy();
}

// ---------------------------------------------------------------------------
// WNDPROC
// ---------------------------------------------------------------------------
LRESULT CALLBACK RmlUiWindow::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    RmlUiWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<RmlUiWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<RmlUiWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self && self->processMessage(hwnd, msg, wp, lp))
        return 0;

    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// create()
// ---------------------------------------------------------------------------
bool RmlUiWindow::create(HINSTANCE hInst, const Config& cfg) {
    m_hInst = hInst;
    s_instance = this;

    // Register window class
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = wndProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = L"ZenithRmlUi";
        RegisterClassExW(&wc);
        registered = true;
    }

    // Centered window
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x  = (sw - m_width)  / 2;
    int y  = (sh - m_height) / 2;

    m_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_LAYERED,
        L"ZenithRmlUi", L"Zenith Settings",
        WS_POPUP | WS_VISIBLE,
        x, y, m_width, m_height,
        nullptr, nullptr, hInst, this);

    if (!m_hwnd) return false;

    // Semi-transparent background  (RmlUi draws on top)
    SetLayeredWindowAttributes(m_hwnd, 0, 245, LWA_ALPHA);

    if (!createDeviceAndSwapChain()) return false;

    // RmlUi context
    m_rmlCtx = Rml::CreateContext("main", Rml::Vector2i(m_width, m_height));
    if (!m_rmlCtx) return false;

    // Load the document
    std::string docPath = s_assetPath + "/settings.rml";
    m_doc = m_rmlCtx->LoadDocument(docPath);
    if (!m_doc) return false;

    // Register the event listener for element clicks handled in C++
    // (tab switching, hotkey binding, apply/discard are dispatched via
    //  RmlUi data events back to C++ event listeners we install below)
    installEventListeners();

    loadConfig(cfg);

    // Start hidden
    ShowWindow(m_hwnd, SW_HIDE);
    return true;
}

// ---------------------------------------------------------------------------
// DX11 Device
// ---------------------------------------------------------------------------
bool RmlUiWindow::createDeviceAndSwapChain() {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = (UINT)m_width;
    sd.BufferDesc.Height                  = (UINT)m_height;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = m_hwnd;
    sd.SampleDesc.Count                   = 1;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    D3D_FEATURE_LEVEL fl;
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, &m_swapChain, &m_device, &fl, &m_context);

    if (FAILED(hr)) return false;

    // Create RTV
    ID3D11Texture2D* backbuf = nullptr;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuf);
    m_device->CreateRenderTargetView(backbuf, nullptr, &m_rtv);
    backbuf->Release();

    // Hand the device to the RmlUi backend
    Backend::SetDevice(m_device, m_context);
    return true;
}

void RmlUiWindow::releaseDevice() {
    if (m_rtv)      { m_rtv->Release();      m_rtv      = nullptr; }
    if (m_swapChain){ m_swapChain->Release(); m_swapChain = nullptr; }
    if (m_context)  { m_context->Release();  m_context  = nullptr; }
    if (m_device)   { m_device->Release();   m_device   = nullptr; }
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------
void RmlUiWindow::render() {
    if (!m_device || !m_rmlCtx) return;

    float clear[4] = { 0.059f, 0.078f, 0.137f, 0.96f };
    m_context->ClearRenderTargetView(m_rtv, clear);
    m_context->OMSetRenderTargets(1, &m_rtv, nullptr);

    m_rmlCtx->Update();
    m_rmlCtx->Render();

    m_swapChain->Present(1, 0);
}

// ---------------------------------------------------------------------------
// resize
// ---------------------------------------------------------------------------
void RmlUiWindow::resize(int w, int h) {
    if (w == m_width && h == m_height) return;
    m_width = w; m_height = h;

    if (m_rtv) { m_rtv->Release(); m_rtv = nullptr; }
    m_swapChain->ResizeBuffers(0, (UINT)w, (UINT)h, DXGI_FORMAT_UNKNOWN, 0);
    ID3D11Texture2D* bb = nullptr;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb);
    m_device->CreateRenderTargetView(bb, nullptr, &m_rtv);
    bb->Release();

    if (m_rmlCtx) m_rmlCtx->SetDimensions(Rml::Vector2i(w, h));
}

// ---------------------------------------------------------------------------
// processMessage
// ---------------------------------------------------------------------------
bool RmlUiWindow::processMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // Feed mouse/keyboard to RmlUi
    if (m_rmlCtx) {
        switch (msg) {
        case WM_MOUSEMOVE:
            m_rmlCtx->ProcessMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp),
                                       m_rmlCtx->GetKeyModifierState());
            break;
        case WM_LBUTTONDOWN:
            m_rmlCtx->ProcessMouseButtonDown(0, m_rmlCtx->GetKeyModifierState());
            SetCapture(hwnd);
            break;
        case WM_LBUTTONUP:
            m_rmlCtx->ProcessMouseButtonUp(0, m_rmlCtx->GetKeyModifierState());
            ReleaseCapture();
            break;
        case WM_RBUTTONDOWN:
            m_rmlCtx->ProcessMouseButtonDown(1, m_rmlCtx->GetKeyModifierState());
            break;
        case WM_RBUTTONUP:
            m_rmlCtx->ProcessMouseButtonUp(1, m_rmlCtx->GetKeyModifierState());
            break;
        case WM_MOUSEWHEEL:
            m_rmlCtx->ProcessMouseWheel(
                -(float)GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA,
                m_rmlCtx->GetKeyModifierState());
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            int vk = (int)wp;
            if (m_isBinding) {
                // Collect keys; commit on non-modifier press
                bool isModifier = (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU ||
                                   vk == VK_LSHIFT || vk == VK_RSHIFT ||
                                   vk == VK_LCONTROL || vk == VK_RCONTROL ||
                                   vk == VK_LMENU || vk == VK_RMENU);
                if (!isModifier) {
                    m_pendingKeys.push_back(vk);
                    commitBinding();
                } else {
                    if (std::find(m_pendingKeys.begin(), m_pendingKeys.end(), vk)
                        == m_pendingKeys.end()) {
                        m_pendingKeys.push_back(vk);
                    }
                    // Show partial combo in the box
                    if (m_doc) {
                        auto* e = m_doc->GetElementById(m_bindingElementId);
                        if (e) e->SetInnerRML(vkeysToString(m_pendingKeys) + "...");
                    }
                }
                return true;
            }
            m_rmlCtx->ProcessKeyDown(Backend::ConvertKey(vk),
                                     m_rmlCtx->GetKeyModifierState());
            break;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:
            m_rmlCtx->ProcessKeyUp(Backend::ConvertKey((int)wp),
                                   m_rmlCtx->GetKeyModifierState());
            break;
        case WM_CHAR:
            m_rmlCtx->ProcessTextInput((Rml::Character)wp);
            break;
        case WM_SIZE:
            resize(LOWORD(lp), HIWORD(lp));
            break;
        case WM_CLOSE:
            hide();
            return true;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            render();
            EndPaint(hwnd, &ps);
            return true;
        }
        case WM_TIMER:
            render();
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Event listeners – C++ callbacks for HTML onclick attributes
// ---------------------------------------------------------------------------

// RmlUi event listener helper class
class ZenithEventListener : public Rml::EventListener {
public:
    std::function<void(Rml::Event&)> handler;
    void ProcessEvent(Rml::Event& ev) override { if (handler) handler(ev); }
};

void RmlUiWindow::installEventListeners() {
    if (!m_doc) return;

    // Tab buttons
    auto tabFn = [this](const std::string& panel) {
        return [this, panel](Rml::Event& ev) {
            // Hide all panels
            for (auto* id : {"panel-capture","panel-recording","panel-video",
                              "panel-overlays","panel-hotkeys"}) {
                auto* p = m_doc->GetElementById(id);
                if (p) p->SetClass("hidden", true);
            }
            for (auto* id : {"tab-capture","tab-recording","tab-video",
                              "tab-overlays","tab-hotkeys"}) {
                auto* t = m_doc->GetElementById(id);
                if (t) t->SetClass("active", false);
            }
            auto* panel = m_doc->GetElementById(panel);
            if (panel) panel->SetClass("hidden", false);
            std::string tabId = "tab-" + panel.substr(6); // strip "panel-"
            auto* tab = m_doc->GetElementById(tabId);
            if (tab) tab->SetClass("active", true);
        };
    };

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
            // Hide all panels / deactivate all tabs
            for (const char* id : {"panel-capture","panel-recording","panel-video",
                                    "panel-overlays","panel-hotkeys"}) {
                auto* p = m_doc->GetElementById(id);
                if (p) p->SetClass("hidden", true);
            }
            for (const char* id : {"tab-capture","tab-recording","tab-video",
                                    "tab-overlays","tab-hotkeys"}) {
                auto* t = m_doc->GetElementById(id);
                if (t) t->SetClass("active", false);
            }
            auto* panel = m_doc->GetElementById(panelId);
            if (panel) panel->SetClass("hidden", false);
            auto* tab = m_doc->GetElementById(tabId);
            if (tab) tab->SetClass("active", true);
        };
        btn->AddEventListener(Rml::EventId::Click, lis);
    }

    // Close button
    {
        auto* btn = m_doc->GetElementById("close-btn");
        if (btn) {
            auto* lis = new ZenithEventListener();
            lis->handler = [this](Rml::Event&) { hide(); };
            btn->AddEventListener(Rml::EventId::Click, lis);
        }
    }

    // Save & Apply
    {
        auto* btn = m_doc->GetElementById("footer-actions");
        auto* saveBtn = m_doc ? m_doc->GetElementById("footer-actions") : nullptr;
        // Find by iterating children or by class; use inline approach:
        if (m_doc) {
            Rml::ElementList btns;
            m_doc->GetElementsByClassName(btns, "btn-primary");
            for (auto* b : btns) {
                auto* lis = new ZenithEventListener();
                lis->handler = [this](Rml::Event&) {
                    if (onApplyConfig) onApplyConfig(readDataModel());
                    hide();
                };
                b->AddEventListener(Rml::EventId::Click, lis);
            }
        }
    }

    // Discard
    {
        if (m_doc) {
            Rml::ElementList btns;
            m_doc->GetElementsByClassName(btns, "btn-secondary");
            for (auto* b : btns) {
                auto* lis = new ZenithEventListener();
                lis->handler = [this](Rml::Event&) { hide(); };
                b->AddEventListener(Rml::EventId::Click, lis);
            }
        }
    }

    // Hotkey binding boxes
    struct HkDef { const char* id; };
    HkDef hks[] = {{"hk-clip-save"},{"hk-record-toggle"},{"hk-overlay-toggle"}};
    for (auto& hk : hks) {
        auto* e = m_doc->GetElementById(hk.id);
        if (!e) continue;
        auto* lis = new ZenithEventListener();
        std::string elemId = hk.id;
        lis->handler = [this, elemId](Rml::Event&) { startBinding(elemId); };
        e->AddEventListener(Rml::EventId::Click, lis);
    }
}

// ---------------------------------------------------------------------------
// show / hide
// ---------------------------------------------------------------------------
void RmlUiWindow::show() {
    if (!m_hwnd) return;
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    if (m_doc) m_doc->Show();
    // Drive rendering via a timer so we don't need a dedicated thread
    SetTimer(m_hwnd, 1, 16, nullptr); // ~60fps
}

void RmlUiWindow::hide() {
    if (!m_hwnd) return;
    KillTimer(m_hwnd, 1);
    if (m_doc) m_doc->Hide();
    ShowWindow(m_hwnd, SW_HIDE);
}

bool RmlUiWindow::isVisible() const {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

// ---------------------------------------------------------------------------
// destroy
// ---------------------------------------------------------------------------
void RmlUiWindow::destroy() {
    if (m_rmlCtx) {
        Rml::RemoveContext("main");
        m_rmlCtx = nullptr;
        m_doc    = nullptr;
    }
    releaseDevice();
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

// ---------------------------------------------------------------------------
// loadConfig  →  populate all UI fields
// ---------------------------------------------------------------------------
void RmlUiWindow::loadConfig(const Config& cfg) {
    if (!m_doc) return;

    setSelectIndex(m_doc, "capture-method",  (int)cfg.captureMethod);
    setVal(m_doc, "monitor-index",    std::to_string(cfg.monitorIndex));
    setVal(m_doc, "target-exe",       cfg.gameCaptureExe);

    setSelectIndex(m_doc, "recording-mode", (int)cfg.recordingMode);
    setVal(m_doc, "clip-duration",    std::to_string(cfg.clipDurationSecs));
    setVal(m_doc, "output-folder",    cfg.outputDirectory);

    setVal(m_doc, "video-width",      std::to_string(cfg.video.width));
    setVal(m_doc, "video-height",     std::to_string(cfg.video.height));
    setVal(m_doc, "video-fps",        std::to_string(cfg.video.fps));
    setVal(m_doc, "video-bitrate",    std::to_string(cfg.video.bitrateKbps));

    // Encoder select
    std::vector<std::string> encMap = {"auto","jim_nvenc","h264_texture_amf","obs_qsv11","obs_x264"};
    int encIdx = 0;
    for (int i = 0; i < (int)encMap.size(); ++i)
        if (encMap[i] == cfg.video.encoder) { encIdx = i; break; }
    setSelectIndex(m_doc, "video-encoder", encIdx);

    setVal(m_doc, "audio-bitrate",    std::to_string(cfg.audio.bitrateKbps));
    setChecked(m_doc, "audio-desktop", cfg.audio.captureDesktop);
    setChecked(m_doc, "audio-mic",     cfg.audio.captureMic);

    setChecked(m_doc, "camera-enabled", cfg.camera.enabled);
    setVal(m_doc, "camera-x",        std::to_string((int)cfg.camera.posX));
    setVal(m_doc, "camera-y",        std::to_string((int)cfg.camera.posY));
    setVal(m_doc, "camera-w",        std::to_string((int)cfg.camera.width));
    setVal(m_doc, "camera-h",        std::to_string((int)cfg.camera.height));
    setVal(m_doc, "camera-rot",      std::to_string((int)cfg.camera.rotation));
    setChecked(m_doc, "camera-fliph", cfg.camera.flipH);

    setChecked(m_doc, "key-enabled",  cfg.keyOverlay.enabled);
    setVal(m_doc, "key-list",         vkeysToKeyNames(cfg.keyOverlay.keys));
    setVal(m_doc, "key-size",         std::to_string((int)cfg.keyOverlay.keySize));

    setText(m_doc, "hk-clip-save",     vkeysToString(cfg.hotkeys.clipSave.vkeys));
    setText(m_doc, "hk-record-toggle", vkeysToString(cfg.hotkeys.recordToggle.vkeys));
    setText(m_doc, "hk-overlay-toggle",vkeysToString(cfg.hotkeys.overlayToggle.vkeys));

    // Store the hotkey vkeys in element attributes for readback
    // We'll use a tiny helper: store encoded vkey list as data-vkeys="90,88"
    auto encodeVkeys = [](const std::vector<int>& vkeys) {
        std::string s;
        for (size_t i = 0; i < vkeys.size(); ++i) {
            if (i) s += ",";
            s += std::to_string(vkeys[i]);
        }
        return s;
    };
    if (auto* e = m_doc->GetElementById("hk-clip-save"))
        e->SetAttribute("data-vkeys", encodeVkeys(cfg.hotkeys.clipSave.vkeys));
    if (auto* e = m_doc->GetElementById("hk-record-toggle"))
        e->SetAttribute("data-vkeys", encodeVkeys(cfg.hotkeys.recordToggle.vkeys));
    if (auto* e = m_doc->GetElementById("hk-overlay-toggle"))
        e->SetAttribute("data-vkeys", encodeVkeys(cfg.hotkeys.overlayToggle.vkeys));
}

// ---------------------------------------------------------------------------
// readDataModel  →  build Config from UI fields
// ---------------------------------------------------------------------------
static std::vector<int> decodeVkeys(const std::string& s) {
    std::vector<int> v;
    if (s.empty()) return v;
    std::istringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        int n = atoi(tok.c_str());
        if (n > 0) v.push_back(n);
    }
    return v;
}

Config RmlUiWindow::readDataModel() const {
    Config cfg;

    cfg.captureMethod  = (CaptureMethod)getSelectIndex(m_doc, "capture-method");
    cfg.monitorIndex   = atoi(getVal(m_doc, "monitor-index").c_str());
    cfg.gameCaptureExe = getVal(m_doc, "target-exe");

    cfg.recordingMode      = (RecordingMode)getSelectIndex(m_doc, "recording-mode");
    cfg.clipDurationSecs   = atoi(getVal(m_doc, "clip-duration").c_str());
    cfg.outputDirectory    = getVal(m_doc, "output-folder");

    cfg.video.width       = atoi(getVal(m_doc, "video-width").c_str());
    cfg.video.height      = atoi(getVal(m_doc, "video-height").c_str());
    cfg.video.fps         = atoi(getVal(m_doc, "video-fps").c_str());
    cfg.video.bitrateKbps = atoi(getVal(m_doc, "video-bitrate").c_str());

    std::vector<std::string> encMap = {"auto","jim_nvenc","h264_texture_amf","obs_qsv11","obs_x264"};
    int encIdx = getSelectIndex(m_doc, "video-encoder");
    cfg.video.encoder = (encIdx < (int)encMap.size()) ? encMap[encIdx] : "auto";

    cfg.audio.bitrateKbps    = atoi(getVal(m_doc, "audio-bitrate").c_str());
    cfg.audio.captureDesktop = getChecked(m_doc, "audio-desktop");
    cfg.audio.captureMic     = getChecked(m_doc, "audio-mic");

    cfg.camera.enabled  = getChecked(m_doc, "camera-enabled");
    cfg.camera.posX     = (float)atof(getVal(m_doc, "camera-x").c_str());
    cfg.camera.posY     = (float)atof(getVal(m_doc, "camera-y").c_str());
    cfg.camera.width    = (float)atof(getVal(m_doc, "camera-w").c_str());
    cfg.camera.height   = (float)atof(getVal(m_doc, "camera-h").c_str());
    cfg.camera.rotation = (float)atof(getVal(m_doc, "camera-rot").c_str());
    cfg.camera.flipH    = getChecked(m_doc, "camera-fliph");

    cfg.keyOverlay.enabled  = getChecked(m_doc, "key-enabled");
    cfg.keyOverlay.keys     = parseKeyNames(getVal(m_doc, "key-list"));
    cfg.keyOverlay.keySize  = (float)atof(getVal(m_doc, "key-size").c_str());

    auto readHk = [&](const std::string& id) -> HotkeyCombo {
        HotkeyCombo hk;
        auto* e = m_doc->GetElementById(id);
        if (e) hk.vkeys = decodeVkeys(e->GetAttribute<Rml::String>("data-vkeys", ""));
        return hk;
    };
    cfg.hotkeys.clipSave     = readHk("hk-clip-save");
    cfg.hotkeys.recordToggle = readHk("hk-record-toggle");
    cfg.hotkeys.overlayToggle= readHk("hk-overlay-toggle");

    return cfg;
}

// ---------------------------------------------------------------------------
// Hotkey binding
// ---------------------------------------------------------------------------
void RmlUiWindow::startBinding(const std::string& elementId) {
    // Cancel any existing binding first
    if (m_isBinding) {
        if (auto* prev = m_doc->GetElementById(m_bindingElementId)) {
            prev->SetClass("binding", false);
        }
    }

    m_bindingElementId = elementId;
    m_pendingKeys.clear();
    m_isBinding = true;

    auto* e = m_doc->GetElementById(elementId);
    if (e) {
        e->SetClass("binding", true);
        e->SetInnerRML("Press keys...");
    }
}

void RmlUiWindow::commitBinding() {
    m_isBinding = false;

    auto* e = m_doc->GetElementById(m_bindingElementId);
    if (e) {
        e->SetClass("binding", false);
        e->SetInnerRML(vkeysToString(m_pendingKeys));

        // Encode vkeys into the element attribute for readback
        std::string encoded;
        for (size_t i = 0; i < m_pendingKeys.size(); ++i) {
            if (i) encoded += ",";
            encoded += std::to_string(m_pendingKeys[i]);
        }
        e->SetAttribute("data-vkeys", encoded);
    }

    m_pendingKeys.clear();
    m_bindingElementId.clear();
}

// ---------------------------------------------------------------------------
// setStatus
// ---------------------------------------------------------------------------
void RmlUiWindow::setStatus(const std::string& statusClass, const std::string& statusText) {
    if (!m_doc) return;
    auto* e = m_doc->GetElementById("status-indicator");
    if (!e) return;

    // Remove all status classes, add the new one
    e->SetClass("status-idle",       statusClass == "status-idle");
    e->SetClass("status-buffering",  statusClass == "status-buffering");
    e->SetClass("status-recording",  statusClass == "status-recording");
    e->SetInnerRML(statusText);
}

} // namespace zenith
