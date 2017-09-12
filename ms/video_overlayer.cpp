extern "C" {
#include "libavfilter/avfiltergraph.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libswscale/swscale.h"
}
#include "video_overlayer.h"
#include "ms_util.h"

#include <string.h>
#include <stdint.h>
#include <algorithm>


namespace ms {

AVRational OverlayBox::DefaultTimeBase = {1,1000};

OverlayBox::OverlayBox(VideoFrameArgs mainArgs, VideoFrameArgs overlayArgs, Position c):filterGraph(NULL),
    buffersrcCtxMain(NULL),
    buffersrcCtxOverlayed(NULL),
    buffersinkCtx(NULL),
    valid(false)

{
    memcpy(&mainFrameArgs, &mainArgs, sizeof(VideoFrameArgs));   
    memcpy(&overlayedFrameArgs, &overlayArgs, sizeof(VideoFrameArgs));
    memcpy(&oc, &c, sizeof(Position));
    config();

}

OverlayBox::~OverlayBox()
{
    avfilter_graph_free(&filterGraph);
}

OverlayResult OverlayBox::Write(AVFrame* mainFrame, AVFrame* overlayedFrame)
{
    OverlayResult r;
    if(!valid){
        av_log(NULL, AV_LOG_ERROR, "video overlayer was not valid\n");
        r.ret = -1;
        return r;
    }

    if(!mainFrame || !overlayedFrame){
        av_log(NULL, AV_LOG_ERROR, "frame not null, mainFrame: %p, overlayedFrame: %p\n", mainFrame, overlayedFrame);
        r.ret = -1;
        return r;
    }
    //set pts
    //overlayedFrame->pts = mainFrame->pts;
    mainFrame->pts = overlayedFrame->pts;

    r.ret = av_buffersrc_add_frame_flags(buffersrcCtxMain, mainFrame, AV_BUFFERSRC_FLAG_KEEP_REF);

    if(r.ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "add main frame failed: %s\n", av_err2str(r.ret));
        return r;
    }

    r.ret = av_buffersrc_add_frame_flags(buffersrcCtxOverlayed, overlayedFrame, AV_BUFFERSRC_FLAG_KEEP_REF);

    if(r.ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "add overlayed frame failed: %s\n", av_err2str(r.ret));
        return r;
    }
    AVFrame* outputFrame = av_frame_alloc();
    r.ret = av_buffersink_get_frame(buffersinkCtx, outputFrame);
    if(r.ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "get output frame failed: %s\n", av_err2str(r.ret));
        return r;
    }
    r.frame = outputFrame;

    return r;
}


bool OverlayBox::CheckArgs(AVFrame* main, AVFrame* top, Position& c)
{
    if(!main || !top) {
        av_log(NULL, AV_LOG_ERROR, "NULL frame, main frame: %p, top frame: %p\n", main, top);
        return false;
    }

    if(main->width != mainFrameArgs.w
            || main->height != mainFrameArgs.h
            || main->format != mainFrameArgs.pixFmt
            || main->sample_aspect_ratio.num != mainFrameArgs.pixelAspect.num
            || main->sample_aspect_ratio.den != mainFrameArgs.pixelAspect.den)
    {
        return false;
    }


    if(top->width != overlayedFrameArgs.w
            || top->height != overlayedFrameArgs.h
            || top->format != overlayedFrameArgs.pixFmt
            || top->sample_aspect_ratio.num != overlayedFrameArgs.pixelAspect.num
            || top->sample_aspect_ratio.den != overlayedFrameArgs.pixelAspect.den)
    {
        return false;
    }

    if(c.w != oc.w
            || c.h != oc.h
            || c.x != oc.x
            || c.y != oc.y
            || (c.opacity - oc.opacity > 1e-6 || c.opacity - oc.opacity < -1e-6))
    {
        return false;
    }

    return true;
}

bool OverlayBox::IsValid()
{
    return valid;
}

void OverlayBox::config()
{
    int ret = 0;
    valid = false;

    //output format
    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };

    //init filter
    AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    AVFilter *buffersink = avfilter_get_by_name("buffersink");
    if(!buffersrc || !buffersink){
        av_log(NULL, AV_LOG_ERROR, "get filter failed\n");
        return;
    }

    AVFilterInOut *overlayed_output;
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    
    filterGraph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filterGraph) {
        av_log(NULL, AV_LOG_ERROR, "alloc outputs failed\n");
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        avfilter_graph_free(&filterGraph);
        return;
    }

    char mainFrameArgsDesc[128];
    //
    snprintf(mainFrameArgsDesc, sizeof(mainFrameArgsDesc),
             "video_size=%dx%d:"
             "pix_fmt=%d:"
             "time_base=%d/%d:"
             "pixel_aspect=%d/%d",
             mainFrameArgs.w, mainFrameArgs.h, 
             mainFrameArgs.pixFmt, 
             mainFrameArgs.timeBase.num, mainFrameArgs.timeBase.den,
             mainFrameArgs.pixelAspect.num, mainFrameArgs.pixelAspect.den
             );
    av_log(NULL, AV_LOG_INFO, "config main frame args: %s\n", mainFrameArgsDesc);

    ret = avfilter_graph_create_filter(&buffersrcCtxMain, buffersrc, "in", mainFrameArgsDesc, NULL, filterGraph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create main buffer source ctx\n");
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        avfilter_graph_free(&filterGraph);
        return;
    }

    char overlayedFrameArgsDesc[128];
    snprintf(overlayedFrameArgsDesc, sizeof(overlayedFrameArgsDesc),
             "video_size=%dx%d:"
             "pix_fmt=%d:"
             "time_base=%d/%d:"
             "pixel_aspect=%d/%d",
             overlayedFrameArgs.w, overlayedFrameArgs.h, 
             overlayedFrameArgs.pixFmt, 
             overlayedFrameArgs.timeBase.num, overlayedFrameArgs.timeBase.den,
             overlayedFrameArgs.pixelAspect.num, overlayedFrameArgs.pixelAspect.den
             );
    av_log(NULL, AV_LOG_INFO, "config overlayed frame args: %s\n", mainFrameArgsDesc);
    ret = avfilter_graph_create_filter(&buffersrcCtxOverlayed, buffersrc, "overlayed", overlayedFrameArgsDesc, NULL, filterGraph);
    if(ret < 0){
        av_log(NULL, AV_LOG_ERROR, "Cannot create overlayed buffer source ctx\n");
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        avfilter_graph_free(&filterGraph);
        return;
    }


    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersinkCtx, buffersink, "out",
                                       NULL, NULL, filterGraph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        avfilter_graph_free(&filterGraph);
        return;
    }

    ret = av_opt_set_int_list(buffersinkCtx, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        avfilter_graph_free(&filterGraph);
        return;
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
    outputs->filter_ctx = buffersrcCtxMain;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;


    overlayed_output = avfilter_inout_alloc();
    overlayed_output->name       = av_strdup("overlayed");
    overlayed_output->filter_ctx = buffersrcCtxOverlayed;
    overlayed_output->pad_idx    = 0;
    overlayed_output->next       = NULL;
    outputs->next = overlayed_output;
    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersinkCtx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    char desc[512] = {0};
    snprintf(desc, sizeof(desc),
             "[overlayed]scale=%dx%d,"
             "format=yuva420p,lutyuv=a='val*%d/100'[top];"
             "[in][top]overlay=x=%d:y=%d[out]",
             oc.w, oc.h,
             (int)(oc.opacity * 100),
             oc.x, oc.y
             );
    av_log(NULL, AV_LOG_DEBUG,"stream desc: %s\n", desc);

    if ((ret = avfilter_graph_parse_ptr(filterGraph, desc, &inputs, &outputs, NULL)) < 0) {
         av_log(NULL, AV_LOG_ERROR, "filter graph parse ptr failed\n");
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        avfilter_graph_free(&filterGraph);
        return;
    }

    if ((ret = avfilter_graph_config(filterGraph, NULL)) < 0){
        av_log(NULL, AV_LOG_ERROR, "filter graph config failed\n");
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        avfilter_graph_free(&filterGraph);
        return;
    }
    //filter is valid
    valid = true;
    
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

}


VideoOverlayer::VideoOverlayer()
{
    sws_ctx = NULL;
}

VideoOverlayer::~VideoOverlayer()
{
    resetFrameConfig();

    if(sws_ctx){
        sws_freeContext(sws_ctx);
    }

    for(auto& box : boxes) {
        if(box.second){
            delete box.second;
            box.second = NULL;
        }
    }

}

int VideoOverlayer::RescalePicture(AVFrame* frame, AVFrame* outputFrame, int w, int h)
{
    if(!frame || w == 0 ||  h == 0){
        LOGE << "rescale error, parameters wrong, "<< " frame: "<< frame << " w: "<<w<<" h: " << h;
        return -1;
    }


    enum AVPixelFormat dstFormat = (enum AVPixelFormat)frame->format;
    int dstW = w;
    int dstH = h;
    sws_ctx = sws_getCachedContext(sws_ctx,
                                         frame->width, frame->height, (enum AVPixelFormat)frame->format,
                                         dstW, dstH, dstFormat,
                                         SWS_BILINEAR,
                                         NULL, NULL, NULL);
    if(sws_ctx == NULL){
        LOGE << "sws ctx alloc failed";
        return -1;
    }

    //alloc dst buf
    outputFrame->format = dstFormat;
    outputFrame->width  = dstW;
    outputFrame->height = dstH;
    outputFrame->pts = frame->pts;

    /* allocate the buffers for the frame data */
    int ret = av_frame_get_buffer(outputFrame, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate frame data.\n");
        return -1;
    }

    //scale picture
    sws_scale(sws_ctx,
              (const uint8_t * const*)frame->data, frame->linesize,
              0, frame->height,
              outputFrame->data, outputFrame->linesize);
    return 0;
}

void VideoOverlayer::resetFrameConfig()
{
    for(auto& frameconfig : frameConfigs) {
        av_frame_unref(frameconfig.frame);
        av_frame_free(&frameconfig.frame);
        frameconfig.frame = NULL;
    }
    //reset
    frameConfigs = std::vector<FrameConfig>();
}

AVFrame* VideoOverlayer::GetFrame()
{
    if(frameConfigs.size() == 0) {
        LOGE << "there was no frame mixing";
        return NULL;
    }
    //sort by order
    std::sort(frameConfigs.begin(), frameConfigs.end(),
              [](FrameConfig a, FrameConfig b){
        return a.config.order < b.config.order;
    });

    int len = frameConfigs.size();
    int valid = 0;
    //find valid mainframe
    for(valid = 0; valid < len; valid++){
        if(frameConfigs[valid].frame){
            //mainFrame = av_frame_alloc();
            //av_frame_ref(mainFrame, frameConfigs[valid].frame);
            break;
        }
    }
    
    if(valid >= len){
        LOGE<< "there was no valid frame, input num: " << len;
        return NULL;
    }
    //valid was index of canvas
    AVFrame* canvas = frameConfigs[valid].frame;
    AVFrame* mainFrame = av_frame_alloc();
    int w = frameConfigs[valid].config.w;
    int h = frameConfigs[valid].config.h;
    int ret = 0;
    if(mainFrame->width != w || mainFrame->height != h){
        ret = RescalePicture(canvas, mainFrame, w, h);
        if(ret != 0){
            LOGE << "rescale picture failed!";
            return NULL;
        }

    }else {
        ret  = av_frame_ref(mainFrame, canvas);
        if(ret != 0){
            LOGE << "ref picture failed: " << av_err2str(ret);
            return NULL;
        }
    }

    //do vedeo mix
    for(int i = valid + 1; i < len; i++) {
        if(!frameConfigs[i].frame) {
            continue;
        }

        OverlayBox* box = findOverlayBox(mainFrame, frameConfigs[i].frame, frameConfigs[i].config, frameConfigs[i].index);
        if(!box){
            av_log(NULL, AV_LOG_ERROR, "find box overlay frame failed\n");
            continue;
        }

        OverlayResult r = box->Write(mainFrame, frameConfigs[i].frame);
        if(r.ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "overlay frame failed\n");
            continue;
        }

        av_frame_unref(mainFrame);
        av_frame_move_ref(mainFrame, r.frame);
        av_frame_free(&r.frame);
    }

    //reset frameConfig
    resetFrameConfig();
    return mainFrame;
}

int VideoOverlayer::Write(AVFrame* frame, Position& c, int index)
{
    //save frame
    if(!frame){
        av_log(NULL, AV_LOG_ERROR, "frame was NULL\n");
        return -1;
    }

    FrameConfig frameconfig = {frame, c, index};
    frameConfigs.push_back(frameconfig);
    return 0;

}

OverlayBox* VideoOverlayer::findOverlayBox(AVFrame* main, AVFrame* top, Position& c, int index)
{
    OverlayBox* box = NULL;
    //check there was valid overlay box
    if(boxes.find(index) == boxes.end()){
        av_log(NULL, AV_LOG_INFO, "can not find valid overlay box, index: %d, construct a new one\n", index);

    }else{
        box = boxes[index];
    }

    // box was valid
    if(box && box->IsValid() && box->CheckArgs(main, top, c)){
        return box;
    }

    if(box){
        delete box;
        boxes.erase(index);
    }

    //reconfig box
    if(!main || !top) {
        av_log(NULL, AV_LOG_ERROR, "frame or canvas was NULL, frame: %p, canvas: %p\n", main, top);
        return NULL;
    }

    VideoFrameArgs mainArgs = {main->width, main->height, main->format, OverlayBox::DefaultTimeBase, main->sample_aspect_ratio};
    VideoFrameArgs topArgs = {top->width, top->height, top->format, OverlayBox::DefaultTimeBase, top->sample_aspect_ratio};

    box = new OverlayBox(mainArgs, topArgs, c);
    if(!box->IsValid()) {
        av_log(NULL, AV_LOG_ERROR, "could not construct a new overlay filter box\n");
        if(box){
            delete box;
            boxes.erase(index);
        }
        return NULL;
    }
   // boxes.insert(std::pair<int, OverlayBox*>(index, box));
    boxes[index] = box;

    return box;
}

}
