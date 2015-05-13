/*
 * Copyright (c) 2015, Juergen Urban
 * All rights reserved.
 *
 * The test plays a mpeg1 stream.
 *
 * TBD: Example is not working.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include "rua.h"
#include "dcc.h"

/** Define to play audio also. */
#define PLAY_AUDIO

/** There is only one chip in the DMA-2500. */
#define DEFAULT_CHIP 0
/** DRAM can be 0 or 1. */
#define DEFAULT_DRAM_CONTROLLER 1
/** Size of buffers used to transfer audio and video data. */
#define DMA_BUFFER_SIZE_LOG2 16
/** Size of buffers used to transfer audio and video data. */
#define DMA_BUFFER_SIZE (1 << DMA_BUFFER_SIZE_LOG2)
/** How many video stream data to buffer until playing should start. */
#define VID_PRE_BUFFER_SIZE 48704
/** Print debug message. */
#define DPRINTF(args...) \
	do { \
		if (debug) { \
			printf(args); \
		} \
	} while(0)
#define NUMCIPHERS 2

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
	RMuint32 demux_decoder;
	RMuint32 video_timer;
#ifdef PLAY_AUDIO
	RMuint32 audio_decoder; // AudioDecoder
#endif
	RMuint32 audio_engine; // AudioEngine
	RMuint32 audio_timer; // 0
	struct DCCDemuxTask *pDemuxTask;
	RMuint32 ciphers[NUMCIPHERS];
} app_rua_context_t;

/** Set to 1 to enable debug output. */
static int debug = 0;
/** Raw video stream data. */
static uint8_t *videodata;
static size_t videosize;
static volatile int stopped = 0;
static app_rua_context_t context_g;

static void cleanup(app_rua_context_t *context)
{
	RMstatus rv;
	int i;

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

	for (i = 0; i < NUMCIPHERS; i++) {
		rv = RUASetProperty(context->pRUA, context->demux_decoder, RMDemuxTaskPropertyID_FreeCipherEntry, &context->ciphers[i], sizeof(context->ciphers[i]), 0);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot free cipher DemuxTask, rv = %d\n", rv); 
		}
		context->ciphers[i] = 0;
	}

	if (context->pDemuxTask) {
		DCCCloseDemuxTask(context->pDemuxTask);
		if (RMFAILED(rv))  {
			fprintf(stderr, "Cannot close DemuxTask, rv = %d\n", rv); 
		}
		context->pDemuxTask = NULL;
	}

	if (context->pStcSource != NULL) {
		rv = DCCSTCClose(context->pStcSource);
		if (RMFAILED(rv))  {
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

	fprintf(stderr, "end cleanup\n");
}

static void signalhandler(int sig)
{
	(void) sig;

	stopped = 1;
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

static RMstatus video_init(app_rua_context_t *context)
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
	struct AudioDecoder_MpegParameters_type mp2_parameters;
#endif
	struct DCCDemuxTaskProfile demux_profile;
	RMuint32 buffer_param[2];
	RMuint32 buffer[1];
	RMuint32 buffer_pes[7];
	int i;

	memset(&demux_profile, 0, sizeof(demux_profile));
	demux_profile.ProtectedFlags = 0;
	demux_profile.BitstreamFIFOSize = 0;
	demux_profile.XferFIFOCount = 0;
	demux_profile.InbandFIFOCount = 0x00600000;
	demux_profile.InputPort = 0x00000060;
	demux_profile.PrimaryMPM = 0x00000200;
	demux_profile.SecondaryMPM = 0;
	demux_profile.DemuxTaskID = 0;
	demux_profile.reserved20 = 3;
	rv = DCCOpenDemuxTask(context->pDCC, &demux_profile, &context->pDemuxTask);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot open DemuxTask, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCGetDemuxTaskInfo(context->pDemuxTask, &context->demux_decoder);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot open video decoder source, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	memset(&stc_profile, 0, sizeof(stc_profile));
	stc_profile.STCID = 0;
	stc_profile.master = Master_STC;
	stc_profile.stc_timer_id = 3 * stc_profile.STCID + 0;
	stc_profile.stc_time_resolution = 90000;
	stc_profile.video_timer_id = 3 * stc_profile.STCID + 1;
	stc_profile.video_time_resolution = 90000;
	stc_profile.video_offset = -(video_delay_ms * (RMint32)stc_profile.video_time_resolution / 1000);
	stc_profile.audio_timer_id = 3 * stc_profile.STCID + 2;
	stc_profile.audio_time_resolution = 90000;
	stc_profile.audio_offset = -(audio_delay_ms * (RMint32)stc_profile.audio_time_resolution / 1000);

	rv = DCCSTCOpen(context->pDCC, &stc_profile, &context->pStcSource);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot open STC, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}
	memset(&video_profile, 0, sizeof(video_profile));

	buffer_param[0] = 0;
	buffer_param[1] = 3;
	rv = RUASetProperty(context->pRUA, context->demux_decoder, RMDemuxTaskPropertyID_InputParameters, buffer_param, sizeof(buffer_param), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed RMDemuxTaskPropertyID_InputParameters, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	buffer[0] = 0;
	rv = RUASetProperty(context->pRUA, context->demux_decoder, 4531, buffer, sizeof(buffer), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed line %u, rv = %d\n", __LINE__, rv);
		cleanup(context);
		return rv;
	}

	buffer[0] = 3;
	rv = RUASetProperty(context->pRUA, context->demux_decoder, RMDemuxTaskPropertyID_IdleInputPort, buffer, sizeof(buffer), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed line %u, rv = %d\n", __LINE__, rv);
		cleanup(context);
		return rv;
	}

	buffer[0] = 0;
	rv = RUASetProperty(context->pRUA, context->demux_decoder, 4645, buffer, sizeof(buffer), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed line %u, rv = %d\n", __LINE__, rv);
		cleanup(context);
		return rv;
	}

	buffer[0] = 0;
	rv = RUASetProperty(context->pRUA, context->demux_decoder, RMDemuxTaskPropertyID_TSSyncLockCount, buffer, sizeof(buffer), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed line %u, rv = %d\n", __LINE__, rv);
		cleanup(context);
		return rv;
	}

	buffer[0] = 0;
	rv = RUASetProperty(context->pRUA, context->demux_decoder, RMDemuxTaskPropertyID_TsSkipBytes, buffer, sizeof(buffer), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed RMDemuxTaskPropertyID_InputParameters, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	buffer_param[0] = 900000;
	buffer_param[1] = 90000;
	rv = RUASetProperty(context->pRUA, context->demux_decoder, RMDemuxTaskPropertyID_PcrDiscontinuity, buffer_param, sizeof(buffer_param), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed line %u, rv = %d\n", __LINE__, rv);
		cleanup(context);
		return rv;
	}

	buffer[0] = 1;
	rv = RUASetProperty(context->pRUA, context->demux_decoder, RMDemuxTaskPropertyID_TimerSync, buffer, sizeof(buffer), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed line %u, rv = %d\n", __LINE__, rv);
		cleanup(context);
		return rv;
	}

	buffer_param[0] = 0;
	buffer_param[1] = 0;
	rv = RUASetProperty(context->pRUA, context->demux_decoder, RMDemuxTaskPropertyID_FirstPcrOrPtsDetection, buffer_param, sizeof(buffer_param), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed line %u, rv = %d\n", __LINE__, rv);
		cleanup(context);
		return rv;
	}

	for (i = 0; i < NUMCIPHERS; i++) {
		context->ciphers[i] = 0;
		rv = RUAGetProperty(context->pRUA, context->demux_decoder, RMDemuxTaskPropertyID_AllocateCipherEntry, &context->ciphers[i], sizeof(context->ciphers[i]));
		if (RMFAILED(rv)) {
			cleanup(context);
			return rv;
		}
	}

	memset(buffer_pes, 0, sizeof(buffer_pes));
	buffer_pes[0] = 0; /* index */
	buffer_pes[1] = 0x012a00e0; /* TBD: stream id */
	buffer_pes[2] = 0; /* sub stream id */
	buffer_pes[3] = 0x1001; /* input */
	buffer_pes[4] = 1; /* enable */
	buffer_pes[5] = 0; /* cipher mask */
	buffer_pes[6] = 0; /* cipher index */
	rv = RUASetProperty(context->pRUA, context->demux_decoder, RMDemuxTaskPropertyID_PesEntry, buffer_pes, sizeof(buffer_pes), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed RMDemuxTaskPropertyID_PesEntry, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	memset(buffer_pes, 0, sizeof(buffer_pes));
	buffer_pes[0] = 1;
	buffer_pes[1] = 0x012a00c0;
	buffer_pes[2] = 0;
	buffer_pes[3] = 0x1001;
	buffer_pes[4] = 10;
	buffer_pes[5] = 0;
	buffer_pes[6] = 0;
	rv = RUASetProperty(context->pRUA, context->demux_decoder, RMDemuxTaskPropertyID_PesEntry, buffer_pes, sizeof(buffer_pes), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed RMDemuxTaskPropertyID_PesEntry, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	memset(buffer_pes, 0, sizeof(buffer_pes));
	buffer_pes[0] = 2;
	buffer_pes[1] = 0x012a0000;
	buffer_pes[2] = 0;
	buffer_pes[3] = 0x1001;
	buffer_pes[4] = 4;
	buffer_pes[5] = 0;
	buffer_pes[6] = 0;
	rv = RUASetProperty(context->pRUA, context->demux_decoder, RMDemuxTaskPropertyID_PesEntry, buffer_pes, sizeof(buffer_pes), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed RMDemuxTaskPropertyID_PesEntry, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	memset(buffer_pes, 0, sizeof(buffer_pes));
	buffer_pes[0] = 3;
	buffer_pes[1] = 0x012a0000;
	buffer_pes[2] = 0;
	buffer_pes[3] = 0x1000;
	buffer_pes[4] = 10;
	buffer_pes[5] = 0;
	buffer_pes[6] = 0;
	rv = RUASetProperty(context->pRUA, context->demux_decoder, RMDemuxTaskPropertyID_PesEntry, buffer_pes, sizeof(buffer_pes), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed RMDemuxTaskPropertyID_PesEntry, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	video_profile.BitstreamFIFOSize = 4 * 1024 * 1024;
	video_profile.XferFIFOCount = 0;
	video_profile.MpegEngineID = DEFAULT_DRAM_CONTROLLER;
	video_profile.VideoDecoderID = 0;
	video_profile.ProtectedFlags = 0;
	video_profile.PtsFIFOCount = 512;
	video_profile.InbandFIFOCount = 128;
	video_profile.XtaskInbandFIFOCount = 0;
	video_profile.SPUBitstreamFIFOSize = 0;
	video_profile.SPUXferFIFOCount = 0;
	video_profile.STCID = stc_profile.STCID; // 0
	video_profile.Codec = EMhwlibVideoCodec_MPEG2;
	video_profile.Profile = 0;
	video_profile.Level = 0;
	video_profile.ExtraPictureBufferCount = 0;
	video_profile.MaxWidth = 1920;
	video_profile.MaxHeight = 1080;
	video_profile.SPUCodec = EMhwlibDVDSpuCodec;
	video_profile.SPUMaxWidth = 720;
	video_profile.SPUMaxHeight = 576;

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

	buffer[0] = 8;
	rv = RUASetProperty(context->pRUA, DemuxOutput, RMDemuxOutputPropertyID_Enable, buffer, sizeof(buffer), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed RMDemuxOutputPropertyID_Enable, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	buffer[0] = 7;
	rv = RUASetProperty(context->pRUA, DemuxOutput, RMDemuxOutputPropertyID_DataType, buffer, sizeof(buffer), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed line %u, rv = %d\n", __LINE__, rv);
		cleanup(context);
		return rv;
	}

#if 1
	buffer_param[0] = 0; // partial read
	buffer_param[1] = 0; // size
	rv = RUASetProperty(context->pRUA, DemuxOutput, RMGenericPropertyID_Threshold, buffer_param, sizeof(buffer_param), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed line %u, rv = %d\n", __LINE__, rv);
#if 0 // TBD: Fix
		cleanup(context);
		return rv;
#endif
	}
#endif

#ifdef PLAY_AUDIO
	audio_profile.BitstreamFIFOSize = 512 * 1024;
	audio_profile.XferFIFOCount = 0;
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

	mp2_parameters.OutputDualMode = DualMode_Stereo;
	mp2_parameters.Acmod2DualMode = 0;
	mp2_parameters.OutputChannels = Audio_Out_Ch_LR;
	mp2_parameters.OutputLfe = 0;
	mp2_parameters.OutputSurround20 = SurroundAsStream;
	mp2_parameters.OutputSpdif = OutputSpdif_Uncompressed;
	mp2_parameters.BassMode = 0;

	rv = DCCSetAudioMpegFormat(context->pAudioSource, &mp2_parameters);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set audio mp2 format source info, rv = %d\n", rv);
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
	rv = DCCSetRouteDisplayAspectRatio(context->pDCC, DCCRoute_Main, 16, 9);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set display aspect ratio, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCGetVideoDecoderSourceInfo(context->pVideoSource, &context->video_decoder, &context->spu_decoder, &context->video_timer);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot get video decoder source info, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}
	return RM_OK;
}

static RMstatus transfer_data(app_rua_context_t *context, RMuint32 *transferred, RMuint8 *data, RMuint32 datasize, RMuint32 decoder, RMuint8 **pbuffer)
{
	RMuint32 size;
	RMstatus rv;
	struct emhwlib_info video_info;

	if (*transferred >= datasize) {
		return RM_OK;
	}

	size = datasize - *transferred;
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
		memcpy(*pbuffer, &data[*transferred], size);
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
	*transferred += size;
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
	return RM_OK;
}

static RMstatus play_video(app_rua_context_t *context)
{
	RMstatus rv;
	RMuint32 demuxtransferred;
	int playing = 0;
	RMuint64 time;
	RMuint8 *demuxbuffer = NULL;
	RMuint32 buffer[1];
	RMuint32 buffer_param[2];

	if (stopped) {
		printf("Received signal, stopping...\n");
		return RM_OK;
	}

	rv = RUAOpenPool(context->pRUA, context->demux_decoder, 96, DMA_BUFFER_SIZE_LOG2, RUA_POOL_DIRECTION_SEND, &context->pDMA);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot open RUA pool, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	buffer_param[0] = context->demux_decoder;
	buffer_param[1] = context->video_decoder;
	rv = RUASetProperty(context->pRUA, DemuxOutput, RMDemuxOutputPropertyID_Connect, buffer_param, sizeof(buffer_param), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed RMDemuxOutputPropertyID_Connect, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	buffer[0] = 0;
	rv = RUASetProperty(context->pRUA, DemuxOutput, RMDemuxOutputPropertyID_Trigger, buffer, sizeof(buffer), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed line %u, rv = %d\n", __LINE__, rv);
		cleanup(context);
		return rv;
	}

	buffer[0] = 0;
	rv = RUASetProperty(context->pRUA, DemuxOutput, RMDemuxOutputPropertyID_TransportPriority, buffer, sizeof(buffer), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed line %u, rv = %d\n", __LINE__, rv);
		cleanup(context);
		return rv;
	}

	buffer[0] = 1;
	rv = RUASetProperty(context->pRUA, DemuxOutput, RMDemuxOutputPropertyID_Trigger, buffer, sizeof(buffer), 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Failed line %u, rv = %d\n", __LINE__, rv);
		cleanup(context);
		return rv;
	}

	rv = DCCSTCSetTimeResolution(context->pStcSource, DCC_Stc, 24000);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set time resolution for stc, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCSTCSetTimeResolution(context->pStcSource, DCC_Video, 30000);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set time resolution for video, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCSTCSetTimeResolution(context->pStcSource, DCC_Audio, 90000);
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

	rv = DCCSTCSetTime(context->pStcSource, 0, 45000);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set time, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCSTCSetSpeed(context->pStcSource, 1, 1);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot set play speed, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	/* Call DCCPlayMultipleAudioSource() instead. */
	rv = DCCPlayVideoSource(context->pVideoSource, DCCVideoPlayFwd);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot play video source, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

#ifdef PLAY_AUDIO
	rv = DCCPlayAudioSource(context->pAudioSource);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot play audio source, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}
#endif
	rv = DCCPlayDemuxTask(context->pDemuxTask);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot play demux task, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCSTCGetTime(context->pStcSource, &time, 90000); // TBD Use this to synchronize?
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot get time, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	demuxtransferred = 0;

	while (demuxtransferred < videosize) {
		if (stopped) {
			printf("Received signal, stopping...\n");
			break;
		}
		if (demuxtransferred < videosize) {
			/* Send video stream data which should be played. */
			rv = transfer_data(context, &demuxtransferred, videodata, videosize, context->demux_decoder, &demuxbuffer);
			if ((rv != RM_OK) && (rv != RM_PENDING)) {
				return rv;
			}
		}
		if (stopped) {
			printf("Received signal, stopping...\n");
			break;
		}
		if (!playing && (demuxtransferred > VID_PRE_BUFFER_SIZE)) {
			printf("Start play\n");

			rv = DCCSTCPlay(context->pStcSource);
			if (RMFAILED(rv)) {
				fprintf(stderr, "Cannot set play mode, rv = %d\n", rv);
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
			/* TBD: Use DCCPlayMultipleAudioSource() instead. */
			rv = DCCPlayAudioSource(context->pAudioSource);
			if (RMFAILED(rv)) {
				fprintf(stderr, "Cannot play video source, rv = %d\n", rv);
				cleanup(context);
				return rv;
			}
#endif
			playing = 1;
		}
	}

	if (demuxbuffer != NULL) {
		rv = RUAReleaseBuffer(context->pDMA, demuxbuffer);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Failed to release buffer, rv = %d\n", rv);
		}
		demuxbuffer = NULL;
	}

	if (playing) {
		if (!stopped) {
			sleep(3); /* TBD: Find a better way to detect if playing of the video finished. */
		}

		printf("Stop play\n");

		rv = DCCStopDemuxTask(context->pDemuxTask);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot stop demux task, rv = %d\n", rv);
			cleanup(context);
			return rv;
		}
		rv = DCCSTCStop(context->pStcSource);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot stop, rv = %d\n", rv);
			cleanup(context);
			return rv;
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
		playing = 0;
	}

	rv = RUAClosePool(context->pDMA);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot close pool, rv = %d\n", rv); 
	}
	context->pDMA = NULL;
	return RM_OK;
}

static int read_file(const char *filename, uint8_t **data, size_t *size)
{
	int fd;
	off_t offset;
	int rv = -1;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Error: Failed to open \"%s\".\n", filename);
		return -1;
	}
	offset = lseek(fd, 0, SEEK_END);
	if (offset != -1) {
		void *ptr;

		ptr = mmap(NULL, offset, PROT_READ, MAP_SHARED, fd, 0);
		if (ptr == MAP_FAILED) {
			rv = -1;
		} else {
			rv = 0;
			*data = ptr;
			*size = offset;
		}
	}
	close(fd);
	return rv;
}

static void usage(char *argv[])
{
	fprintf(stderr, "%s [video file] [audio file]\n", argv[0]);
	fprintf(stderr, "\n");
	fprintf(stderr, "This program plays mpeg1 videos.\n");
	fprintf(stderr, "\n");
}

int main(int argc, char *argv[])
{
	RMstatus rv;
	int ret;
	const char *videofile;
	app_rua_context_t *context = &context_g;

	if (argc < 2) {
		fprintf(stderr, "Error: Parameter missing.\n");

		usage(argv);
		exit(1);
	}
	videofile = argv[1];

	videodata = NULL;
	videosize = 0;
	ret = read_file(videofile, &videodata, &videosize);
	if (ret < 0) {
		fprintf(stderr, "Error failed to read \"%s\".\n", videofile);
		return ret;
	}

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

	rv = video_init(context);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error failed video_init! %d\n", rv);
		return rv;
	}
	DPRINTF("video_init() success\n");

	rv = configure_video(context);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error failed configure_video! %d\n", rv);
		return rv;
	}
	DPRINTF("configure_video() success\n");

	rv = play_video(context);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error failed play_video! %d\n", rv);
		return rv;
	}
	DPRINTF("play_video() success\n");

	cleanup(context);
	DPRINTF("Clean up finished\n");

	return 0;
}
