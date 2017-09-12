#include "ms/ms_util.h"
#include "ms/input_file.h"
#include "ms/demuxer.h"
#include "ms/decoder.h"
#include "stream_encoder.h"

#include "log.h"
#include "segment_stream.h"
#include "segmenter.h"

#include <iostream>
#include <string>
#include <thread>

using namespace ms;


int main(int argc, char** argv)
{
    InitFFmpeg();
    //test("/home/huheng/Videos/samplemedia/Victorias.flv");
    Segmenter* segmenter = new Segmenter("rtmp://live.mudu.tv/watch/nt33at");
    //Segmenter* segmenter = new Segmenter("rtmp://mudu.ns.4l.hk/live/xx");
    //Segmenter* segmenter = new Segmenter("http://vod.mudu.tv/watch/eh0e0a_5.m3u8");
    //Segmenter* segmenter = new Segmenter("/home/huheng/Videos/samplemedia/xinwen.mp4");
    //Segmenter* segmenter = new Segmenter("/home/huheng/Videos/samplemedia/Victorias.flv");
    //Segmenter* segmenter = new Segmenter("rtmp://live.mudu.tv/watch/8ong6e");

    //test assemble
    M3u8Assembler ma;
    for(int i=0; i < 100; ++i){
        char filepath[20];
        sprintf(filepath, "%03d.ts", i);
        SegmentInfo si = {filepath, 0 + i, 0+i, 1.0, i};
        ma.AppendSegmentInfo(si);
    }
    std::vector<std::pair<double, double>> time_intervals;
    //time_intervals.push_back(std::pair<double, double>(1.0,5.0));
    time_intervals.emplace_back(1.0,5.0);
    time_intervals.emplace_back(7.0,9.0);
    //time_intervals.emplace_back(12.0,20.0);
    time_intervals.emplace_back(std::pair<double, double>(7.0,9.0));
    time_intervals.push_back(std::pair<double, double>(12.0,200.0));

    std::string filename;
    ma.AssembleByRelativeTime(time_intervals, filename);

    dlog("filename: %s", filename.c_str());

    segmenter->ProcessStream();

    return 0;
}
