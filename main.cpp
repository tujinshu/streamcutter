#include "ms/ms_util.h"
#include "ms/input_file.h"
#include "ms/demuxer.h"
#include "ms/decoder.h"
#include "stream_encoder.h"

#include "log.h"
#include "stream_info.h"
#include "segmenter.h"

#include <iostream>
#include <string>
#include <thread>

using namespace ms;


int main(int argc, char** argv)
{
    InitFFmpeg();
    //test("/home/huheng/Videos/samplemedia/Victorias.flv");
    //Segmenter* segmenter = new Segmenter("rtmp://live.mudu.tv/watch/nt33at", 1000);
    //Segmenter* segmenter = new Segmenter("http://1212.long-vod.cdn.aodianyun.com/u/1212/m3u8/640x360/7e0cd690f4c6a44beee8b047a84bde03/7e0cd690f4c6a44beee8b047a84bde03.m3u8", 1000);
    //Segmenter* segmenter = new Segmenter("rtmp://mudu.ns.4l.hk/live/xx");
    //Segmenter* segmenter = new Segmenter("http://vod.mudu.tv/watch/eh0e0a_5.m3u8");
    //Segmenter* segmenter = new Segmenter("/home/huheng/Videos/samplemedia/xinwen.mp4");
    //Segmenter* segmenter = new Segmenter("/home/huheng/Videos/samplemedia/Victorias.flv");
    Segmenter* segmenter = new Segmenter("rtmp://live.mudu.tv/watch/8ong6e", 1000);

    segmenter->StartProcess();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    //test playlist
    std::vector<std::pair<int64_t, int64_t>> time_intervals;
    int64_t current_time = av_gettime() / 1000;
    time_intervals.emplace_back(current_time - 9000, current_time - 2000);
    //time_intervals.emplace_back(7.0,9.0);
    std::string playlist;
    segmenter->GetPlayList(time_intervals, playlist);
    std::cout << "play list url: " << playlist << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(10000));

    return 0;
}
