#ifndef STREAM_ENCODER_H
#define STREAM_ENCODER_H

#include "ms/audio_encoder.h"
#include "ms/video_encoder.h"

#include <thread>
#include <mutex>
#include <deque>
#include <map>

using namespace ms;

namespace ms
{
class OutputFile;
class Muxer;
class VideoEncoder;
class AudioEncoder;
}

typedef struct OutputConfig {
    bool streaming;
    OutputFile* output;
} OutputConfig;

//ffmpeg
struct AVFrame;

class StreamEncoder
{
public:
    StreamEncoder(std::string outputFileName, std::string formatName, AudioEncodeConfig& ac, VideoEncodeConfig& vc);
    ~StreamEncoder();

    int WriteVideo(AVFrame*);
    int WriteAudio(AVFrame*);
    
    void SetDelaySec(int sec);

    void StartEncodeThread();
    void StopEncodeThread();

    void StopAndFlush();

private:
    void flushVideoFrameQ();
    void flushAudioFrameQ();
    void flushPacketQ();

    void videoEncode();
    void audioEncode();

    void streamingOut();

    void ensureAudioEncoderValid(AVFrame* frame);
    void ensureVideoEncoderValid(AVFrame* frame);

    void clearExpiredPackets();

    AudioEncodeConfig audioConfig;
    VideoEncodeConfig videoConfig;

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

    //std::mutex vPacketQMutex;
    //std::mutex aPacketQMutex;
    std::mutex muxMutex;
    std::deque<AVPacket> vPacketQ;
    std::deque<AVPacket> aPacketQ;

    AudioEncoder* audioEncoder;
    VideoEncoder* videoEncoder;
    
    AVCodecParameters* videoCodecPar;
    AVCodecParameters* audioCodecPar;

    int delaySec;
};


#endif
