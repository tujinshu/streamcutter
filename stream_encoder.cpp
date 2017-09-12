
#include "ms/muxer.h"
#include "ms/output_file.h"

#include "stream_encoder.h"
#include "ms/ms_util.h"

#include <thread>
#include <chrono>
#include <cstdlib>

StreamEncoder::StreamEncoder(std::string outputFileName, std::string formatName, AudioEncodeConfig& ac, VideoEncodeConfig& vc):
    audioConfig(ac),
    videoConfig(vc),
    abortRequest(false)
{
    videoEncodeThread = NULL;
    audioEncodeThread = NULL;
    streamingOutThread = NULL;

    audioEncoder = new AudioEncoder(ac);
    videoEncoder = new VideoEncoder(vc);
    videoCodecPar = avcodec_parameters_alloc();
    audioCodecPar = avcodec_parameters_alloc();
    avcodec_parameters_from_context(videoCodecPar, videoEncoder->encodeCtx);
    avcodec_parameters_from_context(audioCodecPar, audioEncoder->encodeCtx);
    output = new OutputFile(outputFileName, formatName, audioCodecPar, videoCodecPar);
    muxer = new Muxer(output->FmtCtx);
    //delete videoCodecPar;
    //delete audioCodecPar;
    delaySec = 0;
}

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

void StreamEncoder::SetDelaySec(int sec)
{
    delaySec = sec;
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

//only using in streamingOut
void StreamEncoder::clearExpiredPackets()
{
    muxMutex.lock();
    LOGW << "clear expired packets";
    while(vPacketQ.size() > 0 &&
          av_rescale_q(vPacketQ.back().dts - vPacketQ.front().dts, videoEncoder->encodeCtx->time_base, {1, 1000}) > delaySec * 1000) 
    {
        AVPacket pkt = vPacketQ.front();
        vPacketQ.pop_front();
        av_packet_unref(&pkt);
    }
   
    while(aPacketQ.size() > 0 &&
          av_rescale_q(aPacketQ.back().dts - aPacketQ.front().dts, {1, audioEncoder->encodeCtx->sample_rate}, {1, 1000}) > delaySec * 1000) 
    {
        AVPacket pkt = aPacketQ.front();
        aPacketQ.pop_front();
        av_packet_unref(&pkt);
    }

    muxMutex.unlock();
}


void StreamEncoder::streamingOut()
{
    LOGI << "streaming out thread start";
    int ret = 0;
    //int count = 0;
    //int64_t start_time = av_gettime_relative();
    int segment_count = 0;
    int64_t last_pts = 0;

    while(!abortRequest){
        if(vPacketQ.size() == 0 && aPacketQ.size() == 0) {
            av_usleep(10000);
            continue;
        }

        if(!output->Available()){
            LOGE<< "output error, reopen"; 
            output->ReOpen();
            delete muxer;
            muxer = new Muxer(output->FmtCtx);
        }

        if(!output->Available()){
            clearExpiredPackets();           
            av_usleep(1000000);
            continue;
        }

        //detemine packet outputing
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
            //ret = muxer->Write(&pkt);

        }else if(vQLen == 0 && aQLen > 0){
            //output audio packet
            pkt = aPacketQ.front();
            aPacketQ.pop_front();
            av_packet_rescale_ts(&pkt, {1, audioEncoder->encodeCtx->sample_rate}, output->AudioStream->time_base);
            pkt.stream_index = output->AudioStream->index;
            //ret = muxer->Write(&pkt);

        }else if(vQLen > 0 && aQLen > 0 && av_compare_ts(vPacketQ.front().dts,
                                    videoEncoder->encodeCtx->time_base,
                                     aPacketQ.front().dts,
                                     {1, audioEncoder->encodeCtx->sample_rate}) < 0){
            //output video packet
            pkt = vPacketQ.front();
            vPacketQ.pop_front();
            av_packet_rescale_ts(&pkt, videoEncoder->encodeCtx->time_base, output->VideoStream->time_base);
            pkt.stream_index = output->VideoStream->index;
            //ret = muxer->Write(&pkt);

        }else if(vQLen > 0 && aQLen > 0 && av_compare_ts(vPacketQ.front().dts,
                                    videoEncoder->encodeCtx->time_base,
                                     aPacketQ.front().dts,
                                     {1, audioEncoder->encodeCtx->sample_rate}) >= 0){
            //output audio packet
            pkt = aPacketQ.front();
            aPacketQ.pop_front();
            av_packet_rescale_ts(&pkt, {1, audioEncoder->encodeCtx->sample_rate}, output->AudioStream->time_base);
            pkt.stream_index = output->AudioStream->index;
            //ret = muxer->Write(&pkt);

        }else {}

        muxMutex.unlock();

        if(pkt.stream_index == output->VideoStream->index && pkt.flags & 0x01 != 0) {
            std::cout << "time interval: " << pkt.dts - last_pts << std::endl;
            last_pts = pkt.dts;

            //segment++
            char filename[10];
            sprintf(filename, "%03d.ts", segment_count);
            segment_count++;
            delete muxer;
            delete output;
            output = new OutputFile(filename, "", audioCodecPar, videoCodecPar);
            muxer = new Muxer(output->FmtCtx);
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
}


void StreamEncoder::ensureAudioEncoderValid(AVFrame* frame)
{
    audioConfig.channelLayout = frame->channel_layout;
    audioConfig.sampleFmt = frame->format;
    audioConfig.sampleRate = frame->sample_rate;
    if(audioEncoder){
        delete audioEncoder;
    }
    audioEncoder = new AudioEncoder(audioConfig);
}

void StreamEncoder::ensureVideoEncoderValid(AVFrame* frame)
{
    videoConfig.width = frame->width;
    videoConfig.height = frame->height;
    videoConfig.videoFormat = frame->format;
    if(videoEncoder) {
        delete videoEncoder;
    }
    videoEncoder = new VideoEncoder(videoConfig);
}

void StreamEncoder::StartEncodeThread()
{
    audioEncodeThread = new std::thread(&StreamEncoder::audioEncode, this);
    videoEncodeThread = new std::thread(&StreamEncoder::videoEncode, this);
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
    int ret = muxer->Write(NULL);
    delete muxer;
    delete output;
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
