#ifndef MS_INPUT_FILE
#define MS_INPUT_FILE

#include <string>

extern "C"{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
#include "libavutil/time.h"
#include "libavutil/imgutils.h"
}

namespace ms {


class InputFile {
public:
    AVFormatContext *FmtCtx;

    InputFile(const std::string &url, int (*callback)(void*), void *opaque);
    ~InputFile();
    bool Available();

private:
    bool OpenOk;
};


}

#endif