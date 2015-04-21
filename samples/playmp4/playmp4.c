/*
 * Copyright (c) 2015, Juergen Urban
 * All rights reserved.
 *
 * Test playback of MP4 video.
 */

#include <stdio.h>
#include <string.h>

#include "rua.h"
#include "dcc.h"

#define DEFAULT_CHIP 0
#define DEFAULT_DRAM_CONTROLLER 0

typedef struct {
	struct RUA *pRUA;
	struct DCC *pDCC;
	struct DCCSTCSource *pStcSource;
	struct DCCVideoSource *pVideoSource;
} app_rua_context_t;

static void cleanup(app_rua_context_t *context)
{
	RMstatus rv;

	if (context == NULL) {
		return;
	}

	if (context->pVideoSource != NULL) {
		rv = DCCCloseVideoSource(context->pVideoSource);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot close video source %d\n", rv); 
		}
		context->pVideoSource = NULL;
	}

	if (context->pStcSource != NULL) {
		rv = DCCSTCClose(context->pStcSource);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot close STC %d\n", rv); 
		}
		context->pStcSource = NULL;
	}

	if (context->pDCC != NULL) {
		rv = DCCClose(context->pDCC);
		context->pDCC = NULL;
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot close DCC %d\n", rv); 
		}
	}

	if (context->pRUA != NULL){
		rv = RUADestroyInstance(context->pRUA);
		context->pRUA = NULL;
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot destroy RUA instance %d\n", rv);
		}
	}

	fprintf(stderr, "end cleanup\n");
}

static RMstatus video_init(app_rua_context_t *context)
{
	RMstatus rv;

	memset(context, 0, sizeof(*context));

	rv = RUACreateInstance(&context->pRUA, DEFAULT_CHIP);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error creating RUA instance! %d\n", rv);
		return rv;
	}

	rv = DCCOpen(context->pRUA, &context->pDCC);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error Opening DCC! %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCInitMicroCodeEx(context->pDCC, DCCInitMode_LeaveDisplay);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot initialize microcode %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCSetMemoryManager(context->pDCC, DEFAULT_DRAM_CONTROLLER);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot initialize dram %d\n", rv);
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

	memset(&stc_profile, 0, sizeof(stc_profile));
	stc_profile.STCID = 0;
	stc_profile.master = Master_STC;
	stc_profile.stc_timer_id = 3 * stc_profile.STCID + 0;
	stc_profile.stc_time_resolution = 90000;
	stc_profile.video_timer_id = 3 * stc_profile.STCID + 1;
	stc_profile.video_time_resolution = 90000;
	stc_profile.video_offset = -(video_delay_ms * (RMint32)stc_profile.video_time_resolution / 1000);;
	stc_profile.audio_timer_id = 3 * stc_profile.STCID + 2;
	stc_profile.audio_time_resolution = 90000;
	stc_profile.audio_offset = -(audio_delay_ms * (RMint32)stc_profile.audio_time_resolution / 1000);

	rv = DCCSTCOpen(context->pDCC, &stc_profile, &context->pStcSource);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot open STC %d\n", rv);
		cleanup(context);
		return rv;
	}
	memset(&video_profile, 0, sizeof(video_profile));

	video_profile.BitstreamFIFOSize = 1024 * 1024;
	video_profile.XferFIFOCount = 1024;
	video_profile.MpegEngineID = 0;
	video_profile.VideoDecoderID = 0;
	video_profile.ProtectedFlags = 0;
	video_profile.PtsFIFOCount = 180;
	video_profile.InbandFIFOCount = 16;
	video_profile.XtaskInbandFIFOCount = 0;
	video_profile.SPUBitstreamFIFOSize = 0;
	video_profile.SPUXferFIFOCount = 0;
	video_profile.STCID = stc_profile.STCID;
	video_profile.Codec = EMhwlibVideoCodec_MPEG4;
	video_profile.Profile = 0;
	video_profile.Level = 0;
	video_profile.ExtraPictureBufferCount = 0;
	video_profile.MaxWidth = 720;
	video_profile.MaxHeight = 576;

	rv = DCCXOpenVideoDecoderSource(context->pDCC, &video_profile, &context->pVideoSource);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot open video decoder source %d\n", rv);
		cleanup(context);
		return rv;
	}
	return RM_OK;
}

int main(void)
{
	app_rua_context_t context;
	RMstatus rv;

	printf("playmp4\n");

	rv = video_init(&context);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error failed video_init! %d\n", rv);
		return rv;
	}
	printf("video_init() success\n");

	rv = configure_video(&context);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error failed configure_video! %d\n", rv);
		return rv;
	}
	printf("configure_video() success\n");

	/* TBD: Playing not implemented. */

	cleanup(&context);
	printf("Clean up finished\n");

	return 0;
}
