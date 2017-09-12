#ifndef AUDIO_MIXER_H
#define AUDIO_MIXER_H

extern "C" {
#include "libavutil/avutil.h"
}

#include <map>
#include <string>
#include <vector>

struct AVFilterGraph;
struct AVFilterContext;
struct AVFrame;


namespace ms {
    
typedef struct AudioFrameChannel
{
    AVFrame* frame;
    int channeltype;
    std::string volume;
} AudioFrameChannel;

typedef struct AudioFrameArgs 
{
   uint64_t channelLayout;
   AVRational timeBase;
   int rate;
   int fmt;
   std::string volume;
   int channeltype;
   bool operator == (const AudioFrameArgs& b) {
       return this->channelLayout == b.channelLayout
               && this->timeBase.den == b.timeBase.den
               && this->timeBase.num == b.timeBase.num
               && this->rate == b.rate
               && this->channeltype == b.channeltype;
   }

   bool operator != (const AudioFrameArgs& b) {
       return !(*this==b);
   }

} AudioFrameArgs;


typedef struct AudioFrameSrcs
{
    AudioFrameArgs args;
    //
    AVFilterContext* abuffersrcCtx;
    AVFilterContext* volumeCtx;
    AVFilterContext* aformatCtx;
    AVFilterContext* channelmapCtx;
    int padIndex;

} AudioFrameSrcs;

typedef struct MixResult
{
    int ret;
    std::vector<AVFrame*> frames;
} MixResult;


class AudioMixer
{
public:
    AudioMixer(std::map<int, AudioFrameArgs>& audioArgs, AudioFrameArgs& outputArgs);
    ~AudioMixer();

    int SetVolume(std::string volume, int index);
    //int SetAudioChannel(int type, int index);
    MixResult WriteFrame(std::map<int, std::vector<AudioFrameChannel>>&);

    
    bool CheckValid(std::map<int, AudioFrameArgs>& audioArgs, AudioFrameArgs& outputArgs);

    int writeFrame(AVFrame* frame, int index);
    int getFrame(AVFrame* frame);

private:
    int config(std::map<int, AudioFrameArgs>& audioArgs, AudioFrameArgs& outputArgs);
    //int writeFrame(AVFrame* frame, int index);
    //int getFrame(AVFrame* frame);
    std::map<int, AudioFrameArgs> inputAudioArgs;
    AudioFrameArgs outputAudioArgs;

//    AVFrame* dummyAudio;
    AVFilterGraph* filterGraph;
    std::map<int, AudioFrameSrcs> abuffersrcs;
    AVFilterContext* abuffersinkCtx;
    bool valid;
};

}

#endif
