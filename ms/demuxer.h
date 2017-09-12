#ifndef MS_DEMUXER_H
#define MS_DEMUXER_H

extern "C"{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
#include "libavutil/time.h"
#include "libavutil/imgutils.h"
}

namespace ms {

typedef enum {
    PACKET_UNKNOWN,
    PACKET_AUDIO,
    PACKET_VIDEO,
} PacketType;

typedef struct DemuxedData{
    PacketType Type;
    AVPacket   Packet;
} DemuxedData;

class Demuxer {
public:
    int AudioStreamIndex;
    int VideoStreamIndex;

    Demuxer(AVFormatContext *FmtCtx);
    ~Demuxer();
    DemuxedData *ReadAvPacket();

private:
    AVFormatContext *AVFmtCtx;
    DemuxedData Data;
};


}

#endif
