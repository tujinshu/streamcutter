extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
}
#include "ms_util.h"
#include "video_encoder.h"

namespace ms {

VideoEncoder::VideoEncoder(VideoEncodeConfig& c):encodeCtx(NULL)
{
    init(c);
}

VideoEncoder::~VideoEncoder()
{
    if(encodeCtx){
        avcodec_close(encodeCtx);
        avcodec_free_context(&encodeCtx);
    }
}

int VideoEncoder::init(VideoEncodeConfig& c)
{
    int ret;
    AVCodec* codec;
    codec = avcodec_find_encoder_by_name(c.codecName.c_str());
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "find video codec by name failed\n");
        codec = avcodec_find_encoder(c.videoCodecID);
    }
    
    if(!codec){
        av_log(NULL, AV_LOG_ERROR, "find video codec by id failed\n");
        return -1;
    }
    //alloc encode context
    encodeCtx = avcodec_alloc_context3(codec);
    if(!encodeCtx){
        av_log(NULL, AV_LOG_ERROR, "alloc video context failed\n");
        return -1;
    }
    //set codecContext
    //set baseline
    encodeCtx->codec_id = c.videoCodecID;

    encodeCtx->bit_rate = c.videoBitRate;
    if(c.bcType == CBR){
        encodeCtx->rc_max_rate = c.videoBitRate;
        encodeCtx->rc_min_rate = c.videoBitRate;
        encodeCtx->rc_buffer_size = c.videoBitRate;
    }

    /* Resolution must be a multiple of two. */
    encodeCtx->width    = c.width;
    encodeCtx->height   = c.height;

    /* timebase: This is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. For fixed-fps content,
     * timebase should be 1/framerate and timestamp increments should be
     * identical to 1. */
//    encodeCtx->time_base       = (AVRational){1, c.frameRate};
    encodeCtx->time_base = c.timeBase;
  
    encodeCtx->gop_size      = c.gopSize; /* emit one intra frame every got_size frames at most */
    encodeCtx->keyint_min = c.gopSize; /* emit one intra frame every got_size frames at most */

    encodeCtx->scenechange_threshold = 0;
    encodeCtx->pix_fmt       = (enum AVPixelFormat)c.videoFormat;
//    encodeCtx->delay = 0;
//    encodeCtx->max_b_frames = c.maxBFrames;
//    encodeCtx->thread_count = 1;
//    encodeCtx->sample_aspect_ratio = c.sampleAspectRatio;

    if(encodeCtx->codec_id == AV_CODEC_ID_H264){
        encodeCtx->profile = FF_PROFILE_H264_BASELINE;
    
        av_opt_set(encodeCtx->priv_data, "preset", "ultrafast", 0);
        //av_opt_set(encodeCtx->priv_data,"crf","23",AV_OPT_SEARCH_CHILDREN);
        //av_opt_set(encodeCtx->priv_data, "preset", "veryfast", 0);
        //av_opt_set(encodeCtx->priv_data, "tune", "zerolatency", 0);
        //av_opt_set_int(encodeCtx->priv_data, "g", 25, 0);
        //av_opt_set_int(encodeCtx->priv_data, "keyint_min", 25, 0);
        //av_opt_set_int(encodeCtx->priv_data, "sc_threshold", -1, 0);
        //AVDictionary* dic = NULL;
        //av_dict_set(&dic, "keyint", )
        //av_opt_set_dict_val(encodeCtx->priv_data, "x264-params");
    }
//    AVDictionary *opts = NULL;
//    av_dict_set(&opts, "profile", "high", 0);

    encodeCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
//    encodeCtx->flags2 |= AV_CODEC_FLAG2_FAST;

    //open codec
    ret = avcodec_open2(encodeCtx, codec, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open video codec: %s\n", av_err2str(ret));
        return ret;
    }
    return 0;
}

VideoEncodeResult VideoEncoder::WriteFrame(AVFrame* frame)
{
    VideoEncodeResult r;
    int ret = 0;
    r.ret = avcodec_send_frame(encodeCtx, frame);
    if(r.ret < 0){
        av_log(NULL, AV_LOG_ERROR, "send frame failed: %s\n", av_err2str(r.ret));
        return r;
    }

    while(1){
        //AVPacket* pkt = av_packet_alloc();
        AVPacket pkt;
        av_init_packet(&pkt);
        
        r.ret = avcodec_receive_packet(encodeCtx, &pkt);
        if(r.ret == 0){
          //  AVPacket* ppkt = av_packet_clone(&pkt);
            AVPacket pushedPkt = {0};
            //av_packet_move_ref(&pushedPkt, &pkt);
            ret = av_packet_ref(&pushedPkt, &pkt);
            av_packet_unref(&pkt);
            if(ret < 0){
                break;
            }
            r.pkts.push_back(pushedPkt);

        } else if (r.ret == AVERROR(EAGAIN) || r.ret == AVERROR(EOF)) {
            //need more inputs
            r.ret = 0;
            break;

        } else {
            av_log(NULL, AV_LOG_ERROR, "receive packet  failed: %s\n", av_err2str(r.ret));
            break;
        }
    }
    return r;
}

}
