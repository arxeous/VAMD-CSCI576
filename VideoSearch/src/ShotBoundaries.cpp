#include "ShotBoundaries.h"

// find shot boundaries
std::vector<int> shotBoundaries(const std::string& input_vid, double threshold, double downscale_factor) {
    cv::VideoCapture video(input_vid);
    if (!video.isOpened()) {
        std::cout << "Error opening video file!" << std::endl;
        exit(EXIT_FAILURE);
    }

    int total_frames = static_cast<int>(video.get(cv::CAP_PROP_FRAME_COUNT)); // Total number of frames
    std::cout << "Total frames: " << total_frames << std::endl;

    cv::Mat prev_frame;
    int frame_count = 0;
    std::vector<int> shot_boundaries; // Vector to store detected shot boundaries

    while (true) {
        cv::Mat curr_frame;
        video >> curr_frame;
        if (curr_frame.empty()) {
            break;
        }

        // Resize frame to a lower resolution
        cv::Mat curr_frame_resized;
        cv::resize(curr_frame, curr_frame_resized, cv::Size(), downscale_factor, downscale_factor);

        if (!prev_frame.empty()) {
            cv::Mat prev_frame_resized;
            cv::resize(prev_frame, prev_frame_resized, cv::Size(), downscale_factor, downscale_factor);

            cv::Mat frame_diff;
            /*std::cout << "Prev frame after resize: " << prev_frame_resized.size() << std::endl;
            std::cout << "Curr frame after resize: " << curr_frame_resized.size() << std::endl;*/
            cv::absdiff(prev_frame_resized, curr_frame_resized, frame_diff);

            cv::Scalar mean_diff = cv::mean(frame_diff);

            // Check if mean differences exceed the threshold to detect shot boundaries
            if (mean_diff[0] > threshold || mean_diff[1] > threshold || mean_diff[2] > threshold) {
                shot_boundaries.push_back(frame_count);
                //std::cout << "Frame count: " << frame_count << std::endl;
            }
        }

        prev_frame = curr_frame.clone();
       /* std::cout << "Frame count: " << frame_count << std::endl;*/
        frame_count++;
    }

    std::cout << "Shot boundaries for " << input_vid << ":" << std::endl;
    for (int boundary : shot_boundaries) {
        std::cout << boundary << " ";
    }
    std::cout << std::endl;

    video.release();
    cv::destroyAllWindows();
    return shot_boundaries;
}

// make maps for all source videos
std::map<std::string, std::vector<int>> makeShotBoundariesMap() {
    // Define the video paths within the function
    std::vector<std::string> videoPaths = {
        "video2.mp4",
        "video3.mp4",
    };

    std::map<std::string, std::vector<int>> shotBoundariesMap;

    for (const auto& videoPath : videoPaths) {
        std::vector<int> boundaries = shotBoundaries(videoPath); // Assuming shotBoundaries is your function
        shotBoundariesMap[videoPath] = boundaries;
    }

    return shotBoundariesMap;
}

// calculate difference array
std::vector<int> calculateDifferences(const std::vector<int>& arr) {
    std::vector<int> diffArr;

    for (size_t i = 1; i < arr.size(); ++i) {
        diffArr.push_back(arr[i] - arr[i - 1]);
    }

    return diffArr;
}

// find array index
std::vector<int> findArrayIndex(const std::vector<int>& long_arr, const std::vector<int>& short_arr) {
    std::vector<int> indices;

    if (long_arr.size() < short_arr.size()) return indices;

    for (size_t i = 0; i <= long_arr.size() - short_arr.size(); ++i) {
        bool match = true;

        // Check if the elements in long_arr starting at index i match short_arr
        for (size_t j = 0; j < short_arr.size(); ++j) {
            if (long_arr[i + j] != short_arr[j]) {
                match = false;
                break;
            }
        }

        if (match) {
            indices.push_back(static_cast<int>(i)); // Record the starting index
        }
    }

    return indices; // Return a vector of indices where the short array is found in the long array
}


// final function
std::map<std::string, std::vector<int>> matches(const std::string& videoPath, std::map<std::string, std::pair<std::vector<int>, std::vector<int>>> shotBoundariesMap) {
    // Find shot boundaries and differences for the query video
      // Display shot boundaries map
    /*std::cout << "Shot Boundaries map" << std::endl;
    for (const auto& entry : shotBoundariesMap) {
        std::cout << "Video: " << entry.first << std::endl;
        std::cout << "Shot Boundaries: ";
        for (int boundary : entry.second.first) {
            std::cout << boundary << " ";
        }
        std::cout << "| Differences: ";
        for (int diff : entry.second.second) {
            std::cout << diff << " ";
        }
        std::cout << std::endl;
    }*/
    std::vector<int> queryBoundaries = shotBoundaries(videoPath);
    std::vector<int> queryDifferences = calculateDifferences(queryBoundaries);
    for (int diff : queryDifferences) {
        std::cout << "Diff: " << diff << std::endl;
    }
    std::map<std::string, std::vector<int>> possibleMatches; // Modified to store multiple indices

    if (queryDifferences.size() < 1) return possibleMatches;

    for (const auto& entry : shotBoundariesMap) {
        /*for (int e : entry.second.second) {
           std::cout << "entry from " << entry.first << ": " << e << std::endl;
        }*/
        
        std::vector<int> indices = findArrayIndex(entry.second.second, queryDifferences);
        if (!indices.empty()) {
            for (int index : indices) {
                possibleMatches[entry.first].push_back(entry.second.first[index] - queryBoundaries[0]);
            }
        }
    }


    // Display possible source videos and their starting frames
    for (const auto& match : possibleMatches) {
        std::cout << "Possible Source Video: " << match.first << " Starting Frames: ";
        for (int startingFrame : match.second) {
            std::cout << startingFrame << " ";
        }
        std::cout << std::endl;
    }

    return possibleMatches;
}