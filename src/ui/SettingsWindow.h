#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include "../config/Config.h"

namespace zenith {

// ---------------------------------------------------------------------------
// SettingsWindow
//
// A native Win32 dialog for configuring the application.
// ---------------------------------------------------------------------------

class SettingsWindow {
public:
    SettingsWindow();
    ~SettingsWindow();

    bool create(HINSTANCE hInst, HWND hParent, const Config& cfg);
    void destroy();

    void show();
    void hide();
    bool isVisible() const;

    // Called when the user clicks Save & Apply
    std::function<void(const Config&)> onApplyConfig;

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    
    void loadConfigToUI();
    void saveUIToConfig();

    HWND m_hwnd = nullptr;
    Config m_cfg;

    // UI Controls
    HWND m_cmbCaptureMethod = nullptr;
    HWND m_edtTargetExe     = nullptr;
    
    HWND m_cmbRecordingMode = nullptr;
    HWND m_edtClipDuration  = nullptr;
    HWND m_edtOutputFolder  = nullptr;
    
    HWND m_cmbVideoEncoder  = nullptr;
    HWND m_edtBitrate       = nullptr;
    
    HWND m_chkCamera        = nullptr;
    HWND m_chkKeyOverlay    = nullptr;
    
    HWND m_btnSave          = nullptr;
    HWND m_btnCancel        = nullptr;
    
    // Helpers
    HWND createLabel(int x, int y, int w, int h, const wchar_t* text);
    HWND createCombo(int x, int y, int w, int h, int id);
    HWND createEdit(int x, int y, int w, int h, int id);
    HWND createCheck(int x, int y, int w, int h, int id, const wchar_t* text);
};

} // namespace zenith
