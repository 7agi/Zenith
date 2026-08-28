#pragma once
#include "../config/Config.h"
#include "CircularBuffer.h"
#include "ClipSaver.h"

// Forward-declare OBS types to avoid pulling in all OBS headers here.
struct obs_output;
struct obs_source;
struct obs_encoder;
struct obs_service;
typedef struct obs_output  obs_output_t;
typedef struct obs_source  obs_source_t;
typedef struct obs_encoder obs_encoder_t;
typedef struct obs_service obs_service_t;

#include <string>
#include <atomic>
#include <functional>
#include <thread>
#include <memory>

namespace zenith {

// ---------------------------------------------------------------------------
// Recorder
//
// Owns the libobs scene, sources, encoders and outputs.  Provides a simple
// high-level API:
//
//   recorder.init(cfg)         – call once at startup
//   recorder.startBuffer()     – start circular buffer (ALWAYS_ON / CLIP_ONLY)
//   recorder.saveClip()        – flush buffer → MP4 (async)
//   recorder.startRecording()  – begin full file recording
//   recorder.stopRecording()   – stop and finalize
//   recorder.shutdown()        – tear down OBS
// ---------------------------------------------------------------------------

enum class RecorderState {
    IDLE,
    BUFFERING,          // circular buffer running, not recording to file
    RECORDING,          // full recording in progress
    BUFFERING_RECORDING // both simultaneously (ALWAYS_ON mode)
};

class Recorder {
public:
    Recorder();
    ~Recorder();

    // Not copyable
    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------
    bool init(const Config& cfg);
    void shutdown();

    // -----------------------------------------------------------------------
    // Apply a new config (updates sources/encoders without full restart
    // where possible).
    // -----------------------------------------------------------------------
    void applyConfig(const Config& cfg);

    // -----------------------------------------------------------------------
    // Buffer / Clip
    // -----------------------------------------------------------------------
    void startBuffer();
    void stopBuffer();
    // Async: saves clip on a background thread; calls `callback(path)` when done.
    void saveClip(std::function<void(const std::string&)> callback = nullptr);

    // -----------------------------------------------------------------------
    // Full recording
    // -----------------------------------------------------------------------
    void startRecording();
    void stopRecording();

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    RecorderState state() const { return m_state.load(); }
    bool isBuffering()  const { return m_bufferActive.load(); }
    bool isRecording()  const { return m_recordActive.load(); }
    double bufferedSecs() const { return m_circBuf.bufferedDuration(); }

    // -----------------------------------------------------------------------
    // Camera / Key overlay – delegates to OverlayManager but controlled here
    // -----------------------------------------------------------------------
    void setCameraEnabled(bool enabled);
    void setKeyOverlayEnabled(bool enabled);

    // -----------------------------------------------------------------------
    // Status callbacks
    // -----------------------------------------------------------------------
    std::function<void(RecorderState)> onStateChanged;
    std::function<void(const std::string& path)> onClipSaved;
    std::function<void(const std::string& err)>  onError;

private:
    // OBS initialisation helpers
    bool setupVideo(const Config& cfg);
    bool setupAudio(const Config& cfg);
    bool setupScene(const Config& cfg);
    bool setupEncoders(const Config& cfg);
    bool setupOutput(const Config& cfg);

    // OBS output raw callback (called by OBS encoder thread)
    static void encodedPacketCallback(void* param,
                                       struct encoder_packet* packet,
                                       bool* received_packet);

    void setState(RecorderState s);
    std::string buildOutputPath() const;

    // OBS objects (raw pointers – managed by obs_* reference counting)
    obs_source_t*  m_captureSource  = nullptr;
    obs_source_t*  m_audioDesktop   = nullptr;
    obs_source_t*  m_audioMic       = nullptr;
    obs_encoder_t* m_videoEncoder   = nullptr;
    obs_encoder_t* m_audioEncoder   = nullptr;
    obs_output_t*  m_fileOutput     = nullptr;
    obs_output_t*  m_bufOutput      = nullptr;   // custom output feeding CircularBuffer

    // Circular buffer
    CircularBuffer m_circBuf;
    ClipSaver      m_clipSaver;

    // State
    std::atomic<RecorderState> m_state{RecorderState::IDLE};
    std::atomic<bool>          m_bufferActive{false};
    std::atomic<bool>          m_recordActive{false};

    // Saved config
    Config m_cfg;

    bool m_obsInited = false;
};

} // namespace zenith
