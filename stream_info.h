#ifndef M3U8_ASSEMBLER_H
#define M3U8_ASSEMBLER_H

#include <vector>
#include <mutex>

struct SegmentInfo
{
    std::string file_path;
    std::string thumbnail_path;
    int64_t real_start_time; //from epoch in milliseconds
    int64_t start_time; //from 0
    int64_t duration; //in milliseconds
    int serial_num;
    int sequence_num;
};

//a serial of segments
struct SegmentList
{
    std::vector<SegmentInfo> list;
    int serial_num;
};

struct StreamInfo
{
public:
    StreamInfo();
    //StreamInfo(url);
    void InsertSegmentList();
    void AppendSegment(SegmentInfo& segment_info);
    bool AssembleM3u8ListByRealTime(std::vector<std::pair<int64_t, int64_t>> time_intervals, std::string& playlist_path);

private:
    std::vector<std::vector<SegmentInfo>> stream_lists;
    std::mutex stream_info_mutex;
    bool list_start;
};

class M3u8Assembler
{
public:
    M3u8Assembler();
    void AppendSegmentInfo(SegmentInfo& segmentInfo);
    bool AssembleByRealTime(std::vector<std::pair<double, double>>, std::string& listPath);
    bool AssembleByRelativeTime(std::vector<std::pair<double, double>>, std::string& listPath);
    bool AssembleList(std::vector<SegmentInfo>& segments, std::string& listPath);

private:
    int64_t real_start_time;
    std::vector<SegmentInfo> segmentsInfo;
    std::mutex segmentInfoMutex;
    int count;
};


#endif
