#pragma once
#include <cstdio>
#include <stdio.h> 
#include <iostream> 
#include <vector>
#include <map> 
#include <fstream>
#include <string>
#include <filesystem>
#include "opencv2/opencv.hpp" 
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/videoio.hpp"
#include "Dense"

std::vector<int> shotBoundaries(const std::string& input_vid, double threshold = 50, double downscale_factor = 0.15);
std::vector<int> findArrayIndex(const std::vector<int>& long_arr, const std::vector<int>& short_arr);
std::map<std::string, std::vector<int>> makeShotBoundariesMap();
std::vector<int> calculateDifferences(const std::vector<int>& arr);
std::map<std::string, std::vector<int>> matches(const std::string& videoPath, std::map<std::string, std::pair<std::vector<int>, std::vector<int>>> shotBoundariesMap);