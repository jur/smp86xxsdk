/*
 * Copyright (c) 2015, Juergen Urban
 * All rights reserved.
 *
 * The test play mp4 video from the raw video and audio stream.
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

#include "rua.h"
#include "dcc.h"

/** There is only one chip in the DMA-2500. */
#define DEFAULT_CHIP 0
/** DRAM can be 0 or 1. */
#define DEFAULT_DRAM_CONTROLLER 1
/** Size of buffers used to transfer audio and video data. */
#define DMA_BUFFER_SIZE_LOG2 14
/** Size of buffers used to transfer audio and video data. */
#define DMA_BUFFER_SIZE (1 << DMA_BUFFER_SIZE_LOG2)
/** How many video stream data to buffer until playing should start. */
#define VID_PRE_BUFFER_SIZE 48704

typedef struct {
	struct RUA *pRUA;
	struct DCC *pDCC;
	struct RUABufferPool *pDMA;
	struct DCCSTCSource *pStcSource;
	struct DCCVideoSource *pVideoSource;
	struct DCCAudioSource *pAudioSource;
	RMuint32 SurfaceID;
	RMuint32 video_decoder;
	RMuint32 spu_decoder;
	RMuint32 video_timer;
	RMuint32 audio_decoder; // AudioDecoder
	RMuint32 audio_engine; // AudioEngine
	RMuint32 audio_timer; // 0
} app_rua_context_t;

/** Raw video stream data. */
static uint8_t *videodata;
static size_t videosize;
/** Raw audio stream data. */
static uint8_t *audiodata;
static size_t audiosize;

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

	if (context->pAudioSource != NULL) {
		rv = DCCCloseAudioSource(context->pAudioSource);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot close audio source, rv = %d\n", rv); 
		}
		context->pAudioSource = NULL;
	}

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

	fprintf(stderr, "end cleanup\n");
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
	struct DCCAudioProfile audio_profile;
	struct AudioDecoder_AACParameters_type aac_parameters;

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
	video_profile.Codec = EMhwlibVideoCodec_H264;
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

	audio_profile.BitstreamFIFOSize = 1024 * 1024;
	audio_profile.XferFIFOCount = 1024;
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
			fprintf(stderr, "Cannot get buffer, rv = %d\n", rv);
			if (rv != RM_PENDING) {
				cleanup(context);
			}
			return rv;
		}
		memcpy(*pbuffer, &data[*transferred], size);
	}

	memset(&video_info, 0, sizeof(video_info));
	printf("RUASendData(%p, (%u, %u), %p, %p, %u, %p, %u)\n", context->pRUA, (decoder >> 16) & 0xFF, decoder & 0xFF, context->pDMA, *pbuffer, size, &video_info, sizeof(video_info));
	rv = RUASendData(context->pRUA, decoder, context->pDMA, *pbuffer, size, &video_info, sizeof(video_info));
	printf("RUASendData rv = %d\n", rv);
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
	RMuint32 videotransferred;
	RMuint32 audiotransferred;
	int playing = 0;
	RMuint64 time;
	RMuint8 *videobuffer = NULL;
	RMuint8 *audiobuffer = NULL;
	RMuint32 videonumbuffers;
	RMuint32 audionumbuffers;
	RMuint32 vafactor;
	RMuint32 vvalue;
	RMuint32 avalue;

	rv = RUAOpenPool(context->pRUA, 0, 64, DMA_BUFFER_SIZE_LOG2, RUA_POOL_DIRECTION_SEND, &context->pDMA);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot open RUA pool, rv = %d\n", rv);
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

#if 0 /* TBD: How to calculate the time? */
	rv = DCCSTCSetTime(context->pStcSource, 0, 0);
#else
	rv = DCCSTCSetTime(context->pStcSource, -1, -1000); // TBD Check
#endif
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

	// Called by DCCPlayMultipleAudioSource()
	rv = DCCPlayVideoSource(context->pVideoSource, DCCVideoPlayFwd);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot play video source, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCPlayAudioSource(context->pAudioSource);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot play video source, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	rv = DCCSTCGetTime(context->pStcSource, &time, 90000); // TBD Use this to synchronize?
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot get time, rv = %d\n", rv);
		cleanup(context);
		return rv;
	}

	videotransferred = 0;
	audiotransferred = 0;
	videonumbuffers = videosize / DMA_BUFFER_SIZE;
	audionumbuffers = audiosize / DMA_BUFFER_SIZE;

	/* Calculate how many bytes need to be transferred until the next audiodata can be transferred. */
	vafactor = (((RMuint64) DMA_BUFFER_SIZE) * ((RMuint64) videosize)) / ((RMuint64) audiosize);
	vvalue = 0;
	avalue = 0;
	while (videotransferred < videosize) {
		if (videotransferred < videosize) {
			/* Send video stream data which should be played. */
			rv = transfer_data(context, &videotransferred, videodata, videosize, context->video_decoder, &videobuffer);
			if ((rv != RM_OK) && (rv != RM_PENDING)) {
				return rv;
			}
		}
		if (audiotransferred < audiosize) {
			/* Audio should not use all buffers, so only transfer audio when already enough video data were transferred. */
			if (videotransferred >= vvalue) {
				/* Send audio stream data which should be played. */
				rv = transfer_data(context, &audiotransferred, audiodata, audiosize, context->audio_decoder, &audiobuffer);
				if ((rv != RM_OK) && (rv != RM_PENDING)) {
					return rv;
				}
				if (avalue < audiotransferred) {
					vvalue += vafactor;
					if (vvalue >= videosize) {
						vvalue = videosize;
					}
					avalue += DMA_BUFFER_SIZE;
					if (avalue >= audiosize) {
						avalue = audiosize;
					}
				}
			}
		}
		if (!playing && (videotransferred > VID_PRE_BUFFER_SIZE)) {
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
			/* TBD: Use DCCPlayMultipleAudioSource() instead. */
			rv = DCCPlayAudioSource(context->pAudioSource);
			if (RMFAILED(rv)) {
				fprintf(stderr, "Cannot play video source, rv = %d\n", rv);
				cleanup(context);
				return rv;
			}
			playing = 1;
		}
	}

	if (playing) {
		usleep(30000000); /* TBD: Find a better way to detect if playing of the video finished. */

		printf("Stop play\n");
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
		rv = DCCStopAudioSource(context->pAudioSource);
		if (RMFAILED(rv)) {
			fprintf(stderr, "Cannot stop audio source, rv = %d\n", rv);
			cleanup(context);
			return rv;
		}
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
	fprintf(stderr, "This program plays mp4 videos from raw video and audio stream.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Video file must be in format JVT NAL sequence, H.264 video.\n");
	fprintf(stderr, "A file with the name video.mp4 can be converted by the following command:\n");
	fprintf(stderr, "ffmpeg -i video.mp4 -vbsf h264_mp4toannexb -vcodec copy -an -f rawvideo video.raw\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Audio file must be in format MPEG ADTS, AAC.\n");
	fprintf(stderr, "A file with the name video.mp4 can be converted by the following command:\n");
	fprintf(stderr, "ffmpeg -i video.mp4 -vn -sn -f adts -acodec copy audio.raw\n");
}

int main(int argc, char *argv[])
{
	app_rua_context_t context;
	RMstatus rv;
	int ret;
	const char *videofile;
	const char *audiofile;

	if (argc < 2) {
		fprintf(stderr, "Error: Paremeter missing.\n");

		usage(argv);
		exit(1);
	}
	videofile = argv[1];
	audiofile = argv[2];

	videodata = NULL;
	videosize = 0;
	ret = read_file(videofile, &videodata, &videosize);
	if (ret < 0) {
		fprintf(stderr, "Error failed to read \"%s\".\n", videofile);
		return ret;
	}

	audiodata = NULL;
	audiosize = 0;
	ret = read_file(audiofile, &audiodata, &audiosize);
	if (ret < 0) {
		fprintf(stderr, "Error failed to read \"%s\".\n", audiofile);
		return ret;
	}

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

	rv = play_video(&context);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error failed play_video! %d\n", rv);
		return rv;
	}
	printf("play_video() success\n");

	cleanup(&context);
	printf("Clean up finished\n");

	return 0;
}
