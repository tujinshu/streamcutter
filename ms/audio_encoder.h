#ifndef AUDIO_ENCODER_H
#define AUDIO_ENCODER_H

extern "C" {
#include "libavcodec/avcodec.h"
}

#include <vector>

struct AVFrame;
struct AVPacket;

namespace ms {

typedef struct AudioEncodeConfig
{
    uint64_t channelLayout;
    int sampleFmt;
    int sampleRate;
    int64_t bitRate;
    enum AVCodecID audioCodecID;

} AudioEncodeConfig;

typedef struct AudioEncodeResult
{
    int ret;
    std::vector<AVPacket> pkts;

} AudioEncodeResult;

class AudioEncoder
{
public:
    AudioEncoder(AudioEncodeConfig& c);
    ~AudioEncoder();
    AudioEncodeResult WriteFrame(AVFrame* frame);

    AVCodecContext* encodeCtx;
private:
    int init(AudioEncodeConfig& c);
    
    //AVCodecID
};

}

#endif
