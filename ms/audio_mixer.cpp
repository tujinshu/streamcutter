extern "C" {
#include "libavfilter/avfiltergraph.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
}

#include "audio_mixer.h"
#include "ms_util.h"


namespace ms {

static int CreateAbufferSrc(AVFilterContext** filterContext, AudioFrameArgs& audioArg, char* name, AVFilterGraph* graphCtx)
{
    int ret = 0;
    if (!graphCtx) {
        av_log(NULL, AV_LOG_ERROR, "graph was null\n");
        return -1;
    }

    char args[256];
    AVFilter* abuffersrcFilter = avfilter_get_by_name("abuffer");
    if(!abuffersrcFilter) {
        av_log(NULL, AV_LOG_ERROR, "get filter failed\n");
        return -1;
    }

    snprintf(args, sizeof(args),
             "time_base=%d/%d:"
             "sample_rate=%d:"
             "sample_fmt=%s:"
             "channel_layout=0x%" PRIx64,
             audioArg.timeBase.num, audioArg.timeBase.den,
             audioArg.rate,
             av_get_sample_fmt_name((enum AVSampleFormat)audioArg.fmt), 
             audioArg.channelLayout);

    ret = avfilter_graph_create_filter(filterContext, abuffersrcFilter, name, args, NULL, graphCtx);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        return ret;
    }
    return 0;
}

static int CreateVolumeFilterCtx(AVFilterContext** filterContext, char* volumeDesc, char* name, AVFilterGraph* graphCtx)
{
    int ret = 0;
    if (!graphCtx) {
        av_log(NULL, AV_LOG_ERROR, "graph was null\n");
        return -1;
    }

    AVFilter* volumeFilter = avfilter_get_by_name("volume");
    if (!volumeFilter) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the volume filter\n");
        return -1;
    }


    ret = avfilter_graph_create_filter(filterContext, volumeFilter, name, volumeDesc, NULL, graphCtx);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not create volume filter context\n");
        return ret;
    }
    return 0;

}

static int CreateAformatFilterCtx(AVFilterContext** filterContext, char* aformatDesc, char* name, AVFilterGraph* graphCtx)
{
    int ret = 0;
    if (!graphCtx) {
        av_log(NULL, AV_LOG_ERROR, "graph was null\n");
        return -1;
    }
    
    AVFilter* aformatFilter = avfilter_get_by_name("aformat");
    if (!aformatFilter) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the aformat filter\n");
        return -1;
    }
    
    ret = avfilter_graph_create_filter(filterContext, aformatFilter, name, aformatDesc, NULL, graphCtx);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not create aformatFilter filter context\n");
        return ret;
    }
    return 0;
    
}
    
static int CreateChannelMapFilterCtx(AVFilterContext** filterContext, char* channelmapDesc, char* name, AVFilterGraph* graphCtx)
{
    int ret = 0;
    if (!graphCtx) {
        av_log(NULL, AV_LOG_ERROR, "graph was null\n");
        return -1;
    }
    
    AVFilter* channelmapFilter = avfilter_get_by_name("channelmap");
    if (!channelmapFilter) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the channelmap filter\n");
        return -1;
    }
    
    
    ret = avfilter_graph_create_filter(filterContext, channelmapFilter, name, channelmapDesc, NULL, graphCtx);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not create channelmapFilter filter context\n");
        return ret;
    }
    return 0;
    
}


static int CreateAudioMixerFilterCtx(AVFilterContext** filterContext, int inputNum, AVFilterGraph* graphCtx)
{
    AVFilter* mix_filter = avfilter_get_by_name("amix_mudu");
    if (!mix_filter) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the amix_mudu filter.\n");

        mix_filter = avfilter_get_by_name("amix");
        if (!mix_filter) {
            return -1;
        }
    }
    char args[128];
    //numbers of audio_to_mix
    snprintf(args, sizeof(args), "inputs=%d:duration=first:dropout_transition=3", inputNum);
    
    int ret = avfilter_graph_create_filter(filterContext, mix_filter, "amix_mudu",
                                       args, NULL, graphCtx);

    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio amix filter\n");
        return ret;
    }
    return ret;

}

static int CreateAbuffersink(AVFilterContext** filterContext, AudioFrameArgs& audioArgs, AVFilterGraph* filterGraph)
{
    AVFilter* abuffersinkFilter = avfilter_get_by_name("abuffersink");
    if (!abuffersinkFilter) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the abuffersink filter.\n");
        return -1;
    }
    
    *filterContext = avfilter_graph_alloc_filter(filterGraph, abuffersinkFilter, "sink");
    AVFilterContext* sinkCtx = *filterContext;
    if (!sinkCtx) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate the abuffersink instance.\n");
        return -1;
    }
    enum AVSampleFormat sample_fmts[] = { (enum AVSampleFormat)(audioArgs.fmt), AV_SAMPLE_FMT_NONE };
    /* Same sample fmts as the output file. */
    int ret = av_opt_set_int_list(sinkCtx, "sample_fmts",
                              sample_fmts,
                              AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could set abuffersink_ctx.\n");
        return ret;
    }

    int out_sample_rates[] = {audioArgs.rate, -1};
    ret = av_opt_set_int_list(sinkCtx, "sample_rates", out_sample_rates, -1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could set sample_rates to the abuffersink instance: %s\n", av_err2str(ret));
        return ret;
    }
   // uint8_t ch_layout[64];
  //  av_get_channel_layout_string((char *)ch_layout, sizeof(ch_layout), 0, audioArgs.channelLayout);
 //   std::string tt((char*)ch_layout);
//    ret = av_opt_set(sinkCtx, "channel_layouts", (char *)ch_layout, AV_OPT_SEARCH_CHILDREN);
    uint64_t out_channel_layouts[] = {audioArgs.channelLayout, (uint64_t)-1};
    ret = av_opt_set_int_list(sinkCtx, "channel_layouts", out_channel_layouts, -1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could set channel_layouts to the abuffersink instance: %s\n", av_err2str(ret));
        return ret;
    }

    ret = avfilter_init_str(sinkCtx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not initialize the abuffersink instance.\n");
        return ret;
    }
    return 0;

}


AudioMixer::AudioMixer(std::map<int, AudioFrameArgs>& audioArgs,
                       AudioFrameArgs& outputArgs):
    inputAudioArgs(audioArgs),
    outputAudioArgs(outputArgs),
    //dummyAudio(NULL),
    filterGraph(NULL),
    abuffersinkCtx(NULL),
    valid(false)
{
    config(audioArgs, outputArgs);
}

AudioMixer::~AudioMixer()
{
    avfilter_graph_free(&filterGraph);
}

int AudioMixer::SetVolume(std::string volume, int index)
{
    char target[64];
    char res[64];
    snprintf(target, sizeof(target), "inputVolume%d", index);
    int ret = avfilter_graph_send_command(filterGraph, target, "volume", volume.c_str(), res, sizeof(res), AVFILTER_CMD_FLAG_ONE);
    return ret;

}

//int AudioMixer::SetAudioChannel(int type, int index)
//{
//    int ret = 0;
//    inputAudioArgs[index].channeltype = type;
//    return ret;
//    
//}

int AudioMixer::writeFrame(AVFrame* frame, int index)
{
    int ret = av_buffersrc_add_frame_flags(abuffersrcs[index].abuffersrcCtx, frame, AV_BUFFERSRC_FLAG_KEEP_REF | AV_BUFFERSRC_FLAG_PUSH);
    return ret;
}

int AudioMixer::getFrame(AVFrame* frame)
{
    int ret = av_buffersink_get_samples(abuffersinkCtx, frame, 1024);
    return ret;
}


MixResult AudioMixer::WriteFrame(std::map<int, std::vector<AudioFrameChannel>>& framesmap)
{
    MixResult r;
    //send frames
    for(auto& framespair : framesmap){
        int index = framespair.first;
        for(auto& audioframechannel : framespair.second){
            r.ret = writeFrame(audioframechannel.frame, index);
            //set volume
            if(audioframechannel.volume != inputAudioArgs[index].volume){

                SetVolume(audioframechannel.volume, index);
                inputAudioArgs[index].volume = audioframechannel.volume;
            }
            av_frame_free(&audioframechannel.frame);
            if(r.ret < 0){
                av_log(NULL, AV_LOG_ERROR, "mixer write frame error, index: %d, error: %s", index, av_err2str(r.ret));
                return r;
            }
        }
    }

    //get frames
    while(1){
        AVFrame* frame = av_frame_alloc();
        r.ret = getFrame(frame);
        if(r.ret == 0){
            r.frames.push_back(frame);

        } else if(r.ret == AVERROR(EAGAIN)){
            av_frame_free(&frame);
            r.ret = 0;
            break;

        }else {
            av_frame_free(&frame);
            break;
        }
    }
    return r;

}

bool AudioMixer::CheckValid(std::map<int, AudioFrameArgs>& audioArgs, AudioFrameArgs& outputArgs)
{
   // if(audioArgs.size() != inputAudioArgs.size()) {
   //     return false;
   // }

    if(outputArgs != outputAudioArgs){
        return false;
    }

    for(auto& audioArg : audioArgs) {
        if(inputAudioArgs.find(audioArg.first) == inputAudioArgs.end())
            return false;
        if(inputAudioArgs[audioArg.first] != audioArg.second)
            return false;
    }

    return true;

}

int AudioMixer::config(std::map<int, AudioFrameArgs>& audioArgs, AudioFrameArgs& outputArgs)
{
    int ret = 0;
    filterGraph = avfilter_graph_alloc();
    if (!filterGraph) {
        av_log(NULL, AV_LOG_ERROR, "Unable to create filter graph.\n");
        return -1;
    }

    //create abuffersrc filter ctx and volume filter ctx
    int padIndex = 0;
    for(auto& audioArg : audioArgs) {
        AVFilterContext* abuffersrc;
        char abuffersrcName[64];
        snprintf(abuffersrcName, sizeof(abuffersrcName), "abuffersrc%d", audioArg.first);
        ret = CreateAbufferSrc(&abuffersrc, audioArg.second, abuffersrcName, filterGraph);
        if(ret < 0){
            av_log(NULL, AV_LOG_ERROR, "create abuffersrc failed: %d\n", audioArg.first);
            return ret;
        }

        //create volume filter ctx
        AVFilterContext* volumeCtx;
        char audioVolumeDesc[64];

        if(audioArg.second.volume == ""){
            snprintf(audioVolumeDesc, sizeof(audioVolumeDesc), "volume=1.0");
        } else {
            snprintf(audioVolumeDesc, sizeof(audioVolumeDesc), "volume=%s", audioArg.second.volume.c_str());
        }
        char audioVolumeName[64];
        snprintf(audioVolumeName, sizeof(audioVolumeName), "inputVolume%d", audioArg.first);

        ret = CreateVolumeFilterCtx(&volumeCtx, audioVolumeDesc, audioVolumeName, filterGraph);
        if(ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "create volume failed: %d\n", audioArg.first);
            return ret;
        }
        
        //create aformat filter ctx
        AVFilterContext* aformatCtx;
        char aformatDesc[64];
        
        snprintf(aformatDesc, sizeof(aformatDesc), "channel_layouts=0x3");
        
        char aformatName[64];
        snprintf(aformatName, sizeof(aformatName), "inputaformat%d", audioArg.first);
        
        ret = CreateAformatFilterCtx(&aformatCtx, aformatDesc, aformatName, filterGraph);
        if(ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "create aformat failed: %d\n", audioArg.first);
            return ret;
        }
        
        //create channelmap filter ctx
        AVFilterContext* channelmapCtx;
        char channelmapDesc[64];
        
        switch (audioArg.second.channeltype) {
            case 1://Left
                snprintf(channelmapDesc, sizeof(channelmapDesc), "map=0|0");
                break;
            case 2://Right
                snprintf(channelmapDesc, sizeof(channelmapDesc), "map=1|1");
                break;
            default://Both
                snprintf(channelmapDesc, sizeof(channelmapDesc), "map=0|1");
            break;
        }
        
        av_log(NULL, AV_LOG_INFO, "channelmapconfigtype index:%d type:%d map:%s\n", audioArg.first, audioArg.second.channeltype, channelmapDesc);
        
        char channelmapName[64];
        snprintf(channelmapName, sizeof(channelmapName), "inputchannelmap%d", audioArg.first);
        
        ret = CreateChannelMapFilterCtx(&channelmapCtx, channelmapDesc, channelmapName, filterGraph);
        if(ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "create channelmap failed: %d\n", audioArg.first);
            return ret;
        }

        //save abuffersrc
        //abuffersrcs.insert(std::pair<int, AudioFrameSrcs>(audioArg.first, {audioArg.second, abuffersrc, volumeCtx, padIndex}));
        abuffersrcs[audioArg.first] = {audioArg.second, abuffersrc, volumeCtx,aformatCtx, channelmapCtx, padIndex};
        padIndex++;
    }

    //create amix
    AVFilterContext* amixerFilterCtx;
    int inputNum = audioArgs.size();
    ret = CreateAudioMixerFilterCtx(&amixerFilterCtx, inputNum, filterGraph);
    if(ret < 0){
        av_log(NULL, AV_LOG_ERROR, "create audio mixer filter ctx failed\n");
        return ret;
    }

    //create abuffersink
    ret = CreateAbuffersink(&abuffersinkCtx, outputArgs, filterGraph);
    if(ret < 0){
        av_log(NULL, AV_LOG_ERROR, "create abuffersink ctx failed\n");
        return ret;
    }


    //create output volume filter ctx
    AVFilterContext* outputVolumeCtx;
    char outputVolumeDesc[64];
    snprintf(outputVolumeDesc, sizeof(outputVolumeDesc), "volume=1.0");
    char outputVolumeName[64];
    snprintf(outputVolumeName, sizeof(outputVolumeName), "outputVolume");

    ret = CreateVolumeFilterCtx(&outputVolumeCtx, outputVolumeDesc, outputVolumeName, filterGraph);
    if(ret < 0){
        av_log(NULL, AV_LOG_ERROR, "create output volume failed\n");
        return ret;
    }

    //link buffersrc and volume
    for(auto& abuffersrc : abuffersrcs) {
        //link filter
        ret = avfilter_link(abuffersrc.second.abuffersrcCtx, 0, abuffersrc.second.volumeCtx, 0);
        if(ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "link src and volume failed: %d\n", abuffersrc.first);
            return ret;
        }

        ret = avfilter_link(abuffersrc.second.volumeCtx, 0, abuffersrc.second.aformatCtx, 0);
        if(ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "link volume and channelmap failed: %d\n", abuffersrc.first);
            return ret;
        }
        
        ret = avfilter_link(abuffersrc.second.aformatCtx, 0, abuffersrc.second.channelmapCtx, 0);
        if(ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "link channelmap and mixer failed: %d\n", abuffersrc.first);
            return ret;
        }
        
        ret = avfilter_link(abuffersrc.second.channelmapCtx, 0, amixerFilterCtx, abuffersrc.second.padIndex);
        if(ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "link channelmap and mixer failed: %d\n", abuffersrc.first);
            return ret;
        }

    }

    //link mixer and volume
    ret = avfilter_link(amixerFilterCtx, 0, outputVolumeCtx, 0);
    if(ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "link mixer and output volume failed\n");
        return ret;
    }

    //link output volume and buffer sink
    ret = avfilter_link(outputVolumeCtx, 0, abuffersinkCtx, 0);
    if(ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "link output volume and abuffersink failed\n");
        return ret;
    }

    //config filter graph
    ret = avfilter_graph_config(filterGraph, NULL);
    if(ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "config filter graph failed\n");
    }

    //dump
    char* dump =avfilter_graph_dump(filterGraph, NULL);
    av_log(NULL, AV_LOG_INFO, "Audio Graph :\n%s\n", dump);
    //av_log(NULL, AV_LOG_INFO, "Audio Graph : reconfig\n");
    av_free(dump);
    valid = true;
    return 0;
}

}

