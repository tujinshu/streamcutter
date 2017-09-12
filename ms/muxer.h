#ifndef MS_MUXER_H
#define MS_MUXER_H

extern "C"{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
#include "libavutil/time.h"
#include "libavutil/imgutils.h"
}

namespace ms {

class Muxer {
public:
    Muxer(AVFormatContext *FmtCtx);
    ~Muxer();
    int Write(AVPacket *Packet);

private:
    AVFormatContext *AVFmtCtx;
};

}

#endif
