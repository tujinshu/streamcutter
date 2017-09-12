#ifndef VIDEO_ENCODER_H 
#define VIDEO_ENCODER_H

extern "C" {
#include "libavutil/avutil.h"
}

#include <vector>
#include <string>

struct AVFrame;
struct AVPacket;

namespace ms {

enum BitrateControl{
   VBR = 0,
   CBR
};

typedef struct VideoEncodeConfig
{
    int width;
    int height;
    int videoFormat;
    AVRational timeBase;
    int64_t videoBitRate;
    int gopSize;
    enum AVCodecID videoCodecID;
    //name 
    std::string codecName;
    AVRational sampleAspectRatio;
    int maxBFrames;
    enum BitrateControl bcType;

} VideoEncodeConfig;

typedef struct VideoEncodeResult
{
    int ret;
    std::vector<AVPacket> pkts;

} VideoEncodeResult;

class VideoEncoder
{
public:
    VideoEncoder(VideoEncodeConfig& );
    ~VideoEncoder();
    //int WriteFrame(AVFrame* frame, std::vector<AVPacket*>& pkts);
    VideoEncodeResult WriteFrame(AVFrame* frame);
    
    AVCodecContext* encodeCtx;
private:
    int init(VideoEncodeConfig&);

};

}

#endif
