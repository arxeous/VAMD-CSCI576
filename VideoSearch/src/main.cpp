#include "VideoState.h"
#include <fftw3.h>
#include "AudioFile.h"
#include "AudioFP.h"
#include <iostream>
#include <string>

#define CREATE_FINGERPRINTS 0

int main(int argc, char* argv[])
{	
	int num_peaks = 8; //The number of peaks to find in a window
	int subband_limits[8] = { 250, 500, 1000, 2000, 3000, 4000, 5000, 6000 }; //Define subbands to look for peaks V1
	int pred_milli; //Returns the millisecond at which Shazam predicts the start to be. Will be -1 if no good result found.
	int pred_frame; //Simple conversion from milli to frame

	//Fingerprint the original files.
	if (CREATE_FINGERPRINTS) {
		AudioFile<double> origFile;
		//////////////////// Load Files ////////////////////	
		

		for (int i = 1; i < 12; i++) {
			std::string i1 = "orig_wav/video";
			std::string i2 = ".wav";
			std::string o1 = "fp_wav/v";
			std::string o2 = "_108d5.txt";

			std::string input = i1 + std::to_string(i) + i2;
			std::string output = o1 + std::to_string(i) + o2;

			printf("Loading %s\n", input.c_str());

			//auto start_clock = high_resolution_clock::now();
			origFile.load(input);
			//auto stop_clock = high_resolution_clock::now();
			//auto duration = duration_cast<microseconds>(stop_clock - start_clock);
			//printf("video %d: microseconds %d\n", i, duration.count());

			origFile.printSummary();

			//////////////////// Peform fingerprinting ////////////////////
			int sampling_rate = static_cast<int>(origFile.getSampleRate()); //samples per second
			int window_millisecs = 100; //Window size in millisecs
			int wind_size = sampling_rate / 1000.0 * window_millisecs; //Window size of sliding window in num samples
			int overlap = floor(.5 * wind_size); //Overlap between windows
			int channel = 0; //audio channel to perform on

			fftw_complex* buff_in, * buff_out;
			fftw_plan p;
			buff_in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * wind_size);
			buff_out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * wind_size);
			p = fftw_plan_dft_1d(wind_size, buff_in, buff_out, FFTW_FORWARD, FFTW_ESTIMATE);

			std::unordered_map<std::size_t, int> original_fingerprints[11]; //Where all the fingerprints of original files will be stored
			original_fingerprints[0] = computeOrigFingerprint(origFile, sampling_rate, wind_size, overlap, buff_in, buff_out, p, subband_limits, num_peaks);

			//Save as text file for later use
			std::ofstream outputFile(output);
			for (const auto& pair : original_fingerprints[0]) {
				outputFile << pair.first << ' ' << pair.second << '\n';
			}
			outputFile.close();

			fftw_destroy_plan(p);
			fftw_free(buff_in); fftw_free(buff_out);
		}
		printf("Done creating original fingerprints.\n");
	}

	//Decode fingerprints and search for query file
	AudioFile<double> queryFile;
	//////////////////// Decode fingerprints //////////////////// 
	printf("Decoding fingerprints...\n");
	std::unordered_map<std::size_t, int> original_fingerprints[11]; //Where all the fingerprints of original files will be stored

	for (int i = 1; i < 12; i++) {
		std::string i1 = "fp_wav/v";
		std::string i2 = "_108d5.txt";

		std::string input = i1 + std::to_string(i) + i2;
		printf("Decoding %s\n", input.c_str());
		//Create an input file stream
		std::ifstream in(input);

		// As long as we haven't reached the end of the file, keep reading entries.
		size_t key;
		int val;
		while (in >> key >> val) {
			original_fingerprints[i - 1][key] = val;
		}
	}
	printf("Fingerprints ready.\n");

	//////////////////// Load Files ////////////////////
	std::string query = argv[2];
	printf("\nLoading Query File %s...\n", query.c_str());


	auto start_clock = std::chrono::high_resolution_clock::now();
	queryFile.load("query_wav/"+query);
	//queryFile.printSummary();

	//////////////////// Peform fingerprinting //////////////////// 
	// printf("Creating Query fingerprint...\n");
	int sampling_rate = static_cast<int>(queryFile.getSampleRate()); //samples per second
	int window_millisecs = 100; //Window size in millisecs
	int wind_size = sampling_rate / 1000.0 * window_millisecs; //Window size of sliding window in num samples
	int overlap = floor(.5 * wind_size); //Overlap between windows
	int channel = 0; //audio channel to perform on

	fftw_complex* buff_in, * buff_out;
	fftw_plan p;
	buff_in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * wind_size);
	buff_out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * wind_size);
	p = fftw_plan_dft_1d(wind_size, buff_in, buff_out, FFTW_FORWARD, FFTW_ESTIMATE);

	int num_samples = queryFile.getNumSamplesPerChannel();
	int num_windows = static_cast<int>(floor((num_samples - overlap) / (wind_size - overlap)));

	size_t* fingerprint = (size_t*)malloc(sizeof(size_t) * num_windows);
	computeQueryFingerprint(queryFile, sampling_rate, wind_size, overlap, buff_in, buff_out, p, subband_limits, num_peaks, fingerprint);

	//printf("Searching for match...\n");

	int* shazam_results = (int*)malloc(sizeof(int) * num_windows);
	int highest_match = -1;
	int best_map = -1;
	int* best_results = (int*)malloc(sizeof(int) * num_windows);

	for (int map_num = 0; map_num < 11; map_num++) { //Iterate through each map
		//printf("\n dictionary %d\n", map_num+1); 
		int this_match = 0;
		for (int wind_num = 0; wind_num < num_windows; wind_num++) { //Iterate through each window
			int this_result = original_fingerprints[map_num][fingerprint[wind_num]];

			if (this_result > 0) { //If result is nonzero, then there most likely was a match.
				this_match++;
			}
			shazam_results[wind_num] = this_result;
			//printf("%d, ", original_fingerprints[map_num][fingerprint[wind_num]]);
		}

		if (this_match > highest_match) {
			highest_match = this_match;
			best_map = map_num + 1;
			memcpy(best_results, shazam_results, sizeof(int) * num_windows);
		}
		//printf("\n");
	}


	/*for (int wind_num = 0; wind_num < num_windows; wind_num++) {
		printf("%d, ", best_results[wind_num]);
	}*/

	LineEquation best_line;
	linear_model_ransac(best_results, num_windows, 100, 7, 0.6, &best_line);
	auto stop_clock = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop_clock - start_clock);
	printf("\nSearch time in millisecs: %d\n", duration.count());

	pred_milli = static_cast<int>(get_y(0, best_line));
	if (pred_milli >= 0) {
		pred_frame = static_cast<int>(floor(pred_milli * .03));
		printf("SHAZAM:\n");
		printf("Predicted video: %d\n", best_map);
		printf("Predicted beginning in milli: %d\n", pred_milli);
		printf("Predicted beginning frame: %d\n\n", pred_frame);
	}
	else {
		printf("SHAZAM:\nNo good Shazam prediction.\n\n");
	}
		
	fftw_destroy_plan(p);
	fftw_free(buff_in); fftw_free(buff_out);
	
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