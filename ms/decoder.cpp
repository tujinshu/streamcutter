extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
}

#include "decoder.h"
#include "ms_util.h"

namespace ms {

Decoder::Decoder(AVCodecParameters* codecpar):codecCtx(NULL)
{
    if(codecpar == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "codecpar was null\n");
        return;
    }

    AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (codec == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Unsupported codec!\n");
        return;
    }

    codecCtx = avcodec_alloc_context3(codec);
    if(!codecCtx){
        av_log(NULL, AV_LOG_ERROR, "alloc codec ctx failed!\n");
        return;
    }

    if (avcodec_parameters_to_context(codecCtx,codecpar)) {
        av_log(NULL, AV_LOG_ERROR, "Couldn't copy codec context\n");
        return;
    }

    av_opt_set_int(codecCtx, "refcounted_frames", 1, 0);

    if (avcodec_open2(codecCtx, codec, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open decoder\n");
        return;
    }
}

Decoder::~Decoder()
{
    if(codecCtx == NULL){
        return;
    }
    avcodec_close(codecCtx);
    avcodec_free_context(&codecCtx);
}

DecodeResult Decoder::WritePacket(AVPacket* pkt)
{
    DecodeResult r;
    r.ret = 0;
    if(codecCtx == NULL){
        av_log(NULL, AV_LOG_ERROR, "codec ctx was null, can't write\n");
        r.ret = -1;
        return r;
    }

    r.ret = avcodec_send_packet(codecCtx, pkt);
    if(r.ret < 0){
        av_log(NULL, AV_LOG_ERROR, "send packet failed: %s\n", av_err2str(r.ret));
        return r;
    }
    
    do {
        AVFrame* frame = av_frame_alloc();
        r.ret = avcodec_receive_frame(codecCtx, frame);
        
        if (r.ret == 0) {
            //got frame and send to frame queue
            frame->pts = av_frame_get_best_effort_timestamp(frame);
            r.frames.push_back(frame);

        } else {
            //free frame
            av_frame_free(&frame);
        }
    } while(r.ret == 0);

    //ret was eagain or error
    if(r.ret != AVERROR(EAGAIN)) {
        av_log(NULL, AV_LOG_ERROR, "get frame failed: %s\n", av_err2str(r.ret));
        //free frames
        for(AVFrame* frame : r.frames){
            av_frame_unref(frame);
            av_frame_free(&frame);
        }
        r.frames = std::vector<AVFrame*>();
        return r;
    }
    r.ret = 0;
    return r;
}

}
