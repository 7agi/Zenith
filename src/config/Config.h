#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace zenith {

// ---------------------------------------------------------------------------
//  Enums
// ---------------------------------------------------------------------------

enum class CaptureMethod {
    DXGI_DUPLICATION = 0,   // Monitor capture via DXGI (no yellow border)
    GAME_CAPTURE      = 1,   // Process hook (best for full-screen games)
    WGC               = 2,   // Windows Graphics Capture (may show border on Win11)
};

enum class RecordingMode {
    MANUAL_RECORD = 0,  // Start/stop hotkey – no circular buffer
    ALWAYS_ON     = 1,  // Circular buffer; clip hotkey exports last N min; can also full-record
    CLIP_ONLY     = 2,  // Circular buffer only; no full-record
    RECORD_ONLY   = 3,  // Manual start/stop; no clip functionality
};

// ---------------------------------------------------------------------------
//  Sub-structures
// ---------------------------------------------------------------------------

struct HotkeyCombo {
    std::vector<int> vkeys;  // Virtual key codes (e.g. {VK_F9})
};

struct HotkeyConfig {
    HotkeyCombo clipSave;       // Save clip (shadowplay-style)
    HotkeyCombo recordToggle;   // Start/stop manual recording
    HotkeyCombo overlayToggle;  // Show/hide key overlay
};

struct VideoConfig {
    int  width        = 1920;
    int  height       = 1080;
    int  fps          = 60;
    int  bitrateKbps  = 8000;
    std::string encoder = "auto";  // Will try NVENC -> AMF -> QSV -> x264
};

struct AudioConfig {
    int  bitrateKbps  = 160;
    bool captureDesktop = true;
    bool captureMic     = false;
    std::string micDeviceId;
};

struct CameraOverlayConfig {
    bool    enabled   = false;
    std::string deviceId;           // DirectShow device ID
    float   posX      = 20.0f;     // px from left
    float   posY      = 20.0f;     // px from top
    float   width     = 320.0f;
    float   height    = 240.0f;
    float   rotation  = 0.0f;      // degrees
    bool    flipH     = false;
    bool    flipV     = false;
    float   opacity   = 1.0f;
};

struct KeyOverlayConfig {
    bool                    enabled      = true;
    std::vector<int>        keys         = {90, 88};  // Z=90, X=88
    float                   posX         = 50.0f;
    float                   posY         = 50.0f;
    float                   keySize      = 48.0f;
    float                   keySpacing   = 8.0f;
    uint32_t                colorActive  = 0xFFFFFFFF; // ARGB
    uint32_t                colorIdle    = 0x80808080;
    uint32_t                colorText    = 0xFF000000;
    float                   opacity      = 0.9f;
    bool                    showLabels   = true;
};

// ---------------------------------------------------------------------------
//  Top-level Config
// ---------------------------------------------------------------------------

struct Config {
    // Capture
    CaptureMethod captureMethod = CaptureMethod::DXGI_DUPLICATION;
    int           monitorIndex  = 0;
    std::string   gameCaptureExe;   // e.g. "RiotClientServices.exe"

    // Recording
    RecordingMode  recordingMode    = RecordingMode::ALWAYS_ON;
    int            clipDurationSecs = 30;   // circular buffer length
    std::string    outputDirectory;         // empty = %USERPROFILE%\Videos\Zenith

    // Video / Audio
    VideoConfig video;
    AudioConfig audio;

    // Hotkeys
    HotkeyConfig hotkeys;

    // Overlays
    CameraOverlayConfig camera;
    KeyOverlayConfig    keyOverlay;

    // Misc
    bool launchOnStartup   = false;
    bool showStatusDot     = true;      // small floating REC indicator

    // ---------------------------------------------------------------------------
    //  Persistence
    // ---------------------------------------------------------------------------
    static Config load();
    void save() const;
    static std::string configPath();
};

} // namespace zenith
