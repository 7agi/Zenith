#include "OverlayManager.h"

namespace zenith {

OverlayManager::OverlayManager() {}

OverlayManager::~OverlayManager() {
    shutdown();
}

bool OverlayManager::init(obs_scene_t* scene, HINSTANCE hInst, const Config& cfg) {
    m_cfg = cfg;
    
    if (!m_camera.init(scene, cfg.camera)) {
        return false;
    }
    
    if (!m_keyOverlay.create(hInst, cfg.keyOverlay)) {
        return false;
    }
    
    // Save new position back to config when dragged
    m_keyOverlay.onPositionChanged = [this](float x, float y) {
        m_cfg.keyOverlay.posX = x;
        m_cfg.keyOverlay.posY = y;
        m_cfg.save(); // persist drag
    };
    
    return true;
}

void OverlayManager::shutdown() {
    m_camera.shutdown();
    m_keyOverlay.destroy();
}

void OverlayManager::applyConfig(const Config& cfg) {
    m_cfg = cfg;
    m_camera.applyConfig(cfg.camera);
    m_keyOverlay.applyConfig(cfg.keyOverlay);
    
    // Ensure visibility matches config state after apply
    if (cfg.keyOverlay.enabled != m_keyOverlay.isVisible()) {
        m_keyOverlay.setVisible(cfg.keyOverlay.enabled);
    }
}

void OverlayManager::toggleKeyOverlay() {
    m_cfg.keyOverlay.enabled = !m_cfg.keyOverlay.enabled;
    m_keyOverlay.setVisible(m_cfg.keyOverlay.enabled);
    m_cfg.save();
}

void OverlayManager::onKeyStateChanged(int vk, bool pressed) {
    m_keyOverlay.onKeyStateChanged(vk, pressed);
}

} // namespace zenith
