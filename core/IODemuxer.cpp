// core/IODemuxer.cpp
#include "IODemuxer.hpp"
#include <cstring>
#include <cstdio>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

namespace FFmpegPlugin {

IODemuxer::IODemuxer() 
    : formatCtx(nullptr)
    , packet(nullptr)
    , streamInfoCache(nullptr)
    , streamCount(0)
{
    memset(jsonBuffer, 0, sizeof(jsonBuffer));
    packet = av_packet_alloc();
}

IODemuxer::~IODemuxer() {
    Close();
    if (packet) {
        av_packet_free(&packet);
    }
    
    // Clean up all stream buffers
    for (auto& pair : streamBuffers) {
        delete pair.second;
    }
    streamBuffers.clear();
    
    if (streamInfoCache) {
        delete[] streamInfoCache;
    }
}

const char* IODemuxer::Open(const char* filename) {
    if (formatCtx) {
        SetJsonError(1001, "Demuxer already open");
        return jsonBuffer;
    }
    
    formatCtx = avformat_alloc_context();
    if (!formatCtx) {
        SetJsonError(1002, "Failed to allocate AVFormatContext");
        return jsonBuffer;
    }
    
    int ret = avformat_open_input(&formatCtx, filename, nullptr, nullptr);
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        SetJsonError(1003, errBuf);
        avformat_free_context(formatCtx);
        formatCtx = nullptr;
        return jsonBuffer;
    }
    
    ret = avformat_find_stream_info(formatCtx, nullptr);
    if (ret < 0) {
        SetJsonError(1004, "Failed to find stream info");
        avformat_close_input(&formatCtx);
        return jsonBuffer;
    }
    
    BuildStreamInfoCache();
    SetJsonSuccess("File opened successfully");
    return jsonBuffer;
}

const char* IODemuxer::Close() {
    if (formatCtx) {
        avformat_close_input(&formatCtx);
        formatCtx = nullptr;
    }
    
    // Clean up stream buffers
    for (auto& pair : streamBuffers) {
        delete pair.second;
    }
    streamBuffers.clear();
    
    if (streamInfoCache) {
        delete[] streamInfoCache;
        streamInfoCache = nullptr;
        streamCount = 0;
    }
    
    SetJsonSuccess("Demuxer closed");
    return jsonBuffer;
}

StreamInformation* IODemuxer::GetAllStreamsInformation(int32_t* outCount) {
    if (outCount) {
        *outCount = streamCount;
    }
    return streamInfoCache;
}

const char* IODemuxer::GetAllStreamsInformationAsJSON() {
    if (!formatCtx || !streamInfoCache) {
        SetJsonError(2001, "No file open or stream info unavailable");
        return jsonBuffer;
    }
    
    // Build JSON manually
    int offset = snprintf(jsonBuffer, sizeof(jsonBuffer), 
        "{\"ErrorID\":0,\"StreamCount\":%d,\"Streams\":[", streamCount);
    
    for (int32_t i = 0; i < streamCount && offset < sizeof(jsonBuffer) - 100; i++) {
        StreamInformation& info = streamInfoCache[i];
        const char* typeStr = (info.type == MediaType::Video) ? "Video" :
                              (info.type == MediaType::Audio) ? "Audio" : "Subtitle";
        
        offset += snprintf(jsonBuffer + offset, sizeof(jsonBuffer) - offset,
            "%s{\"Index\":%d,\"Type\":\"%s\",\"Codec\":\"%s\"",
            (i > 0 ? "," : ""), info.streamIndex, typeStr, info.codecName);
        
        if (info.type == MediaType::Video) {
            offset += snprintf(jsonBuffer + offset, sizeof(jsonBuffer) - offset,
                ",\"Width\":%d,\"Height\":%d,\"FPS\":%.2f",
                info.width, info.height, info.fps);
        } else if (info.type == MediaType::Audio) {
            offset += snprintf(jsonBuffer + offset, sizeof(jsonBuffer) - offset,
                ",\"SampleRate\":%d,\"Channels\":%d",
                info.sampleRate, info.channels);
        }
        
        offset += snprintf(jsonBuffer + offset, sizeof(jsonBuffer) - offset, "}");
    }
    
    snprintf(jsonBuffer + offset, sizeof(jsonBuffer) - offset, "]}");
    return jsonBuffer;
}

const char* IODemuxer::SeekStreamByPercentage(int32_t streamIndex, int32_t percent) {
    if (!formatCtx) {
        SetJsonError(3001, "No file open");
        return jsonBuffer;
    }
    
    if (streamIndex < 0 || streamIndex >= (int32_t)formatCtx->nb_streams) {
        SetJsonError(3002, "Invalid stream index");
        return jsonBuffer;
    }
    
    AVStream* stream = formatCtx->streams[streamIndex];
    int64_t duration = stream->duration;
    
    if (duration <= 0) {
        duration = formatCtx->duration * stream->time_base.den / 
                   (stream->time_base.num * AV_TIME_BASE);
    }
    
    int64_t targetPts = (duration * percent) / 100;
    int ret = av_seek_frame(formatCtx, streamIndex, targetPts, AVSEEK_FLAG_BACKWARD);
    
    if (ret < 0) {
        SetJsonError(3003, "Seek failed");
        return jsonBuffer;
    }
    
    SetJsonSuccess("Seek successful");
    return jsonBuffer;
}

const char* IODemuxer::SeekStreamBySeconds(int32_t streamIndex, int64_t seconds) {
    if (!formatCtx) {
        SetJsonError(3001, "No file open");
        return jsonBuffer;
    }
    
    if (streamIndex < 0 || streamIndex >= (int32_t)formatCtx->nb_streams) {
        SetJsonError(3002, "Invalid stream index");
        return jsonBuffer;
    }
    
    AVStream* stream = formatCtx->streams[streamIndex];
    int64_t targetPts = av_rescale_q(seconds * AV_TIME_BASE, 
                                      AV_TIME_BASE_Q, stream->time_base);
    
    int ret = av_seek_frame(formatCtx, streamIndex, targetPts, AVSEEK_FLAG_BACKWARD);
    
    if (ret < 0) {
        SetJsonError(3003, "Seek failed");
        return jsonBuffer;
    }
    
    SetJsonSuccess("Seek successful");
    return jsonBuffer;
}

const uint8_t* IODemuxer::GetNextPacket(PacketInfo* outInfo, uint8_t* outEOF) {
    if (!formatCtx) {
        if (outEOF) *outEOF = 1;
        return nullptr;
    }
    
    // Read next packet from any stream
    av_packet_unref(packet);
    int ret = av_read_frame(formatCtx, packet);
    
    if (ret == AVERROR_EOF) {
        if (outEOF) *outEOF = 1;
        return nullptr;
    }
    
    if (ret < 0) {
        if (outEOF) *outEOF = 1;
        return nullptr;
    }
    
    // Fill packet info
    if (outInfo) {
        outInfo->pts = packet->pts;
        outInfo->dts = packet->dts;
        outInfo->duration = packet->duration;
        outInfo->streamIndex = packet->stream_index;
        outInfo->size = packet->size;
        outInfo->keyframe = (packet->flags & AV_PKT_FLAG_KEY) != 0;
    }
    
    if (outEOF) *outEOF = 0;
    
    // Get or create stream buffer for this stream
    int32_t streamIdx = packet->stream_index;
    EnsureStreamBufferSize(streamIdx, packet->size);
    
    // Copy packet data to the stream-specific buffer
    memcpy(streamBuffers[streamIdx]->buffer, packet->data, packet->size);
    
    return streamBuffers[streamIdx]->buffer;
}

void IODemuxer::SetJsonError(int32_t errorID, const char* message) {
    snprintf(jsonBuffer, sizeof(jsonBuffer), 
             "{\"ErrorID\":%d,\"ReturnMessage\":\"%s\"}", 
             errorID, message);
}

void IODemuxer::SetJsonSuccess(const char* message) {
    snprintf(jsonBuffer, sizeof(jsonBuffer), 
             "{\"ErrorID\":0,\"ReturnMessage\":\"%s\"}", 
             message);
}

void IODemuxer::EnsureStreamBufferSize(int32_t streamIndex, size_t requiredSize) {
    // Check if buffer exists for this stream
    if (streamBuffers.find(streamIndex) == streamBuffers.end()) {
        streamBuffers[streamIndex] = new StreamBuffer();
    }
    
    StreamBuffer* sb = streamBuffers[streamIndex];
    
    // Resize if needed
    if (sb->bufferSize < requiredSize) {
        delete[] sb->buffer;
        sb->bufferSize = requiredSize * 2; // Allocate extra to reduce reallocs
        sb->buffer = new uint8_t[sb->bufferSize];
    }
}

void IODemuxer::BuildStreamInfoCache() {
    if (!formatCtx) return;
    
    streamCount = formatCtx->nb_streams;
    streamInfoCache = new StreamInformation[streamCount];
    
    for (int32_t i = 0; i < streamCount; i++) {
        AVStream* stream = formatCtx->streams[i];
        AVCodecParameters* codecpar = stream->codecpar;
        StreamInformation& info = streamInfoCache[i];
        
        info.streamIndex = i;
        info.duration = stream->duration;
        strncpy(info.codecName, avcodec_get_name(codecpar->codec_id), sizeof(info.codecName) - 1);
        info.codecName[sizeof(info.codecName) - 1] = '\0';
        
        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            info.type = MediaType::Video;
            info.width = codecpar->width;
            info.height = codecpar->height;
            info.pixelFormat = codecpar->format;
            info.fps = av_q2d(stream->avg_frame_rate);
            info.sampleRate = 0;
            info.channels = 0;
            info.sampleFormat = 0;
        } else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            info.type = MediaType::Audio;
            info.sampleRate = codecpar->sample_rate;
            info.channels = codecpar->ch_layout.nb_channels;
            info.sampleFormat = codecpar->format;
            info.width = 0;
            info.height = 0;
            info.pixelFormat = 0;
            info.fps = 0.0;
        } else {
            info.type = MediaType::Subtitle;
            info.width = 0;
            info.height = 0;
            info.pixelFormat = 0;
            info.fps = 0.0;
            info.sampleRate = 0;
            info.channels = 0;
            info.sampleFormat = 0;
        }
    }
}

} // namespace FFmpegPlugin