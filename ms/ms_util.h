#ifndef MIX_UTIL_H
#define MIX_UTIL_H

extern "C"{
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavformat/avformat.h>
}
#include <iostream>

//#include <glog/logging.h>
//
//extern int LOG_LEVEL;
//
//#define LOGE LOG_IF(ERROR, (LOG_LEVEL >= 1))   << "function: {" << __FUNCTION__ << "}"
//#define LOGW LOG_IF(WARNING, (LOG_LEVEL >= 2)) << "function: {" << __FUNCTION__ << "}"
//#define LOGI LOG_IF(INFO, (LOG_LEVEL >= 3))    << "function: {" << __FUNCTION__ << "}"
//#define LOGD LOG_IF(INFO, (LOG_LEVEL >= 4))    << "function: {" << __FUNCTION__ << "}"
//#define LOGE_EVERY_N(n) LOG_IF_EVERY_N(ERROR, (LOG_LEVEL >= 1), (n))   << "function: {" << __FUNCTION__ << "}"
//#define LOGW_EVERY_N(n) LOG_IF_EVERY_N(WARNING, (LOG_LEVEL >= 2), (n)) << "function: {" << __FUNCTION__ << "}"
//#define LOGI_EVERY_N(n) LOG_IF_EVERY_N(INFO, (LOG_LEVEL >= 3), (n))    << "function: {" << __FUNCTION__ << "}"
//#define LOGD_EVERY_N(n) LOG_IF_EVERY_N(INFO, (LOG_LEVEL >= 4), (n))    << "function: {" << __FUNCTION__ << "}"

#define LOGI std::cout
#define LOGW std::cout
#define LOGE std::cout




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


void InitFFmpeg();

AVFrame *AllocAudioFrame(enum AVSampleFormat sample_fmt,
                                  uint64_t channel_layout,
                                  int sample_rate, int nb_samples);


#endif // MIX_UTIL_H
