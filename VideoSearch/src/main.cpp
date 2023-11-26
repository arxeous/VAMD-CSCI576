#include "VideoState.h"

int main(int argc, char* argv[])
{
	SDL_Event event;
	VideoState* video;
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
		SDL_WaitEvent(&event);
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
			else
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
			stream_seek(global_video_state, 0, -1);
			break;
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym) 
			{
			case SDLK_LEFT:
				increment = -1.0;
				goto do_seek;
			case SDLK_RIGHT:
				increment = 1.0;
				goto do_seek;
			case SDLK_UP:
				increment = 5.0;
				goto do_seek;
			case SDLK_DOWN:
				increment = -5.0;
				goto do_seek;
			do_seek:
				if (global_video_state)
				{
					pos = get_master_clock(global_video_state);
					std::printf("Position: %f\n New Position: %f\n\n", pos, pos + increment);
					pos += increment;
					stream_seek(global_video_state, (int64_t)(pos * AV_TIME_BASE), increment);
				}
				break;
			case SDLK_SPACE:
				std::printf("PAUSE\n");
				set_pause(global_video_state);
				break;
			}
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