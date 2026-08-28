#include "CircularBuffer.h"
#include <algorithm>

namespace zenith {

CircularBuffer::CircularBuffer(int maxDurationSecs)
    : m_maxDurationSecs(maxDurationSecs) {}

void CircularBuffer::setMaxDuration(int secs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxDurationSecs = secs;
    trimOld();
}

void CircularBuffer::push(EncodedPacket pkt) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_packets.push_back(std::move(pkt));
    trimOld();
}

void CircularBuffer::trimOld() {
    // Called under lock.
    if (m_packets.empty()) return;

    int64_t latestPts = m_packets.back().ptsMs;
    int64_t cutoff    = latestPts - (int64_t)m_maxDurationSecs * 1000;

    // Find first packet still within window
    size_t firstKeep = 0;
    for (size_t i = 0; i < m_packets.size(); ++i) {
        if (m_packets[i].ptsMs >= cutoff) {
            firstKeep = i;
            break;
        }
    }
    if (firstKeep > 0) {
        m_packets.erase(m_packets.begin(), m_packets.begin() + firstKeep);
    }
}

std::vector<EncodedPacket> CircularBuffer::drain(int durationSecs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_packets.empty()) return {};

    int secs = (durationSecs < 0) ? m_maxDurationSecs : durationSecs;
    int64_t latestPts = m_packets.back().ptsMs;
    int64_t cutoff    = latestPts - (int64_t)secs * 1000;

    std::vector<EncodedPacket> result;
    for (const auto& pkt : m_packets) {
        if (pkt.ptsMs >= cutoff) {
            result.push_back(pkt);
        }
    }
    return result;
}

void CircularBuffer::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_packets.clear();
}

double CircularBuffer::bufferedDuration() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_packets.size() < 2) return 0.0;
    int64_t span = m_packets.back().ptsMs - m_packets.front().ptsMs;
    return span / 1000.0;
}

} // namespace zenith
