#pragma once
#include <cstdint>
#include <vector>
#include <mutex>
#include <functional>

namespace zenith {

// ---------------------------------------------------------------------------
// CircularBuffer
//
// Stores encoded audio/video packets (as raw byte blobs) in a ring buffer
// of fixed maximum duration.  Designed to be fed by an OBS output callback
// and flushed to disk on demand (shadowplay-style clip save).
//
// Thread-safe: one producer thread (OBS encoder callback), one consumer
// (clip-save thread).
// ---------------------------------------------------------------------------

struct EncodedPacket {
    bool        isVideo;
    bool        isKeyframe;
    int64_t     ptsMs;      // presentation timestamp in milliseconds
    int64_t     dtsMs;      // decode timestamp in milliseconds
    std::vector<uint8_t> data;
};

class CircularBuffer {
public:
    explicit CircularBuffer(int maxDurationSecs = 30);

    // Called from the OBS output callback (producer).
    void push(EncodedPacket pkt);

    // Drain all packets whose PTS falls within the last `durationSecs`
    // (or the configured max if durationSecs == -1).
    // Returns the packets in chronological order.
    // Caller takes ownership; the internal buffer is NOT cleared (continues
    // accumulating for future clips).
    std::vector<EncodedPacket> drain(int durationSecs = -1);

    // Clear all buffered packets.
    void clear();

    // Change the window size at runtime.
    void setMaxDuration(int secs);
    int  maxDuration() const { return m_maxDurationSecs; }

    // How many seconds of footage is currently buffered?
    double bufferedDuration() const;

private:
    void trimOld();

    mutable std::mutex          m_mutex;
    std::vector<EncodedPacket>  m_packets;
    int                         m_maxDurationSecs;
};

} // namespace zenith
