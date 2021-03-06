#ifndef SEGMENTER_H
#define SEGMENTER_H

#include "ms/input_file.h"
#include "ms/decoder.h"
#include "ms/audio_encoder.h"
#include "ms/video_encoder.h"
#include "ms/demuxer.h"
#include "ms/muxer.h"
#include "ms/ms_util.h"
#include "audio_filter.h"
#include "stream_encoder.h"
#include "stream_info.h"

#include <string>

class Segmenter
{
public:
    Segmenter(std::string url, int segmentTime);
    ~Segmenter();
    void StartProcess();
    void StopProcessStream();
    bool GetPlayList(std::vector<std::pair<int64_t, int64_t>> time_intervals, std::string& playlist_url);

private:
    void processStream();
    bool init();
    bool checkPktDtsJumped(ms::DemuxedData*);
    void reinitStreamEncoder();
    void flushStreamEncoder();
    bool consumePacket(ms::DemuxedData*);

    StreamInfo stream_info;
    std::string filename;
    InputFile* input;
    Decoder* videoDecoder;
    Decoder* audioDecoder;
    Demuxer* demuxer;
    StreamEncoder* streamEncoder;
    AudioFilter* audioFilter;
    int segment_time;
    bool thread_exit;
    bool has_video;
    bool has_audio;
    std::thread processThread;
    int64_t last_pkt_dts; // in millisecond
    VideoEncodeConfig vc;
    AudioEncodeConfig ac;
    VideoEncodeConfig* pvc;
    AudioEncodeConfig* pac;
    int serial_num;
};

#endif
