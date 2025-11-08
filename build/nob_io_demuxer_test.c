// nob.c
#define NOB_IMPLEMENTATION
#include "..\libs\nob\nob.h"

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    
    Nob_Cmd cmd = {0};
        
    // Build IODemuxer test
    nob_log(NOB_INFO, "Building IODemuxer test...");
    
    cmd.count = 0;
    nob_cmd_append(&cmd, "g++");
    nob_cmd_append(&cmd, "-g");
    nob_cmd_append(&cmd, "..\\tests\\IODemuxer_test.cpp");
    nob_cmd_append(&cmd, "..\\core\\IODemuxer.cpp");
    nob_cmd_append(&cmd, "-I..\\libs\\ffmpeg\\include");
    nob_cmd_append(&cmd, "-L..\\libs\\ffmpeg\\lib");
    nob_cmd_append(&cmd, "-lavformat");
    nob_cmd_append(&cmd, "-lavcodec");
    nob_cmd_append(&cmd, "-lavutil");
    nob_cmd_append(&cmd, "-lswscale");
    nob_cmd_append(&cmd, "-lswresample");
    nob_cmd_append(&cmd, "-o", "..\\bin\\iodemuxertest.exe");
    
    if (!nob_cmd_run_sync(cmd)) return 1;
    
    nob_log(NOB_INFO, "Build successful!");
            
    return 0;
}