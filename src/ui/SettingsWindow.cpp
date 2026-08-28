#include "SettingsWindow.h"
#include <commctrl.h>
#include <string>

// Control IDs
#define ID_CMB_CAPTURE_METHOD 101
#define ID_EDT_TARGET_EXE     102
#define ID_CMB_REC_MODE       103
#define ID_EDT_CLIP_DUR       104
#define ID_EDT_OUT_FOLDER     105
#define ID_CMB_ENCODER        106
#define ID_EDT_BITRATE        107
#define ID_CHK_CAMERA         108
#define ID_CHK_KEYOVERLAY     109
#define ID_BTN_SAVE           110
#define ID_BTN_CANCEL         111

namespace zenith {

SettingsWindow::SettingsWindow() {}

SettingsWindow::~SettingsWindow() {
    destroy();
}

bool SettingsWindow::create(HINSTANCE hInst, HWND hParent, const Config& cfg) {
    m_cfg = cfg;

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = wndProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
        wc.lpszClassName = L"ZenithSettings";
        RegisterClassExW(&wc);
        registered = true;
    }

    m_hwnd = CreateWindowExW(
        0, L"ZenithSettings", L"Zenith Settings",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 460, 520,
        hParent, nullptr, hInst, this);

    if (!m_hwnd) return false;

    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    auto setFont = [&](HWND h) { SendMessageW(h, WM_SETFONT, (WPARAM)hFont, FALSE); };

    int y = 20;

    // --- Capture Settings ---
    setFont(createLabel(20, y, 150, 20, L"Capture Method:"));
    m_cmbCaptureMethod = createCombo(180, y, 240, 200, ID_CMB_CAPTURE_METHOD); setFont(m_cmbCaptureMethod);
    SendMessageW(m_cmbCaptureMethod, CB_ADDSTRING, 0, (LPARAM)L"DXGI Desktop Duplication");
    SendMessageW(m_cmbCaptureMethod, CB_ADDSTRING, 0, (LPARAM)L"Windows Graphics Capture");
    SendMessageW(m_cmbCaptureMethod, CB_ADDSTRING, 0, (LPARAM)L"Game Capture (Process)");
    y += 35;

    setFont(createLabel(20, y, 150, 20, L"Target EXE (Game Cap):"));
    m_edtTargetExe = createEdit(180, y, 240, 22, ID_EDT_TARGET_EXE); setFont(m_edtTargetExe);
    y += 45;

    // --- Recording Settings ---
    setFont(createLabel(20, y, 150, 20, L"Recording Mode:"));
    m_cmbRecordingMode = createCombo(180, y, 240, 200, ID_CMB_REC_MODE); setFont(m_cmbRecordingMode);
    SendMessageW(m_cmbRecordingMode, CB_ADDSTRING, 0, (LPARAM)L"Manual Record");
    SendMessageW(m_cmbRecordingMode, CB_ADDSTRING, 0, (LPARAM)L"Always On (Clip + Manual)");
    SendMessageW(m_cmbRecordingMode, CB_ADDSTRING, 0, (LPARAM)L"Clip Only");
    SendMessageW(m_cmbRecordingMode, CB_ADDSTRING, 0, (LPARAM)L"Record Only");
    y += 35;

    setFont(createLabel(20, y, 150, 20, L"Clip Duration (sec):"));
    m_edtClipDuration = createEdit(180, y, 100, 22, ID_EDT_CLIP_DUR); setFont(m_edtClipDuration);
    y += 35;

    setFont(createLabel(20, y, 150, 20, L"Output Folder:"));
    m_edtOutputFolder = createEdit(180, y, 240, 22, ID_EDT_OUT_FOLDER); setFont(m_edtOutputFolder);
    y += 45;

    // --- Video Settings ---
    setFont(createLabel(20, y, 150, 20, L"Video Encoder:"));
    m_cmbVideoEncoder = createCombo(180, y, 240, 200, ID_CMB_ENCODER); setFont(m_cmbVideoEncoder);
    SendMessageW(m_cmbVideoEncoder, CB_ADDSTRING, 0, (LPARAM)L"Auto (GPU Fallback)");
    SendMessageW(m_cmbVideoEncoder, CB_ADDSTRING, 0, (LPARAM)L"NVIDIA NVENC");
    SendMessageW(m_cmbVideoEncoder, CB_ADDSTRING, 0, (LPARAM)L"AMD AMF");
    SendMessageW(m_cmbVideoEncoder, CB_ADDSTRING, 0, (LPARAM)L"Intel QuickSync");
    SendMessageW(m_cmbVideoEncoder, CB_ADDSTRING, 0, (LPARAM)L"CPU (x264)");
    y += 35;

    setFont(createLabel(20, y, 150, 20, L"Bitrate (kbps):"));
    m_edtBitrate = createEdit(180, y, 100, 22, ID_EDT_BITRATE); setFont(m_edtBitrate);
    y += 45;

    // --- Overlays ---
    m_chkCamera = createCheck(180, y, 240, 20, ID_CHK_CAMERA, L"Enable Camera Overlay"); setFont(m_chkCamera);
    y += 30;

    m_chkKeyOverlay = createCheck(180, y, 240, 20, ID_CHK_KEYOVERLAY, L"Enable Key Overlay"); setFont(m_chkKeyOverlay);
    y += 45;

    // --- Buttons ---
    m_btnSave = CreateWindowW(L"BUTTON", L"Save && Apply", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        180, y, 110, 30, m_hwnd, (HMENU)ID_BTN_SAVE, hInst, nullptr); setFont(m_btnSave);

    m_btnCancel = CreateWindowW(L"BUTTON", L"Cancel", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        310, y, 110, 30, m_hwnd, (HMENU)ID_BTN_CANCEL, hInst, nullptr); setFont(m_btnCancel);

    loadConfigToUI();

    return true;
}

HWND SettingsWindow::createLabel(int x, int y, int w, int h, const wchar_t* text) {
    return CreateWindowW(L"STATIC", text, WS_VISIBLE | WS_CHILD, x, y, w, h, m_hwnd, nullptr, (HINSTANCE)GetWindowLongPtr(m_hwnd, GWLP_HINSTANCE), nullptr);
}

HWND SettingsWindow::createCombo(int x, int y, int w, int h, int id) {
    return CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, x, y, w, h, m_hwnd, (HMENU)(uintptr_t)id, (HINSTANCE)GetWindowLongPtr(m_hwnd, GWLP_HINSTANCE), nullptr);
}

HWND SettingsWindow::createEdit(int x, int y, int w, int h, int id) {
    return CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, x, y, w, h, m_hwnd, (HMENU)(uintptr_t)id, (HINSTANCE)GetWindowLongPtr(m_hwnd, GWLP_HINSTANCE), nullptr);
}

HWND SettingsWindow::createCheck(int x, int y, int w, int h, int id, const wchar_t* text) {
    return CreateWindowW(L"BUTTON", text, WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, x, y, w, h, m_hwnd, (HMENU)(uintptr_t)id, (HINSTANCE)GetWindowLongPtr(m_hwnd, GWLP_HINSTANCE), nullptr);
}

void SettingsWindow::destroy() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void SettingsWindow::show() {
    if (m_hwnd) {
        loadConfigToUI(); // reload in case edited elsewhere
        ShowWindow(m_hwnd, SW_SHOW);
        SetForegroundWindow(m_hwnd);
    }
}

void SettingsWindow::hide() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

bool SettingsWindow::isVisible() const {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

void SettingsWindow::loadConfigToUI() {
    // Capture Method
    int captureIdx = 0;
    if (m_cfg.captureMethod == CaptureMethod::WGC) captureIdx = 1;
    else if (m_cfg.captureMethod == CaptureMethod::GAME_CAPTURE) captureIdx = 2;
    SendMessageW(m_cmbCaptureMethod, CB_SETCURSEL, captureIdx, 0);

    // Target EXE
    std::wstring wExe(m_cfg.gameCaptureExe.begin(), m_cfg.gameCaptureExe.end());
    SetWindowTextW(m_edtTargetExe, wExe.c_str());

    // Recording Mode
    int modeIdx = (int)m_cfg.recordingMode;
    SendMessageW(m_cmbRecordingMode, CB_SETCURSEL, modeIdx, 0);

    // Clip Duration
    std::wstring wDur = std::to_wstring(m_cfg.clipDurationSecs);
    SetWindowTextW(m_edtClipDuration, wDur.c_str());

    // Output Folder
    std::wstring wOut(m_cfg.outputDirectory.begin(), m_cfg.outputDirectory.end());
    SetWindowTextW(m_edtOutputFolder, wOut.c_str());

    // Encoder
    int encIdx = 0;
    if (m_cfg.video.encoder == "jim_nvenc") encIdx = 1;
    else if (m_cfg.video.encoder == "h264_texture_amf") encIdx = 2;
    else if (m_cfg.video.encoder == "obs_qsv11") encIdx = 3;
    else if (m_cfg.video.encoder == "obs_x264") encIdx = 4;
    SendMessageW(m_cmbVideoEncoder, CB_SETCURSEL, encIdx, 0);

    // Bitrate
    std::wstring wBitrate = std::to_wstring(m_cfg.video.bitrateKbps);
    SetWindowTextW(m_edtBitrate, wBitrate.c_str());

    // Overlays
    SendMessageW(m_chkCamera, BM_SETCHECK, m_cfg.camera.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkKeyOverlay, BM_SETCHECK, m_cfg.keyOverlay.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
}

void SettingsWindow::saveUIToConfig() {
    // Capture Method
    int capIdx = (int)SendMessageW(m_cmbCaptureMethod, CB_GETCURSEL, 0, 0);
    if (capIdx == 1) m_cfg.captureMethod = CaptureMethod::WGC;
    else if (capIdx == 2) m_cfg.captureMethod = CaptureMethod::GAME_CAPTURE;
    else m_cfg.captureMethod = CaptureMethod::DXGI_DUPLICATION;

    // Target EXE
    wchar_t buf[MAX_PATH];
    GetWindowTextW(m_edtTargetExe, buf, MAX_PATH);
    std::wstring wExe(buf);
    m_cfg.gameCaptureExe = std::string(wExe.begin(), wExe.end());

    // Recording Mode
    int modeIdx = (int)SendMessageW(m_cmbRecordingMode, CB_GETCURSEL, 0, 0);
    m_cfg.recordingMode = static_cast<RecordingMode>(modeIdx);

    // Clip Duration
    GetWindowTextW(m_edtClipDuration, buf, MAX_PATH);
    m_cfg.clipDurationSecs = _wtoi(buf);

    // Output Folder
    GetWindowTextW(m_edtOutputFolder, buf, MAX_PATH);
    std::wstring wOut(buf);
    m_cfg.outputDirectory = std::string(wOut.begin(), wOut.end());

    // Encoder
    int encIdx = (int)SendMessageW(m_cmbVideoEncoder, CB_GETCURSEL, 0, 0);
    if (encIdx == 1) m_cfg.video.encoder = "jim_nvenc";
    else if (encIdx == 2) m_cfg.video.encoder = "h264_texture_amf";
    else if (encIdx == 3) m_cfg.video.encoder = "obs_qsv11";
    else if (encIdx == 4) m_cfg.video.encoder = "obs_x264";
    else m_cfg.video.encoder = "auto";

    // Bitrate
    GetWindowTextW(m_edtBitrate, buf, MAX_PATH);
    m_cfg.video.bitrateKbps = _wtoi(buf);

    // Overlays
    m_cfg.camera.enabled = (SendMessageW(m_chkCamera, BM_GETCHECK, 0, 0) == BST_CHECKED);
    m_cfg.keyOverlay.enabled = (SendMessageW(m_chkKeyOverlay, BM_GETCHECK, 0, 0) == BST_CHECKED);
}

LRESULT CALLBACK SettingsWindow::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    SettingsWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<SettingsWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        switch (msg) {
            case WM_COMMAND: {
                int id = LOWORD(wp);
                if (id == ID_BTN_SAVE) {
                    self->saveUIToConfig();
                    if (self->onApplyConfig) {
                        self->onApplyConfig(self->m_cfg);
                    }
                    self->hide();
                } else if (id == ID_BTN_CANCEL) {
                    self->hide();
                }
                return 0;
            }
            case WM_CTLCOLORSTATIC: {
                // Make static labels transparent background so they match the window
                HDC hdc = (HDC)wp;
                SetBkMode(hdc, TRANSPARENT);
                return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
            }
            case WM_CLOSE:
                self->hide();
                return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace zenith
