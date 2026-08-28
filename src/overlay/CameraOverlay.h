#pragma once
#include "../config/Config.h"
#include <string>

// Forward-declare OBS types
struct obs_source;
struct obs_scene_item;
struct obs_scene;
typedef struct obs_source obs_source_t;
typedef struct obs_scene_item obs_sceneitem_t;
typedef struct obs_scene obs_scene_t;

namespace zenith {

// ---------------------------------------------------------------------------
// CameraOverlay
//
// Manages the OBS "video-capture-device-win" (DirectShow) source for a webcam.
// The camera is added as a scene item to the main OBS scene, composited
// server-side so it appears in the recording. No preview is rendered locally.
//
// It stores its transform (position, size, rotation, flip) which is applied
// directly to the OBS scene item.
// ---------------------------------------------------------------------------

class CameraOverlay {
public:
    CameraOverlay();
    ~CameraOverlay();

    // Not copyable
    CameraOverlay(const CameraOverlay&) = delete;
    CameraOverlay& operator=(const CameraOverlay&) = delete;

    // Initialize the camera source and add it to the given scene.
    bool init(obs_scene_t* scene, const CameraOverlayConfig& cfg);
    void shutdown();

    // Enable/disable visibility in the scene.
    void setEnabled(bool enabled);
    bool isEnabled() const;

    // Update settings (e.g. device changed or transform changed).
    // If deviceId changes, the source is recreated.
    void applyConfig(const CameraOverlayConfig& cfg);

private:
    void applyTransform();
    void recreateSource();

    obs_scene_t*        m_scene      = nullptr;
    obs_source_t*       m_camSource  = nullptr;
    obs_sceneitem_t*    m_sceneItem  = nullptr;

    CameraOverlayConfig m_cfg;
};

} // namespace zenith
