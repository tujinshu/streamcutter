extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
}

#include "ms_util.h"

//int LOG_LEVEL = 0;

void InitFFmpeg()
{
    av_register_all();
    avfilter_register_all();
    avformat_network_init();
}

AVFrame *AllocAudioFrame(enum AVSampleFormat sample_fmt,
                                  uint64_t channel_layout,
                                  int sample_rate, int nb_samples)
{
    AVFrame *frame = av_frame_alloc();
    int ret;

    if (!frame) {
        //LOGE << "Error allocating an audio frame";
        return NULL;
    }

    frame->format = sample_fmt;
    frame->channel_layout = channel_layout;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;

    if (nb_samples) {
        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) {
            //LOGE << "Error allocating an audio buffer";
            av_frame_unref(frame);
            av_frame_free(&frame);
            return NULL;
        }
    }
    for(int i = 0; i < av_frame_get_channels(frame); ++i){
        memset(frame->data[i], 0, frame->linesize[0]);
    }
    return frame;
}


