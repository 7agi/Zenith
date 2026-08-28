#pragma once
#include "../config/Config.h"
#include "CameraOverlay.h"
#include "KeyOverlay.h"
#include <windows.h>
#include <memory>

// Forward-declare OBS types
struct obs_scene;
typedef struct obs_scene obs_scene_t;

namespace zenith {

// ---------------------------------------------------------------------------
// OverlayManager
//
// Orchestrates all overlays. It holds the CameraOverlay (OBS source) and the
// KeyOverlayWindow (Win32 window).
// ---------------------------------------------------------------------------

class OverlayManager {
public:
    OverlayManager();
    ~OverlayManager();

    // Not copyable
    OverlayManager(const OverlayManager&) = delete;
    OverlayManager& operator=(const OverlayManager&) = delete;

    // Initialize all overlays.
    // scene: the main OBS scene where camera should be composited.
    // hInst: application instance (for window creation).
    bool init(obs_scene_t* scene, HINSTANCE hInst, const Config& cfg);
    void shutdown();

    // Apply new config to all overlays.
    void applyConfig(const Config& cfg);

    // Toggle key overlay visibility (from hotkey).
    void toggleKeyOverlay();
    
    // Notify key overlay of state changes.
    void onKeyStateChanged(int vk, bool pressed);

private:
    CameraOverlay m_camera;
    KeyOverlayWindow m_keyOverlay;
    
    Config m_cfg;
};

} // namespace zenith
