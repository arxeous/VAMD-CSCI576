//#ifndef TEST_COLOR
//#define TEST_COLOR
#pragma once
#include <cstdio>
#include <stdio.h> 
#include <iostream> 
#include <map> 
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>
#include <chrono>
#include "opencv2/opencv.hpp" 
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/videoio.hpp"
#include "Dense"

//void showAverageColor(const Eigen::Vector3f& rgbValues);
bool compareVectorsThreshold(const std::vector<Eigen::Vector3f>& v1, const std::vector<Eigen::Vector3f>& v2, float threshold, float& minError, std::size_t& matchIndex);
std::pair<std::string, std::size_t> findSubarrayThreshold(const std::unordered_map<std::string, std::vector<Eigen::Vector3f>>& videoMap, const std::vector<Eigen::Vector3f>& queryVector, float threshold);
std::pair<int, int> searchForQuery(const std::string& videoPath, std::unordered_map<std::string, std::vector<Eigen::Vector3f>> myDict);
//void preprocessVideos();


//#endif TEST_COLOR