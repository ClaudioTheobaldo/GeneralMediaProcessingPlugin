// core/IODemuxer.hpp
#pragma once

#include <cstdint>
#include <map>

// Forward declarations
struct AVFormatContext;
struct AVPacket;

namespace FFmpegPlugin {

enum class MediaType {
    Video,
    Audio,
    Subtitle
};

struct PacketInfo {
    int64_t pts;
    int64_t dts;
    int64_t duration;
    int32_t streamIndex;
    int32_t size;
    bool keyframe;
};

struct StreamInformation {
    int32_t streamIndex;
    MediaType type;
    int32_t width;          // Video only
    int32_t height;         // Video only
    int32_t pixelFormat;    // Video only - AVPixelFormat
    int32_t sampleRate;     // Audio only
    int32_t channels;       // Audio only
    int32_t sampleFormat;   // Audio only - AVSampleFormat
    int64_t duration;
    double fps;             // Video only
    char codecName[64];
};

class IODemuxer {
public:
    IODemuxer();
    ~IODemuxer();

    // Returns JSON error/success message
    const char* Open(const char* filename);
    const char* Close();
    
    // Stream information
    StreamInformation* GetAllStreamsInformation(int32_t* outCount);
    const char* GetAllStreamsInformationAsJSON();
    
    // Seeking
    const char* SeekStreamByPercentage(int32_t streamIndex, int32_t percent);
    const char* SeekStreamBySeconds(int32_t streamIndex, int64_t seconds);
    
    // Packet reading
    const uint8_t* GetNextPacket(PacketInfo* outInfo, uint8_t* outEOF);

private:
    // Per-stream packet buffer
    struct StreamBuffer {
        uint8_t* buffer;
        size_t bufferSize;
        
        StreamBuffer() : buffer(nullptr), bufferSize(0) {}
        ~StreamBuffer() {
            delete[] buffer;
        }
    };
    
    // FFmpeg context
    AVFormatContext* formatCtx;
    AVPacket* packet;
    
    // Static JSON error buffer (per instance)
    char jsonBuffer[4096 * 4 + 1];
    
    // Per-stream packet buffers
    std::map<int32_t, StreamBuffer*> streamBuffers;
    
    // Stream information cache
    StreamInformation* streamInfoCache;
    int32_t streamCount;
    
    // Helper methods
    void SetJsonError(int32_t errorID, const char* message);
    void SetJsonSuccess(const char* message = "OK");
    void EnsureStreamBufferSize(int32_t streamIndex, size_t requiredSize);
    void BuildStreamInfoCache();
};

} // namespace FFmpegPlugin