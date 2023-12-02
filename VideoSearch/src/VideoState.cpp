#include "VideoState.h"

uint64_t global_video_pkt_pts = AV_NOPTS_VALUE;
VideoState* global_video_state;
SDL_Window* screen;
SDL_Renderer* renderer;
AVPacket flush_pkt;

int decode_interrupt_cb(void* opaque) {
	return (global_video_state && global_video_state->quit);
}

void packet_queue_init(PacketQueue* q) {
	memset(q, 0, sizeof(PacketQueue));
	q->mutex = SDL_CreateMutex();
	q->cond = SDL_CreateCond();
}

int packet_queue_put(PacketQueue* q, AVPacket* pkt) {

	if (!pkt)
	{
		pkt = av_packet_alloc();
		if (pkt != &flush_pkt && !pkt)
		{
			printf("Could not allocate packet or packet was another flush packet -%s\n", SDL_GetError());
			return -1;
		}
	}

	AVPacketList* pkt1;
	pkt1 = (AVPacketList*)av_malloc(sizeof(AVPacketList));
	if (pkt1 == NULL)
	{
		printf("Could not allocate packet list -%s\n", SDL_GetError());
		return -1;
	}

	pkt1->pkt = *pkt;
	pkt1->next = NULL;


	SDL_LockMutex(q->mutex);

	if (!q->lastPacket)
		q->firstPacket = pkt1;
	else
		q->lastPacket->next = pkt1;
	q->lastPacket = pkt1;
	q->nb_packets++;
	q->size += pkt1->pkt.size;
	SDL_CondSignal(q->cond);

	SDL_UnlockMutex(q->mutex);
	return 0;
}


static int packet_queue_get(PacketQueue* q, AVPacket* pkt, int block)
{
	AVPacketList* pkt1;
	int ret;

	SDL_LockMutex(q->mutex);

	for (;;) {

		if (global_video_state->quit) {
			ret = -1;
			break;
		}

		pkt1 = q->firstPacket;
		if (pkt1) {
			q->firstPacket = pkt1->next;
			if (!q->firstPacket)
				q->lastPacket = NULL;
			q->nb_packets--;
			q->size -= pkt1->pkt.size;
			*pkt = pkt1->pkt;
			av_free(pkt1);
			ret = 1;
			break;
		}
		else if (!block) {
			ret = 0;
			break;
		}
		else {
			SDL_CondWait(q->cond, q->mutex);
		}
	}
	SDL_UnlockMutex(q->mutex);
	return ret;
}

double get_audio_clock(VideoState* state)
{

	double pts;
	int hw_buf_size, bytes_per_sec, n;

	pts = state->audioClock; /* maintained in the audio thread */
	hw_buf_size = state->audioBuffSize - state->audioBuffIndex;
	bytes_per_sec = 0;
	n = state->audioCtx->ch_layout.nb_channels * 2;
	if (state->audioStream) {
		bytes_per_sec = state->audioStream->codecpar->sample_rate * n;
	}
	if (bytes_per_sec) {
		pts -= (double)hw_buf_size / bytes_per_sec;
	}
	return pts;
}

int64_t get_correct_av_time(VideoState* state)
{
	return av_gettime() - state->pauseDuration;
}

double get_video_clock(VideoState* state)
{
	double delta;
	delta = (get_correct_av_time(state) - state->videoCurrPtsTime) / 1000000.0;
	return state->videoCurrPts + delta;
}

double get_external_clock(VideoState* state)
{
	return get_correct_av_time(state) / 1000000.0;
}

double get_master_clock(VideoState* state)
{
	if (state->avSyncType == AV_SYNC_VIDEO_MASTER) {
		return get_video_clock(state);
	}
	else if (state->avSyncType == AV_SYNC_AUDIO_MASTER) {
		return get_audio_clock(state);
	}
	else {
		return get_external_clock(state);
	}
}

int synchronize_audio(VideoState* state, short* samples, int samples_size, double pts)
{
	int n;
	double ref_clock;

	n = 2 * state->audioCtx->ch_layout.nb_channels;

	if (state->avSyncType != AV_SYNC_AUDIO_MASTER)
	{
		double diff, avg_diff;
		int wanted_size, min_size, max_size; // # of samples.
		ref_clock = get_master_clock(state);
		printf("Master clock time sync audio thread: %f\n", ref_clock);
		diff = get_audio_clock(state) - ref_clock;

		printf("Audio clock: %f\n", get_audio_clock(state));
		//printf("Diff: %f\n", diff);


		if (diff < AV_NOSYNC_THRESHOLD)
		{
			state->audioDiffCum = diff + (state->audioDiffAvgCoef * state->audioDiffCum);
			if (state->audioDiffAvgCount < AUDIO_DIFF_AVG_NB)
			{
				state->audioDiffAvgCount++;
			}
			else
			{
				avg_diff = state->audioDiffCum * (1.0 - state->audioDiffAvgCoef);
				if (fabs(avg_diff) >= state->audioDiffThreshold)
				{
					wanted_size = samples_size + ((int)(diff * state->audioStream->codecpar->sample_rate) * n);
					min_size = samples_size * ((100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100);
					max_size = samples_size * ((100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100);
					if (wanted_size < min_size)
					{
						wanted_size = min_size;
					}
					else if (wanted_size > max_size)
					{
						wanted_size = max_size;
					}
					if (wanted_size < samples_size)
					{
						samples_size = wanted_size;
					}
					else if (wanted_size > samples_size)
					{
						uint8_t* samples_end, * q;
						int nb;

						nb = (samples_size - wanted_size);
						samples_end = (uint8_t*)samples + samples_size - n;
						q = samples_end + n;

						while (nb > 0)
						{
							memcpy(q, samples_end, n);
							q += n;
							nb -= n;
						}
						samples_size = wanted_size;
					}
				}
			}
		}
		else
		{
			state->audioDiffAvgCount = 0;
			state->audioDiffCum = 0;
		}
	}

	return samples_size;
}

int audio_decode_frame(VideoState* state, double* pts_ptr)
{
	int len1, dataSize = 0, n;
	AVPacket* pkt = &state->audioPacket;
	static AVFrame* audioFrame = av_frame_alloc();
	int errBuffSize = 100;
	char* errBuff = new char[errBuffSize];
	double pts;

	int ret;

	for (;;)
	{
		while (state->audioPacketSize > 0)
		{

			ret = avcodec_send_packet(state->audioCtx, pkt);
			len1 = pkt->size;
			if (ret < 0 || len1 < 0)
			{
				fprintf(stderr, "Error during sending packet - exiting\n");
				state->audioPacketSize = 0;
				break;
			}

			while (ret >= 0)
			{
				ret = avcodec_receive_frame(state->audioCtx, &state->audioFrame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				{
					av_strerror(ret, errBuff, errBuffSize);
					//std::printf("%s AUDIO THREAD 1\n", errBuff);
					break;
				}
				if (ret < 0)
				{
					fprintf(stderr, "Error during decoding - exiting\n");
					exit(1);
				}
				int dst_samples = state->audioFrame.ch_layout.nb_channels * av_rescale_rnd(
					swr_get_delay(state->resampler, state->audioFrame.sample_rate)
					+ state->audioFrame.nb_samples,
					44100,
					state->audioFrame.sample_rate,
					AV_ROUND_UP);
				uint8_t* audiobuf = NULL;
				ret = av_samples_alloc(&audiobuf,
					NULL,
					1,
					dst_samples,
					AV_SAMPLE_FMT_S16,
					1);
				int conv_samples = state->audioFrame.ch_layout.nb_channels * swr_convert(
					state->resampler,
					&audiobuf,
					dst_samples,
					(const uint8_t**)state->audioFrame.data,
					state->audioFrame.nb_samples);
				ret = av_samples_fill_arrays(audioFrame->data,
					audioFrame->linesize,
					audiobuf,
					1,
					conv_samples,
					AV_SAMPLE_FMT_S16,
					1);

				dataSize = audioFrame->linesize[0];

				if (av_strerror(ret, errBuff, errBuffSize) == 0)
				{
					//std::printf("%s AUDIO THREAD 2\n", errBuff);
					break;
				}

				memcpy(state->audioBuff, audioFrame->data[0], dataSize);
			}


			state->audioPacketData += len1;
			state->audioPacketSize -= len1;
			if (dataSize <= 0)
			{
				continue;
			}
			pts = state->audioClock;
			*pts_ptr = pts;
			n = 2 * state->audioCtx->ch_layout.nb_channels;
			state->audioClock += (double)dataSize / (double)(n * state->audioStream->codecpar->sample_rate);

			return dataSize;
		}

		if (pkt->data)
		{
			av_packet_unref(pkt);
		}
		if (state->quit)
		{
			return -1;
		}

		// Get next packet
		if (packet_queue_get(&state->audioQ, pkt, 1) < 0)
		{
			return -1;
		}

		if (pkt->opaque == flush_pkt.opaque)
		{
			avcodec_flush_buffers(state->audioCtx);
			continue;
		}
		state->audioPacketData = pkt->data;
		state->audioPacketSize = pkt->size;

		if (pkt->pts != AV_NOPTS_VALUE)
		{
			state->audioClock = av_q2d(state->audioStream->time_base) * pkt->pts;
		}
	}
	return -1;
}

// A simple loop that will pull in data from another function we have written called audio_decode_frame,
// store the result in an intermediary buffer, write len bytes to stream
// get more data if theres not enough or save it for later if we got some left over.
void audio_callback(void* userdata, Uint8* stream, int len)
{
	VideoState* state = (VideoState*)userdata;
	int len1, audioSize;
	double pts;

	while (len > 0 )
	{
		if (!state->pause)
		{
			if (state->audioBuffIndex >= state->audioBuffSize)
			{
				audioSize = audio_decode_frame(state, &pts);

				if (audioSize < 0)
				{
					state->audioBuffSize = 1024;
					memset(state->audioBuff, 0, state->audioBuffSize);
				}
				else
				{
					audioSize = synchronize_audio(state, (int16_t*)state->audioBuff, audioSize, pts);
					state->audioBuffSize = audioSize;
				}
				state->audioBuffIndex = 0;
			}

			len1 = state->audioBuffSize - state->audioBuffIndex;
			if (len1 > len)
			{
				len1 = len;
			}

			memcpy(stream, (uint8_t*)state->audioBuff + state->audioBuffIndex, len1);
			len -= len1;
			stream += len1;
			state->audioBuffIndex += len1;
		}
	}

}
void display_controls(VideoState* state)
{
	ImGui_ImplSDLRenderer2_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
	VideoWindow* vw = &state->pictQ[state->pictQRIndex];

	ImGui::Begin("Controls");

	if (ImGui::Button("Pause"))
	{
		if (!state->pause)
		{
			state->pause = true;
			state->pauseTime = av_gettime();
			std::printf("\nPAUSE\n");
		}

	}
	ImGui::SameLine();
	if (ImGui::Button("Play"))
	{
		if (state->pause)
		{
			state->pause = false;
			state->resumeTime = av_gettime();
			state->pauseDuration += state->resumeTime - state->pauseTime;
		}
		
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset"))
	{
		printf("\nRESET\n");
		state->videoCurrPtsTime = av_gettime();
		SDL_Event event;
		event.type = FF_RESET_STREAM_EVENT;
		SDL_PushEvent(&event);
		state->pauseDuration = 0;
	}

	ImGui::End();
	ImGui::Render();

	if (state->pause)
	{
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, vw->texture, NULL, NULL);
		ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
		SDL_RenderPresent(renderer);
	}

}



void video_display(VideoState* state)
{
	VideoWindow* vw;
	SDL_Rect rect;
	float aspect_ratio;
	int w, h, x, y;

	vw = &state->pictQ[state->pictQRIndex];

	if (vw->texture)
	{
		if (state->videoCtx->sample_aspect_ratio.num == 0)
		{
			aspect_ratio = 0;
		}
		else
		{
			aspect_ratio = av_q2d(state->videoCtx->sample_aspect_ratio) * (state->videoCtx->width / state->videoCtx->height);
		}
		if (aspect_ratio <= 0.0)
		{
			aspect_ratio = (float)state->videoCtx->width / (float)state->videoCtx->height;
		}
		h = state->videoCtx->height;
		w = ((int)rint(h * aspect_ratio)) & -3;

		if (w > state->videoCtx->width)
		{
			w = state->videoCtx->width;
			h = ((int)rint(w / aspect_ratio)) & -3;
		}
		SDL_UpdateYUVTexture(
			vw->texture,
			NULL,
			state->yPlane,
			state->videoCtx->width,
			state->uPlane,
			state->uvPitch,
			state->vPlane,
			state->uvPitch
		);

	

		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, vw->texture, NULL, NULL);
		display_controls(state);
		ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
		SDL_RenderPresent(renderer);
	}
}



void video_refresh_timer(void* userdata) {

	VideoState* state = (VideoState*)userdata;
	VideoWindow* vw;
	double actual_delay, delay, sync_threshold, ref_clock, diff;

	if (state->videoStream)
	{
		
		if (state->pictQSize == 0) 
		{
			display_controls(state);
			schedule_refresh(state, 1);
		}
		else {
			vw = &state->pictQ[state->pictQRIndex];

			state->videoCurrPts = vw->pts;
			state->videoCurrPtsTime = get_correct_av_time(state);
			
			delay = vw->pts - state->frameLastPts; /* the pts from last time */
			//printf("Delay refresh: %f\n", delay);
			if (delay <= 0 || delay >= 1.0) {
				/* if incorrect delay, use previous one */
				delay = state->frameLastDelay;
			}
			/* save for next time */
			state->frameLastDelay = delay;
			state->frameLastPts = vw->pts;

			if (state->avSyncType != AV_SYNC_VIDEO_MASTER)
			{
				/* update delay to sync to audio */
				ref_clock = get_master_clock(state);
				diff = vw->pts - ref_clock;

				/* Skip or repeat the frame. Take delay into account
			   FFPlay still doesn't "know if this is the best guess." */
				sync_threshold = (delay > AV_SYNC_THRESHOLD) ? delay : AV_SYNC_THRESHOLD;
				if (fabs(diff) < AV_NOSYNC_THRESHOLD) {
					if (diff <= -sync_threshold) {
						delay = 0;
					}
					else if (diff >= sync_threshold) {
						delay = 2 * delay;
					}
				}
			}


			state->frameTimer += delay;
			/* compute the REAL delay */
			double test = state->pauseTime / 1000000.0;

			actual_delay = state->frameTimer - (get_correct_av_time(state) / 1000000.0);
			if (actual_delay < 0.010) {
				/* Really it should skip the picture instead */
				actual_delay = 0.010;
			}
			//printf("Current delay time:%f\n", actual_delay);
			schedule_refresh(state, (int)(actual_delay * 1000 + 0.5));
			/* show the picture! */
			video_display(state);

			/* update queue for next picture! */
			if (++state->pictQRIndex == VIDEO_PICTURE_QUEUE_SIZE) {
				state->pictQRIndex = 0;
			}
			SDL_LockMutex(state->pictQMutex);
			state->pictQSize--;
			SDL_CondSignal(state->pictQCond);
			SDL_UnlockMutex(state->pictQMutex);
		}
	}
	else 
	{
		schedule_refresh(state, 100);
	}
}


void alloc_picture(void* userdata)
{
	VideoState* state = (VideoState*)userdata;
	VideoWindow* vw;

	vw = &state->pictQ[state->pictQWIndex];

	if (vw->texture)
	{
		SDL_DestroyTexture(vw->texture);
	}
	vw->texture = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_YV12,
		SDL_TEXTUREACCESS_STREAMING,
		state->videoStream->codecpar->width,
		state->videoStream->codecpar->height
	);

	vw->width = state->videoStream->codecpar->width;
	vw->height = state->videoStream->codecpar->height;

	SDL_LockMutex(state->pictQMutex);
	vw->allocated = true;
	SDL_CondSignal(state->pictQCond);
	SDL_UnlockMutex(state->pictQMutex);
}

int queue_picture(VideoState* state, AVFrame* pFrame, double pts)
{
	VideoWindow* vw;
	int dst_pix_fmt;
	AVFrame pict;
	void* pixels;
	int pitch;

	// Wait until we have space for a new frame
	SDL_LockMutex(state->pictQMutex);

	while (state->pictQSize >= VIDEO_PICTURE_QUEUE_SIZE && !state->quit)
	{
		//printf("waiting for condition signal as we have too many items in the queue\n");
		SDL_CondWait(state->pictQCond, state->pictQMutex);
	}
	SDL_UnlockMutex(state->pictQMutex);

	if (state->quit)
	{
		return -1;
	}

	// Window index is set to 0 initially
	vw = &state->pictQ[state->pictQWIndex];

	if (!vw->texture || vw->width != state->videoCtx->width || vw->height != vw->height)
	{
		SDL_Event event;
		vw->allocated = false;
		event.type = FF_ALLOC_EVENT;
		event.user.data1 = state;
		SDL_PushEvent(&event);

		SDL_LockMutex(state->pictQMutex);
		while (!vw->allocated && !state->quit)
		{
			printf("waiting for condition signal for texture intialization/resize\n");
			SDL_CondWait(state->pictQCond, state->pictQMutex);
		}
		SDL_UnlockMutex(state->pictQMutex);
		if (state->quit)
		{
			return -1;
		}
	}

	if (vw->texture)
	{
		if (SDL_LockTexture(vw->texture, NULL, &pixels, &pitch) != 0)
		{
			printf("Error locking texture\n");
			return -1;
		}

		dst_pix_fmt = AV_PIX_FMT_YUV420P;

		pict.data[0] = state->yPlane;
		pict.data[1] = state->uPlane;
		pict.data[2] = state->vPlane;

		pict.linesize[0] = state->videoCtx->width;
		pict.linesize[1] = state->uvPitch;
		pict.linesize[2] = state->uvPitch;

		// Here is where the actual conversion of the image into YUV that SDL2 uses happens
		sws_scale(state->swsCtx, (uint8_t const* const*)pFrame->data,
			pFrame->linesize, 0, state->videoCtx->height, pict.data, pict.linesize);

		SDL_UnlockTexture(vw->texture);
		vw->pts = pts;

		if (++state->pictQWIndex == VIDEO_PICTURE_QUEUE_SIZE)
		{
			state->pictQWIndex = 0;
		}
		SDL_LockMutex(state->pictQMutex);
		state->pictQSize++;
		SDL_UnlockMutex(state->pictQMutex);
	}
	return 0;
}

double synchronize_video(VideoState* state, AVFrame* srcFrame, double pts) {

	double frame_delay;

	if (pts != 0) 
	{
		state->videoClock = pts;
	}
	else
	{
		pts = state->videoClock;
	}
	/* update the video clock */
	frame_delay = av_q2d(state->videoStream->time_base);
	/* if we are repeating a frame, adjust clock accordingly */
	frame_delay += srcFrame->repeat_pict * (frame_delay * 0.5);
	state->videoClock += frame_delay;
	printf("Video Clock sync video: %f\n", state->videoClock);
	return pts;
}

int video_thread(void* arg)
{
	VideoState* state = (VideoState*)arg;
	AVPacket pkt1, * packet = &pkt1;
	AVFrame* pFrame;
	int ret;
	int errBuffSize = 100;
	char* errBuff = new char[errBuffSize];
	// Presentation time stamps. Remember I, B, P frames? WELL GUESS WHAT BABY YOU DIDNT LEARN EM FOR NOTHING WOOOO
	// Your presentation time stamps are basically the order that you send your frames so you can present them properly, i.e. I P B B
	// I frames are always 1st , followed by P frames and then B frames which are dependent on both.
	// Theres is another variable called dts, the decoding time stamps, which gives us the order we should decode our stuff.
	// So if were sent frames as I P B B, our pts is 1 4 2 3, and our dts is just 1 2 3 4. Should make sense,this stuff engraved into
	// our heads at this point.
	double pts;

	pFrame = av_frame_alloc();

	for (;;)
	{
		//std::printf("We are currently in the video thread, getting packets from the queue.\n");
		if (!state->pause)
		{
			if (packet_queue_get(&state->videoQ, packet, 1) < 0)
			{
				// We quit getting packets
				break;
			}
			if (packet->opaque == flush_pkt.opaque)
			{
				avcodec_flush_buffers(state->videoCtx);
				continue;
			}
			pts = 0;
			//global_video_pkt_pts = packet->pts;

			ret = avcodec_send_packet(state->videoCtx, packet);
			if (ret < 0)
			{
				fprintf(stderr, "Error during sending packet - exiting\n");
				exit(1);
			}



			while (ret >= 0)
			{
				ret = avcodec_receive_frame(state->videoCtx, pFrame);
				// Did we recieve a frame??
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				{
					av_strerror(ret, errBuff, errBuffSize);
					//std::printf("%s VIDEO THREAD\n", errBuff);
					break;
				}
				if (ret < 0)
				{
					fprintf(stderr, "Error during decoding - exiting\n");
					exit(1);
				}


				if (packet->dts == AV_NOPTS_VALUE && pFrame->pts > 0 && pFrame->pts != AV_NOPTS_VALUE)
				{
					pts = pFrame->pts;
				}
				else if (packet->dts != AV_NOPTS_VALUE)
				{
					pts = packet->dts;
				}
				else
				{
					pts = 0;
				}
				pts *= av_q2d(state->videoStream->time_base);

				pts = synchronize_video(state, pFrame, pts);

				if (queue_picture(state, pFrame, pts) < 0)
				{
					break;
				}

			}
			av_packet_unref(packet);
		}

	}

	av_frame_unref(pFrame);
	delete[] errBuff;
	return 0;
}

int stream_component_open(VideoState* state, int stream_index)
{

	AVFormatContext* pFormatCtx = state->pFormatCtx;
	AVCodecContext* codecCtx = NULL;
	AVCodecParameters* codecPar = NULL;
	const AVCodec* codec = NULL;
	SDL_AudioSpec wanted_spec, spec;



	if (stream_index < 0 || stream_index >= pFormatCtx->nb_streams)
	{
		return -1;
	}

	codecPar = pFormatCtx->streams[stream_index]->codecpar;
	if (codecPar->codec_type == AVMEDIA_TYPE_AUDIO)
	{
		// Set audio settings from codec info
		wanted_spec.freq = codecPar->sample_rate;
		wanted_spec.format = AUDIO_S16SYS;
		wanted_spec.channels = codecPar->ch_layout.nb_channels;
		wanted_spec.silence = 0;
		wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
		wanted_spec.callback = audio_callback;
		wanted_spec.userdata = state;
		state->dev = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec, 0);
		if (state->dev < 0)
		{
			fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
			return -1;
		}
		SDL_PauseAudioDevice(state->dev, 0);
	}

	codec = avcodec_find_decoder(codecPar->codec_id);
	if (!codec)
	{
		fprintf(stderr, "Unsupported codec!\n");
		return -1;
	}

	codecCtx = avcodec_alloc_context3(codec);
	if (avcodec_parameters_to_context(codecCtx, pFormatCtx->streams[stream_index]->codecpar) != 0)
	{
		fprintf(stderr, "Couldn't copy codec context");
		return -1; // Error copying codec context
	}


	if (avcodec_open2(codecCtx, codec, NULL) < 0)
	{
		fprintf(stderr, "Unsupported codec!\n");
		return -1;
	}

	switch (codecCtx->codec_type) {
	case AVMEDIA_TYPE_AUDIO:
		state->audioStreamLoc = stream_index;
		state->audioStream = pFormatCtx->streams[stream_index];
		state->audioCtx = codecCtx;
		state->audioBuffSize = 0;
		state->audioBuffIndex = 0;
		memset(&state->audioPacket, 0, sizeof(state->audioPacket));
		packet_queue_init(&state->audioQ);
		state->resampler = swr_alloc();
		swr_alloc_set_opts2(&state->resampler,
			&codecCtx->ch_layout,
			AV_SAMPLE_FMT_S16,
			44100,
			&codecCtx->ch_layout,
			codecCtx->sample_fmt,
			codecCtx->sample_rate,
			0,
			NULL);
		swr_init(state->resampler);
		break;
	case AVMEDIA_TYPE_VIDEO:
		state->videoStreamLoc = stream_index;
		state->videoStream = pFormatCtx->streams[stream_index];

		state->frameTimer = (double)av_gettime() / 1000000.0;
		state->frameLastDelay = 40e-3;
		state->videoCurrPtsTime = av_gettime();
		state->pauseTime = 0;


		state->videoCtx = codecCtx;
		packet_queue_init(&state->videoQ);
		state->videoThreadId = SDL_CreateThread(video_thread, "video", state);
		state->swsCtx = sws_getContext(state->videoCtx->width, state->videoCtx->height,
			state->videoCtx->pix_fmt, state->videoCtx->width,
			state->videoCtx->height, AV_PIX_FMT_YUV420P,
			SWS_BILINEAR, NULL, NULL, NULL
		);

		// Set up YV12 pixel array (12 bits per pixel).
		// For this media player we are taking in frames in RGB and turning them into YUV. Its just better for videos.
		// If you look above youll notive YUV420P which is just 4:2:0 sub sampling + P - which means planar, which really means
		// the components are stored in their own sperate arrays.
		state->yPlaneSz = state->videoCtx->width * state->videoCtx->height;
		// Again, this is 4:2:0 subsampling, our chorminance values size is basically quartered.
		state->uvPlaneSz = (state->videoCtx->width * state->videoCtx->height) / 4;
		state->yPlane = (Uint8*)malloc(state->yPlaneSz);
		state->uPlane = (Uint8*)malloc(state->uvPlaneSz);
		state->vPlane = (Uint8*)malloc(state->uvPlaneSz);
		// Pitch is just the width of a given line of data.
		state->uvPitch = state->videoCtx->width / 2;
		if (!state->yPlane || !state->uPlane || !state->vPlane) {
			fprintf(stderr, "Could not allocate pixel buffers - exiting\n");
			exit(1);
		}

		break;
	default:
		break;
	}
}

// This function is just the abstraction for reading the video file,
// appropriately setting the stream location for our video state structure, and then reading in packets.
// The first hald is really just the set up functions, i.e. avformat_open_input, find_stream_info ect.
// plus the fcn that dump the info about the file to our console.
// The stream_component_open function is what takes care of getting the actual codecs for the respective streams
// and setting up the threads for both audio and video packet decoding.
// The second half is then where we call the av_read_frame to get packets, which we then put into the respective queues
// so the respsective threads can decode them.
int decode_thread(void* arg)
{
	VideoState* state = (VideoState*)arg;
	AVFormatContext* pFormatCtx = avformat_alloc_context();
	AVPacket pkt1, * packet = &pkt1;
	int errBuffSize = 100;
	char* errBuff = new char[errBuffSize];
	int errCodeVid;
	int errCodeAudio;
	int errCode;
	int vidLoc = -1;
	int audioLoc = -1;

	state->videoStreamLoc = -1;
	state->audioStreamLoc = -1;
	state->pause = false;
	AVDictionary* io_dict = NULL;
	AVIOInterruptCB callback;

	global_video_state = state;

	callback.callback = decode_interrupt_cb;
	callback.opaque = state;
	if (avio_open2(&state->ioCtx, state->filename, 0, &callback, &io_dict))
	{
		fprintf(stderr, "Unable to open I/O for %s\n", state->filename);
		return -1;
	}



	// Open the file, and read the header.
	errCode = avformat_open_input(&pFormatCtx, state->filename, NULL, NULL);
	if (errCode != 0)
	{
		std::printf("Could not open file\n");
		av_strerror(errCode, errBuff, errBuffSize);
		//std::printf("%s DECODE THREAD\n", errBuff);
		return -1;
	}
	// Set the format context of our global state
	state->pFormatCtx = pFormatCtx;


	// To get the actual stream information, which will have our frames, we use the fcn below
	// The AVFormatContext struct has a member called streams, which is the array of pointers
	// to the information we want to look at/display.
	// The size of this array of pointers is in pVideo->nb_streams
	errCode = avformat_find_stream_info(pFormatCtx, NULL);
	if (errCode < 0)
	{
		std::printf("Could not read packets from stream\n");
		av_strerror(errCode, errBuff, errBuffSize);
		//std::printf("%s DECODE THREAD\n", errBuff);
		return -1;
	}

	// This is just a debug fcn that will print out info about the video we loaded.
	av_dump_format(pFormatCtx, 0, state->filename, 0);


	for (int i = 0; i < pFormatCtx->nb_streams; i++)
	{
		if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && vidLoc < 0)
		{
			vidLoc = i;
		}
		if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioLoc < 0)
		{
			audioLoc = i;
		}
	}

	if (audioLoc >= 0)
	{
		stream_component_open(state, audioLoc);
	}
	if (vidLoc >= 0)
	{
		stream_component_open(state, vidLoc);
	}

	if (state->videoStreamLoc < 0 || state->audioStreamLoc < 0)
	{
		// Im gonna be honest I hate using gotos, especially just blindly like this, but im too lazy to 
		// diverge from the tutorial so itll have to do.
		std::printf("Couldnt open codes\n");
		goto fail;
	}
	SDL_Event event;
	event.type = FF_CREATE_WINDOW_EVENT;
	event.user.data1 = state;
	SDL_PushEvent(&event);


	// Heres the main loop for decoding
	// Its just a loop that reads in packets and puts them into the right queue
	for (;;)
	{
		if (state->quit)
		{
			break;
		}
		if (!state->pause)
		{
			if (state->seek_req)
			{
				int stream_index = -1;
				int stream_index_2 = -1;
				int64_t seek_target_vid = state->seek_pos;
				int64_t seek_target_audio = state->seek_pos;

				if (state->videoStreamLoc >= 0)
				{
					stream_index = state->videoStreamLoc;
				}
				if (state->audioStreamLoc >= 0)
				{
					stream_index_2 = state->audioStreamLoc;
				}

				if (stream_index >= 0)
				{
					seek_target_vid = av_rescale_q(seek_target_vid, AV_TIME_BASE_Q, pFormatCtx->streams[stream_index]->time_base);
				}
				if (stream_index_2 >= 0)
				{
					seek_target_audio = av_rescale_q(seek_target_audio, AV_TIME_BASE_Q, pFormatCtx->streams[stream_index_2]->time_base);
				}
				errCodeVid = av_seek_frame(pFormatCtx, stream_index, seek_target_vid, state->seek_flags);
				errCodeAudio = av_seek_frame(pFormatCtx, stream_index_2, seek_target_audio, state->seek_flags);
				if (errCodeVid < 0)
				{
					std::printf("Error while seeking Video.\n");
					av_strerror(errCodeVid, errBuff, errBuffSize);
					//std::printf("%s DECODE THREAD SEEKING LOOP\n", errBuff);
				}
				else if (errCodeAudio < 0)
				{
					std::printf("Error while seeking Audio.\n");
					av_strerror(errCodeAudio, errBuff, errBuffSize);
					//std::printf("%s DECODE THREAD SEEKING LOOP\n", errBuff);
				}
				else
				{
					if (state->audioStreamLoc >= 0)
					{
						packet_queue_flush(&state->audioQ);
						packet_queue_put(&state->audioQ, &flush_pkt);
					}
					if (state->videoStreamLoc >= 0)
					{
						packet_queue_flush(&state->videoQ);
						packet_queue_put(&state->videoQ, &flush_pkt);
					}
				}

				state->seek_req = false;
			}

			if (state->audioQ.size > MAX_AUDIOQ_SIZE || state->videoQ.size > MAX_VIDEOQ_SIZE)
			{
				SDL_Delay(1);
				continue;
			}
			if (av_read_frame(state->pFormatCtx, packet) < 0)
			{
				if (state->pFormatCtx->pb->error == 0)
				{
					SDL_Delay(100);
					continue;
				}
				else
				{
					break;
				}
			}

			if (packet->stream_index == state->videoStreamLoc)
			{
				//printf("This should be a thread running decode_thread, and we are currently placing a video packet in our q\n");
				packet_queue_put(&state->videoQ, packet);
			}
			else if (packet->stream_index == state->audioStreamLoc)
			{
				packet_queue_put(&state->audioQ, packet);
			}
			else
			{
				av_packet_unref(packet);
			}

		}
		

	}

	delete[] errBuff;

	// This code in particular is to have the code wait for either the program to end, or informing it that weve ended
	while (!state->quit)
	{
		SDL_Delay(100);
	}

	// here is where we get the values for the user events.
	// We can pass user data to the event, which we do here, along with an event type
	// We push it to the event queue, and catch it later.
fail:
	if (1) {
		SDL_Event event;
		event.type = FF_QUIT_EVENT;
		event.user.data1 = state;
		SDL_PushEvent(&event);
	}


	return 0;
}

void stream_seek(VideoState* state, int64_t pos, int rel)
{
	if (!state->seek_req)
	{
		state->seek_pos = pos;
		state->seek_flags = rel < 0 ? AVSEEK_FLAG_BACKWARD : 0;
		state->seek_req = true;
	}
}

void set_pause(VideoState* state)
{
	state->pause = !state->pause;
}