#include "VideoState.h"
#include <fftw3.h>
#include "AudioFile.h"
#include "AudioFP.h"
#include "ShotBoundaries.h"
#include "combine.h"
#include <iostream>
#include <string>
#include <filesystem>

#include "opencv2/opencv.hpp" 
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/videoio.hpp"

#define CREATE_FINGERPRINTS 0

int main(int argc, char* argv[])
{
	int num_fp = 20;

	// Get audio fingerprints
	if (CREATE_FINGERPRINTS) {
		createAllOriginalAudioFingerprints(num_fp);
	}
	std::unordered_map<std::size_t, int> original_fingerprints[20];
	decodeOrigFingerprints(num_fp, original_fingerprints);


	// Get shot boundary list
	/*std::string filename = "shot_boundaries_maps.txt";
	std::ifstream file(filename);

	std::map<std::string, std::pair<std::vector<int>, std::vector<int>>> shotBoundariesMap;

	if (file.is_open()) {
		// Read shot boundaries maps from the file
		std::cout << "Decoding shot boundary file..." << std::endl;
		std::string line;
		while (std::getline(file, line)) {
			std::string videoName = line.substr(0, line.find(":"));
			line.erase(0, line.find(":") + 2);
			std::istringstream iss(line);
			std::vector<int> boundaries, differences;
			int num;
			bool readingBoundaries = true;
			while (iss >> num) {
				if (num == '|') {
					readingBoundaries = false;
					continue;
				}
				if (readingBoundaries) {
					boundaries.push_back(num);
				}
				else {
					differences.push_back(num);
				}
			}
			shotBoundariesMap[videoName] = std::make_pair(boundaries, differences);
		}
		file.close();
	}
	else {
		std::cout << "Unable to locate shot boundary file" << std::endl;
	}*/

	// Get color map
	std::ifstream inFile("colorDatabase100x100.txt");
	// create an unordered_map with string keys and vector of Eigen::Vector3f values
	std::unordered_map<std::string, std::vector<Eigen::Vector3f>> myDict;

	if (inFile.is_open()) {
		std::cout << "Decoding color map file...\n";
		// read each line from the file
		std::string line;

		while (std::getline(inFile, line)) {
			//std::cout << line << std::endl;
			// parse the line
			std::istringstream iss(line);
			std::string key;
			float x, y, z;

			// read the key
			iss >> key;

			// create a vector to store Eigen::Vector3f components
			std::vector<Eigen::Vector3f> vectorComponents;

			// read the vector components
			while (iss >> x >> y >> z) {
				vectorComponents.push_back(Eigen::Vector3f(x, y, z));
			}
			myDict[key] = vectorComponents;
		}
		inFile.close();
	}
	else {
		std::cout << "Unable to locate color map file\n";
	}

	// Get frame prediction
	std::string query_video = argv[1];
	std::string query_audio = argv[2];
	Result output = startFrame(query_video, query_audio, original_fingerprints, myDict);
	int final_video_prediction = output.final_video_prediction;
	int final_frame_prediction = output.final_frame_prediction;
	double final_second_prediction = output.final_second_prediction;

	std::cout << "Video prediction: " << final_video_prediction << std::endl;
	std::cout << "Frame prediction: " << final_frame_prediction << std::endl; 
	std::cout << "Second prediction: " << final_second_prediction << std::endl;

	
	SDL_Event event;
	VideoState* video;
	char newFile[1024];
	bool nextQuery = false;
	av_init_packet(&flush_pkt);
	flush_pkt.opaque = "FLUSH";
		
	video = (VideoState *)av_mallocz(sizeof(VideoState));
	// Initializing SDL with some flags for video/audio/etc.
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_EVENTS))
	{
		printf("Could not initialize SDL2 -%s\n", SDL_GetError());
		exit(1);
	}

	std::string mp4_dir = "orig_mp4/";
	std::string mp4_a = "video";
	std::string mp4_b = ".mp4";
	std::string play_this = mp4_dir + mp4_a + std::to_string(final_video_prediction) + mp4_b;

	strncpy_s(video->filename, play_this.c_str(), sizeof(video->filename));
	video->pictQMutex = SDL_CreateMutex();
	video->pictQCond = SDL_CreateCond();
	video->avSyncType = DEFAULT_AV_SYNC_TYPE;
	video->seek_pos = (int64_t)(static_cast<int>(final_second_prediction * AV_TIME_BASE));
	video->seek_req = true;
	video->seek_flags = AVSEEK_FLAG_BACKWARD;

	schedule_refresh(video, 40);
	// Spawns a thread that starts running on the function we pass it along with user defined data
	video->parseThreadId = SDL_CreateThread(decode_thread, "decode" ,video);
	if (!video->parseThreadId)
	{
		av_free(video);
		return -1;
	}

	for (;;)
	{
		double increment, pos;
		int result;
		result = SDL_WaitEvent(&event);
		if (renderer)
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
		}
		switch (event.type)
		{
		case FF_QUIT_EVENT:
		case SDL_QUIT:
			video->quit = true;
			SDL_Quit();
			return 0;
			break;
		case FF_ALLOC_EVENT:
			alloc_picture(event.user.data1);
			break;
		case FF_REFRESH_EVENT:
			video_refresh_timer((void*)global_video_state);
			break;
		case FF_CREATE_WINDOW_EVENT:
			screen = SDL_CreateWindow(video->filename,
				SDL_WINDOWPOS_UNDEFINED,
				SDL_WINDOWPOS_UNDEFINED,
				video->videoCtx->width,
				video->videoCtx->height,
				SDL_WINDOW_OPENGL);
			renderer = SDL_CreateRenderer(screen, -1, 0);

			if (!renderer)
			{
				fprintf(stderr, "SDL: Could not create renderer - exiting\n");
				exit(1);
			}
			else if(ImGui::GetCurrentContext() == NULL)
			{
				IMGUI_CHECKVERSION();
				ImGui::CreateContext();
				ImGuiIO& io = ImGui::GetIO(); 
				(void)io;
				//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

				// Setup Dear ImGui style
				ImGui::StyleColorsDark();

				ImGui_ImplSDL2_InitForSDLRenderer(screen, renderer);
				ImGui_ImplSDLRenderer2_Init(renderer);
			}

			break;
		case FF_RESET_STREAM_EVENT:
			// Clean up
			nextQuery = video->getNextQuery;
			query_audio = video->nextQuery;
			query_video = video->nextQuery;
			SDL_WaitThread(video->parseThreadId, NULL);
			SDL_PauseAudioDevice(video->dev, 1);
			SDL_CloseAudioDevice(video->dev);
			avformat_close_input(&video->pFormatCtx);
			av_free(video);
			ImGui_ImplSDLRenderer2_Shutdown();
			ImGui_ImplSDL2_Shutdown();
			ImGui::DestroyContext();
			SDL_DestroyRenderer(renderer);
			renderer = NULL;
			SDL_DestroyWindow(screen);
			screen = NULL;
			//Re initialization of video
			video = (VideoState*)av_mallocz(sizeof(VideoState));
			if (nextQuery)
			{
				std::filesystem::path pathObj(query_audio);
				query_audio = pathObj.filename().replace_extension(".wav").string();
				query_video = pathObj.filename().replace_extension(".mp4").string();

				output = startFrame(query_video, query_audio, original_fingerprints, myDict);
				//output = startFrame(query_video, query_audio, original_fingerprints);
				final_video_prediction = output.final_video_prediction;
				final_frame_prediction = output.final_frame_prediction;
				final_second_prediction = output.final_second_prediction;

				play_this = mp4_dir + mp4_a + std::to_string(final_video_prediction) + mp4_b;
				video->seek_req = true;
				video->seek_pos = (int64_t)(static_cast<int>(final_second_prediction * AV_TIME_BASE));
				video->getNextQuery = nextQuery = false;
			}
			strncpy_s(video->filename, play_this.c_str(), sizeof(video->filename));
			video->pictQMutex = SDL_CreateMutex();
			video->pictQCond = SDL_CreateCond();
			video->avSyncType = DEFAULT_AV_SYNC_TYPE;
			video->videoStream = NULL;
			global_video_state = video;

			video->parseThreadId = SDL_CreateThread(decode_thread, "decode", video);
			break;
		default:
			break;
		}


		
	}

	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(screen);
	SDL_Quit();
	return 0;
}