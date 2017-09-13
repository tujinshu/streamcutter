
#include "ms/muxer.h"
#include "ms/output_file.h"

#include "stream_encoder.h"
#include "ms/ms_util.h"

#include <thread>
#include <chrono>
#include <cstdlib>

static void clearFrameQueue(std::deque<AVFrame*>& frameQ)
{
    for(auto* frame : frameQ){
        av_frame_free(&frame);
    }
    frameQ.clear();
}

static void clearPacketQueue(std::deque<AVPacket>& packetQ)
{
    for(auto& packet : packetQ){
        av_packet_unref(&packet);
    }
    packetQ.clear();
}

StreamEncoder::StreamEncoder(std::string outputFileName, std::string formatName, AudioEncodeConfig* ac, VideoEncodeConfig* vc):
    abortRequest(false)
{
    videoEncodeThread = NULL;
    audioEncodeThread = NULL;
    streamingOutThread = NULL;
    audioEncoder = NULL;
    videoEncoder = NULL;
    audioCodecPar = NULL;
    videoCodecPar = NULL;

    last_key_pts = AV_NOPTS_VALUE;
    current_pts = AV_NOPTS_VALUE;

    segment_count = 0;
    serial_num = 0;

    if(ac) {
        audioEncoder = new AudioEncoder(*ac);
        audioCodecPar = avcodec_parameters_alloc();
        avcodec_parameters_from_context(audioCodecPar, audioEncoder->encodeCtx);
        audioConfig = new AudioEncodeConfig();
        *audioConfig = *ac;
    }

    if(vc) {
        videoEncoder = new VideoEncoder(*vc);
        videoCodecPar = avcodec_parameters_alloc();
        avcodec_parameters_from_context(videoCodecPar, videoEncoder->encodeCtx);
        videoConfig = new VideoEncodeConfig();
        *videoConfig = *vc;
    }

    segment_info.real_start_time = av_gettime();
    segment_info.duration = 0;
    openFile();
}

StreamEncoder::~StreamEncoder()
{
    //clear queue
    clearFrameQueue(aFrameQ);
    clearFrameQueue(vFrameQ);
    clearPacketQueue(aPacketQ);
    clearPacketQueue(vPacketQ);
   
    if(audioEncoder) {
        delete audioEncoder;
    }

    if(videoEncoder) {
        delete videoEncoder;
    }
    
    if(muxer){
        delete muxer;
    }

    if(output){
        delete output;
    }
    
    if(videoCodecPar)
        delete videoCodecPar;
   
    if(audioCodecPar)
        delete audioCodecPar;
}

void StreamEncoder::openFile()
{
    char filename[20];
    char thumbnail_filename[20];
    sprintf(filename, "%03d-%05d.ts", serial_num, segment_count);
    sprintf(thumbnail_filename, "%03d-%05d.jpeg", serial_num, segment_count);
    output = new OutputFile(filename, "", audioCodecPar, videoCodecPar);
    muxer = new Muxer(output->FmtCtx);
    //
    segment_info.file_path = filename;
    segment_info.thumbnail_path = thumbnail_filename;
    segment_info.serial_num = serial_num;
    segment_info.start_time = 0;
    //add last duration
    segment_info.real_start_time += segment_info.duration;
}

void StreamEncoder::closeFile()
{
    delete output;
    output = NULL;
    delete muxer;
    muxer = NULL;

    //post request
    stream_info->AppendSegment(segment_info);

    segment_count++;
}

int StreamEncoder::WriteVideo(AVFrame* frame)
{
    if(frame->pts == AV_NOPTS_VALUE){
        LOGE<<"frame has no pts" << "width: "<< frame->width << "height: " <<frame->height;
    }
    vFrameQMutex.lock();
    vFrameQ.push_back(frame);
    vFrameQMutex.unlock();
    return 0;
}

int StreamEncoder::WriteAudio(AVFrame* frame)
{
    if(frame->pts == AV_NOPTS_VALUE){
        LOGE<<"audio frame has no pts";
    }
    aFrameQMutex.lock();
    aFrameQ.push_back(frame);
    aFrameQMutex.unlock();
    return 0;

}

void StreamEncoder::videoEncode()
{
    LOGI << "video encoder thread start";
    while(!abortRequest) {
        if(vFrameQ.size() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        //get a packet
        vFrameQMutex.lock();
        AVFrame* frame = vFrameQ.front();
        vFrameQ.pop_front();
        vFrameQMutex.unlock();

        //add filter
    
        //LOG_EVERY_N(INFO, 100) << "encoder video frame queue size: "<< vFrameQ.size();
        //LOG_IF(WARNING, vFrameQ.size() > 100)<< "encoder video frame queue size: " << vFrameQ.size();
        //LOGI << "encoder: video frame pts"<<frame->pts;
        //encoder should be valid, otherwise reopen
        VideoEncodeResult r = videoEncoder->WriteFrame(frame);
        if(r.ret < 0)
        {
            LOGE << "stream encoder: " << this << " error write frame to video encoder";
            ensureVideoEncoderValid(frame);
            av_frame_free(&frame);
            continue;
        }
        //av_frame_unref(frame);
        av_frame_free(&frame);
 
    
        //LOGI << "pkt num: "<<r.pkts.size();
        muxMutex.lock();
        for(auto& pkt : r.pkts) {
            vPacketQ.push_back(pkt);
        }
        muxMutex.unlock();
    }

}

void StreamEncoder::audioEncode()
{
    LOGI << "audio encoder thread start";
    while(!abortRequest) {
        if(aFrameQ.size() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        //get a packet
        aFrameQMutex.lock();
        AVFrame* frame = aFrameQ.front();
        aFrameQ.pop_front();
        aFrameQMutex.unlock();
        //LOG_EVERY_N(INFO, 100) << "encoder audio frame queue size: "<< aFrameQ.size();
        //LOG_IF(WARNING, aFrameQ.size() > 100)<< "encoder audio frame queue size: " << aFrameQ.size();

        //encoder should be valid, otherwise reopen
        AudioEncodeResult r = audioEncoder->WriteFrame(frame);
        if(r.ret < 0)
        {
            LOGE << "stream encoder: " << this << " error write frame to audio encoder";
            //maybe sleep 10ms
            ensureAudioEncoderValid(frame);
            av_frame_free(&frame);
            continue;
        }
        av_frame_free(&frame);
        //FIXME: ensure output muxer
        muxMutex.lock();
        for(auto& pkt : r.pkts) {
            //LOGI << "audio dts"<<pkt.dts;
            aPacketQ.push_back(pkt);

            //LOGI << "audio pkt dts: " << pkt.dts;
            //LOG_EVERY_N(INFO, 40)<<"write audio pkt: "<< pkt.dts;
            //ret = muxer->Write(&pkt);
            //if(ret < 0) {
            //    LOGE << "write audio pkt error: " << ret;
            //}
        }
        muxMutex.unlock();
    }
}


void StreamEncoder::writePacket()
{
    int ret = 0;
    muxMutex.lock();
    AVPacket pkt;
    int aQLen = aPacketQ.size();
    int vQLen = vPacketQ.size();
    if(aQLen == 0 && vQLen == 0){
        LOGE << "happen: vpacket queue 0 and apacket 0";

    }else if(aQLen == 0 && vQLen > 0){
        //output video packet
        pkt = vPacketQ.front();
        vPacketQ.pop_front();
        av_packet_rescale_ts(&pkt, videoEncoder->encodeCtx->time_base, output->VideoStream->time_base);
        pkt.stream_index = output->VideoStream->index;
        current_pts = av_rescale_q(pkt.dts, output->VideoStream->time_base, {1,1000});

    }else if(vQLen == 0 && aQLen > 0){
        //output audio packet
        pkt = aPacketQ.front();
        aPacketQ.pop_front();
        av_packet_rescale_ts(&pkt, {1, audioEncoder->encodeCtx->sample_rate}, output->AudioStream->time_base);
        pkt.stream_index = output->AudioStream->index;
        current_pts = av_rescale_q(pkt.dts, output->AudioStream->time_base, {1,1000});

    }else if(vQLen > 0 && aQLen > 0 && av_compare_ts(vPacketQ.front().dts,
                                videoEncoder->encodeCtx->time_base,
                                 aPacketQ.front().dts,
                                 {1, audioEncoder->encodeCtx->sample_rate}) < 0){
        //output video packet
        pkt = vPacketQ.front();
        vPacketQ.pop_front();
        av_packet_rescale_ts(&pkt, videoEncoder->encodeCtx->time_base, output->VideoStream->time_base);
        pkt.stream_index = output->VideoStream->index;
        current_pts = av_rescale_q(pkt.dts, output->VideoStream->time_base, {1,1000});

    }else if(vQLen > 0 && aQLen > 0 && av_compare_ts(vPacketQ.front().dts,
                                videoEncoder->encodeCtx->time_base,
                                 aPacketQ.front().dts,
                                 {1, audioEncoder->encodeCtx->sample_rate}) >= 0){
        //output audio packet
        pkt = aPacketQ.front();
        aPacketQ.pop_front();
        av_packet_rescale_ts(&pkt, {1, audioEncoder->encodeCtx->sample_rate}, output->AudioStream->time_base);
        pkt.stream_index = output->AudioStream->index;
        current_pts = av_rescale_q(pkt.dts, output->AudioStream->time_base, {1,1000});
    }else {}

    muxMutex.unlock();

    if(pkt.stream_index == output->VideoStream->index && (pkt.flags & 0x01) != 0 && last_key_pts != AV_NOPTS_VALUE) {
        std::cout << "time interval: " << pkt.dts - last_key_pts << std::endl;
        int64_t duration = av_rescale_q(pkt.dts - last_key_pts, output->VideoStream->time_base, {1, 1000});
        segment_info.duration = duration;
        last_key_pts = pkt.dts;

        closeFile();
        openFile();
    }
    ret = muxer->Write(&pkt);


    if(ret < 0) {
        LOGE<< "mux error: " << av_err2str(ret);
        //reopen formatctx
        av_usleep(1000000);

        output->ReOpen();

        delete muxer;
        muxer = new Muxer(output->FmtCtx);
    }

}


void StreamEncoder::streamingOut()
{
    LOGI << "streaming out thread start";

    while(!abortRequest) {
        if(vPacketQ.size() == 0 && aPacketQ.size() == 0) {
            av_usleep(10000);
            continue;
        }

        if(!output->Available()) {
            LOGE<< "output error, reopen"; 
            output->ReOpen();
            delete muxer;
            muxer = new Muxer(output->FmtCtx);
        }

        if(!output->Available()) {
            av_usleep(1000000);
            continue;
        }

        //detemine packet outputing
        writePacket();
    }
}


void StreamEncoder::ensureAudioEncoderValid(AVFrame* frame)
{
    audioConfig->channelLayout = frame->channel_layout;
    audioConfig->sampleFmt = frame->format;
    audioConfig->sampleRate = frame->sample_rate;
    if(audioEncoder){
        delete audioEncoder;
    }
    audioEncoder = new AudioEncoder(*audioConfig);
}

void StreamEncoder::ensureVideoEncoderValid(AVFrame* frame)
{
    videoConfig->width = frame->width;
    videoConfig->height = frame->height;
    videoConfig->videoFormat = frame->format;
    if(videoEncoder) {
        delete videoEncoder;
    }
    videoEncoder = new VideoEncoder(*videoConfig);
}

void StreamEncoder::StartEncodeThread()
{
    if(audioEncoder){
        audioEncodeThread = new std::thread(&StreamEncoder::audioEncode, this);
    }

    if(videoEncoder){
        videoEncodeThread = new std::thread(&StreamEncoder::videoEncode, this);
    }

    streamingOutThread = new std::thread(&StreamEncoder::streamingOut, this);
}

void StreamEncoder::StopEncodeThread()
{
    abortRequest = true;
    if(audioEncodeThread) {
        audioEncodeThread->join();
        delete audioEncodeThread;
        audioEncodeThread = NULL;
    }

    if(videoEncodeThread) {
        videoEncodeThread->join();
        delete videoEncodeThread;
        videoEncodeThread = NULL;
    }
    
    if(streamingOutThread) {
        streamingOutThread->join();
        delete streamingOutThread;
        streamingOutThread = NULL;
    }

}

void StreamEncoder::flushVideoFrameQ()
{
    //get a packet
    while(vFrameQ.size() > 0) {
        //vFrameQMutex.lock();
        AVFrame* frame = vFrameQ.front();
        vFrameQ.pop_front();
        //vFrameQMutex.unlock();
        VideoEncodeResult r = videoEncoder->WriteFrame(frame);
        av_frame_free(&frame);
        if(r.ret < 0)
        {
            //dlog("encoder valid, exit");
            return;
        }
        muxMutex.lock();
        for(auto& pkt : r.pkts) {
            vPacketQ.push_back(pkt);
        }
        muxMutex.unlock();
    }
}

void StreamEncoder::flushAudioFrameQ()
{
    //get a packet
    while(aFrameQ.size() > 0) {
        AVFrame* frame = aFrameQ.front();
        aFrameQ.pop_front();
        AudioEncodeResult r = audioEncoder->WriteFrame(frame);
        av_frame_free(&frame);
        if(r.ret < 0)
        {
            //dlog("encoder valid, exit");
            return;
        }
        muxMutex.lock();
        for(auto& pkt : r.pkts) {
            aPacketQ.push_back(pkt);
        }
        muxMutex.unlock();
    }

}

void StreamEncoder::flushPacketQ()
{
    //flush packet
    while(true) {
        if(vPacketQ.size() == 0 && aPacketQ.size() == 0) {
            break;
        }
        writePacket();
    }
    int64_t last_pts = av_rescale_q(last_key_pts, output->VideoStream->time_base, {1, 1000});
    segment_info.duration = current_pts - last_pts;
    closeFile();
}


//flush all frames and packets
void StreamEncoder::StopAndFlush()
{
    abortRequest = true;
    if(audioEncodeThread) {
        audioEncodeThread->join();
        delete audioEncodeThread;
        audioEncodeThread = NULL;
    }
    flushAudioFrameQ();

    if(videoEncodeThread) {
        videoEncodeThread->join();
        delete videoEncodeThread;
        videoEncodeThread = NULL;
    }
    flushVideoFrameQ();

    if(streamingOutThread) {
        streamingOutThread->join();
        delete streamingOutThread;
        streamingOutThread = NULL;
    }
    flushPacketQ();
}
