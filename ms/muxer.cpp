#include "muxer.h"
#include "ms_util.h"

namespace ms {

Muxer::Muxer(AVFormatContext *FmtCtx)
{
   AVFmtCtx = FmtCtx;
}

Muxer::~Muxer()
{

}

int Muxer::Write(AVPacket *Packet) {
    if (NULL == Packet) {
        av_log(NULL, AV_LOG_ERROR, "pkt was null\n");
        return -1;
    }

    if (NULL == AVFmtCtx) {
        av_log(NULL, AV_LOG_ERROR, "format ctx was null\n");
        return -1;
    }
    //LOGI << "index: " << Packet->stream_index << " dts: "<< Packet->dts;
   // if(Packet->stream_index == 1){
   //     //LOGD_EVERY_N(250) << "index: " << Packet->stream_index << " dts: "<< Packet->dts;
   // }
   // if(Packet->stream_index == 0){
   //     //LOGD_EVERY_N(250) << "index: " << Packet->stream_index << " dts: "<< Packet->dts;
   // }
    //LOG_EVERY_N(INFO, 40)<<"write  pkt: "<< pkt.dts;
    return av_interleaved_write_frame(AVFmtCtx, Packet);
}


}
