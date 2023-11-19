#include <cstdio>
#include <stdio.h>
#include <assert.h>
#include "SDL.h"
#include "SDL_thread.h"

//  ffmpeg requires C linkage, not C++. Youre gonna need this extern "c" for all of its libraries
extern "C" 
{ 
	#include "libavcodec/avcodec.h"
	#include "libavformat/avformat.h"
	#include "libswscale/swscale.h"
	#include"libavutil/imgutils.h"
}

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000

typedef struct PacketQueue
{
	AVPacketList *firstPacket, *lastPacket;
	// number of packets
	int nb_packets;
	// Byte size 
	int size;
	SDL_mutex *mutex;
	SDL_cond *cond;
} PacketQueue;

PacketQueue audioQ;

void packet_queue_init(PacketQueue* q) {
	memset(q, 0, sizeof(PacketQueue));
	q->mutex = SDL_CreateMutex();
	q->cond = SDL_CreateCond();
}

int packet_queue_put(PacketQueue* q, AVPacket* pkt) {

	if (!pkt)
	{
		pkt = av_packet_alloc();
		if (!pkt)
		{
			printf("Could not allocate packet -%s\n", SDL_GetError());
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

int quit = 0;
static int packet_queue_get(PacketQueue* q, AVPacket* pkt, int block)
{
	AVPacketList* pkt1;
	int ret;

	SDL_LockMutex(q->mutex);

	for (;;) {

		if (quit) {
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

int audio_decode_frame(AVCodecContext* audioCodecCtx, uint8_t* audio_buf, int buffSize)
{
	static AVPacket pkt;
	static uint8_t* audioPacketData = NULL;
	static int audioPacketSize = 0;
	static AVFrame *frame = av_frame_alloc();
	int errBuffSize = 100;
	char* errBuff = new char[errBuffSize];
	
	int ret;
	int len1, dataSize = 0;

	for (;;)
	{
		while (audioPacketSize > 0)
		{

			ret = avcodec_send_packet(audioCodecCtx, &pkt);
			len1 = pkt.size;
			if (ret < 0 || len1 < 0)
			{
				fprintf(stderr, "Error during sending packet - exiting\n");
				audioPacketSize = 0;
				break;
			}

			
			audioPacketData += len1;
			audioPacketSize -= len1;
			dataSize = 0;

			while (ret >= 0)
			{
				ret = avcodec_receive_frame(audioCodecCtx, frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				{
					av_strerror(ret, errBuff, errBuffSize);
					std::printf("%s", errBuff);
					break;
				}
				if (ret < 0)
				{
					fprintf(stderr, "Error during decoding - exiting\n");
					exit(1);
				}
				dataSize = av_samples_get_buffer_size(NULL, audioCodecCtx->ch_layout.nb_channels, frame->nb_samples, audioCodecCtx->sample_fmt, 1);
				assert(dataSize <= buffSize);
				memcpy(audio_buf, frame->data[0], dataSize);
			}
			if (dataSize <= 0)
			{
				continue;
			}
			return dataSize;	
		}

		if (pkt.data)
		{
			av_packet_unref(&pkt);
		}
		if (quit)
		{
			return -1;
		}

		if (packet_queue_get(&audioQ, &pkt, 1) < 0)
		{
			return -1;
		}
		audioPacketData = pkt.data;
		audioPacketSize = pkt.size;
	}

	av_frame_free(&frame);
}

// A simple loop that will pull in data from another function we have written called audio_decode_frame,
// store the result in an intermediary buffer, write len bytes to stream
// get more data if theres not enough or save it for later if we got some left over.
void audio_callback(void* userdata, Uint8* stream, int len)
{
	AVCodecContext* audioCodecCtx = (AVCodecContext*)userdata;
	int len1, audioSize;

	static uint8_t audioBuff[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
	static unsigned int audioBuffSize = 0;
	static unsigned int audioBuffIndex = 0;

	while (len > 0)
	{
		if (audioBuffIndex >= audioBuffSize)
		{
			audioSize = audio_decode_frame(audioCodecCtx, audioBuff, sizeof(audioBuff));
			
			if (audioSize < 0)
			{
				audioBuffSize = 1024;
				memset(audioBuff, 0, audioBuffSize);
			}
			else
			{
				audioBuffSize = audioSize;
			}
			audioBuffIndex = 0;
		}
		
		len1 = audioBuffSize - audioBuffIndex;
		if (len1 > len)
		{
			len1 = len;
		}

		memcpy(stream, (uint8_t*)audioBuff + audioBuffIndex, len1);
		len -= len1;
		stream += len1;
		audioBuffIndex += len1;
	}

}

int main(int argc, char* argv[])
{
	AVFormatContext* pVideo = NULL;
	int buffSize = 100;
	char* errBuff = new char[buffSize];
	int vidStreamLoc = -1;
	int audioStreamLoc = -1;
	AVCodecContext* pCodecCtx = NULL;
	AVCodecParameters* pCodecPar = NULL;
	AVCodecContext* pAudioCodecCtx = NULL;
	AVCodecParameters* pAudioCodecPar = NULL;
	const AVCodec* pCodec = NULL;
	const AVCodec* pAudioCodec = NULL;
	AVFrame* pFrame = NULL;
	AVPacket packet;

	int ret;

	struct SwsContext* sws_ctx = NULL;
	SDL_Event event;
	SDL_Window* screen;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
	SDL_AudioSpec wanted_spec, spec;
	SDL_AudioDeviceID audioDevice;
	Uint8* yPlane, * uPlane, * vPlane;
	size_t yPlaneSz, uvPlaneSz;
	int uvPitch;

	// Initializing SDL with some flags for video/audio/etc.
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		printf("Could not initialize SDL2 -%s\n", SDL_GetError());
		exit(1);
	}
	

	// Below is our file I/O context.


	// The function below reads the header of our video file, and ONLY the header.
	int errCode;
	errCode = avformat_open_input(&pVideo, argv[1], NULL, NULL);
	if (errCode != 0)
	{
		std::printf("Could not open file\n");
		av_strerror(errCode, errBuff, buffSize);
		std::printf("%s", errBuff);
		return -1;
	}

	// To get the actual stream information, which will have our frames we use the fcn below
	// The AVFormatContext struct has a member called streams, which is the array of pointers
	// to the information we want to look at/display.
	// The size of this array of pointers is in pVideo->nb_streams
	if (avformat_find_stream_info(pVideo, NULL) < 0)
	{
		std::printf("Could not read packets from stream\n");
		return -1;
	}

	// This is just a debug fcn that will print out info about the video we loaded.
	av_dump_format(pVideo, 0, argv[1], 0);
	

	// So now were gonna walkthough the array of pointers until we find a video stream
	//CodecParameters are where all the information about the codec that the stream is using is stored, i.e. MP4, etc.



	for (int i = 0; i < pVideo->nb_streams; i++)
	{
		if (pVideo->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && vidStreamLoc < 0)
		{
			vidStreamLoc = i;
		}
		if (pVideo->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamLoc < 0)
		{
			audioStreamLoc = i;
		}
	}
	if (vidStreamLoc == -1)
	{
		std::printf("Could not find video stream\n");
		return -1;
	}

	if (audioStreamLoc == -1)
	{
		std::printf("Could not find audio stream\n");
		return -1;
	}

	pAudioCodecPar = pVideo->streams[audioStreamLoc]->codecpar;
	pAudioCodec = avcodec_find_decoder(pAudioCodecPar->codec_id);
	
	if (pAudioCodec == NULL)
	{
		std::printf("Could not find Audio Codec\n");
		return -1;
	}
	pAudioCodecCtx = avcodec_alloc_context3(pAudioCodec);


	if (avcodec_parameters_to_context(pAudioCodecCtx, pAudioCodecPar) < 0)
	{
		std::printf("Could not copy context for audio\n");
		return -1;
	}


	if (avcodec_open2(pAudioCodecCtx, pAudioCodec, NULL) < 0)
	{
		std::printf("Could not open audio codec\n");
		return -1;
	}

	// Set audio settings from codec info
	wanted_spec.freq = pAudioCodecCtx->sample_rate;
	wanted_spec.format = AUDIO_S16SYS;
	wanted_spec.channels = pAudioCodecCtx->ch_layout.nb_channels;
	wanted_spec.silence = 0;
	wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
	wanted_spec.callback = audio_callback;
	wanted_spec.userdata = pAudioCodecCtx;

	if (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
		fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
		return -1;
	}


	packet_queue_init(&audioQ);
	SDL_PauseAudio(0);


	// We shouldnt actually use the AVCodecContext from the video stream directly, so we isntead use the param to ctx fcn
	// to create a context with our params in another location

	pCodecPar = pVideo->streams[vidStreamLoc]->codecpar;
	pCodec = avcodec_find_decoder(pCodecPar->codec_id);
	

	// Even though we just obtained the pointer to the codecs info, we still need the find the actual codec
	// and open it, which is what were doing below.
	if (pCodec == NULL)
	{
		std::printf("Could not find Codec\n");
		return -1;
	}
	pCodecCtx = avcodec_alloc_context3(pCodec);


	if (avcodec_parameters_to_context(pCodecCtx, pCodecPar) < 0)
	{
		std::printf("Could not copy context\n");
		return -1;
	}


	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
	{
		std::printf("Could not open codec\n");
		return -1;
	}
	
	//Allocate a video frame for our stream.
	pFrame = av_frame_alloc();
	// Once we turn our parameters into a context, we can go ahead and create a screen whose size is the frame width/height
	screen = SDL_CreateWindow(argv[1], 
		SDL_WINDOWPOS_UNDEFINED, 
		SDL_WINDOWPOS_UNDEFINED, 
		pCodecCtx->width,
		pCodecCtx->height,
		SDL_WINDOW_OPENGL);

	if (!screen)
	{
		fprintf(stderr, "SDL: Could not create window - exiting\n");
		exit(1);
	}

	renderer = SDL_CreateRenderer(screen, -1, 0);
 
	if (!renderer)
	{
		fprintf(stderr, "SDL: Could not create renderer - exiting\n");
		exit(1);
	}

	texture = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_YV12,
		SDL_TEXTUREACCESS_STREAMING,
		pCodecCtx->width,
		pCodecCtx->height
	);

	if (!texture)
	{
		fprintf(stderr, "SDL: Could not create texture - exiting\n");
		exit(1);
	}

	sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, 
		pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height,
		AV_PIX_FMT_YUV420P,
		SWS_BILINEAR,
		NULL,
		NULL,
		NULL);

	// Set up YV12 pixel array (12 bits per pixel).
	// For this media player we are taking in frames in RGB and turning them into YUV. Its just better for videos.
	// If you look above youll notive YUV420P which is just 4:2:0 sub sampling + P - which means planar, which really means
	// the components are stored in their own sperate arrays.
	yPlaneSz = pCodecCtx->width * pCodecCtx->height;
	// Again, this is 4:2:0 subsampling, our chorminance values size is basically quartered.
	uvPlaneSz = pCodecCtx->width * pCodecCtx->height / 4;
	yPlane = (Uint8*)malloc(yPlaneSz);
	uPlane = (Uint8*)malloc(uvPlaneSz);
	vPlane = (Uint8*)malloc(uvPlaneSz);
	if (!yPlane || !uPlane || !vPlane) {
		fprintf(stderr, "Could not allocate pixel buffers - exiting\n");
		exit(1);
	}

	// Pitch is just the width of a given line of data.
	uvPitch = pCodecCtx->width / 2;

	while (av_read_frame(pVideo, &packet) >= 0)
	{
		if (packet.stream_index == vidStreamLoc)
		{
			ret = avcodec_send_packet(pCodecCtx, &packet);
			if (ret < 0)
			{
				fprintf(stderr, "Error during sending packet - exiting\n");
				exit(1);
			}

			while (ret >= 0)
			{
				ret = avcodec_receive_frame(pCodecCtx, pFrame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				{
					/*av_strerror(ret, errBuff, buffSize);
					std::printf("%s", errBuff);*/
					break;
				}
				if (ret < 0)
				{
					fprintf(stderr, "Error during decoding - exiting\n");
					exit(1);
				}
				AVFrame pict;
				pict.data[0] = yPlane;
				pict.data[1] = uPlane;
				pict.data[2] = vPlane;
				pict.linesize[0] = pCodecCtx->width;
				pict.linesize[1] = uvPitch;
				pict.linesize[2] = uvPitch;

				// Here is where the actual conversion of the image into YUV that SDL2 uses happens
				sws_scale(sws_ctx, (uint8_t const* const*)pFrame->data,
					pFrame->linesize, 0, pCodecCtx->height, pict.data, pict.linesize);


				SDL_UpdateYUVTexture(
					texture,
					NULL,
					yPlane,
					pCodecCtx->width,
					uPlane,
					uvPitch,
					vPlane,
					uvPitch
				);

				SDL_RenderClear(renderer);
				SDL_RenderCopy(renderer, texture, NULL, NULL);
				SDL_RenderPresent(renderer);
			}
		}
		else if (packet.stream_index == audioStreamLoc)
		{
			packet_queue_put(&audioQ, &packet);
		}
		else
		{
			av_packet_unref(&packet);
		}


		SDL_PollEvent(&event);
		switch (event.type)
		{
		case SDL_QUIT:
			SDL_DestroyTexture(texture);
			SDL_DestroyRenderer(renderer);
			SDL_DestroyWindow(screen);
			quit = 1;
			SDL_Quit();
			exit(0);
			break;
		default:break;
		}
	}

	av_frame_free(&pFrame);
	free(yPlane);
	free(uPlane);
	free(vPlane);

	avcodec_close(pCodecCtx);
	avcodec_parameters_free(&pCodecPar);
	avcodec_close(pAudioCodecCtx);
	avcodec_parameters_free(&pAudioCodecPar);
	//avformat_close_input(&pVideo);

	return 0;
	
}