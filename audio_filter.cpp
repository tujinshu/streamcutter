#include "audio_filter.h"


static const char *filters_descr = "aformat=sample_fmts=fltp:channel_layouts=stereo";

bool AudioFilter::checkNeedReconfig(AudioFrameArgs& cur_args)
{
    if(!valid || cur_args.time_base.num != args_.time_base.num || 
         cur_args.time_base.den != args_.time_base.den ||
         cur_args.sample_rate != args_.sample_rate ||
         cur_args.channel_layout != args_.channel_layout ||
         cur_args.sample_fmt != args_.sample_fmt
    ){
        printf("reconfig audio filter!\n");
        return true; 
    }
    return false;
}

int AudioFilter::config(AudioFrameArgs audio_args, const char* desc)
{
    int ret = 0;
    valid = false;
    char args[512];
    enum AVSampleFormat out_sample_fmts[] = { AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_NONE };
    AVFilter *abuffersrc  = avfilter_get_by_name("abuffer");
    AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    const int64_t out_channel_layouts[] = { AV_CH_LAYOUT_STEREO, -1 };
    const int out_sample_rates[] = { audio_args.sample_rate, -1 };
    const AVFilterLink *outlink;
    //AVFormatContext* fmt_ctx = is->fmt_ctx;
    //AVCodecContext* dec_ctx = is->audioDecoder->avctx;
    //int audio_stream_index = is->audioDecoder->stream_index;
    //AVRational time_base = fmt_ctx->streams[audio_stream_index]->time_base;
    //free
    avfilter_graph_free(&filter_graph);
    filter_graph = NULL;
    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* buffer audio source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
            "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%" PRIx64,
             audio_args.time_base.num, audio_args.time_base.den, audio_args.sample_rate,
             audio_args.sample_fmt.c_str(), audio_args.channel_layout);

    ret = avfilter_graph_create_filter(&buffersrc_ctx, abuffersrc, "in",
                                       args, NULL, filter_graph);

    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        goto end;
    }

    /* buffer audio sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, abuffersink, "out",
                                       NULL, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
        goto end;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "sample_fmts", out_sample_fmts, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
        goto end;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "channel_layouts", out_channel_layouts, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
        goto end;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "sample_rates", out_sample_rates, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
        goto end;
    }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
                                        &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

    /* Print summary of the sink buffer
     * Note: args buffer is reused to store channel layout string */
    outlink = buffersink_ctx->inputs[0];
    av_get_channel_layout_string(args, sizeof(args), -1, outlink->channel_layout);
    av_log(NULL, AV_LOG_INFO, "Output: srate:%dHz fmt:%s chlayout:%s\n",
           (int)outlink->sample_rate,
           (char *)av_x_if_null(av_get_sample_fmt_name((enum AVSampleFormat)outlink->format), "?"),
           args);
    args_ = audio_args;
    valid = true;
end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return ret;
}

bool AudioFilter::push_main_frame(AVFrame* frame)
{
    if (valid && av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) >= 0) {
        av_log(NULL, AV_LOG_DEBUG, "push frame\n");
        return true;
    }
    return false;
}

int AudioFilter::pop_frame(AVFrame* frame)
{
    int ret;
    /* pull filtered frames from the filtergraph */
    //should be pop immediately
    //ret = av_buffersink_get_frame(buffersink_ctx, frame);
    ret = av_buffersink_get_samples(buffersink_ctx, frame, 1024);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
       // printf("huheng get frame from filter failed: %s!\n",ret == AVERROR(EAGAIN) ? "eagain":"error");
        return ret;
    }
    if (ret < 0){
        //printf("huheng %d\n",ret);
        return ret;
    }
    //success
    return 1;
}
