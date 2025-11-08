#include <iostream>
#include <vector>
#include "..\core\IODemuxer.hpp"

using namespace FFmpegPlugin;

int main() {
    std::string *s = new std::string("C:\\Users\\Claudio\\Documents\\C\\GeneralMediaProcessingPlugin\\media\\Digimon World 100% - Walkthrough [01] [z3dAroxJ7Gg].mp4");
    std::printf("Path: %s\n", s->c_str());

    // Load
    IODemuxer iod = IODemuxer{};
    const char *open_r = iod.Open((const char*)s->c_str());
    std::printf("Open result -> \n %s \n", open_r);

    // Print stream information
    int out_count = 0;
    StreamInformation *sis = iod.GetAllStreamsInformation(&out_count);
    for (int i = 0; i < out_count; i++)  {
        StreamInformation si = sis[i]; 
        std::printf("StreamIndex: %d\n", si.streamIndex);
        if (si.type == MediaType::Video) {
            std::printf("MediaType: Video\n");
            std::printf("Width: %d\n", si.width);
            std::printf("Height: %d\n", si.height);
            std::printf("Format: %d\n", si.pixelFormat);
        } else if (si.type == MediaType::Audio) {
            std::printf("MediaType: Audio\n");
            std::printf("SampleRate: %d\n", si.sampleRate);
            std::printf("Channels: %d\n", si.channels);
            std::printf("Format: %d\n", si.sampleFormat);
        }
        std::printf("CodecName: %s\n", si.codecName);
        std::printf("Duration: %lld seconds \n", si.duration);
        std::printf("FPS: %.5f\n", si.fps);
    }

    // Print all packets
    std::vector<int32_t> packets_count = std::vector<int32_t>(2);
    PacketInfo pi;
    uint8_t eof = 0;
    while (!eof) {
        const uint8_t *packet_data = iod.GetNextPacket(&pi, &eof);
        packets_count[pi.streamIndex]++;
        // std::printf("StreamIndex: %ld\n", pi.streamIndex);
        // std::printf("PTS: %lld\n", pi.pts);
        // std::printf("DTS: %lld\n", pi.dts);
        // std::printf("Size: %ld\n", pi.size);
    }

    for (int i = 0; i < packets_count.size(); i++)
        std::printf("StreamIndex: %ld - PacketCount: %ld\n", i, packets_count[i]);

    // Close
    const char *close_r = iod.Close();
    std::printf("Close response -> \n %s \n", close_r);
    delete s;
}