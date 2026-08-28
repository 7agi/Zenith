#pragma once
#include "CircularBuffer.h"
#include <string>
#include <vector>

namespace zenith {

// ---------------------------------------------------------------------------
// ClipSaver
//
// Takes a snapshot of packets from the CircularBuffer and writes them to
// an MP4 file using FFmpeg (linked via libobs's bundled ffmpeg).
//
// The save operation runs on a background thread so it never blocks the
// recording pipeline.
// ---------------------------------------------------------------------------

class ClipSaver {
public:
    // `outputDir` is the directory where clips are saved.
    explicit ClipSaver(const std::string& outputDir);

    // Save `packets` as an MP4 file.  Generates a timestamped filename.
    // Returns the path of the saved file, or empty string on failure.
    // Runs synchronously (call from a thread if needed).
    std::string save(const std::vector<EncodedPacket>& packets,
                     int videoWidth, int videoHeight, int videoFps,
                     int audioSampleRate = 48000, int audioChannels = 2);

    void setOutputDir(const std::string& dir);
    const std::string& outputDir() const { return m_outputDir; }

private:
    std::string generateFilename() const;

    std::string m_outputDir;
};

} // namespace zenith
