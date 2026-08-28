#include "CameraOverlay.h"
#include <obs.h>

namespace zenith {

CameraOverlay::CameraOverlay() {}

CameraOverlay::~CameraOverlay() {
    shutdown();
}

bool CameraOverlay::init(obs_scene_t* scene, const CameraOverlayConfig& cfg) {
    m_scene = scene;
    m_cfg   = cfg;

    if (m_cfg.enabled && !m_cfg.deviceId.empty()) {
        recreateSource();
    }
    return true;
}

void CameraOverlay::shutdown() {
    if (m_sceneItem) {
        // We do not release scene items, OBS manages them, but we remove it from the scene
        obs_sceneitem_remove(m_sceneItem);
        m_sceneItem = nullptr;
    }
    if (m_camSource) {
        obs_source_release(m_camSource);
        m_camSource = nullptr;
    }
}

void CameraOverlay::setEnabled(bool enabled) {
    m_cfg.enabled = enabled;
    if (m_sceneItem) {
        obs_sceneitem_set_visible(m_sceneItem, enabled);
    } else if (enabled && !m_cfg.deviceId.empty()) {
        recreateSource();
    }
}

bool CameraOverlay::isEnabled() const {
    return m_cfg.enabled;
}

void CameraOverlay::applyConfig(const CameraOverlayConfig& cfg) {
    bool deviceChanged = (cfg.deviceId != m_cfg.deviceId);
    m_cfg = cfg;

    if (deviceChanged) {
        if (m_cfg.enabled && !m_cfg.deviceId.empty()) {
            recreateSource();
        } else {
            shutdown();
        }
    } else {
        setEnabled(m_cfg.enabled);
        if (m_sceneItem) {
            applyTransform();
        }
    }
}

void CameraOverlay::recreateSource() {
    if (m_sceneItem) {
        obs_sceneitem_remove(m_sceneItem);
        m_sceneItem = nullptr;
    }
    if (m_camSource) {
        obs_source_release(m_camSource);
        m_camSource = nullptr;
    }

    if (m_cfg.deviceId.empty()) return;

    obs_data_t* settings = obs_data_create();
    obs_data_set_string(settings, "video_device_id", m_cfg.deviceId.c_str());

    // Use dshow input (DirectShow) for webcams on Windows
    m_camSource = obs_source_create("dshow_input", "ZenithCamera", settings, nullptr);
    obs_data_release(settings);

    if (m_camSource) {
        m_sceneItem = obs_scene_add(m_scene, m_camSource);
        obs_sceneitem_set_visible(m_sceneItem, m_cfg.enabled);
        applyTransform();
    }
}

void CameraOverlay::applyTransform() {
    if (!m_sceneItem) return;

    // Position
    struct vec2 pos;
    vec2_set(&pos, m_cfg.posX, m_cfg.posY);
    obs_sceneitem_set_pos(m_sceneItem, &pos);

    // Scale/Size - OBS scales based on base source resolution, so we need to calculate scale
    uint32_t cx = obs_source_get_width(m_camSource);
    uint32_t cy = obs_source_get_height(m_camSource);

    struct vec2 scale;
    vec2_set(&scale, 1.0f, 1.0f);
    
    if (cx > 0 && cy > 0) {
        scale.x = m_cfg.width / cx;
        scale.y = m_cfg.height / cy;
    }
    
    if (m_cfg.flipH) scale.x = -scale.x;
    if (m_cfg.flipV) scale.y = -scale.y;
    
    obs_sceneitem_set_scale(m_sceneItem, &scale);

    // Rotation
    obs_sceneitem_set_rot(m_sceneItem, m_cfg.rotation);
}

} // namespace zenith
