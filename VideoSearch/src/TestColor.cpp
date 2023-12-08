#include "TestColor.h"


//void showColor(const Eigen::Vector3f& rgbValues) {
//    // Create a single-pixel image with the specified color
//    cv::Mat colorImage(1, 1, CV_8UC3);
//    colorImage.at<cv::Vec3b>(0, 0) = cv::Vec3b(rgbValues[0], rgbValues[1], rgbValues[2]);
//
//    // Display the color image
//    cv::imshow("Color Image", colorImage);
//    cv::waitKey(0);
//}


bool compareVectorsThreshold(const std::vector<Eigen::Vector3f>& v1, const std::vector<Eigen::Vector3f>& v2, float threshold, std::size_t& matchIndex) 
{
    // check if vid size is smaller than query
    if (v1.size() < v2.size()) {
        return false;
    }

    // iterate through & compare sub vec 
    // get absolute difference & check below the threshold
    // if match get index and return true
    for (std::size_t i = 0; i <= v1.size() - v2.size(); ++i) {
        bool match = true;
        for (std::size_t j = 0; j < v2.size(); ++j) {
            if ((v1[i + j] - v2[j]).norm() > threshold) {
                match = false;
                break;
            }
        }
        if (match) {
            matchIndex = i;
            return true;
        }
    }

    return false;
}


std::pair<std::string, std::size_t> findSubarrayThreshold(const std::unordered_map<std::string, std::vector<Eigen::Vector3f>>& videoMap, const std::vector<Eigen::Vector3f>& queryVector, float threshold) 
{
    std::size_t matchIndex = 0;
    // loop through all vids in database
    // compare query with each 
    for (const auto& entry : videoMap) {
        // Compare the query vector with each vid rhen return if exits or not
        if (compareVectorsThreshold(entry.second, queryVector, threshold, matchIndex)) {
            return {entry.first, matchIndex};
        }
    }
    return {"", 0};
}


std::pair<int, int> searchForQuery(const std::string& videoPath, std::unordered_map<std::string, std::vector<Eigen::Vector3f>> myDict)
{
    // open vid
    cv::VideoCapture vid; 
	vid.open(videoPath, cv::CAP_FFMPEG);
    std::vector<Eigen::Vector3f> averageColors;
    
    // return nothing if video does not exit
    if (!vid.isOpened()) {
        std::cout << "ERROR NOTHING" << std::endl;
        return {-1,-1};
	}

    // grab firts 5 seconds 
	double fps = vid.get(cv::CAP_PROP_FPS);
    int numFramesToProcess = static_cast<int>(1 * fps);

    // Process each frame for 5 seconds
    for (int frameCount = 0; frameCount < numFramesToProcess; ++frameCount) {
        cv::Mat frame;
        vid >> frame;

        if (frame.empty()) {
            break;
        }

        // convert frame to RGB format
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);

        // resize frame to match the desired size
        cv::resize(frame, frame, cv::Size(100, 100));

		// calculate the average RGB value for whole frame --> so becomes 1 RGBA vec 
		cv::Scalar meanColor = cv::mean(frame);
		Eigen::Vector3f averageColor(static_cast<float>(meanColor[0]), static_cast<float>(meanColor[1]), static_cast<float>(meanColor[2]));

        // save the average color to the vector
        averageColors.push_back(averageColor);

	}

	vid.release();

	/*std::cout << "TESTING" << std::endl;
    std::cout << "Average Color for Frame 0: " << std::fixed << std::setprecision(6) << averageColors[0].transpose() << std::endl;
    std::cout << "Average Color for Frame 0: " << averageColors[0].transpose() << std::endl;
    std::cout << "Average Color for Frame 1: " << averageColors[1].transpose() << std::endl;
    std::cout << "Average Color for Frame 2: " << averageColors[2].transpose() << std::endl;
    std::cout << "Average Color for Frame 3: " << averageColors[3].transpose() << std::endl;

    std::cout << myDict["video2.mp4"][8340] << std::endl;
    std::cout << myDict["video2.mp4"][8341] << std::endl;
    std::cout << myDict["video2.mp4"][8342] << std::endl;
    std::cout << myDict["video2.mp4"][8343] << std::endl;*/

    // call comparison funct
    std::pair<std::string, int> test = findSubarrayThreshold(myDict, averageColors, 0.02);
    if(test.first.empty()){
        //std::cout << "                NOTHING FOUND                " << std::endl;
        return std::make_pair(-1, -1);
    }
    /*auto stopClock2 = std::chrono::high_resolution_clock::now();
	auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(stopClock2-startClock2);
	printf("comparison takes %d microseconds\n", duration2.count());*/

    //std::cout << "length = " <<  myDict["video11.mp4"].size() << std::endl;
    //char sixthChar = test.first[5]; // Array notation, indexing starts from 0
    //int vidInt = sixthChar - '0';
    size_t startPos = test.first.find_first_of("0123456789");
    std::string numberStr = test.first.substr(startPos);
    std::istringstream iss(numberStr);
    int vidInt;
    iss >> vidInt;

    return std::make_pair(vidInt, test.second);
}

//void preprocessVideos()
//{   
//    std::unordered_map<std::string, std::vector<Eigen::Vector3f>> myDict;
//    cv::VideoCapture vid; 
//    std::vector<Eigen::Vector3f> averageColors;
//
//    // VIDEO_1 DATABASE ======================================================================================= 
//	vid.open("video1.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//	vid.release();
//
//    myDict["video1.mp4"] = averageColors;
//    averageColors.clear();
//
//    // VIDEO_2 DATABASE ======================================================================================= 
//	vid.open("video2.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    std::cout << "Average Color for Frame 8340: " << averageColors[8340].transpose() << std::endl;
//    std::cout << "Average Color for Frame 8341: " << averageColors[8341].transpose() << std::endl;
//    std::cout << "Average Color for Frame 8342: " << averageColors[8342].transpose() << std::endl;
//
//    myDict["video2.mp4"] = averageColors;
//    averageColors.clear();
//
//    // VIDEO_3 DATABASE =======================================================================================
//	vid.open("video3.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    std::cout << "Average Color for Frame 13350: " << averageColors[13350].transpose() << std::endl;
//    std::cout << "Average Color for Frame 13351: " << averageColors[13351].transpose() << std::endl;
//    std::cout << "Average Color for Frame 13352: " << averageColors[13352].transpose() << std::endl;
//
//    myDict["video3.mp4"] = averageColors;
//    averageColors.clear();
//
//    // VIDEO_4 DATABASE =======================================================================================
//	vid.open("video4.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    myDict["video4.mp4"] = averageColors;
//    averageColors.clear();
//
//
//    // VIDEO_5 DATABASE =======================================================================================
//	vid.open("video5.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    myDict["video5.mp4"] = averageColors;
//    averageColors.clear();
//
//
//    // VIDEO_6 DATABASE =======================================================================================
//	vid.open("video6.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    myDict["video6.mp4"] = averageColors;
//    averageColors.clear();
//
//
//    // VIDEO_7 DATABASE =======================================================================================
//	vid.open("video7.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    myDict["video7.mp4"] = averageColors;
//    averageColors.clear();
//
//
//    // VIDEO_8 DATABASE =======================================================================================
//	vid.open("video8.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    myDict["video8.mp4"] = averageColors;
//    averageColors.clear();
//
//
//    // VIDEO_9 DATABASE =======================================================================================
//	vid.open("video9.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    myDict["video9.mp4"] = averageColors;
//    averageColors.clear();
//
//
//    // VIDEO_10 DATABASE =======================================================================================
//	vid.open("video10.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    myDict["video10.mp4"] = averageColors;
//    averageColors.clear();
//
//
//    // VIDEO_11 DATABASE =======================================================================================
//	vid.open("video11.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    myDict["video11.mp4"] = averageColors;
//    averageColors.clear();
//
//
//    // VIDEO_12 DATABASE =======================================================================================
//	vid.open("video12.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    myDict["video12.mp4"] = averageColors;
//    averageColors.clear();
//
//
//    // VIDEO_13 DATABASE =======================================================================================
//	vid.open("video13.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    myDict["video13.mp4"] = averageColors;
//    averageColors.clear();
//
//
//    // VIDEO_14 DATABASE =======================================================================================
//	vid.open("video14.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    myDict["video14.mp4"] = averageColors;
//    averageColors.clear();
//
//
//    // VIDEO_15 DATABASE =======================================================================================
//	vid.open("video15.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    myDict["video15.mp4"] = averageColors;
//    averageColors.clear();
//
//
//    // VIDEO_16 DATABASE =======================================================================================
//	vid.open("video16.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    myDict["video16.mp4"] = averageColors;
//    averageColors.clear();
//
//
//    // VIDEO_17 DATABASE =======================================================================================
//	vid.open("video17.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    myDict["video17.mp4"] = averageColors;
//    averageColors.clear();
//
//
//    // VIDEO_18 DATABASE =======================================================================================
//	vid.open("video18.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    myDict["video18.mp4"] = averageColors;
//    averageColors.clear();
//
//
//    // VIDEO_19 DATABASE =======================================================================================
//	vid.open("video19.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    myDict["video19.mp4"] = averageColors;
//    averageColors.clear();
//
//
//    // VIDEO_20 DATABASE =======================================================================================
//	vid.open("video20.mp4", cv::CAP_FFMPEG);
//    
//    if (!vid.isOpened()) {
//        std::cout << "ERROR NOTHING" << std::endl;
//	}
//
//    // Process whole video
//    while (true) {
//        cv::Mat frame;
//        vid >> frame;
//
//        if (frame.empty()) {
//            break;
//        }
//
//        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
//        cv::resize(frame, frame, cv::Size(100, 100));
//
//		cv::Scalar meanColor = cv::mean(frame);
//		Eigen::Vector3f averageColor(meanColor[0], meanColor[1], meanColor[2]);
//		
//        averageColors.push_back(averageColor);
//
//	}
//
//
//	vid.release();
//
//    myDict["video20.mp4"] = averageColors;
//    averageColors.clear();
//
//    
//    // Write to file 
//    std::ofstream outFile("colorDatabase100x100.txt");
//
//    if (outFile.is_open()) {
//        // Iterate through the map
//        for (const auto& entry : myDict) {
//            // Write the key
//            outFile << entry.first << " ";
//
//            // Iterate through the vector of Eigen::Vector3f
//            for (const auto& vector : entry.second) {
//                // Write each Eigen::Vector3f
//                outFile << vector[0] << " " << vector[1] << " " << vector[2] << " ";
//            }
//
//            outFile << "\n";
//        }
//
//        // Close the file
//        outFile.close();
//
//        std::cout << "Map written to file successfully.\n";
//    } else {
//        std::cerr << "Error opening file for writing.\n";
//    }
//}