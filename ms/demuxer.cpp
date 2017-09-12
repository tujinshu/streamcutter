#include "demuxer.h"

namespace ms {


Demuxer::Demuxer(AVFormatContext *FmtCtx) {
    AVCodec *Dec = NULL;

    if (NULL == FmtCtx) {
        return;
    }

    AVFmtCtx = FmtCtx;

    AudioStreamIndex = av_find_best_stream(AVFmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &Dec, 0);
    VideoStreamIndex = av_find_best_stream(AVFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &Dec, 0);
}

Demuxer::~Demuxer()
{

}

DemuxedData *Demuxer::ReadAvPacket() {
    int ret = 0;
    AVPacket *Packet = NULL;

    if (NULL == AVFmtCtx) {
        return NULL;
    }

    Packet = &Data.Packet;

    av_init_packet(Packet);
    ret = av_read_frame(AVFmtCtx, Packet);

    if (0 > ret) {
        return NULL;
    }

    if (Packet->stream_index == AudioStreamIndex) {
        Data.Type = PACKET_AUDIO;
    } else if (Packet->stream_index == VideoStreamIndex) {
        Data.Type = PACKET_VIDEO;
    } else {
        Data.Type = PACKET_UNKNOWN;
    }

    return &Data;
}


}
