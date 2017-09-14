#include "stream_info.h"
#include "log.h"
#include <fstream>

StreamInfo::StreamInfo():list_start(true), count(0)
{

}

void StreamInfo::InsertSegmentList()
{
    std::unique_lock<std::mutex> lock(stream_info_mutex);
    stream_lists.push_back(std::vector<SegmentInfo>());
    list_start = true;
}

void StreamInfo::AppendSegment(SegmentInfo &segment_info)
{
    std::unique_lock<std::mutex> lock(stream_info_mutex);
    int length = stream_lists.size();
    if(length <= 0){
        dlog("there was no list to append segment");
        return;
    }
    stream_lists[length - 1].push_back(segment_info);
}

bool StreamInfo::AssembleM3u8ListByRealTime(std::vector<std::pair<int64_t, int64_t> > time_intervals, std::string &playlist_path)
{
    if(time_intervals.size() == 0){
        dlog("time intervals was null");
        return false;
    }
    std::unique_lock<std::mutex> lock(stream_info_mutex);
    //check segments should be assembled
    size_t index = 0;
    int64_t start_time = time_intervals[0].first;
    int64_t end_time = time_intervals[0].second;
    std::vector<std::vector<SegmentInfo>> result;
    result.push_back(std::vector<SegmentInfo>());
    bool finish = false;
    for(size_t m = 0; m < stream_lists.size() && !finish; ++m){
        int length = result.size();
        //new list
        if(result[length - 1].size() != 0){
            result.push_back(std::vector<SegmentInfo>());
        }
        for(size_t i = 0; i < stream_lists[m].size(); ){
            SegmentInfo& seg_info = stream_lists[m][i];
            if(seg_info.real_start_time + seg_info.duration <= start_time){
                //pass
                i++;
                continue;
            }
            if(seg_info.real_start_time >= end_time){
                //check next time interval
                index++;
                if(index >= time_intervals.size()){
                    //finish
                    finish = true;
                    break;
                }
                //reset start and end time
                start_time = time_intervals[index].first;
                end_time = time_intervals[index].second;
                int length = result.size();
                //new list
                if(result[length - 1].size() != 0){
                    result.push_back(std::vector<SegmentInfo>());
                }
                continue;
            }
            //hit segment
            int length = result.size();
            result[length - 1].push_back(seg_info);
            ++i;
        }
    }

    return AssembleList(result, playlist_path);
}

bool StreamInfo::AssembleList(std::vector<std::vector<SegmentInfo>>& segments, std::string& listPath)
{
    int length = segments.size();
    if(length == 0 || segments[length - 1].size() == 0) {
        dlog("segment was null");
        return false;
    }

    char filename[20];
    sprintf(filename, "join-%d.m3u8", count);
    listPath = filename;
    count++;
    std::ofstream file(filename);
    file << "#EXTM3U" << std::endl;
    file << "#EXT-X-VERSION:3" <<std::endl;
    file << "#EXT-X-MEDIA-SEQUENCE:0" << std::endl;
    file << "#EXT-X-TARGETDURATION:5" << std::endl;

    for(size_t m = 0; m < segments.size(); ++m) {
        if(m != 0 && segments[m].size() != 0) {
            //add discontinue tag
            file << "#EXT-X-DISCONTINUITY" << std::endl;
        }

        for(size_t i = 0; i < segments[m].size(); ++i) {
            file << "#EXTINF: " << segments[m][i].duration / 1000 << std::endl;
            file << segments[m][i].file_path << std::endl;
        }
    }

    file << "#EXT-X-ENDLIST" <<std::endl;
    return true;
}
