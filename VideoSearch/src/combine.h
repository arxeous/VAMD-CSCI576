#pragma once
#include <string>
#include <unordered_map>
#include <map> 
#include <chrono>

// Output format
typedef struct  {
    int final_video_prediction;
    int final_frame_prediction;
    double final_second_prediction;
}Result;

double error(std::string query_video, int src_video, int start_frame, double threshold = 0.5);
Result audioFrame(std::string query_audio, std::unordered_map<std::size_t, int> original_fingerprints[]);
Result shotFrame(std::string query_vid, std::map<std::string, std::pair<std::vector<int>, std::vector<int>>> shotBoundariesMap);
Result colorFrame(std::string query_vid);
Result startFrame(std::string query_vid, std::string query_audio, 
    std::unordered_map<std::size_t, int> original_fingerprints[],
    std::map<std::string, std::pair<std::vector<int>, std::vector<int>>> shotBoundariesMap
);
void test();