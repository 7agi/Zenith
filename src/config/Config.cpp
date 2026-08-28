#include "Config.h"

// nlohmann/json — fetched via CMake FetchContent, header-only
#include <nlohmann/json.hpp>

#include <fstream>
#include <filesystem>
#include <ShlObj.h>     // SHGetKnownFolderPath
#include <windows.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers – virtual key code  <->  string
// ---------------------------------------------------------------------------

static std::string vkToString(int vk) {
    char buf[64]{};
    UINT scanCode = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
    GetKeyNameTextA((LONG)(scanCode << 16), buf, sizeof(buf));
    if (buf[0]) return std::string(buf);
    // fallback
    return std::to_string(vk);
}

// ---------------------------------------------------------------------------
// JSON helpers for our types
// ---------------------------------------------------------------------------

static json comboToJson(const zenith::HotkeyCombo& c) {
    json arr = json::array();
    for (int vk : c.vkeys) arr.push_back(vk);
    return arr;
}

static zenith::HotkeyCombo comboFromJson(const json& j) {
    zenith::HotkeyCombo c;
    if (j.is_array()) {
        for (auto& v : j) c.vkeys.push_back(v.get<int>());
    }
    return c;
}

// ---------------------------------------------------------------------------
// Config path
// ---------------------------------------------------------------------------

std::string zenith::Config::configPath() {
    PWSTR wPath = nullptr;
    SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &wPath);
    fs::path base(wPath);
    CoTaskMemFree(wPath);
    base /= L"Zenith";
    fs::create_directories(base);
    return (base / L"config.json").string();
}

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------

zenith::Config zenith::Config::load() {
    Config cfg;
    std::string path = configPath();

    if (!fs::exists(path)) return cfg;  // use defaults

    std::ifstream f(path);
    if (!f.is_open()) return cfg;

    try {
        json j;
        f >> j;

        cfg.captureMethod    = static_cast<CaptureMethod>(j.value("captureMethod", 0));
        cfg.monitorIndex     = j.value("monitorIndex", 0);
        cfg.gameCaptureExe   = j.value("gameCaptureExe", "");
        cfg.recordingMode    = static_cast<RecordingMode>(j.value("recordingMode", 1));
        cfg.clipDurationSecs = j.value("clipDurationSecs", 30);
        cfg.outputDirectory  = j.value("outputDirectory", "");
        cfg.launchOnStartup  = j.value("launchOnStartup", false);
        cfg.showStatusDot    = j.value("showStatusDot", true);

        // Video
        if (j.contains("video")) {
            auto& v = j["video"];
            cfg.video.width       = v.value("width", 1920);
            cfg.video.height      = v.value("height", 1080);
            cfg.video.fps         = v.value("fps", 60);
            cfg.video.bitrateKbps = v.value("bitrateKbps", 8000);
            cfg.video.encoder     = v.value("encoder", "obs_x264");
        }

        // Audio
        if (j.contains("audio")) {
            auto& a = j["audio"];
            cfg.audio.bitrateKbps    = a.value("bitrateKbps", 160);
            cfg.audio.captureDesktop = a.value("captureDesktop", true);
            cfg.audio.captureMic     = a.value("captureMic", false);
            cfg.audio.micDeviceId    = a.value("micDeviceId", "");
        }

        // Hotkeys
        if (j.contains("hotkeys")) {
            auto& h = j["hotkeys"];
            cfg.hotkeys.clipSave      = comboFromJson(h.value("clipSave",     json::array()));
            cfg.hotkeys.recordToggle  = comboFromJson(h.value("recordToggle", json::array()));
            cfg.hotkeys.overlayToggle = comboFromJson(h.value("overlayToggle",json::array()));
        }
        // Sensible defaults if no hotkeys saved
        if (cfg.hotkeys.clipSave.vkeys.empty())
            cfg.hotkeys.clipSave.vkeys = {VK_F10};
        if (cfg.hotkeys.recordToggle.vkeys.empty())
            cfg.hotkeys.recordToggle.vkeys = {VK_F9};

        // Camera
        if (j.contains("camera")) {
            auto& c = j["camera"];
            cfg.camera.enabled  = c.value("enabled", false);
            cfg.camera.deviceId = c.value("deviceId", "");
            cfg.camera.posX     = c.value("posX", 20.0f);
            cfg.camera.posY     = c.value("posY", 20.0f);
            cfg.camera.width    = c.value("width", 320.0f);
            cfg.camera.height   = c.value("height", 240.0f);
            cfg.camera.rotation = c.value("rotation", 0.0f);
            cfg.camera.flipH    = c.value("flipH", false);
            cfg.camera.flipV    = c.value("flipV", false);
            cfg.camera.opacity  = c.value("opacity", 1.0f);
        }

        // Key overlay
        if (j.contains("keyOverlay")) {
            auto& k = j["keyOverlay"];
            cfg.keyOverlay.enabled     = k.value("enabled", true);
            cfg.keyOverlay.posX        = k.value("posX", 50.0f);
            cfg.keyOverlay.posY        = k.value("posY", 50.0f);
            cfg.keyOverlay.keySize     = k.value("keySize", 48.0f);
            cfg.keyOverlay.keySpacing  = k.value("keySpacing", 8.0f);
            cfg.keyOverlay.colorActive = k.value("colorActive", 0xFFFFFFFF);
            cfg.keyOverlay.colorIdle   = k.value("colorIdle",   0x80808080);
            cfg.keyOverlay.colorText   = k.value("colorText",   0xFF000000);
            cfg.keyOverlay.opacity     = k.value("opacity", 0.9f);
            cfg.keyOverlay.showLabels  = k.value("showLabels", true);
            if (k.contains("keys") && k["keys"].is_array()) {
                cfg.keyOverlay.keys.clear();
                for (auto& kv : k["keys"]) cfg.keyOverlay.keys.push_back(kv.get<int>());
            }
        }

    } catch (...) {
        // Return defaults on any parse error
        return Config{};
    }

    return cfg;
}

// ---------------------------------------------------------------------------
// Save
// ---------------------------------------------------------------------------

void zenith::Config::save() const {
    json j;
    j["captureMethod"]    = static_cast<int>(captureMethod);
    j["monitorIndex"]     = monitorIndex;
    j["gameCaptureExe"]   = gameCaptureExe;
    j["recordingMode"]    = static_cast<int>(recordingMode);
    j["clipDurationSecs"] = clipDurationSecs;
    j["outputDirectory"]  = outputDirectory;
    j["launchOnStartup"]  = launchOnStartup;
    j["showStatusDot"]    = showStatusDot;

    j["video"] = {
        {"width",       video.width},
        {"height",      video.height},
        {"fps",         video.fps},
        {"bitrateKbps", video.bitrateKbps},
        {"encoder",     video.encoder}
    };
    j["audio"] = {
        {"bitrateKbps",    audio.bitrateKbps},
        {"captureDesktop", audio.captureDesktop},
        {"captureMic",     audio.captureMic},
        {"micDeviceId",    audio.micDeviceId}
    };
    j["hotkeys"] = {
        {"clipSave",      comboToJson(hotkeys.clipSave)},
        {"recordToggle",  comboToJson(hotkeys.recordToggle)},
        {"overlayToggle", comboToJson(hotkeys.overlayToggle)}
    };
    j["camera"] = {
        {"enabled",  camera.enabled},
        {"deviceId", camera.deviceId},
        {"posX",     camera.posX},
        {"posY",     camera.posY},
        {"width",    camera.width},
        {"height",   camera.height},
        {"rotation", camera.rotation},
        {"flipH",    camera.flipH},
        {"flipV",    camera.flipV},
        {"opacity",  camera.opacity}
    };

    json kKeys = json::array();
    for (int k : keyOverlay.keys) kKeys.push_back(k);
    j["keyOverlay"] = {
        {"enabled",     keyOverlay.enabled},
        {"keys",        kKeys},
        {"posX",        keyOverlay.posX},
        {"posY",        keyOverlay.posY},
        {"keySize",     keyOverlay.keySize},
        {"keySpacing",  keyOverlay.keySpacing},
        {"colorActive", keyOverlay.colorActive},
        {"colorIdle",   keyOverlay.colorIdle},
        {"colorText",   keyOverlay.colorText},
        {"opacity",     keyOverlay.opacity},
        {"showLabels",  keyOverlay.showLabels}
    };

    std::ofstream f(configPath());
    f << j.dump(4);
}
