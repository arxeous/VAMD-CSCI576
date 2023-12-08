#include <iostream>
#include "combine.h"

// Libraries
#include <cstdio>
#include <stdio.h> 
#include <vector>
#include <map> 
#include <fstream>
#include <string>
#include <filesystem>
#include "opencv2/opencv.hpp" 
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/videoio.hpp"

//#include "Dense"

// Audio files
#include "AudioFile.h"
#include "AudioFP.h"

// Shot boundary files
#include "ShotBoundaries.h"

// Color files
#include "TestColor.h"


double error(std::string q_video, int src_video, int start_frame, double threshold) {

    std::string source_video = "./orig_mp4/video" + std::to_string(src_video) + ".mp4";
    std::string query_video = "./query_mp4/" + q_video;

    cv::VideoCapture ogVid(source_video);

    if (!ogVid.isOpened()) {
        std::cerr << "Error opening " << source_video << std::endl;
        return -1;
    }

    cv::VideoCapture queryVid(query_video);

    if (!queryVid.isOpened()) {
        std::cerr << "Error opening " << query_video << std::endl;
        return -1;
    }

    // get indexes
    int ogVidIndex = start_frame;
    int queryVidIndex = 0;

    ogVid.set(cv::CAP_PROP_POS_FRAMES, ogVidIndex);
    cv::Mat frame1;
    ogVid.read(frame1);

    //queryVid.set(cv::CAP_PROP_POS_FRAMES, queryVidIndex);
    cv::Mat frame2;
    queryVid.read(frame2);

    // Convert frames to grayscale
    cv::Mat grayFrame1, grayFrame2;
    cv::cvtColor(frame1, grayFrame1, cv::COLOR_BGR2GRAY);
    cv::cvtColor(frame2, grayFrame2, cv::COLOR_BGR2GRAY);

    // Compute the absolute difference
    cv::Mat frameDifference;
    cv::absdiff(grayFrame1, grayFrame2, frameDifference);

    // count how many non-zeros there are 
    int nonZeroPixels = cv::countNonZero(frameDifference);
    double matchThreshold = 100;

    ogVid.release();
    queryVid.release();

    if (nonZeroPixels < matchThreshold) {
        std::cout << "Frames at indices " << ogVidIndex << " and " << queryVidIndex << " match!" << std::endl;
    }
    else {
        std::cout << "Frames at indices " << ogVidIndex << " and " << queryVidIndex << " do not match." << std::endl;
    }

    std::cout << "Total non-zeros are ..." << nonZeroPixels << std::endl;
    return true;
}

Result audioFrame(std::string query_audio, std::unordered_map<std::size_t, int> original_fingerprints[]) {
    int final_video_prediction = 0;
    int final_frame_prediction = 0;
    double final_second_prediction = 0; 
 
    // Do Shazam 
    shazam(query_audio, 20, original_fingerprints, &final_video_prediction, &final_second_prediction, &final_frame_prediction);
    return { final_video_prediction, final_frame_prediction, final_second_prediction};
}

Result shotFrame(std::string query_vid, std::map<std::string, std::pair<std::vector<int>, std::vector<int>>> shotBoundariesMap) {
    return { -1, -1, -1 };
}

Result colorFrame(std::string q_video) {
    std::string query_video = "./query_mp4/" + q_video;
    std::pair<int, int> output = searchForQuery(query_video);
    int final_video_prediction = output.first;
    int final_frame_prediction = output.second;
    double final_second_prediction = output.second / 30;
    return { final_video_prediction, final_frame_prediction, final_second_prediction };
}

Result startFrame(std::string query_vid, std::string query_audio, std::unordered_map<std::size_t, int> original_fingerprints[], std::map<std::string, std::pair<std::vector<int>, std::vector<int>>> shotBoundariesMap) {
    
    auto start_clock = std::chrono::high_resolution_clock::now();
    int src_video;
    int frame;
    double seconds;
    Result output;

    /*output = shotFrame(query_vid);
    src_video = output.final_video_prediction;
    seconds = output.final_second_prediction;
    frame = output.final_frame_prediction;

    if (src_video != -1 && correct(query_vid, src_video, frame)) {
        std::cout << "Frame detected with shot boundaries method" << std::endl;
        auto stop_clock = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop_clock - start_clock);
        printf("\nSearch time in millisecs: %d\n", duration.count());
        return { src_video, frame, seconds };
    }*/

   /* output = audioFrame(query_audio, original_fingerprints);
    src_video = output.final_video_prediction;
    seconds = output.final_second_prediction;
    frame = output.final_frame_prediction;

    if (src_video != -1 && correct(query_vid, src_video, frame)) {
        std::cout << "Frame detected with audio sync method" << std::endl;
        auto stop_clock = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop_clock - start_clock);
        printf("\nSearch time in ms: %d\n", duration.count());
        return { src_video, frame, seconds };
    }*/

    output = colorFrame(query_vid);
    src_video = output.final_video_prediction;
    seconds = output.final_second_prediction;
    frame = output.final_frame_prediction;

    if (src_video != -1 &&  error(query_vid, src_video, frame) > 0) {
        std::cout << "Frame detected with color method" << std::endl;
        auto stop_clock = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop_clock - start_clock);
        printf("\nSearch time in millisecs: %d\n", duration.count());
        return { src_video, frame, seconds };
    }
   

    // Return a default value if no correct frame found
    return { -1, -1, -1 }; // Or any other suitable default value
}
