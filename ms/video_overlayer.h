#ifndef VIDEO_OVERLAYER_H
#define VIDEO_OVERLAYER_H

extern "C" {
#include "libavutil/avutil.h"
}

#include <vector>
#include <map>

struct AVFrame;
struct AVFilterGraph;
struct AVFilterContext;
struct SwsContext;


namespace ms {

typedef struct VideoFrameArgs{
    int w;
    int h;
    int pixFmt;
    AVRational timeBase;
    AVRational pixelAspect;

} VideoFrameArgs;

typedef struct Position {
    int w;
    int h;
    int x;
    int y;
    int order;
    double opacity;

} Position;

typedef struct OverlayResult
{
    int ret;
    AVFrame* frame;

} OverlayResult;

class OverlayBox
{
public:
    OverlayBox(VideoFrameArgs mainArgs, VideoFrameArgs overlayArgs, Position c);
    ~OverlayBox();
    //outputFrame was a pointer to an allocated frame that will be filled with data
    OverlayResult Write(AVFrame* mainFrame, AVFrame* overlayedFrame);

    bool IsValid();
    bool CheckArgs(AVFrame* main, AVFrame* top, Position& c);

    static AVRational DefaultTimeBase; //

private:
   //config filter graph
   void config();

   //maybe save the current frames' args 
   VideoFrameArgs mainFrameArgs;
   VideoFrameArgs overlayedFrameArgs;
   Position oc;
   AVFilterGraph* filterGraph;
   AVFilterContext* buffersrcCtxMain;
   AVFilterContext* buffersrcCtxOverlayed;
   AVFilterContext* buffersinkCtx;
   bool valid;
};

typedef struct FrameConfig
{
    AVFrame* frame;
    Position config;
    int index;          //stream index
} FrameConfig;

class VideoOverlayer
{
public:
    VideoOverlayer();
    ~VideoOverlayer();
    int Write(AVFrame* frame, Position& c, int index);
    AVFrame* GetFrame();

private:
    OverlayBox* findOverlayBox(AVFrame* main, AVFrame* top, Position& c, int index);
    int RescalePicture(AVFrame* frame, AVFrame* outputFrame, int w, int h);
    struct SwsContext* sws_ctx;
    void resetFrameConfig();
    std::vector<FrameConfig> frameConfigs;
    std::map<int, OverlayBox*> boxes;
};

}

#endif
