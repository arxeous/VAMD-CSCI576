#include "VideoState.h"
#include <fftw3.h>
#include "AudioFile.h"
#include "AudioFP.h"
#include <iostream>
#include <string>
#include <filesystem>

#define CREATE_FINGERPRINTS 0

int main(int argc, char* argv[])
{	
	int final_video_prediction = 0;
	double final_second_prediction = 0; //This should ultimately have the predicted start time in seconds
	int num_fp = 20;

	//////////////////// Fingerprint the original files. //////////////////// 
	if (CREATE_FINGERPRINTS) {
		createAllOriginalAudioFingerprints(num_fp);
	}

	//////////////////// Decode fingerprints //////////////////// 
	std::unordered_map<std::size_t, int> original_fingerprints[20]; //Where all the fingerprints of original files will be stored
	decodeOrigFingerprints(num_fp, original_fingerprints);

	//////////////////// Do Shazam ////////////////////
	/*
	Will return a value to:
	  - final_video_prediction 
	  - final_second_prediction
	*/
	std::string query = argv[2];
	shazam(query, num_fp, original_fingerprints, &final_video_prediction, &final_second_prediction);
		

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
			video_refresh_timer(event.user.data1);
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
		// Key stroke catch
		case FF_RESET_STREAM_EVENT:
			// Clean up
			nextQuery = video->getNextQuery;
			query = video->nextQuery;
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
				std::filesystem::path pathObj(query);
				query = pathObj.filename().string();
				shazam(query, num_fp, original_fingerprints, &final_video_prediction, &final_second_prediction);
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
			schedule_refresh(video, 40);
			global_video_state = video;

			// Clearing out event buffer for previous video.
			SDL_Event event3;
			while (SDL_PollEvent(&event3)) {
				// Discard the event
			}
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