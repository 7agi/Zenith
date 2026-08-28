#include "ClipSaver.h"
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <stdexcept>

// FFmpeg C API (bundled with libobs)
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
}

namespace fs = std::filesystem;
using namespace zenith;

ClipSaver::ClipSaver(const std::string& outputDir)
    : m_outputDir(outputDir) {}

void ClipSaver::setOutputDir(const std::string& dir) {
    m_outputDir = dir;
}

std::string ClipSaver::generateFilename() const {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << "Zenith_" << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S") << ".mp4";
    return ss.str();
}

std::string ClipSaver::save(const std::vector<EncodedPacket>& packets,
                             int videoWidth, int videoHeight, int videoFps,
                             int audioSampleRate, int audioChannels) {
    if (packets.empty()) return {};

    // Ensure output directory exists
    fs::path outDir(m_outputDir);
    if (outDir.empty()) {
        // Default: %USERPROFILE%\Videos\Zenith
        wchar_t* uh = nullptr;
        _wdupenv_s(&uh, nullptr, L"USERPROFILE");
        if (uh) { outDir = fs::path(uh) / L"Videos" / L"Zenith"; free(uh); }
        else outDir = fs::path("Zenith");
    }
    fs::create_directories(outDir);

    std::string outPath = (outDir / generateFilename()).string();

    // ---------- FFmpeg muxer setup ----------
    AVFormatContext* fmtCtx = nullptr;
    if (avformat_alloc_output_context2(&fmtCtx, nullptr, nullptr, outPath.c_str()) < 0)
        return {};

    // Video stream
    AVStream* vStream = avformat_new_stream(fmtCtx, nullptr);
    if (!vStream) { avformat_free_context(fmtCtx); return {}; }
    vStream->id = 0;
    vStream->codecpar->codec_type  = AVMEDIA_TYPE_VIDEO;
    vStream->codecpar->codec_id    = AV_CODEC_ID_H264;
    vStream->codecpar->width       = videoWidth;
    vStream->codecpar->height      = videoHeight;
    vStream->time_base             = {1, videoFps};

    // Audio stream
    AVStream* aStream = avformat_new_stream(fmtCtx, nullptr);
    if (!aStream) { avformat_free_context(fmtCtx); return {}; }
    aStream->id = 1;
    aStream->codecpar->codec_type    = AVMEDIA_TYPE_AUDIO;
    aStream->codecpar->codec_id      = AV_CODEC_ID_AAC;
    aStream->codecpar->sample_rate   = audioSampleRate;
    aStream->codecpar->ch_layout.nb_channels = audioChannels;
    aStream->time_base               = {1, audioSampleRate};

    if (!(fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmtCtx->pb, outPath.c_str(), AVIO_FLAG_WRITE) < 0) {
            avformat_free_context(fmtCtx);
            return {};
        }
    }

    if (avformat_write_header(fmtCtx, nullptr) < 0) {
        avio_closep(&fmtCtx->pb);
        avformat_free_context(fmtCtx);
        return {};
    }

    // ---------- Write packets ----------
    int64_t baseVideo = -1, baseAudio = -1;
    for (const auto& pkt : packets) {
        if (pkt.data.empty()) continue;

        AVPacket* ap = av_packet_alloc();
        if (!ap) continue;

        av_new_packet(ap, (int)pkt.data.size());
        std::memcpy(ap->data, pkt.data.data(), pkt.data.size());

        if (pkt.isVideo) {
            if (baseVideo < 0) baseVideo = pkt.ptsMs;
            ap->stream_index = vStream->index;
            ap->pts = av_rescale_q(pkt.ptsMs - baseVideo,
                                   {1, 1000}, vStream->time_base);
            ap->dts = av_rescale_q(pkt.dtsMs - baseVideo,
                                   {1, 1000}, vStream->time_base);
            ap->duration = av_rescale_q(1, {1, videoFps}, vStream->time_base);
            if (pkt.isKeyframe) ap->flags |= AV_PKT_FLAG_KEY;
        } else {
            if (baseAudio < 0) baseAudio = pkt.ptsMs;
            ap->stream_index = aStream->index;
            ap->pts = av_rescale_q(pkt.ptsMs - baseAudio,
                                   {1, 1000}, aStream->time_base);
            ap->dts = ap->pts;
        }

        av_interleaved_write_frame(fmtCtx, ap);
        av_packet_free(&ap);
    }

    av_write_trailer(fmtCtx);
    if (!(fmtCtx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&fmtCtx->pb);
    avformat_free_context(fmtCtx);

    return outPath;
}
