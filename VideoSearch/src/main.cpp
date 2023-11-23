#include "VideoState.h"

int main(int argc, char* argv[])
{
	SDL_Event event;
	VideoState* video;
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
		SDL_WaitEvent(&event);
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
			break;
		default:
			break;
		}
	}
	return 0;
	
}