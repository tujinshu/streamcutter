#ifndef AUDIO_FILTER_H
#define AUDIO_FILTER_H
extern "C" {
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/avfiltergraph.h"
#include "libavutil/avutil.h"
#include "libavutil/channel_layout.h"
#include "libavutil/opt.h"
}

#include <string>

struct AudioFrameArgs {
    AVRational time_base;
    int sample_rate;
    int64_t channel_layout;
    std::string sample_fmt;
};


class AudioFilter {
public:
    AudioFilter():
        valid(false),
        filter_graph(NULL),
        buffersrc_ctx(NULL),
        buffersink_ctx(NULL),
        desc(NULL){}

    ~AudioFilter(){
        avfilter_graph_free(&filter_graph);
    }
    bool checkNeedReconfig(AudioFrameArgs& cur_args);
    int config(AudioFrameArgs args, const char* desc);
    bool push_main_frame(AVFrame* );
    int pop_frame(AVFrame*);
    bool valid;
private:
    AudioFrameArgs args_;
    AVFilterGraph* filter_graph;
    AVFilterContext* buffersrc_ctx;
    AVFilterContext* buffersink_ctx;
    char* desc;
};

#endif
