#pragma once
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
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
#include "libavutil/time.h"
#include "libswresample/swresample.h"
}

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 1920000

#define MAX_AUDIOQ_SIZE (5 * 16 * 1024)
#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)

#define AV_SYNC_THRESHOLD 0.01
#define AV_NOSYNC_THRESHOLD 10.0

#define SAMPLE_CORRECTION_PERCENT_MAX 10
#define AUDIO_DIFF_AVG_NB 20

#define FF_ALLOC_EVENT   (SDL_USEREVENT)
#define FF_REFRESH_EVENT (SDL_USEREVENT + 1)
#define FF_CREATE_WINDOW_EVENT (SDL_USEREVENT + 2)
#define FF_RESET_STREAM_EVENT (SDL_USEREVENT + 3)
#define FF_QUIT_EVENT (SDL_USEREVENT + 4)

#define VIDEO_PICTURE_QUEUE_SIZE 1

#define DEFAULT_AV_SYNC_TYPE AV_SYNC_VIDEO_MASTER

enum {
	AV_SYNC_AUDIO_MASTER,
	AV_SYNC_VIDEO_MASTER,
	AV_SYNC_EXTERNAL_MASTER,
};

typedef struct PacketQueue
{
	AVPacketList* firstPacket, * lastPacket;
	// number of packets
	int				nb_packets;
	// Byte size 
	int				size;
	SDL_mutex* mutex;
	SDL_cond* cond;
} PacketQueue;

typedef struct VideoWindow
{
	SDL_Texture* texture;
	double			pts;
	int				uvPitch;
	int				width, height;
	bool			allocated;
} VideoWindow;

typedef struct VideoState {

	double				videoClock; 
	double				audioClock;
	double				frameTimer;
	double				frameLastPts;
	double				frameLastDelay;
	double				videoCurrPts;
	int64_t				videoCurrPtsTime;
	int					avSyncType;

	AVFormatContext*	pFormatCtx;
	int                 videoStreamLoc, audioStreamLoc;
	SDL_AudioDeviceID	dev;
	AVStream*			audioStream;
	AVCodecContext*		audioCtx;
	PacketQueue         audioQ;
	uint8_t             audioBuff[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
	unsigned int        audioBuffSize;
	unsigned int        audioBuffIndex;
	AVFrame             audioFrame;
	AVPacket            audioPacket;
	uint8_t*			audioPacketData;
	int                 audioPacketSize;
	double				audioDiffCum;		
	double				audioDiffThreshold;
	double				audioDiffAvgCoef;
	int					audioDiffAvgCount;

	AVStream*			videoStream;
	AVCodecContext*		videoCtx;
	PacketQueue         videoQ;
	struct SwsContext*	swsCtx;
	struct SwrContext*	resampler;

	VideoWindow         pictQ[VIDEO_PICTURE_QUEUE_SIZE];
	int                 pictQSize, pictQRIndex, pictQWIndex;
	SDL_mutex*			pictQMutex;
	SDL_cond*			pictQCond;

	Uint8*				yPlane, * uPlane, * vPlane;
	size_t				yPlaneSz, uvPlaneSz;
	int					uvPitch;


	AVIOContext*		ioCtx;
	SDL_Thread*			parseThreadId;
	SDL_Thread*			videoThreadId;

	bool				seek_req;
	int					seek_flags;
	int					seek_pos;

	char                filename[1024];
	bool                quit;
	bool				pause;
} VideoState;

extern VideoState* global_video_state;
extern SDL_Window* screen;
extern SDL_Renderer* renderer;
extern uint64_t global_video_pkt_pts;
extern AVPacket flush_pkt;

int decode_interrupt_cb(void* opaque);
void packet_queue_init(PacketQueue* q);
int packet_queue_put(PacketQueue* q, AVPacket* pkt);
static int packet_queue_get(PacketQueue* q, AVPacket* pkt, int block);
double get_audio_clock(VideoState* state);
double get_video_clock(VideoState* state);
double get_external_clock(VideoState* state);
double get_master_clock(VideoState* state);
int synchronize_audio(VideoState* state, short* samples, int samples_size, double pts);
int audio_decode_frame(VideoState* state, double* pts_ptr);
void audio_callback(void* userdata, Uint8* stream, int len);
void video_display(VideoState* state);
void video_refresh_timer(void* userdata);
void alloc_picture(void* userdata);
int queue_picture(VideoState* state, AVFrame* pFrame, double pts);
double synchronize_video(VideoState* state, AVFrame* srcFrame, double pts);
int video_thread(void* arg);
int stream_component_open(VideoState* state, int stream_index);
int decode_thread(void* arg);
void display_controls();

void stream_seek(VideoState* state, int64_t pos, int rel);
void set_pause(VideoState* state);

static Uint32 sdl_refresh_timer_cb(Uint32 interval, void* opaque)
{
	SDL_Event event;
	VideoState* state = (VideoState*)opaque;
	event.type = FF_REFRESH_EVENT;
	event.user.data1 = opaque;
	SDL_PushEvent(&event);
	return 0;
}

static void schedule_refresh(VideoState* state, int delay)
{
	//printf("Schedule refresh for %i ms\n", delay);
	SDL_AddTimer(delay, sdl_refresh_timer_cb, state);
}

static void packet_queue_flush(PacketQueue* q)
{
	AVPacketList* pkt, * pkt1;
	SDL_LockMutex(q->mutex);

	for (pkt = q->firstPacket; pkt != NULL; pkt = pkt1)
	{
		pkt1 = pkt->next;
		av_packet_unref(&pkt->pkt);
		av_freep(&pkt);
	}

	q->lastPacket = NULL;
	q->firstPacket = NULL;
	q->nb_packets = 0;
	q->size = 0;
	SDL_UnlockMutex(q->mutex);

}