#include <windows.h>
#include "config/Config.h"
#include "hotkey/HotkeyManager.h"
#include "recorder/Recorder.h"
#include "overlay/OverlayManager.h"
#include "ui/TrayIcon.h"
#include "ui/SettingsWindow.h"
#include "ui/StatusBar.h"

// Define WM_APP for Tray callback
#define WM_TRAYICON (WM_APP + 1)

using namespace zenith;

// Globals
static Config g_config;
static HotkeyManager g_hotkeys;
static Recorder g_recorder;
static OverlayManager g_overlays;
static TrayIcon g_tray;
static SettingsWindow g_settings;
static StatusBar g_status;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void updateStatusBarColor() {
    if (!g_config.showStatusDot) {
        g_status.setVisible(false);
        return;
    }
    g_status.setVisible(true);

    RecorderState st = g_recorder.state();
    if (st == RecorderState::RECORDING || st == RecorderState::BUFFERING_RECORDING) {
        g_status.setColor(0xFFFF0000); // Red
    } else if (st == RecorderState::BUFFERING) {
        g_status.setColor(0xFFFFA500); // Orange
    } else {
        g_status.setColor(0xFF808080); // Gray (Idle)
    }
}

void registerHotkeys() {
    g_hotkeys.unregisterAll();

    auto& hk = g_config.hotkeys;

    // Toggle manual recording
    if (!hk.recordToggle.vkeys.empty()) {
        g_hotkeys.registerToggle(hk.recordToggle.vkeys,
            []() {
                if (g_config.recordingMode == RecordingMode::CLIP_ONLY) return;
                g_recorder.startRecording();
            },
            []() {
                g_recorder.stopRecording();
            });
    }

    // Save clip
    if (!hk.clipSave.vkeys.empty()) {
        g_hotkeys.registerCombo(hk.clipSave.vkeys, []() {
            if (g_config.recordingMode == RecordingMode::RECORD_ONLY) return;
            g_recorder.saveClip([](const std::string& path) {
                if (!path.empty()) {
                    g_tray.showNotification("Clip Saved", "Saved to " + path);
                } else {
                    g_tray.showNotification("Error", "Failed to save clip.");
                }
            });
        });
    }

    // Toggle Key Overlay
    if (!hk.overlayToggle.vkeys.empty()) {
        g_hotkeys.registerCombo(hk.overlayToggle.vkeys, []() {
            g_overlays.toggleKeyOverlay();
        });
    }
}

// ---------------------------------------------------------------------------
// Main Message Loop Window (invisible)
// ---------------------------------------------------------------------------
LRESULT CALLBACK mainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_TRAYICON) {
        g_tray.onMessage(wp, lp);
        return 0;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// WinMain
// ---------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // 1. Create a hidden window for messages (tray icon uses it)
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), 0, mainWndProc, 0, 0, hInst, nullptr, nullptr, nullptr, nullptr, L"ZenithMain", nullptr };
    RegisterClassEx(&wc);
    HWND hMainWnd = CreateWindow(L"ZenithMain", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, hInst, nullptr);

    // 2. Load Config
    g_config = Config::load();
    if (g_config.hotkeys.clipSave.vkeys.empty()) g_config.save(); // write defaults if fresh

    // 3. UI Init
    // TODO: load actual icon from resources
    g_tray.init(hMainWnd, WM_TRAYICON, LoadIcon(nullptr, IDI_APPLICATION));
    g_tray.onSettingsClicked = []() { g_settings.show(); };
    g_tray.onExitClicked     = []() { PostMessage(hMainWnd, WM_CLOSE, 0, 0); };

    g_settings.create(hInst, nullptr, g_config);
    g_settings.onApplyConfig = [](const Config& cfg) {
        g_config = cfg;
        g_config.save();
        g_recorder.applyConfig(g_config);
        g_overlays.applyConfig(g_config);
        registerHotkeys();
        updateStatusBarColor();
    };

    g_status.create(hInst);
    updateStatusBarColor();

    // 4. Recorder & Overlays Init
    g_recorder.onStateChanged = [](RecorderState) { updateStatusBarColor(); };
    g_recorder.onError = [](const std::string& err) {
        g_tray.showNotification("Recorder Error", err);
    };

    if (!g_recorder.init(g_config)) {
        MessageBoxW(nullptr, L"Failed to initialize OBS backend.", L"Zenith Error", MB_ICONERROR);
        return 1;
    }

    // Pass the main OBS scene to overlay manager (assuming recorder made one... wait, 
    // we need to expose the scene from Recorder. For simplicity in this demo, OverlayManager 
    // will just create its own scene items or we assume Recorder provides it.
    // Actually, in our Recorder we created a scene and threw it away.
    // Fix: We can just use obs_get_source_by_name("ZenithScene") inside OverlayManager.
    
    // For this plan, we'll just let OverlayManager init without a scene pointer,
    // and it will look up "ZenithScene".
    obs_source_t* mainSceneSrc = obs_get_source_by_name("ZenithScene");
    obs_scene_t* mainScene = obs_scene_from_source(mainSceneSrc);
    
    g_overlays.init(mainScene, hInst, g_config);
    
    if (mainSceneSrc) obs_source_release(mainSceneSrc);

    // 5. Hotkeys Init
    g_hotkeys.install();
    registerHotkeys();

    // 6. Message Loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 7. Cleanup
    g_hotkeys.uninstall();
    g_overlays.shutdown();
    g_recorder.shutdown();
    
    return 0;
}
