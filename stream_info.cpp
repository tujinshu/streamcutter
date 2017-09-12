#include "stream_info.h"
#include "log.h"
#include <fstream>

StreamInfo::StreamInfo():list_start(true)
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

}

M3u8Assembler::M3u8Assembler():count(0)
{

}

void M3u8Assembler::AppendSegmentInfo(SegmentInfo& segmentInfo)
{
    std::unique_lock<std::mutex> lock(segmentInfoMutex);
    segmentsInfo.push_back(segmentInfo);
}

bool M3u8Assembler::AssembleByRealTime(std::vector<std::pair<double, double>> time_intervals, std::string& listPath)
{
    //transform
    return true;
}

bool M3u8Assembler::AssembleByRelativeTime(std::vector<std::pair<double, double>> time_intervals, std::string& listPath)
{
    if(time_intervals.size() == 0){
        dlog("time intervals was null");
        return false;
    }
    //segmentInfoMutex.lock();
    std::unique_lock<std::mutex> lock(segmentInfoMutex);
    //check segments should be assembled
    int index = 0;
    int64_t start_time = time_intervals[0].first;
    int64_t end_time = time_intervals[0].second;
    std::vector<SegmentInfo> result;
    for(int i = 0; i < segmentsInfo.size(); ){
        if(segmentsInfo[i].start_time + segmentsInfo[i].duration <= start_time){
            //pass, segment ++
            i++;
            continue;
        } else if(segmentsInfo[i].start_time >= end_time){
            //next time interval
            index++;
            if(index >= time_intervals.size()){
                //finish
                break;
            }
            start_time = time_intervals[index].first;
            end_time = time_intervals[index].second;
            continue;
        } else {
            //hit
            result.push_back(segmentsInfo[i]);
            i++;
        }
    }
    return AssembleList(result, listPath);
}

bool M3u8Assembler::AssembleList(std::vector<SegmentInfo>& segments, std::string& listPath)
{
    if(segments.size() == 0){
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
    for(auto& segment : segments){
        file << "#EXTINF: " << segment.duration << std::endl;
        file << segment.filepath << std::endl;
    }
    file << "#EXT-X-ENDLIST" <<std::endl;
    return true;
}
