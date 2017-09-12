#include "input_file.h"

namespace ms {

static int test_io_open(struct AVFormatContext *s, AVIOContext **pb, const char *url,
                   int flags, AVDictionary **options)
{
    av_log(NULL, AV_LOG_INFO, "io open s: %p, pb: %p, url: %s\n", s, *pb, url);
    return 0;
}

InputFile::InputFile(const std::string &url, int (*callback)(void*), void *opaque) {
    OpenOk = false;

    FmtCtx = avformat_alloc_context();
    av_log(NULL, AV_LOG_INFO, "alloc fmt: %p\n", FmtCtx);
    if (!FmtCtx) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
        return;
    }

    if (strstr(url.c_str(), "rtmp://")) {
        //FmtCtx->probesize = 512000;
        FmtCtx->max_analyze_duration = 0;
    }

    FmtCtx->interrupt_callback.callback = callback;
    FmtCtx->interrupt_callback.opaque = opaque;
    //FmtCtx->io_open = test_io_open;
    if (avformat_open_input(&FmtCtx, url.c_str(), NULL, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file: %s\n", url.c_str());
        return;
    }

    if (avformat_find_stream_info(FmtCtx, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        avformat_close_input(&FmtCtx);
        return;
    }

    OpenOk = true;
}

InputFile::~InputFile() {
    avformat_close_input(&FmtCtx);
    avformat_free_context(FmtCtx);
}

bool InputFile::Available() {
    return OpenOk;
}

}
