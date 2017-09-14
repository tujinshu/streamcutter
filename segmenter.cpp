#include "log.h"
#include "segmenter.h"
#include "audio_filter.h"
#include "ms/output_file.h"
#include "ms/muxer.h"
using namespace ms;

static const int64_t MAX_AV_DIFF = 2000; //in milliseconds


Segmenter::Segmenter(std::string url, int segmentTime):
    filename(url),
    input(NULL),
    videoDecoder(NULL),
    audioDecoder(NULL),
    demuxer(NULL),
    streamEncoder(NULL),
    audioFilter(NULL),
    segment_time(segmentTime),
    thread_exit(false),
    has_video(false),
    has_audio(false),
    last_pkt_dts(AV_NOPTS_VALUE),
    pvc(NULL),
    pac(NULL),
    serial_num(0)
{

}

Segmenter::~Segmenter()
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

bool Segmenter::init()
{
    input = new InputFile(filename, NULL, NULL);
    if(!input->Available()){
        dlog("open input failed");
        return false;
    }

    //fetch format info
    AVFormatContext* fmt = input->FmtCtx;
    dlog("start time: %" PRId64, fmt->start_time);
    dlog("start real time: %" PRId64, fmt->start_time_realtime);

   // int64_t gop_start_time = 0;
   // int64_t gop_end_time = 0;
   // int count = 0;
   // int frame_size = 0;
   // int total_frames = 0;

    //demux
    demuxer = new Demuxer(input->FmtCtx);

    bool has_video = demuxer->VideoStreamIndex < 0 ? false : true;
    bool has_audio = demuxer->AudioStreamIndex < 0 ? false : true;

    //decoder
    videoDecoder = NULL;
    audioDecoder = NULL;
    AVCodecParameters* audioPar = NULL;
    AVCodecParameters* videoPar = NULL;
    //VideoEncodeConfig vc, *pvc = NULL;
    //AudioEncodeConfig ac, *pac = NULL;
    pvc = NULL;
    pac = NULL;

    if(has_video) {
        //config
        videoPar = input->FmtCtx->streams[demuxer->VideoStreamIndex]->codecpar;
        videoDecoder = new Decoder(videoPar);
        vc = {
            videoPar->width,
            videoPar->height,
            videoPar->format,
            input->FmtCtx->streams[demuxer->VideoStreamIndex]->time_base,
            2000000,
            25,//gopsize
            AV_CODEC_ID_H264,
            "libopenh264",
            {1, 1},
            0,
            ms::VBR
        };
        pvc = &vc;
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
        pac = &ac;
        audioFilter = new AudioFilter();
        AudioFrameArgs afa = {
            {1, audioPar->sample_rate},
            audioPar->sample_rate,
            audioPar->channel_layout,
            std::string(av_get_sample_fmt_name((enum AVSampleFormat)audioPar->format))
        };
        audioFilter->config(afa, NULL);
    }
    if(!has_audio && !has_video){
        dlog("there was no video or audio");
        return false;
    }
    return true;
}

bool Segmenter::GetPlayList(std::vector<std::pair<int64_t, int64_t> > time_intervals, std::string &playlist_url)
{
    return stream_info.AssembleM3u8ListByRealTime(time_intervals, playlist_url);
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
    last_pkt_dts == AV_NOPTS_VALUE;
    //insert stream info
    stream_info.InsertSegmentList();
    //FIXME: support video only or audio only
    streamEncoder = new StreamEncoder("xx.ts", "", pac, pvc);
    //streamEncoder->SetSegmentTime(segment_time);
    streamEncoder->SetStreamInfo(&stream_info);
    streamEncoder->SetSerialNum(serial_num++);
    streamEncoder->StartEncodeThread();
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
    last_pkt_dts = current_dts;
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
            AVRational audio_time_base = input->FmtCtx->streams[demuxer->AudioStreamIndex]->time_base;
            frame->pts = av_rescale_q(frame->pts, audio_time_base, {1,frame->sample_rate});
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
            streamEncoder->WriteVideo(frame);
        }

    } else {

    }
    av_packet_unref(&data->Packet);
    return true;
}

//thread wrapper for process function
void Segmenter::StartProcess()
{
    processThread = std::thread(&Segmenter::processStream, this);
}

void Segmenter::processStream()
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
            if(checkPktDtsJumped(data)) {
                //dts jump, reinit stream encoder
                reinitStreamEncoder();
            }
            //consume pkt data
            consumePacket(data);
        }
        flushStreamEncoder();
    }
    //
    //clean();
}
