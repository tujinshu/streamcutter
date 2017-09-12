#include "output_file.h"

namespace ms {


OutputFile::OutputFile(const std::string &url, const std::string &FormatName, AVCodecParameters *AudioPar, AVCodecParameters *VideoPar) {
    OutPutUrl = url;
    format = FormatName;
    AudioCodecPar = AudioPar;
    VideoCodecPar = VideoPar;

    Open();
}

OutputFile::~OutputFile() {
    Close();
}

bool OutputFile::Available() {
    return OpenOk;
}

void OutputFile::Open() {
    AVDictionary *dict = nullptr;
    int ret = 0;

    OpenOk = false;

    /* allocate the output media context */
    if (!format.empty()) {
        avformat_alloc_output_context2(&FmtCtx, NULL, format.c_str(), OutPutUrl.c_str());
    } else {
        avformat_alloc_output_context2(&FmtCtx, NULL, NULL, OutPutUrl.c_str());
    }
    if (!FmtCtx) {
        av_log(NULL, AV_LOG_ERROR, "Could not deduce output format from file extension: using FLV.\n");
        avformat_alloc_output_context2(&FmtCtx, NULL, "flv", OutPutUrl.c_str());
    }
    if (!FmtCtx) {
        av_log(NULL, AV_LOG_ERROR, "open output file failed!\n");
        return;
    }

    if (NULL != AudioCodecPar) {
        AudioStream = avformat_new_stream(FmtCtx, NULL);

        ret = avcodec_parameters_copy(AudioStream->codecpar, AudioCodecPar);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not copy the stream parameters\n");
            return;
        }
    }

    if (NULL != VideoCodecPar) {
        VideoStream = avformat_new_stream(FmtCtx, NULL);

        ret = avcodec_parameters_copy(VideoStream->codecpar, VideoCodecPar);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not copy the stream parameters\n");
            return;
        }
        VideoStream->codecpar->codec_tag = 0;
    }

    /* open the output file, if needed */
    if (!(FmtCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&FmtCtx->pb, OutPutUrl.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open '%s': %s\n", OutPutUrl.c_str(), av_err2str(ret));
            return;
        }
    }

    if (!strcmp(format.c_str(), "mp4")) {
        av_dict_set_int(&dict, "reset_timestamps", 1, 0);
        av_dict_set(&dict, "movflags", "faststart", 0);
        av_dict_set(&dict, "movflags", "empty_moov+omit_tfhd_offset+frag_keyframe+default_base_moof", 0);
    }

    ret = avformat_write_header(FmtCtx, &dict);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when write header: %s\n", av_err2str(ret));
        Close();
        return;
    }

    av_dict_free(&dict);

    OpenOk = true;
}

void OutputFile::Close() {
    if (OpenOk) {
        av_write_trailer(FmtCtx);
        OpenOk = false;
    }
    if (FmtCtx && !(FmtCtx->oformat->flags & AVFMT_NOFILE)) {
        avio_close(FmtCtx->pb);
    }
    avformat_free_context(FmtCtx);
    FmtCtx = nullptr;
}

void OutputFile::ReOpen() {
    Close();
    Open();
}


}
