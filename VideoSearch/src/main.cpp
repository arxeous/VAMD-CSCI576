#include "VideoState.h"

int main(int argc, char* argv[])
{
	SDL_Event event;
	VideoState* video;
	char newFile[1024];
	av_init_packet(&flush_pkt);
	flush_pkt.opaque = "FLUSH";
		
	video = (VideoState *)av_mallocz(sizeof(VideoState));
	// Initializing SDL with some flags for video/audio/etc.
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_EVENTS))
	{
		printf("Could not initialize SDL2 -%s\n", SDL_GetError());
		exit(1);
	}

	strncpy_s(video->filename, argv[1], sizeof(video->filename));
	video->pictQMutex = SDL_CreateMutex();
	video->pictQCond = SDL_CreateCond();
	video->avSyncType = DEFAULT_AV_SYNC_TYPE;
	video->seek_pos = (int64_t)(15 * AV_TIME_BASE);
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
			strncpy_s(newFile, 1024, video->nextQuery, 1024);
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
			strncpy_s(video->filename, newFile, sizeof(video->filename));
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