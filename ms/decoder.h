#ifndef VIDEO_DECODER_H
#define VIDEO_DECODER_H

#include <vector>

struct AVCodecParameters;
struct AVPacket;
struct AVFrame;
struct AVCodecContext;

namespace ms {

typedef struct DecodeResult
{
    int ret;
    std::vector<AVFrame*> frames;

} DecodeResult;


class Decoder
{
public:
    Decoder(AVCodecParameters* codecpar);
    ~Decoder();
    //
    //int WritePacket(AVPacket* pkt, std::vector<AVFrame*>& frames);
    DecodeResult WritePacket(AVPacket* pkt);
    AVCodecContext* codecCtx;
private:

};

}


#endif
