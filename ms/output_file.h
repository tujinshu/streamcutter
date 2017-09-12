#ifndef MS_OUTPUTFILE_H
#define MS_OUTPUTFILE_H

#include <string>

extern "C"{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
#include "libavutil/time.h"
#include "libavutil/imgutils.h"
}

/* avoid a temporary address return build error in c++ */
#undef av_err2str
#define av_err2str(errnum) \
    av_make_error_string((char*)__builtin_alloca(AV_ERROR_MAX_STRING_SIZE), AV_ERROR_MAX_STRING_SIZE, errnum)

#undef av_ts2str
#define av_ts2str(ts) \
    av_ts_make_string((char*)__builtin_alloca(AV_TS_MAX_STRING_SIZE), ts)

#undef av_ts2timestr
#define av_ts2timestr(ts, tb) \
    av_ts_make_time_string((char*)__builtin_alloca(AV_TS_MAX_STRING_SIZE), ts, tb)

namespace ms {

class OutputFile {
public:
    AVFormatContext *FmtCtx;
    AVStream *AudioStream;
    AVStream *VideoStream;

    OutputFile(const std::string &url, const std::string &FormatName, AVCodecParameters *AudioPar, AVCodecParameters *VideoPar);
    ~OutputFile();
    bool Available();
    void Open();
    void Close();
    void ReOpen();

private:
    std::string OutPutUrl;
    std::string format;
    AVCodecParameters *AudioCodecPar;
    AVCodecParameters *VideoCodecPar;
    bool OpenOk;
};

}

#endif