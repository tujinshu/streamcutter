extern "C" {
#include "libavcodec/avcodec.h"
}
#include "ms_util.h"
#include "audio_encoder.h"

namespace ms {

AudioEncoder::AudioEncoder(AudioEncodeConfig& c):encodeCtx(NULL)
{
    //init encoder
    init(c);
}

AudioEncoder::~AudioEncoder()
{
    if(encodeCtx){
        avcodec_close(encodeCtx);
        avcodec_free_context(&encodeCtx);
    }

}

int AudioEncoder::init(AudioEncodeConfig& c)
{
    int i, ret;
    AVCodec* codec = avcodec_find_encoder(c.audioCodecID);
    if(!codec){
        av_log(NULL, AV_LOG_ERROR, "find audio codec failed\n");
        return -1;
    }
    
    encodeCtx = avcodec_alloc_context3(codec);
    if(!encodeCtx) {
        av_log(NULL, AV_LOG_ERROR, "alloc audio encode context failed\n");
        return -1;
    }

    encodeCtx->sample_fmt = codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
    encodeCtx->bit_rate = c.bitRate;
    encodeCtx->sample_rate = c.sampleRate;
    if (codec->supported_samplerates) {
        encodeCtx->sample_rate = codec->supported_samplerates[0];
        for (i = 0; codec->supported_samplerates[i]; i++) {
            //find supported rate
            if (codec->supported_samplerates[i] == c.sampleRate){
                encodeCtx->sample_rate = c.sampleRate;
                av_log(NULL, AV_LOG_INFO, "find supported sample rate matching config\n");
            }
        }
    }
    encodeCtx->channel_layout = AV_CH_LAYOUT_STEREO;
    if (codec->channel_layouts) {
        encodeCtx->channel_layout = codec->channel_layouts[0];
        for (i = 0; codec->channel_layouts[i]; i++) {
            //find support channel layout
            if (codec->channel_layouts[i] == c.channelLayout){
                encodeCtx->channel_layout = c.channelLayout;
                av_log(NULL, AV_LOG_INFO, "find supported channel layout rate matching config\n");
            }
        }
    }
    encodeCtx->channels        = av_get_channel_layout_nb_channels(encodeCtx->channel_layout);

    //
    encodeCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    encodeCtx->flags2 |= AV_CODEC_FLAG2_FAST;

    //open codec
    ret = avcodec_open2(encodeCtx, codec, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open audio codec: %s\n", av_err2str(ret));
        return ret;
    }
    return 0;
}

AudioEncodeResult AudioEncoder::WriteFrame(AVFrame* frame)
{
    AudioEncodeResult r;
    int ret = 0;
    r.ret = avcodec_send_frame(encodeCtx, frame);
    if(r.ret < 0){
        av_log(NULL, AV_LOG_ERROR, "send frame failed: %s\n", av_err2str(r.ret));
        return r;
    }
    while(1){
        AVPacket pkt;
        av_init_packet(&pkt);
        r.ret = avcodec_receive_packet(encodeCtx, &pkt);
        if(r.ret == 0){
            AVPacket pushedPkt = {0};
            //av_packet_move_ref(&pushedPkt, &pkt);
            ret = av_packet_ref(&pushedPkt, &pkt);
            av_packet_unref(&pkt);
            if(ret < 0){
                break;
            }
            r.pkts.push_back(pushedPkt);

        } else if (r.ret == AVERROR(EAGAIN)) {
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
