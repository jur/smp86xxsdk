/*
 * Copyright (c) 2015, Juergen Urban
 * All rights reserved.
 *
 * The test play mp4 video file.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 */

#include <sys/mman.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <getopt.h>

#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

#include <rua.h>
#include <dcc.h>
#include <rcc.h>

/** Define to play audio also. */
#define PLAY_AUDIO

/** Define to debug time. */
#undef TIMEDEBUG

/** There is only one chip in the DMA-2500. */
#define DEFAULT_CHIP 0
/** DRAM can be 0 or 1. */
#define DEFAULT_DRAM_CONTROLLER 1
/** Size of buffers used to transfer audio and video data. */
#define DMA_BUFFER_SIZE_LOG2 14
/** Size of buffers used to transfer audio and video data. */
#define DMA_BUFFER_SIZE (1 << DMA_BUFFER_SIZE_LOG2)
/** How many video stream data to buffer until playing should start. */
//#define VID_PRE_BUFFER_SIZE 48704
/** Print debug message. */
#define DPRINTF(args...) \
	do { \
		if (debug) { \
			printf(args); \
		} \
	} while(0)

/** Number of ticks for 1 second audio. */
#define AUDIO_TIME_RES 90000LL
#define VIDEO_TIME_RES 30000LL
#define STC_TIME_RES 24000LL
#define MAX_SPEED 2
#define MIN_SPEED -2
/** Maximum time to buffer data when paused. */
#define MAX_BUFFER_TIME (30 * VIDEO_TIME_RES)
#define MIN_BUFFER_TIME (10 * VIDEO_TIME_RES)
#define REBUFFER_TIME (1 * VIDEO_TIME_RES)
#define JUMP_TIME 60

typedef struct {
	int64_t max_buffer_time;
	int64_t min_buffer_time;
	int64_t rebuffer_time;
	int start_minute;
	int start_second;
	int jump_time;
} play_config_t;

typedef struct {
	struct RUA *pRUA;
	struct DCC *pDCC;
	struct RUABufferPool *pDMA;
	struct DCCSTCSource *pStcSource;
	struct DCCVideoSource *pVideoSource;
#ifdef PLAY_AUDIO
	struct DCCAudioSource *pAudioSource;
#endif
	RMuint32 SurfaceID;
	RMuint32 video_decoder;
	RMuint32 spu_decoder;
	RMuint32 video_timer;
	RMuint8 *videobuffer;
	RMuint32 videotransferred;
#ifdef PLAY_AUDIO
	RMuint32 audio_decoder;
	RMuint8 *audiobuffer;
#endif
	RMuint32 audio_engine;
	RMuint32 audio_timer;
	/** True when video engine plays video. */
	int playing;
	/** True when all data are transferred to the video engine. */
	int ending;

	/* Remote control */
	volatile int stopped;
	volatile int paused;
	volatile int speed;
	int irfd;

	/** Video playback was started. */
	int play_started;

	/** Current time played */
	RMuint64 time;
	/** Last video ime buffered. */
	int64_t last_time;
	/** Current time buffered. */
	int64_t cur_time;
	/** Video should be started at this time (jump). */
	int64_t startplaypts;
	/** True when the player should jump to some position. */
	int jump;
	/** Time base of video. */
	AVRational time_base;

	/** PTS of first video frame. */
	int64_t startpts;
	/** Found first video frame which should be played after jump. */
	int started;

	/* Configuration parameters. */
	play_config_t cfg;
} app_rua_context_t;

/** Set to 1 to enable debug output. */
static int debug = 0;
static app_rua_context_t context_g;

static void cleanup(app_rua_context_t *context)
{
	RMstatus rv;

	if (context == NULL) {
		return;
	}

	if (context->pDMA != NULL) {
		rv = RUAClosePool(context->pDMA);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot close pool, rv = %d\n", rv); 
		}
		context->pDMA = NULL;
	}

#ifdef PLAY_AUDIO
	if (context->pAudioSource != NULL) {
		rv = DCCCloseAudioSource(context->pAudioSource);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot close audio source, rv = %d\n", rv); 
		}
		context->pAudioSource = NULL;
	}
#endif

	if (context->pVideoSource != NULL) {
		rv = DCCCloseVideoSource(context->pVideoSource);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot close video source, rv = %d\n", rv); 
		}
		context->pVideoSource = NULL;
	}

	if (context->pStcSource != NULL) {
		rv = DCCSTCClose(context->pStcSource);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot close STC, rv = %d\n", rv); 
		}
		context->pStcSource = NULL;
	}

	if (context->pDCC != NULL) {
		rv = DCCClose(context->pDCC);
		context->pDCC = NULL;
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot close DCC, rv = %d\n", rv); 
		}
	}

	if (context->pRUA != NULL){
		rv = RUADestroyInstance(context->pRUA);
		context->pRUA = NULL;
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot destroy RUA instance, rv = %d\n", rv);
		}
	}

	if (context->irfd >= 0) {
		RCCClose(context->irfd);
		context->irfd = -1;
	}

	fprintf(stderr, "end cleanup\n");
}

static RMstatus set_speed(app_rua_context_t *context)
{
	int su;
	int sd;

	su = 1;
	sd = 1;
	if (context->speed > 0) {
		su += context->speed;
	} else if (context->speed < 0) {
		sd -= context->speed;
	}
	return DCCSTCSetSpeed(context->pStcSource, su, sd);
}

static unsigned int get_key(app_rua_context_t *context, int usec)
{
	unsigned int key = 0;

	if (context->irfd >= 0) {
		key = RCCGetKey(context->irfd, usec);
		switch(key) {
			case RC_POWER:
			case RC_STOP:
				context->stopped = 1;
				break;

			case RC_OK:
				if (context->paused) {
					context->paused = 0;
					if (context->playing) {
						RMstatus rv;

						printf("Playing\n");
						rv = DCCSTCPlay(context->pStcSource);
						if (RMFAILED(rv)) {
							fprintf(stderr, "Cannot start, rv = %d\n", rv);
						}
					}
				} else {
					if (context->playing) {
						RMstatus rv;

						context->paused = 1;
						printf("Pausing\n");
						rv = DCCSTCStop(context->pStcSource);
						if (RMFAILED(rv)) {
							fprintf(stderr, "Cannot stop, rv = %d\n", rv);
						}
					}
				}
				break;

			case RC_LEFT:
				if (context->playing) {
					int rv;

					if (context->speed > MIN_SPEED) {
						context->speed--;
					}

					rv = set_speed(context);
					if (RMFAILED(rv)) {
						fprintf(stderr, "Cannot set speed to %d, rv = %d\n", context->speed, rv);
					}
				}
				break;

			case RC_RIGHT:
				if (context->playing) {
					int rv;

					if (context->speed < MAX_SPEED) {
						context->speed++;
					}

					rv = set_speed(context);
					if (RMFAILED(rv)) {
						fprintf(stderr, "Cannot set speed to %d, rv = %d\n", context->speed, rv);
					}
				}
				break;

			case RC_UP:
				if (context->playing) {
					context->startplaypts =
						context->time +
						context->cfg.jump_time *
						context->time_base.den/
						context->time_base.num;
					printf("Set startplaypts %s (%s) buffer until %s (%s)\n",
						av_ts2timestr(context->startplaypts, &context->time_base),
						av_ts2str(context->startplaypts),
						av_ts2timestr(context->last_time, &context->time_base),
						av_ts2str(context->last_time));
					context->jump = 1;
				}
				break;

			default:
				break;
		}
	}

	return key;
}

static void signalhandler(int sig)
{
	app_rua_context_t *context = &context_g;

	(void) sig;

	context->stopped = 1;
}

static void signalcleanup(int sig)
{
	printf("Signal %d received, cleanup.\n", sig);

	exit(1);
}

static void exitcleanup(void)
{
	app_rua_context_t *context = &context_g;

	printf("Cleanup at exit.\n");

	cleanup(context);
}

static RMstatus video_init(app_rua_context_t *context, play_config_t *cfg)
{
	RMstatus rv;

	memset(context, 0, sizeof(*context));

	rv = RUACreateInstance(&context->pRUA, DEFAULT_CHIP);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error creating RUA instance! rv =%d\n", rv);
		return rv;
	}

	rv = DCCOpen(context->pRUA, &context->pDCC);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error Opening DCC! rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCInitMicroCodeEx(context->pDCC, DCCInitMode_LeaveDisplay);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot initialize microcode, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCSetMemoryManager(context->pDCC, DEFAULT_DRAM_CONTROLLER);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot initialize dram, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	context->speed = 0;
	context->cfg = *cfg;

	return RM_OK;
}

static RMstatus configure_video(app_rua_context_t *context)
{
	struct DCCStcProfile stc_profile;
	RMstatus rv;
	RMint32 video_delay_ms = 0;
	RMint32 audio_delay_ms = 0;
	struct DCCXVideoProfile video_profile;
#ifdef PLAY_AUDIO
	struct DCCAudioProfile audio_profile;
	struct AudioDecoder_AACParameters_type aac_parameters;
#endif

	memset(&stc_profile, 0, sizeof(stc_profile));
	stc_profile.STCID = 0;
	stc_profile.master = Master_STC;
	stc_profile.stc_timer_id = 3 * stc_profile.STCID + 0;
	stc_profile.stc_time_resolution = AUDIO_TIME_RES;
	stc_profile.video_timer_id = 3 * stc_profile.STCID + 1;
	stc_profile.video_time_resolution = AUDIO_TIME_RES;
	stc_profile.video_offset = -(video_delay_ms * (RMint32)stc_profile.video_time_resolution / 1000);
	stc_profile.audio_timer_id = 3 * stc_profile.STCID + 2;
	stc_profile.audio_time_resolution = AUDIO_TIME_RES;
	stc_profile.audio_offset = -(audio_delay_ms * (RMint32)stc_profile.audio_time_resolution / 1000);

	rv = DCCSTCOpen(context->pDCC, &stc_profile, &context->pStcSource);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot open STC, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}
	memset(&video_profile, 0, sizeof(video_profile));

	video_profile.BitstreamFIFOSize = 8 * 1024 * 1024;
	video_profile.XferFIFOCount = 1024;
	video_profile.MpegEngineID = DEFAULT_DRAM_CONTROLLER;
	video_profile.VideoDecoderID = 0;
	video_profile.ProtectedFlags = 0;
	video_profile.PtsFIFOCount = 600;
	video_profile.InbandFIFOCount = 16;
	video_profile.XtaskInbandFIFOCount = 0;
	video_profile.SPUBitstreamFIFOSize = 0;
	video_profile.SPUXferFIFOCount = 0;
	video_profile.STCID = stc_profile.STCID; // 0
	video_profile.Codec = EMhwlibVideoCodec_H264; // EMhwlibVideoCodec_MPEG2
	video_profile.Profile = 0;
	video_profile.Level = 10;
	video_profile.ExtraPictureBufferCount = 0;
	video_profile.MaxWidth = 1920;
	video_profile.MaxHeight = 1080;

	rv = DCCXOpenVideoDecoderSource(context->pDCC, &video_profile, &context->pVideoSource);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot open video decoder source, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCXSetVideoDecoderSourceCodec(context->pVideoSource, video_profile.Codec);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set video decoder codec, rv =%d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCGetScalerModuleID(context->pDCC, DCCRoute_Main, DCCSurface_Video, 0, &context->SurfaceID);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot get surface id, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCSetSurfaceSource(context->pDCC, context->SurfaceID, context->pVideoSource);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set surface id, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

#ifdef PLAY_AUDIO
	audio_profile.BitstreamFIFOSize = 1024 * 1024; // MP2: 0x00080000
	audio_profile.XferFIFOCount = 1024; // MP2: 0
	audio_profile.DemuxProgramID = 0;
	audio_profile.AudioEngineID = 0;
	audio_profile.AudioDecoderID = 0;
	audio_profile.STCID = 0;
	rv = DCCOpenAudioDecoderSource(context->pDCC, &audio_profile, &context->pAudioSource);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot open audio decoder source, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCGetAudioDecoderSourceInfo(context->pAudioSource, &context->audio_decoder, &context->audio_engine, &context->audio_timer);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot get audio decoder source info, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	aac_parameters.InputFormat = ADTS_header;
	aac_parameters.OutputSpdif = OutputSpdif_Uncompressed;
	aac_parameters.OutputDualMode = DualMode_Stereo;
	aac_parameters.OutputChannels = Aac_LR;
	aac_parameters.Acmod2DualMode = 1;
	aac_parameters.OutputLfe = 0;
	aac_parameters.OutputSurround20 = SurroundAsStream;
	aac_parameters.BassMode = 0;
	rv = DCCSetAudioAACFormat(context->pAudioSource, &aac_parameters);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set audio aac format source info, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCSetAudioSourceVolume(context->pAudioSource, 0x10000000);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set audio volume, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}
#endif

	rv = DCCGetVideoDecoderSourceInfo(context->pVideoSource, &context->video_decoder, &context->spu_decoder, &context->video_timer);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot get video decoder source info, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}
	return RM_OK;
}

static RMstatus transfer_data(app_rua_context_t *context, RMuint8 *data, RMuint32 datasize, RMuint32 decoder, RMuint8 **pbuffer, RMuint32 *bufpos)
{
	RMstatus rv;
	struct emhwlib_info video_info;
	RMuint32 size;

	size = datasize - *bufpos;
	if (size > DMA_BUFFER_SIZE) {
		size = DMA_BUFFER_SIZE;
	}

	if (*pbuffer == NULL) {
		rv = RUAGetBuffer(context->pDMA, pbuffer, 0);
		if (RMFAILED(rv)) {
			*pbuffer = NULL;
			DPRINTF("Cannot get buffer, rv = %d\n", rv);
			if (rv != RM_PENDING) {
				fprintf(stderr, "Cannot get buffer, rv = %d\n", rv);
				cleanup(context);
			}
			return rv;
		}
		memcpy(*pbuffer, &data[*bufpos], size);
	}

	memset(&video_info, 0, sizeof(video_info));
	DPRINTF("RUASendData(%p, (%u, %u), %p, %p, %u, %p, %u)\n", context->pRUA, (decoder >> 16) & 0xFF, decoder & 0xFF, context->pDMA, *pbuffer, size, &video_info, sizeof(video_info));
	rv = RUASendData(context->pRUA, decoder, context->pDMA, *pbuffer, size, &video_info, sizeof(video_info));
	DPRINTF("RUASendData rv = %d\n", rv);
	if (RMFAILED(rv)) {
		if (rv != RM_PENDING) {
			fprintf(stderr, "Cannot send buffer %p, rv = %d\n", *pbuffer, rv);
			cleanup(context);
		}
		return rv;
	}
	/* printf("video_info.ValidFields %lu\n", video_info.ValidFields); */

	do {
		rv = RUAReleaseBuffer(context->pDMA, *pbuffer);
	} while(rv == RM_PENDING);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot release buffer %p, rv = %d\n", *pbuffer, rv);
		cleanup(context);
		*pbuffer = NULL; /* TBD: How to free this? */
		return rv;
	}
	*pbuffer = NULL;
	*bufpos += size;
	if (*bufpos == datasize) {
		*bufpos = 0;
		return RM_OK;
	} else {
		return RM_PENDING;
	}
}

static RMstatus start_play(app_rua_context_t *context)
{
	RMstatus rv;

	printf("Start play\n");
	rv = DCCSTCPlay(context->pStcSource);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set play mode, rv = %d\n", rv);
		return rv;
	}
	if (!context->play_started) {
		rv = DCCPlayVideoSource(context->pVideoSource, DCCVideoPlayFwd);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot play video source, rv = %d\n", rv);
			return rv;
		}
#ifdef PLAY_AUDIO
		/* TBD: Use DCCPlayMultipleAudioSource() instead. */
		rv = DCCPlayAudioSource(context->pAudioSource);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot play video source, rv = %d\n", rv);
			return rv;
		}
#endif
	}
	context->playing = 1;
	context->play_started = 1;
	return RM_OK;
}

static int write_video_packet(void *opaque, uint8_t *buf, int buf_size)
{
	app_rua_context_t *context = opaque;
	RMstatus rv;
	RMuint32 bufpos = 0;

	do {
		rv = transfer_data(context, buf, buf_size, context->video_decoder, &context->videobuffer, &bufpos);
	} while (rv == RM_PENDING);
	if (rv == RM_ERROR) {
		return -1;
	}
	context->videotransferred += buf_size;
#ifdef VID_PRE_BUFFER_SIZE
	if (!context->play_started && (context->videotransferred > VID_PRE_BUFFER_SIZE)) {
		rv = start_play(context);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot start to play video, rv = %d\n", rv);
			return -1;
		}
	}
#endif

	return buf_size;
}

#ifdef PLAY_AUDIO
static int write_audio_packet(void *opaque, uint8_t *buf, int buf_size)
{
	app_rua_context_t *context = opaque;
	RMstatus rv;
	RMuint32 bufpos = 0;
	int printed = 0;

	do {
		rv = transfer_data(context, buf, buf_size, context->audio_decoder, &context->audiobuffer, &bufpos);
		if (rv == RM_PENDING) {
			if (context->stopped) {
				fprintf(stderr, "Buffer overrun while stopped.\n");
				return -1;
			}
			if (context->paused) {
				if (!printed) {
					fprintf(stderr, "Buffer overrun while pausing.\n");
					printed = 1;
				}
				get_key(context, 10000);
			}
		}
	} while (rv == RM_PENDING);
	if (rv == RM_ERROR) {
		return -1;
	}

	return buf_size;
}
#endif

#ifdef TIMEDEBUG
static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt,
	const char *tag, const char *type)
{
	AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

	printf("%s:%s: pts:%llu %s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
		tag, type, pkt->pts, av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
		av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
		av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
		pkt->stream_index);
}
#endif

static RMstatus check_jump(app_rua_context_t *context)
{
	RMstatus rv;

	if (!context->started || context->jump) {
		if ((context->startplaypts == 0) || (context->cur_time >= (context->startplaypts - context->startpts))) {
			printf("start_time %s %lld (%llds)\n", av_ts2timestr(context->startplaypts, &context->time_base), context->startplaypts, context->startplaypts/VIDEO_TIME_RES);
			if (context->started) {
				if ((context->last_time > 0)
					&& (((uint64_t) context->last_time) > (context->time + context->cfg.min_buffer_time))) {
					if (context->playing) {
						if (!context->paused && !context->ending) {
							printf("Stop play\n");
							rv = DCCSTCStop(context->pStcSource);
							if (RMFAILED(rv)) {
								fprintf(stderr, "Cannot stop, rv = %d\n", rv);
								cleanup(context);
								return rv;
							}
						} else {
							context->paused = 0;
						}
						context->playing = 0;
					}
				}
			}
			rv = DCCSTCSetTime(context->pStcSource, context->startplaypts, VIDEO_TIME_RES);
			if (RMFAILED(rv)) {
				fprintf(stderr, "Cannot set time, rv = %d\n", rv);
			}
			context->started = 1;
			context->jump = 0;
		}
	}
	return RM_OK;
}

/**
 * Play mp4 video
 *
 * @param context Context of hardware
 * @param videofile Path to video file to play.
 * @param minutes Time at which playing should start (minutes)
 * @param seconds Time at which playing should start (seconds)
 *
 * @returns Result of operation
 * @retval 0 on success
 * @retval 1 on error
 */
static int play_mp4_video(app_rua_context_t *context, const char *videofile, int minutes, int seconds)
{
	AVFormatContext *ifmt_ctx = NULL;
	AVFormatContext *vidfmt_ctx = NULL;
#ifdef PLAY_AUDIO
	AVFormatContext *audfmt_ctx = NULL;
#endif
	AVBitStreamFilterContext *bsf = NULL;
	AVPacket pkt;
	int ret;
	unsigned int i;
	int buffersize = DMA_BUFFER_SIZE;
	int *streamidxmap = NULL;
	int64_t print_time;

	context->time_base.num = 1;
	context->time_base.den = VIDEO_TIME_RES;
	context->startpts = LLONG_MAX;
	context->started = 0;

	context->last_time = -1;

	/* Start at a specific time to play. */
	context->startplaypts = ((minutes * 60) + seconds) * context->time_base.den/context->time_base.num;

	context->videobuffer = NULL;
	context->videotransferred = 0;

	av_register_all();
	if ((ret = avformat_open_input(&ifmt_ctx, videofile, 0, 0)) < 0) {
		fprintf(stderr, "Could not open input file '%s'", videofile);
		goto end;
	}
	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		fprintf(stderr, "Failed to retrieve input stream information");
		goto end;
	}
	av_dump_format(ifmt_ctx, 0, videofile, 0);

	avformat_alloc_output_context2(&vidfmt_ctx, NULL, "rawvideo", NULL);
	if (!vidfmt_ctx) {
		fprintf(stderr, "Could not create video output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
#ifdef PLAY_AUDIO
	avformat_alloc_output_context2(&audfmt_ctx, NULL, "adts", NULL);
	if (!audfmt_ctx) {
		fprintf(stderr, "Could not create audio output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
#endif
	streamidxmap = malloc(sizeof(streamidxmap[0]) * ifmt_ctx->nb_streams);
	if (streamidxmap == NULL) {
		fprintf(stderr, "out of memory\n");
		goto end;
	}
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVStream *out_stream;
		AVFormatContext *ofmt_ctx;
		const char *type;

		streamidxmap[i] = -1;

		if (in_stream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			ofmt_ctx = vidfmt_ctx;
			type = "video";
		} else if (in_stream->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
#ifdef PLAY_AUDIO
			ofmt_ctx = audfmt_ctx;
			type = "audio";
#else
			fprintf(stderr, "Ignoring audio streams\n");
#endif
		} else {
			fprintf(stderr, "Neither video nor audio stream ignoring\n");
			continue;
		}
		out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);

		if (!out_stream) {
			fprintf(stderr, "Failed allocating output stream\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}
		streamidxmap[i] = out_stream->index;
		ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
		if (ret < 0) {
			fprintf(stderr,
				"Failed to copy context from input to output stream codec context\n");
			goto end;
		}
		out_stream->codec->codec_tag = 0;
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
#ifdef CONVERT_TIME
		out_stream->time_base.num = 1;
		if (in_stream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			out_stream->time_base.den = VIDEO_TIME_RES;
		} else if (in_stream->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			out_stream->time_base.den = AUDIO_TIME_RES;
		}
#else
		out_stream->codec->time_base = in_stream->time_base;
		out_stream->time_base = in_stream->time_base;
#endif
		printf("time_base %s num %u den %u\n", type, out_stream->time_base.num, out_stream->time_base.den);
		out_stream->codec->bit_rate = in_stream->codec->bit_rate;
		out_stream->codec->codec_id = in_stream->codec->codec_id;
		out_stream->codec->codec_type = in_stream->codec->codec_type;
		out_stream->codec->rc_max_rate = in_stream->codec->rc_max_rate;
		out_stream->codec->rc_buffer_size = in_stream->codec->rc_buffer_size;
		out_stream->codec->field_order = in_stream->codec->field_order;
		if (out_stream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
#ifdef CONVERT_TIME
			RMstatus rv;
			RMuint32 timeres;
#endif

			out_stream->codec->pix_fmt = in_stream->codec->pix_fmt;
			out_stream->codec->width = in_stream->codec->width;
			out_stream->codec->height = in_stream->codec->height;
			out_stream->codec->has_b_frames = in_stream->codec->has_b_frames;
			out_stream->codec->sample_aspect_ratio = in_stream->sample_aspect_ratio;

#ifdef CONVERT_TIME
			timeres = out_stream->time_base.den / out_stream->time_base.num;
			printf("Set video time resolution to %u\n", timeres);
			rv = DCCSTCSetTimeResolution(context->pStcSource, DCC_Video, timeres);
			if (RMFAILED(rv)) {
				fprintf(stderr, "Cannot set time resolution for video, rv = %d\n", rv);
				ret = AVERROR_UNKNOWN;
				goto end;
			}
#endif

		} else if (out_stream->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
#ifdef CONVERT_TIME
			RMstatus rv;
			RMuint32 timeres;
#endif

			out_stream->codec->channel_layout = in_stream->codec->channel_layout;
			out_stream->codec->sample_rate = in_stream->codec->sample_rate;
			out_stream->codec->channels = in_stream->codec->channels;
			out_stream->codec->frame_size = in_stream->codec->frame_size;
			out_stream->codec->audio_service_type = in_stream->codec->audio_service_type;
			out_stream->codec->block_align = in_stream->codec->block_align;

#ifdef CONVERT_TIME
			timeres = out_stream->time_base.den / out_stream->time_base.num;
			printf("Set audio time resolution to %u\n", timeres);
			rv = DCCSTCSetTimeResolution(context->pStcSource, DCC_Audio, timeres);
			if (RMFAILED(rv)) {
				fprintf(stderr, "Cannot set time resolution for audio, rv = %d\n", rv);
				ret = AVERROR_UNKNOWN;
				goto end;
			}
#endif
		}

		av_dump_format(ofmt_ctx, 0, type, 1);
	}

	vidfmt_ctx->pb = avio_alloc_context(NULL, buffersize, 1, context, NULL, write_video_packet, NULL);
	if (vidfmt_ctx->pb == NULL) {
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	vidfmt_ctx->pb->direct = AVIO_FLAG_DIRECT;
	vidfmt_ctx->pb->seekable = 0;
	vidfmt_ctx->pb->max_packet_size = DMA_BUFFER_SIZE;

#ifdef PLAY_AUDIO
	audfmt_ctx->pb = avio_alloc_context(NULL, buffersize, 1, context, NULL, write_audio_packet, NULL);
	if (audfmt_ctx->pb == NULL) {
		fprintf(stderr, "Out of memory\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	audfmt_ctx->pb->direct = AVIO_FLAG_DIRECT;
	audfmt_ctx->pb->seekable = 0;
	audfmt_ctx->pb->max_packet_size = DMA_BUFFER_SIZE;
#endif

	bsf = av_bitstream_filter_init("h264_mp4toannexb");
	if (bsf == NULL) {
		fprintf(stderr, "Error occurred when getting filter\n");
		goto end;
	}
	ret = avformat_write_header(vidfmt_ctx, NULL);
	if (ret < 0) {
		fprintf(stderr, "Error occurred when opening output file\n");
		goto end;
	}
#ifdef PLAY_AUDIO
	ret = avformat_write_header(audfmt_ctx, NULL);
	if (ret < 0) {
		fprintf(stderr, "Error occurred when opening output file\n");
		goto end;
	}
#endif
	print_time = 0;
	while (1) {
		AVStream *in_stream, *out_stream;
		AVFormatContext *ofmt_ctx;
		const char *type;
		RMstatus rv;
#ifdef TIMEDEBUG
		RMuint64 time;
#endif

		get_key(context, 0);

		if (context->stopped) {
			printf("Received signal, stopping...\n");
			break;
		}
		rv = DCCSTCGetTime(context->pStcSource, &context->time, VIDEO_TIME_RES);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot get time, rv = %d\n", rv);
			cleanup(context);
			return rv;
		}
		if (context->paused) {
			if ((context->last_time > 0)
				&& (((uint64_t) context->last_time) > (context->time + context->cfg.max_buffer_time))) {
				/* Wait until stuff is played. */
				continue;
			}
		}
		if (context->started && !context->playing) {
			/* Wait until enough frames are buffered then start playing. */
			if ((context->last_time > 0)
				&& (((uint64_t) context->last_time) > (context->time + context->cfg.min_buffer_time))) {
				rv = start_play(context);
				if (RMFAILED(rv)) {
					fprintf(stderr, "Cannot start to play video, rv = %d\n", rv);
					cleanup(context);
					return rv;
				}
			}
		}
		if (context->playing) {
			if ((context->last_time > 0)
				&& (((uint64_t) context->last_time) < (context->time + context->cfg.rebuffer_time))) {
				printf("Rebuffering...\n");
				rv = DCCSTCStop(context->pStcSource);
				if (RMFAILED(rv)) {
					fprintf(stderr, "Cannot stop video, rv = %d\n", rv);
					cleanup(context);
					return rv;
				}
				context->playing = 0;
			}
		}

		ret = av_read_frame(ifmt_ctx, &pkt);
		if (ret < 0)
			break;
		in_stream = ifmt_ctx->streams[pkt.stream_index];
		if (in_stream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			ofmt_ctx = vidfmt_ctx;
			type = "video";
#ifdef PLAY_AUDIO
		} else if (in_stream->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			ofmt_ctx = audfmt_ctx;
			type = "audio";
#endif
		} else {
			av_free_packet(&pkt);
			continue;
		}
#ifdef TIMEDEBUG
		rv = DCCSTCGetTime(context->pStcSource, &time, out_stream->time_base.den/out_stream->time_base.num);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot get time, rv = %d\n", rv);
			ret = AVERROR_UNKNOWN;
			goto end;
		}
		printf("Current time: %llu %s\n", time, av_ts2timestr(time, &out_stream->time_base));
		log_packet(ifmt_ctx, &pkt, "in", type);
#endif
		/* Convert input stream index into output stream index. */
		if ((pkt.stream_index < 0) || (((unsigned int) pkt.stream_index) >= ifmt_ctx->nb_streams)) {
			av_free_packet(&pkt);
			printf("Stream index %u out of range\n", pkt.stream_index);
			continue;
		}
		pkt.stream_index = streamidxmap[pkt.stream_index];
		out_stream = ofmt_ctx->streams[pkt.stream_index];
		/* copy packet */
		if (pkt.pts != AV_NOPTS_VALUE) {
			pkt.pts = av_rescale_q(pkt.pts, in_stream->time_base, out_stream->time_base);
		}
		if (pkt.dts != AV_NOPTS_VALUE) {
			pkt.dts = av_rescale_q(pkt.dts, in_stream->time_base, out_stream->time_base);
		}
		if (pkt.duration > 0) {
			pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		}
		pkt.pos = -1;

		if ((in_stream->codec->codec_type == AVMEDIA_TYPE_VIDEO) && (bsf != NULL)) {
			if (pkt.pts < context->startpts) {
				if (!context->playing) {
					context->startpts = pkt.pts;
				}
			}
		}

		if ((in_stream->codec->codec_type == AVMEDIA_TYPE_VIDEO) && (bsf != NULL)) {
			AVPacket new_pkt = pkt;

			context->last_time = av_rescale_q(pkt.pts + pkt.duration, out_stream->time_base, context->time_base);
#ifdef TIMEDEBUG
			printf("last_time %lld (%llds)\n", context->last_time, context->last_time/VIDEO_TIME_RES);
#endif

			int a = av_bitstream_filter_filter(bsf, out_stream->codec, NULL,
					&new_pkt.data, &new_pkt.size,
					pkt.data, pkt.size,
					pkt.flags & AV_PKT_FLAG_KEY);
			if (a == 0 && new_pkt.data != pkt.data && new_pkt.destruct) {
				uint8_t *t = av_malloc(new_pkt.size + FF_INPUT_BUFFER_PADDING_SIZE); //the new should be a subset of the old so cannot overflow
				if(t) {
					memcpy(t, new_pkt.data, new_pkt.size);
					memset(t + new_pkt.size, 0, FF_INPUT_BUFFER_PADDING_SIZE);
					new_pkt.data = t;
					a = 1;
				} else {
					a = AVERROR(ENOMEM);
				}
			}
			if (a > 0) {
				av_free_packet(&pkt);
				new_pkt.destruct = av_destruct_packet;
			} else if (a < 0) {
				av_log(NULL, AV_LOG_ERROR, "Failed to open bitstream filter %s for stream %d with codec %s",
						bsf->filter->name, pkt.stream_index,
						out_stream->codec->codec ? out_stream->codec->codec->name : "copy");
				goto end;
			}
			pkt = new_pkt;
		}
#ifdef TIMEDEBUG
		log_packet(ofmt_ctx, &pkt, "out", type);
#endif
		context->cur_time = av_rescale_q(pkt.pts, out_stream->time_base, context->time_base);
		if (in_stream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			if ((print_time + 30 * context->time_base.den/context->time_base.num) <= context->cur_time) {
				printf("cur_time %s (%s) startplaypts %s (%s) buffer until %s (%s) started %d\n",
					av_ts2timestr(context->cur_time, &context->time_base),
					av_ts2str(context->cur_time),
					av_ts2timestr(context->startplaypts, &context->time_base),
					av_ts2str(context->startplaypts),
					av_ts2timestr(context->last_time, &context->time_base),
					av_ts2str(context->last_time),
					context->started);
				print_time = context->cur_time;
			}
		}
		if ((context->startplaypts == 0) || (in_stream->codec->codec_type == AVMEDIA_TYPE_VIDEO)) {
			rv = check_jump(context);
			if (RMFAILED(rv)) {
				return rv;
			}
		}
		if (context->started) {
			ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
			if (ret < 0) {
				fprintf(stderr, "Error muxing packet\n");
				break;
			}
		}
		av_free_packet(&pkt);
	}
	av_write_trailer(vidfmt_ctx);
#ifdef PLAY_AUDIO
	av_write_trailer(audfmt_ctx);
#endif
	if (context->started && !context->play_started) {
		/* Wait until enough frames are buffered then start playing. */
		if (context->last_time > 0) {
			RMstatus rv;

			printf("Playing very small video.\n");
			rv = start_play(context);
			if (RMFAILED(rv)) {
				fprintf(stderr, "Cannot start to play video, rv = %d\n", rv);
				cleanup(context);
				return rv;
			}
		}
	}
end:
	if (ifmt_ctx != NULL) {
		avformat_close_input(&ifmt_ctx);
	}

	/* close output */
	if ((vidfmt_ctx != NULL) && (vidfmt_ctx->pb != NULL)) {
		av_free(vidfmt_ctx->pb);
		vidfmt_ctx->pb = NULL;
	}

#ifdef PLAY_AUDIO
	if ((audfmt_ctx != NULL) && (audfmt_ctx->pb != NULL)) {
		av_free(audfmt_ctx->pb);
		audfmt_ctx->pb = NULL;
	}
#endif

	if (bsf != NULL) {
		av_bitstream_filter_close(bsf);
		bsf = NULL;
	}

	if (vidfmt_ctx != NULL) {
		avformat_free_context(vidfmt_ctx);
	}

#ifdef PLAY_AUDIO
	if (audfmt_ctx != NULL) {
		avformat_free_context(audfmt_ctx);
	}
#endif
	if (streamidxmap != NULL) {
		free(streamidxmap);
		streamidxmap = NULL;
	}

	if (ret < 0 && ret != AVERROR_EOF) {
		fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
		return 1;
	}
	return 0;
}

static RMstatus play_video(app_rua_context_t *context, const char *videofile)
{
	RMstatus rv;
	int ret;

	get_key(context, 0);
	context->ending = 0;

	if (context->stopped) {
		printf("Received signal, stopping...\n");
		return RM_OK;
	}

	rv = RUAOpenPool(context->pRUA, 0, 64, DMA_BUFFER_SIZE_LOG2, RUA_POOL_DIRECTION_SEND, &context->pDMA);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot open RUA pool, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}
	rv = DCCSTCSetTimeResolution(context->pStcSource, DCC_Stc, STC_TIME_RES);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set time resolution for stc, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCSTCSetTimeResolution(context->pStcSource, DCC_Video, VIDEO_TIME_RES);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set time resolution for video, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCSTCSetTimeResolution(context->pStcSource, DCC_Audio, AUDIO_TIME_RES);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set time resolution for audio, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCSTCSetVideoOffset(context->pStcSource, 0, 600);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set video offset, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCSTCSetAudioOffset(context->pStcSource, 0, 600);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set audio offset, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCSTCSetTime(context->pStcSource, 0, VIDEO_TIME_RES);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set time, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = set_speed(context);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set play speed, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCPlayVideoSource(context->pVideoSource, DCCVideoPlayFwd);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot play video source, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

#ifdef PLAY_AUDIO
	/* TBD: Use DCCPlayMultipleAudioSource(). */
	rv = DCCPlayAudioSource(context->pAudioSource);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot play audio source, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}
#endif

	ret = play_mp4_video(context, videofile, context->cfg.start_minute, context->cfg.start_second);
	if (ret != 0) {
		fprintf(stderr, "Failed play_mp4_video with %d\n", ret);
	}

	if (context->videobuffer != NULL) {
		rv = RUAReleaseBuffer(context->pDMA, context->videobuffer);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Failed to release buffer, rv = %d\n", rv);
		}
		context->videobuffer = NULL;
	}
#ifdef PLAY_AUDIO
	if (context->audiobuffer != NULL) {
		rv = RUAReleaseBuffer(context->pDMA, context->audiobuffer);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Failed to release buffer, rv = %d\n", rv);
		}
		context->audiobuffer = NULL;
	}
#endif
	context->ending = 1;
	context->cur_time = context->last_time;

	if (context->playing) {
		if ((ret == 0) && !context->stopped) {
			printf("All data buffered, waiting until playing finished at %lld (%llds)\n", context->last_time, context->last_time / VIDEO_TIME_RES);

			do {
				rv = check_jump(context);
				if (RMFAILED(rv)) {
					fprintf(stderr, "Failed jump, rv = %d\n", rv);
					cleanup(context);
					return rv;
				}

				rv = DCCSTCGetTime(context->pStcSource, &context->time, VIDEO_TIME_RES);
				if (RMFAILED(rv)) {
					fprintf(stderr, "Cannot get time, rv = %d\n", rv);
					cleanup(context);
					return rv;
				}
				printf("Current time %llu (%llus) waiting until %lld (%llds)\n", context->time, context->time / VIDEO_TIME_RES, context->last_time, context->last_time / VIDEO_TIME_RES);
				get_key(context, 500000);
				if (context->stopped) {
					break;
				}
			} while((context->last_time > 0) && (((RMuint64) context->last_time) >= context->time));
		}

		if (!context->paused) {
			printf("Stop play\n");
			rv = DCCSTCStop(context->pStcSource);
			if (RMFAILED(rv)) {
				fprintf(stderr, "Cannot stop, rv = %d\n", rv);
				cleanup(context);
				return rv;
			}
		}
		rv = DCCStopVideoSource(context->pVideoSource, DCCStopMode_BlackFrame);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot stop video source, rv = %d\n", rv);
			cleanup(context);
			return rv;
		}
#ifdef PLAY_AUDIO
		rv = DCCStopAudioSource(context->pAudioSource);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot stop audio source, rv = %d\n", rv);
			cleanup(context);
			return rv;
		}
#endif
		context->playing = 0;
	}

	rv = RUAClosePool(context->pDMA);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot close pool, rv = %d\n", rv); 
	}
	context->pDMA = NULL;
	return RM_OK;
}

static void usage(char *argv[])
{
	fprintf(stderr, "%s [OPTION] [video file]\n", argv[0]);
	fprintf(stderr, "\n");
	fprintf(stderr, "-x MAX_BUFFER_TIME    default: %lld\n", MAX_BUFFER_TIME);
	fprintf(stderr, "-m MIN_BUFFER_TIME    default: %lld\n", MIN_BUFFER_TIME);
	fprintf(stderr, "-r REBUFFER_TIME      default: %lld\n", REBUFFER_TIME);
	fprintf(stderr, "-M MINUTE             Minute were to start playing\n");
	fprintf(stderr, "-S SECOND             Second were to start playing\n");
	fprintf(stderr, "-d                    Enable debug output\n");
	fprintf(stderr, "-j SECOND             How much to jump in seconds\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "This program plays mp4 videos on the Zyxel DMA-2500.\n");
	fprintf(stderr, "Time: %lld is 1 second.\n", VIDEO_TIME_RES);
	fprintf(stderr, "\n");
}

int main(int argc, char *argv[])
{
	RMstatus rv;
	const char *videofile;
	app_rua_context_t *context = &context_g;
	int c;
	play_config_t cfg;

	memset(&cfg, 0, sizeof(cfg));
	cfg.max_buffer_time = MAX_BUFFER_TIME;
	cfg.min_buffer_time = MIN_BUFFER_TIME;
	cfg.rebuffer_time = REBUFFER_TIME;
	cfg.start_minute = 0;
	cfg.start_second = 0;
	cfg.jump_time = JUMP_TIME;

	avformat_network_init();

	while((c = getopt (argc, argv, "x:m:r:S:M:dj:")) != -1) {
		switch(c) {
			case 'x':
				cfg.max_buffer_time = strtoull(optarg, NULL, 0);
				break;

			case 'm':
				cfg.min_buffer_time = strtoull(optarg, NULL, 0);
				break;

			case 'r':
				cfg.rebuffer_time = strtoull(optarg, NULL, 0);
				break;

			case 'S':
				cfg.start_second = strtoul(optarg, NULL, 0);
				break;

			case 'M':
				cfg.start_minute = strtoul(optarg, NULL, 0);
				break;

			case 'j':
				cfg.jump_time = strtoul(optarg, NULL, 0);
				break;

			case 'd':
				debug = 1;
				break;

			default:
				usage(argv);
				return 1;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "Error: Parameter missing.\n");

		usage(argv);
		return 1;
	}
	videofile = argv[optind];

	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGSEGV, signalcleanup);
	signal(SIGFPE, signalcleanup);
	signal(SIGILL, signalcleanup);
	signal(SIGABRT, signalcleanup);
	signal(SIGBUS, signalcleanup);
	signal(SIGQUIT, signalhandler);
	signal(SIGINT, signalhandler);
	signal(SIGUSR1, signalhandler);
	signal(SIGUSR2, signalhandler);
	signal(SIGKILL, signalhandler);
	signal(SIGTERM, signalhandler);

	atexit(exitcleanup);

	rv = video_init(context, &cfg);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error failed video_init! %d\n", rv);
		return rv;
	}
	DPRINTF("video_init() success\n");

	context->irfd = RCCOpen();
	if (context->irfd < 0) {
		fprintf(stderr, "Failed to open remote control.");
		cleanup(context);
		return 1;
	}

	rv = configure_video(context);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error failed configure_video! %d\n", rv);
		cleanup(context);
		return rv;
	}
	DPRINTF("configure_video() success\n");

	rv = play_video(context, videofile);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error failed play_video! %d\n", rv);
		cleanup(context);
		return rv;
	}
	DPRINTF("play_video() success\n");

	cleanup(context);
	DPRINTF("Clean up finished\n");

	return 0;
}

