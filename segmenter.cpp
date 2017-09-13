#include "log.h"
#include "segmenter.h"
#include "audio_filter.h"
#include "ms/output_file.h"
#include "ms/muxer.h"
using namespace ms;

static const int64_t MAX_AV_DIFF = 2000; //in milliseconds

static bool GenPng(AVFrame* frame, std::string pngfilename)
{
    VideoEncodeConfig vc = {
        frame->width,
        frame->height,
        //frame->format,
        AV_PIX_FMT_YUVJ420P,
        {1,1},
        0,
        1,
        AV_CODEC_ID_MJPEG,
        "mjpeg",
        {1,1},
        0,
        ms::VBR
    };

    VideoEncoder* picEncoder = new VideoEncoder(vc);
    VideoEncodeResult r =  picEncoder->WriteFrame(frame);
    if(r.pkts.size() == 0){
        r = picEncoder->WriteFrame(NULL);
    }

    if(r.pkts.size() == 0)
        return false;

    //get a packet
    AVCodecParameters* pngCodecPar = avcodec_parameters_alloc();
    avcodec_parameters_from_context(pngCodecPar, picEncoder->encodeCtx);
    OutputFile* pngfile = new OutputFile(pngfilename, "", NULL, pngCodecPar);
    Muxer muxer(pngfile->FmtCtx);
    muxer.Write(&r.pkts[0]);
    avcodec_parameters_free(&pngCodecPar);
    delete pngfile;
    return true;
}

Segmenter::Segmenter(std::string url, int segmentTime):
    stream_info(url),
    filename(url),
    input(nullptr),
    videoDecoder(nullptr),
    audioDecoder(nullptr),
    demuxer(nullptr),
    streamEncoder(nullptr),
    segment_time(segmentTime),
    thread_exit(false),
    has_video(false),
    has_audio(false),
    last_pkt_dts(AV_NOPTS_VALUE)
{

}

void Segmenter::~Segmenter()
{
    if(videoDecoder)
        delete videoDecoder;
    if(audioDecoder)
        delete audioDecoder;
    if(input)
        delete input;
    if(demuxer)
        delete demuxer;
    if(streamEncoder)
        delete streamEncoder;
}

void Segmenter::init()
{
    input = new InputFile(filename, NULL, NULL);
    if(!input->Available()){
        dlog("open input failed");
        return;
    }

    //fetch format info
    AVFormatContext* fmt = input->FmtCtx;
    dlog("start time: %" PRId64, fmt->start_time);
    dlog("start real time: %" PRId64, fmt->start_time_realtime);

    int64_t gop_start_time = 0;
    int64_t gop_end_time = 0;
    int count = 0;
    int frame_size = 0;
    int total_frames = 0;

    //demux
    demuxer = new Demuxer(input->FmtCtx);

    bool has_video = demuxer->VideoStreamIndex < 0 ? false : true;
    bool has_audio = demuxer->AudioStreamIndex < 0 ? false : true;

    //decoder
    videoDecoder = NULL;
    audioDecoder = NULL;
    AVCodecParameters* audioPar = NULL;
    AVCodecParameters* videoPar = NULL;
    VideoEncodeConfig vc;
    AudioEncodeConfig ac;

    if(has_video) {
        //config
        videoPar = input->FmtCtx->streams[demuxer->VideoStreamIndex]->codecpar;
        videoDecoder = new Decoder(videoPar);
        vc = {
            videoPar->width,
            videoPar->height,
            videoPar->format,
            input->FmtCtx->streams[demuxer->VideoStreamIndex]->time_base,
            1000000,
            25,//gopsize
            AV_CODEC_ID_H264,
            "libopenh264",
            {1, 1},
            0,
            ms::VBR
        };
    }

    if(has_audio) {
        audioPar = input->FmtCtx->streams[demuxer->AudioStreamIndex]->codecpar;
        audioDecoder = new Decoder(audioPar);
        ac = {
            audioPar->channel_layout,
            audioPar->format,
            audioPar->sample_rate,
            0,
            AV_CODEC_ID_AAC
        };
        audioFilter = new AudioFilter();
        AudioFrameArgs afa = {
            {1, audioPar->sample_rate},
            audioPar->sample_rate,
            audioPar->channel_layout,
            std::string(av_get_sample_fmt_name((enum AVSampleFormat)audioPar->format))
        };
        audioFilter->config(afa, NULL);
    }
    //FIXME: support video only or audio only
    streamEncoder = new StreamEncoder("xx.ts", "", ac, vc);
    streamEncoder->StartEncodeThread();

}

void Segmenter::GetPlayList(std::vector<std::pair<int64_t, int64_t> > time_intervals, std::string &playlist_url)
{

}

void Segmenter::StopProcessStream()
{
    thread_exit = true;
    processThread.join();
}

void Segmenter::reinitStreamEncoder()
{
    if(streamEncoder){
        flushStreamEncoder();
    }
    //insert stream info
    stream_info.InsertSegmentList();
    streamEncoder = new StreamEncoder();
    //set streamEncoder



}

void Segmenter::flushStreamEncoder()
{
    if(!streamEncoder)
        return;
    //flush stream encoder
    streamEncoder->StopAndFlush();
    delete streamEncoder;
    streamEncoder = NULL;
}

bool Segmenter::checkPktDtsJumped(DemuxedData* data)
{
    int64_t current_dts;  // in milliseconds
    if(data->Type == PACKET_AUDIO){
        if(demuxer->AudioStreamIndex < 0)
            return false;
        AVRational audio_time_base = input->FmtCtx->streams[demuxer->AudioStreamIndex]->time_base;
        current_dts = av_rescale_q(data->Packet.dts, audio_time_base, {1, 1000});

    }else if(data->Type == PACKET_VIDEO){
        if(demuxer->VideoStreamIndex < 0)
            return false;
        AVRational video_time_base = input->FmtCtx->streams[demuxer->VideoStreamIndex]->time_base;
        current_dts = av_rescale_q(data->Packet.dts, video_time_base, {1, 1000});

    }else{
        return false;
    }

    if(last_pkt_dts == AV_NOPTS_VALUE){
        last_pkt_dts = current_dts;
        return false;
    }

    //diff in milliseconds
    int64_t diff = current_dts - last_pkt_dts;
    if(-MAX_AV_DIFF< diff && diff < MAX_AV_DIFF) {
        return false;
    }
    return true;
}

bool Segmenter::consumePacket(DemuxedData* data)
{
    if(data->Type == PACKET_AUDIO) {
        DecodeResult r = audioDecoder->WritePacket(&data->Packet);
        for(auto frame : r.frames) {
            frame->pts = av_rescale_q(frame->pts, audio_timebase, {1,frame->sample_rate});
            audioFilter->push_main_frame(frame);
            av_frame_free(&frame);
            int ret = 0;
            while(1){
                AVFrame* filter_frame = av_frame_alloc();
                ret = audioFilter->pop_frame(filter_frame);
                if(ret != 1){
                    av_frame_free(&filter_frame);
                    break;
                }
                streamEncoder->WriteAudio(filter_frame);
            }
        }

    } else if (data->Type == PACKET_VIDEO) {
        DecodeResult r =  videoDecoder->WritePacket(&data->Packet);
        for(auto frame : r.frames) {
            total_frames++;
            if(total_frames % 25 == 1){
                char pngfilename[20];
                sprintf(pngfilename, "%03d.jpeg", total_frames/25);
                GenPng(frame, pngfilename);
                dlog("generate thumb pic: %s", pngfilename);
            }
            streamEncoder->WriteVideo(frame);
        }

    } else {

    }

    av_packet_unref(&data->Packet);
    return true;
}

void Segmenter::ProcessStream()
{
    while(!thread_exit) {
        //open input and init decoder
        bool ret = init();
        //failed
        if(!ret) {
            av_usleep(40000);
            continue;
        }
        //open success, construct stream encoder
        reinitStreamEncoder();
        while(!thread_exit) {
            //construct stream encoder
            DemuxedData* data = demuxer->ReadAvPacket();
            if(data == NULL){
                dlog("read eof or null");
                break;
            }
            if(!checkPktDtsJump(data)) {
                //dts jump, reinit stream encoder
                reinitStreamEncoder();
            }
            //consume pkt data
            consumePkt(data);
        }
        flushStreamEncoder();
    }
    //
    //clean();
}
