#include "Recorder.h"

// libobs public API
#include <obs.h>
#include <obs-module.h>
#include <media-io/video-io.h>
#include <media-io/audio-io.h>
#include <util/dstr.h>

#include <thread>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;
using namespace zenith;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

Recorder::Recorder()
    : m_circBuf(30)
    , m_clipSaver("") {}

Recorder::~Recorder() {
    shutdown();
}

bool Recorder::init(const Config& cfg) {
    m_cfg = cfg;

    // ------------------------------------------------------------------
    // 1. Start OBS core
    // ------------------------------------------------------------------
    if (!obs_startup("en-US", nullptr, nullptr)) {
        if (onError) onError("obs_startup failed");
        return false;
    }

    // Load plugins bundled with our OBS build
    obs_load_all_modules();
    obs_log_loaded_modules();

    // ------------------------------------------------------------------
    // 2. Video / Audio settings
    // ------------------------------------------------------------------
    if (!setupVideo(cfg) || !setupAudio(cfg)) return false;

    // ------------------------------------------------------------------
    // 3. Scene + capture source
    // ------------------------------------------------------------------
    if (!setupScene(cfg)) return false;

    // ------------------------------------------------------------------
    // 4. Encoders + outputs
    // ------------------------------------------------------------------
    if (!setupEncoders(cfg)) return false;
    if (!setupOutput(cfg))   return false;

    m_circBuf.setMaxDuration(cfg.clipDurationSecs);
    m_clipSaver.setOutputDir(cfg.outputDirectory);

    m_obsInited = true;

    // Auto-start buffer if mode requires it
    if (cfg.recordingMode == RecordingMode::ALWAYS_ON ||
        cfg.recordingMode == RecordingMode::CLIP_ONLY) {
        startBuffer();
    }

    return true;
}

void Recorder::shutdown() {
    if (!m_obsInited) return;

    stopRecording();
    stopBuffer();

    obs_output_release(m_fileOutput);
    obs_encoder_release(m_videoEncoder);
    obs_encoder_release(m_audioEncoder);
    obs_source_release(m_captureSource);
    obs_source_release(m_audioDesktop);
    if (m_audioMic) obs_source_release(m_audioMic);

    obs_shutdown();
    m_obsInited = false;
}

// ---------------------------------------------------------------------------
// Setup helpers
// ---------------------------------------------------------------------------

bool Recorder::setupVideo(const Config& cfg) {
    obs_video_info ovi{};
    ovi.graphics_module = "libobs-d3d11";
    ovi.fps_num         = (uint32_t)cfg.video.fps;
    ovi.fps_den         = 1;
    ovi.base_width      = (uint32_t)cfg.video.width;
    ovi.base_height     = (uint32_t)cfg.video.height;
    ovi.output_width    = (uint32_t)cfg.video.width;
    ovi.output_height   = (uint32_t)cfg.video.height;
    ovi.output_format   = VIDEO_FORMAT_NV12;
    ovi.colorspace      = VIDEO_CS_709;
    ovi.range           = VIDEO_RANGE_PARTIAL;
    ovi.adapter         = 0;
    ovi.gpu_conversion  = true;
    ovi.scale_type      = OBS_SCALE_BICUBIC;

    int ret = obs_reset_video(&ovi);
    if (ret != OBS_VIDEO_SUCCESS) {
        if (onError) onError("obs_reset_video failed: " + std::to_string(ret));
        return false;
    }
    return true;
}

bool Recorder::setupAudio(const Config& cfg) {
    obs_audio_info oai{};
    oai.samples_per_sec = 48000;
    oai.speakers        = SPEAKERS_STEREO;
    return obs_reset_audio(&oai);
}

bool Recorder::setupScene(const Config& cfg) {
    // ---- Capture source ----
    obs_data_t* captureSettings = obs_data_create();

    const char* sourceId = "monitor_capture";
    switch (cfg.captureMethod) {
        case CaptureMethod::DXGI_DUPLICATION:
            // monitor_capture with method=DXGI
            obs_data_set_string(captureSettings, "method", "dxgi");
            obs_data_set_int(captureSettings, "monitor", cfg.monitorIndex);
            break;
        case CaptureMethod::WGC:
            obs_data_set_string(captureSettings, "method", "wgc");
            obs_data_set_int(captureSettings, "monitor", cfg.monitorIndex);
            break;
        case CaptureMethod::GAME_CAPTURE:
            sourceId = "game_capture";
            obs_data_set_string(captureSettings, "capture_mode", "any_fullscreen");
            if (!cfg.gameCaptureExe.empty()) {
                obs_data_set_string(captureSettings, "capture_mode", "window");
                obs_data_set_string(captureSettings, "window", cfg.gameCaptureExe.c_str());
            }
            break;
    }

    m_captureSource = obs_source_create(sourceId, "ZenithCapture", captureSettings, nullptr);
    obs_data_release(captureSettings);

    if (!m_captureSource) {
        if (onError) onError("Failed to create capture source");
        return false;
    }

    // Create a simple scene and add the capture source
    obs_scene_t* scene = obs_scene_create("ZenithScene");
    obs_scene_add(scene, m_captureSource);
    obs_set_output_source(0, obs_scene_get_source(scene));
    obs_scene_release(scene);

    // ---- Desktop audio ----
    if (cfg.audio.captureDesktop) {
        obs_data_t* aSettings = obs_data_create();
        m_audioDesktop = obs_source_create("wasapi_output_capture",
                                            "ZenithDesktopAudio",
                                            aSettings, nullptr);
        obs_data_release(aSettings);
        if (m_audioDesktop) obs_set_output_source(1, m_audioDesktop);
    }

    // ---- Mic ----
    if (cfg.audio.captureMic && !cfg.audio.micDeviceId.empty()) {
        obs_data_t* mSettings = obs_data_create();
        obs_data_set_string(mSettings, "device_id", cfg.audio.micDeviceId.c_str());
        m_audioMic = obs_source_create("wasapi_input_capture",
                                        "ZenithMic", mSettings, nullptr);
        obs_data_release(mSettings);
        if (m_audioMic) obs_set_output_source(2, m_audioMic);
    }

    return true;
}

bool Recorder::setupEncoders(const Config& cfg) {
    // Video encoder
    obs_data_t* vSettings = obs_data_create();
    obs_data_set_int(vSettings, "bitrate", cfg.video.bitrateKbps);
    obs_data_set_string(vSettings, "rate_control", "CBR");
    obs_data_set_string(vSettings, "profile", "high");
    obs_data_set_string(vSettings, "preset", "veryfast");

    m_videoEncoder = obs_video_encoder_create(cfg.video.encoder.c_str(),
                                               "ZenithVideoEncoder",
                                               vSettings, nullptr);

    // Hardware encoder fallback chain if the requested one fails
    if (!m_videoEncoder || cfg.video.encoder == "auto") {
        const char* fallbacks[] = {
            "jim_nvenc",         // NVIDIA H.264
            "h264_texture_amf",  // AMD H.264
            "obs_qsv11",         // Intel QuickSync H.264
            "obs_x264"           // CPU software fallback
        };
        
        for (const char* encName : fallbacks) {
            m_videoEncoder = obs_video_encoder_create(encName, "ZenithVideoEncoder", vSettings, nullptr);
            if (m_videoEncoder) {
                break; // Found a working encoder
            }
        }
    }

    obs_data_release(vSettings);
    if (!m_videoEncoder) {
        if (onError) onError("Failed to create video encoder: " + cfg.video.encoder);
        return false;
    }
    obs_encoder_set_video(m_videoEncoder, obs_get_video());

    // Audio encoder
    obs_data_t* aSettings = obs_data_create();
    obs_data_set_int(aSettings, "bitrate", cfg.audio.bitrateKbps);

    m_audioEncoder = obs_audio_encoder_create("ffmpeg_aac",
                                               "ZenithAudioEncoder",
                                               aSettings, 0, nullptr);
    obs_data_release(aSettings);
    if (!m_audioEncoder) {
        if (onError) onError("Failed to create audio encoder");
        return false;
    }
    obs_encoder_set_audio(m_audioEncoder, obs_get_audio());

    return true;
}

bool Recorder::setupOutput(const Config& cfg) {
    obs_data_t* settings = obs_data_create();
    std::string path = buildOutputPath();
    obs_data_set_string(settings, "path", path.c_str());

    m_fileOutput = obs_output_create("ffmpeg_muxer", "ZenithFileOutput", settings, nullptr);
    obs_data_release(settings);

    if (!m_fileOutput) {
        if (onError) onError("Failed to create file output");
        return false;
    }

    obs_output_set_video_encoder(m_fileOutput, m_videoEncoder);
    obs_output_set_audio_encoder(m_fileOutput, m_audioEncoder, 0);
    return true;
}

std::string Recorder::buildOutputPath() const {
    fs::path dir(m_cfg.outputDirectory);
    if (dir.empty()) {
        wchar_t* uh = nullptr;
        _wdupenv_s(&uh, nullptr, L"USERPROFILE");
        if (uh) { dir = fs::path(uh) / L"Videos" / L"Zenith"; free(uh); }
        else dir = "Zenith";
    }
    fs::create_directories(dir);

    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::ostringstream ss;
    ss << "Zenith_" << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S") << ".mp4";
    return (dir / ss.str()).string();
}

// ---------------------------------------------------------------------------
// Buffer control
// ---------------------------------------------------------------------------

void Recorder::startBuffer() {
    if (m_bufferActive) return;
    m_bufferActive = true;
    m_circBuf.clear();

    // The circular buffer is populated by the encoded packet callback.
    // We start the video/audio encoders (they need to be running).
    // Note: OBS encoders start when any output using them starts.
    // We use a replay buffer output to drive the encoder.

    obs_data_t* settings = obs_data_create();
    m_bufOutput = obs_output_create("replay_buffer", "ZenithReplayBuffer", settings, nullptr);
    obs_data_release(settings);

    if (m_bufOutput) {
        obs_output_set_video_encoder(m_bufOutput, m_videoEncoder);
        obs_output_set_audio_encoder(m_bufOutput, m_audioEncoder, 0);
        obs_output_start(m_bufOutput);
    }

    setState(RecorderState::BUFFERING);
}

void Recorder::stopBuffer() {
    if (!m_bufferActive) return;
    m_bufferActive = false;

    if (m_bufOutput) {
        obs_output_stop(m_bufOutput);
        obs_output_release(m_bufOutput);
        m_bufOutput = nullptr;
    }

    if (!m_recordActive) setState(RecorderState::IDLE);
}

void Recorder::saveClip(std::function<void(const std::string&)> callback) {
    // Drain from circular buffer on a background thread
    auto packets = m_circBuf.drain(m_cfg.clipDurationSecs);
    auto cfg     = m_cfg;
    auto saver   = &m_clipSaver;
    auto cb      = callback ? callback : onClipSaved;

    std::thread([packets = std::move(packets), cfg, saver, cb]() {
        std::string path = saver->save(packets,
            cfg.video.width, cfg.video.height, cfg.video.fps);
        if (cb) cb(path);
    }).detach();
}

// ---------------------------------------------------------------------------
// Full recording
// ---------------------------------------------------------------------------

void Recorder::startRecording() {
    if (m_recordActive) return;

    // Update the output path with current timestamp
    obs_data_t* settings = obs_output_get_settings(m_fileOutput);
    obs_data_set_string(settings, "path", buildOutputPath().c_str());
    obs_output_update(m_fileOutput, settings);
    obs_data_release(settings);

    m_recordActive = true;
    obs_output_start(m_fileOutput);
    setState(m_bufferActive ? RecorderState::BUFFERING_RECORDING : RecorderState::RECORDING);
}

void Recorder::stopRecording() {
    if (!m_recordActive) return;
    m_recordActive = false;
    obs_output_stop(m_fileOutput);
    setState(m_bufferActive ? RecorderState::BUFFERING : RecorderState::IDLE);
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

void Recorder::setState(RecorderState s) {
    m_state = s;
    if (onStateChanged) onStateChanged(s);
}

// ---------------------------------------------------------------------------
// Config apply (hot reload)
// ---------------------------------------------------------------------------

void Recorder::applyConfig(const Config& cfg) {
    m_cfg = cfg;
    m_circBuf.setMaxDuration(cfg.clipDurationSecs);
    m_clipSaver.setOutputDir(cfg.outputDirectory);
    // Full reinit needed for encoder/source changes — for now restart
}

void Recorder::setCameraEnabled(bool) { /* delegated to OverlayManager */ }
void Recorder::setKeyOverlayEnabled(bool) { /* delegated to OverlayManager */ }
