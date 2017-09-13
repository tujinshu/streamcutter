#ifndef STREAM_ENCODER_H
#define STREAM_ENCODER_H

#include "ms/audio_encoder.h"
#include "ms/video_encoder.h"

#include <thread>
#include <mutex>
#include <deque>
#include <map>

#include "stream_info.h"

using namespace ms;

namespace ms
{
class OutputFile;
class Muxer;
class VideoEncoder;
class AudioEncoder;
}

//ffmpeg
struct AVFrame;

class StreamEncoder
{
public:
    StreamEncoder(std::string outputFileName, std::string formatName, AudioEncodeConfig* ac, VideoEncodeConfig* vc);
    ~StreamEncoder();

    int WriteVideo(AVFrame*);
    int WriteAudio(AVFrame*);

    void StartEncodeThread();
    void StopEncodeThread();

    void StopAndFlush();
    void SetStreamInfo(StreamInfo* info);

private:
    void flushVideoFrameQ();
    void flushAudioFrameQ();
    void flushPacketQ();

    void videoEncode();
    void audioEncode();

    void streamingOut();

    void ensureAudioEncoderValid(AVFrame* frame);
    void ensureVideoEncoderValid(AVFrame* frame);

    void writePacket();
    void openFile();
    void closeFile();

    AudioEncodeConfig* audioConfig;
    VideoEncodeConfig* videoConfig;

    bool abortRequest;

    std::mutex vFrameQMutex;
    std::mutex aFrameQMutex;
    std::deque<AVFrame*> vFrameQ;
    std::deque<AVFrame*> aFrameQ;

    std::thread* videoEncodeThread;
    std::thread* audioEncodeThread;

    std::thread* streamingOutThread;

    OutputFile* output;

    Muxer* muxer;

    std::mutex muxMutex;
    std::deque<AVPacket> vPacketQ;
    std::deque<AVPacket> aPacketQ;

    AudioEncoder* audioEncoder;
    VideoEncoder* videoEncoder;
    
    AVCodecParameters* videoCodecPar;
    AVCodecParameters* audioCodecPar;

    int64_t last_key_pts;
    int64_t current_pts; // in milliseconds
    int segment_count;
    int serial_num = 0;
    StreamInfo* stream_info;
    SegmentInfo segment_info;

};

#endif
