/*
 * Copyright (c) Juergen Urban, All rights reserved.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3.0 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 * 
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with this library.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "rua.h"
#include "dcc.h"

/** Print debug message. */
#if 0
#define DPRINTF(args...) printf(args)
#else
#define DPRINTF(args...) do { } while(0)
#endif

/** Print error message. */
#define EPRINTF(format, args...) fprintf(stderr, "librua: " __FILE__ ":%d: Error: " format, __LINE__, ## args)

#define USER_DATA_SIZE 0x1000

typedef RMuint32 dcc_malloc_t(struct RUA *pRua, RMuint32 ModuleID, RMuint32 dramIndex, enum RUADramType dramtype, RMuint32 size);
typedef void dcc_free_t(struct RUA *pRua, RMuint32 addr);

struct DCC {
	struct RUA *pRua; // 0x00
	RMuint32 video_ucode_address;
	RMuint32 audio_ucode_address;
	RMuint32 demux_ucode_address;
	RMuint32 dram; // 0x10
	dcc_malloc_t *rua_malloc; // 0x14
	dcc_free_t *rua_free; // 0x18
};

typedef struct {
	RMuint32 PictureAddr;
	RMuint32 LumaAddress;
	RMuint32 LumaSize;
	RMuint32 ChromaAddress;
	RMuint32 ChromaSize;
	RMuint32 PaletteAddress;
	RMuint32 PaletteSize;
} pic_info_t;

struct SPUDecoderSource {                                                       
};

struct DCCVideoSource {
	struct DCC *pDCC; // 0x00
	struct RUA *pRua; // 0x04
	RMuint32 decodermoduleid; // 0x08
	RMuint32 enginemoduleid; // 0x0c
	RMuint32 spudecodermoduleid; // 0x10
	RMuint32 scalermoduleid; // 0x14
	RMuint32 mixermoduleid;
	RMuint32 picture_count;
	pic_info_t *pic_info;
	RMuint32 bitprot; // 0x24
	RMuint32 unprot; // 0x2c
	RMuint32 picprot; // 0x44
	RMuint32 STCID; // 0x54
	RMuint32 surface; // 0x58
	struct SPUDecoderSource *spu_decoder; // 0x6c
};

struct DCCSTCSource {
	struct RUA *pRua;
	RMuint32 StcModuleId;
};

struct DCCResource {
	RMuint32 schedmem;
	RMuint32 schedmemsize;
	RMuint32 decodershmem;
	RMuint32 decodershmemsize;
	RMuint32 picprot;
	RMuint32 picprotsize;
	RMuint32 bitprot;
	RMuint32 bitprotsize;
	RMuint32 unprot;
	RMuint32 unprotsize;
	RMuint32 reserveddata;
	RMuint32 reservedsize;
};

struct DCCAudioSource {
	struct RUA *pRua; // 0x00
	struct DCC *pDCC; // 0x04
	RMuint32 decodermoduleid; // 0x08
	RMuint32 enginemoduleid; // 0x0c
	RMuint32 STCID; // 0x18
	RMuint32 reserved1C; // 0x1c
	RMuint32 mem1; // 0x20
	RMuint32 mem2; // 0x2c
};

static RMuint32 default_rua_malloc(struct RUA *pRua, RMuint32 ModuleID, RMuint32 dramIndex, enum RUADramType dramtype, RMuint32 size)
{
	(void) ModuleID;

	return RUAMalloc(pRua, dramIndex, dramtype, size);
}

static void default_rua_free(struct RUA *pRua, RMuint32 addr)
{
	RUAFree(pRua, addr);
}

static RMstatus send_video_command(struct RUA *pRua, RMuint32 decodermoduleid, RMuint32 cmd)
{
	RMstatus rv;
	struct RUAEvent event;
	RMuint32 index;
	RMuint32 state;
	RMuint32 status;

	event.ModuleID = decodermoduleid;
	event.Mask = 1;
	rv = RUAResetEvent(pRua, &event);
	if (rv != RM_OK) {
		return rv;
	}

	rv = RUASetProperty(pRua, decodermoduleid, RMVideoDecoderPropertyID_Command, &cmd, sizeof(cmd), 0);
	if (rv != RM_OK) {
		return rv;
	}

	rv = RUAWaitForMultipleEvents(pRua, &event, 1, 1000000, &index);
	if (rv != RM_OK) {
		return rv;
	}

	rv = RUAGetProperty(pRua, decodermoduleid, RMVideoDecoderPropertyID_State, &state, sizeof(state));
	if (rv != RM_OK) {
		return rv;
	}

	rv = RUAGetProperty(pRua, decodermoduleid, RMVideoDecoderPropertyID_CommandStatus, &status, sizeof(status));
	if (rv != RM_OK) {
		return rv;
	}
	return RM_OK;
}

static RMstatus send_audio_command(struct RUA *pRua, RMuint32 decodermoduleid, RMuint32 cmd)
{
	RMstatus rv;
	struct RUAEvent event;
	RMuint32 index;
	RMuint32 state;

	event.ModuleID = decodermoduleid;
	event.Mask = 1;
	rv = RUAResetEvent(pRua, &event);
	if (rv != RM_OK) {
		return rv;
	}

	rv = RUASetProperty(pRua, decodermoduleid, RMAudioDecoderPropertyID_Command, &cmd, sizeof(cmd), 0);
	if (rv != RM_OK) {
		return rv;
	}

	rv = RUAWaitForMultipleEvents(pRua, &event, 1, 1000000, &index);
	if (rv != RM_OK) {
		return rv;
	}

	rv = RUAGetProperty(pRua, decodermoduleid, RMAudioDecoderPropertyID_State, &state, sizeof(state));
	if (rv != RM_OK) {
		return rv;
	}
	return RM_OK;
}

RMstatus DCCOpen(struct RUA *pRua, struct DCC **ppDCC)
{
	struct DCC *pDCC = NULL;

	if (pRua == NULL) {
		return RM_INVALID_PARAMETER;
	}

	pDCC = malloc(sizeof(*pDCC));
	if (pDCC == NULL) {
		return RM_FATALOUTOFMEMORY;
	}
	memset(pDCC, 0, sizeof(*pDCC));
	pDCC->pRua = pRua;
	pDCC->dram = 0;
	pDCC->rua_malloc = default_rua_malloc;
	pDCC->rua_free = default_rua_free;
	*ppDCC = pDCC;

	return RM_OK;
}

RMstatus DCCClose(struct DCC *pDCC)
{
	if (pDCC == NULL) {
		return RM_INVALID_PARAMETER;
	}
	free(pDCC);
	pDCC = NULL;

	return RM_OK;
}

RMstatus DCCInitMicroCodeEx(struct DCC *pDCC, enum DCCInitMode init_mode)
{
	RMbool enabled;
	int rv;
	struct RUA *pRua;

	if (pDCC == NULL) {
		return RM_INVALID_PARAMETER;
	}
	if (pDCC->pRua == NULL) {
		return RM_INVALIDMODE;
	}
	pRua = pDCC->pRua;

	do {
		rv = RUASetProperty(pRua, EMHWLIB_MODULE(DemuxEngine, 0), RMDemuxEnginePropertyID_TimerInit, NULL, 0, 0);
		if (rv != RM_OK) {
			fprintf(stderr, "Error: Failed to initialize timer for DemuxEngine in %s.\n", __FUNCTION__);

			return rv;
		}
	} while (rv == RM_PENDING);

	do {
		rv = RUASetProperty(pRua, EMHWLIB_MODULE(MpegEngine, 0), RMMpegEnginePropertyID_InitMicrocodeSymbols, NULL, 0, 0);
		if (rv != RM_OK) {
			fprintf(stderr, "Error: Failed to initialize micro code for MpegEngine 0 in %s.\n", __FUNCTION__);

			return rv;
		}
	} while (rv == RM_PENDING);

	do {
		rv = RUASetProperty(pRua, EMHWLIB_MODULE(MpegEngine, 1), RMMpegEnginePropertyID_InitMicrocodeSymbols, NULL, 0, 0);
		if (rv != RM_OK) {
			fprintf(stderr, "Error: Failed to initialize micro code for MpegEngine 1 in %s.\n", __FUNCTION__);

			return rv;
		}
	} while (rv == RM_PENDING);

	if (init_mode != DCCInitMode_LeaveDisplay) {
		fprintf(stderr, "Error: DCCInitMode_LeaveDisplay not implemented.\n");
		return RM_NOTIMPLEMENTED;
	}

	rv = RUAGetProperty(pRua, EMHWLIB_MODULE(DispMainVideoScaler, 0), RMGenericPropertyID_Enable, &enabled, sizeof(enabled));
	if (rv == RM_OK) {
		if (enabled) {
			return RM_OK;
		} else {
			fprintf(stderr, "DispMainVideoScaler - RMGenericPropertyID_Enable %d\n", enabled);
			fprintf(stderr, "Error: Scaler is not enabled! ENabling not implemented.\n");
			return RM_NOTIMPLEMENTED;
		}
	} else {
		fprintf(stderr, "Error: Failed setup in RUAGetProperty.\n");

		return rv;
	}
	return RM_OK;
}

static void get_event_mask(RMuint32 ModuleID, struct RUAEvent *evt)
{
	evt->ModuleID = EMHWLIB_MODULE(DisplayBlock, 0);

	switch(ModuleID & 0xFF) {
		case DisplayBlock:
			evt->Mask = 0;
			break;

		case DispOSDScaler:
			evt->Mask = 0x100;
			break;

		case DispHardwareCursor:
			evt->Mask = 0x200;
			break;

		case DispMainVideoScaler:
			evt->Mask = 0x400;
			break;

		case DispSubPictureScaler:
			evt->Mask = 0x800;
			break;

		case DispVCRMultiScaler:
			evt->Mask = 0x1000;
			break;

		case DispGFXMultiScaler:
			evt->Mask = 0x4000;
			break;

		case DispMainMixer:
			evt->Mask = 0x0040;
			break;

		case DispColorBars:
			evt->Mask = 0x0010;
			break;

		case DispRouting:
			evt->Mask = 0x0020;
			break;

		case DispVideoInput:
			evt->Mask = 0x8000;
			break;

		case DispGraphicInput:
			evt->Mask = 0x00010000;
			break;

		case DispDigitalOut:
			evt->Mask = 0x0001;
			break;

		case DispMainAnalogOut:
			evt->Mask = 0x0002;
			break;

		case DispComponentOut:
			evt->Mask = 0x0004;
			break;

		case DispCompositeOut:
			evt->Mask = 0x0008;
			break;

		case CPUBlock:
			evt->Mask = 0;
			break;

		case DemuxEngine:
			evt->Mask = 0;
			break;

		case MpegEngine:
			evt->Mask = 0;
			break;

		case VideoDecoder:
			evt->Mask = 0;
			break;

		case AudioEngine:
			evt->Mask = 0;
			break;

		case AudioDecoder:
			evt->Mask = 0;
			break;

		case CRCDecoder:
			evt->Mask = 0;
			break;

		case XCRCDecoder:
			evt->Mask = 0;
			break;

		case I2C:
			evt->Mask = 0;
			break;

		case GFXEngine:
			evt->Mask = 0;
			break;

		case MM:
			evt->Mask = 0;
			break;

		case SpuDecoder:
			evt->Mask = 0;
			break;

		case ClosedCaptionDecoder:
			evt->Mask = 0;
			break;

		case StreamCapture:
			evt->Mask = 0;
			break;

		case STC:
			evt->Mask = 0;
			break;

		case DispVideoPlane:
			evt->Mask = 0x2000;
			break;

		case DispHDSDConverter:
			evt->Mask = 0x0080;
			break;

		default:
			fprintf(stderr, "Error: ModuleID %d not supported in %s.\n", ModuleID & 0xFF, __FUNCTION__);
			evt->Mask = 0;
			break;
	}
}

static RMstatus set_property(struct RUA *pRua, RMuint32 ModuleID, RMuint32 PropertyID, void *pValue, RMuint32 ValueSize)
{
	RMstatus rv;
	int n = 0;

	if (pRua == NULL) {
		return RM_INVALID_PARAMETER;
	}

	do {
		struct RUAEvent evt;

		get_event_mask(ModuleID, &evt);
		if (evt.Mask != 0) {
			rv = RUAResetEvent(pRua, &evt);
			if (rv != RM_OK) {
				fprintf(stderr, "reset_event failed for ModuleID %d\n", ModuleID & 0xFF);
				return rv;
			}
		}

		rv = RUASetProperty(pRua, ModuleID, PropertyID, pValue,  ValueSize, 0);
		if ((rv == RM_PENDING) && (evt.Mask != 0)) {
			RMuint32 index;

			rv = RUAWaitForMultipleEvents(pRua, &evt, 1, 1000000, &index);
			if (rv != RM_OK) {
				if (rv != RM_PENDING) {
					return rv;
				}
			}
			rv = RUASetProperty(pRua, ModuleID, PropertyID, pValue,  ValueSize, 0);
		}
		n++;
	} while ((n < 5) && (rv == RM_PENDING));

	return rv;
}

RMstatus DCCSetSurfaceSource(struct DCC *pDCC, RMuint32 surfaceID, struct DCCVideoSource *pVideoSource)
{
	RMuint32 surface;

	if (pDCC == NULL) {
		return RM_INVALID_PARAMETER;
	}
	if (pDCC->pRua == NULL) {
		return RM_INVALIDMODE;
	}

	if (pVideoSource == NULL) {
		surface = 0;

		return set_property(pDCC->pRua, surfaceID, RMGenericPropertyID_Surface, &surface, sizeof(surface));
	} else {
		RMstatus rv;
		RMbool enable;

		pVideoSource->scalermoduleid = surfaceID;

		switch (surfaceID) {
			case DispMainVideoScaler:
			case DispSubPictureScaler:
			case DispOSDScaler:
				pVideoSource->mixermoduleid = EMHWLIB_MODULE(DispMainMixer, 0);
				break;
			default:
				pVideoSource->mixermoduleid = 0;
				fprintf(stderr, "Error: %s is not implemented for surfaceID %d.\n", __FUNCTION__, surfaceID);
				return RM_NOTIMPLEMENTED;
		}
	
		surface = pVideoSource->surface;
		rv = set_property(pDCC->pRua, surfaceID, RMGenericPropertyID_Surface, &surface, sizeof(surface));
		if (rv != RM_OK) {
			return rv;
		}

		enable = FALSE;
		rv = set_property(pDCC->pRua, surfaceID, RMGenericPropertyID_PersistentSurface, &enable, sizeof(enable));
		if (rv != RM_OK) {
			return rv;
		}
		rv = set_property(pDCC->pRua, surfaceID, RMGenericPropertyID_Validate, NULL, 0);
		if (rv != RM_OK) {
			return rv;
		}
		return RM_OK;
	}
}

RMstatus DCCOpenMultiplePictureOSDVideoSource(struct DCC *pDCC, struct DCCOSDProfile *profile, RMuint32 picture_count, struct DCCVideoSource **ppVideoSource, struct DCCSTCSource *pStcSource)
{
	struct DCCVideoSource *pVideoSource;
	RMstatus rv;
	RMuint32 surface_size;
	RMuint32 addr;
	RMuint32 i;

	if (pDCC == NULL) {
		return RM_INVALID_PARAMETER;
	}
	if (pDCC->pRua == NULL) {
		return RM_INVALIDMODE;
	}

	pVideoSource = malloc(sizeof(*pVideoSource));
	if (pVideoSource == NULL) {
		fprintf(stderr, "Error: out of memory\n");

		return RM_FATALOUTOFMEMORY;
	}
	memset(pVideoSource, 0, sizeof(*pVideoSource));
	*ppVideoSource = pVideoSource;
	pVideoSource->pRua = pDCC->pRua;
	pVideoSource->pDCC = pDCC;

	pVideoSource->pic_info = malloc(sizeof(*pVideoSource->pic_info) * picture_count);
	if (pVideoSource->pic_info == NULL) {
		free(pVideoSource);

		fprintf(stderr, "Error: out of memory\n");

		return RM_FATALOUTOFMEMORY;
	}
	memset(pVideoSource->pic_info, 0, sizeof(*pVideoSource->pic_info) * picture_count);

	rv = RUAExchangeProperty(pDCC->pRua, EMHWLIB_MODULE(DisplayBlock, 0), RMDisplayBlockPropertyID_MultiplePictureSurfaceSize, &picture_count, sizeof(picture_count), &surface_size, sizeof(surface_size));
	if (rv != RM_OK) {
		fprintf(stderr, "Error: Failed to set multiple picture surface.\n");
		return rv;
	}
	RMuint32 pic_in[5];
	RMuint32 pic_out[4];

	memset(pic_in, 0, sizeof(pic_in));
	memset(pic_out, 0, sizeof(pic_out));

	pic_in[0] = profile->ColorMode;
	pic_in[1] = profile->ColorFormat;
	pic_in[2] = profile->SamplingMode;
	pic_in[3] = profile->Width;
	pic_in[4] = profile->Height;

	rv = RUAExchangeProperty(pDCC->pRua, EMHWLIB_MODULE(DisplayBlock, 0), RMDisplayBlockPropertyID_PictureSize, &pic_in, sizeof(pic_in), &pic_out, sizeof(pic_out));
	if (rv != RM_OK) {
		fprintf(stderr, "Error: Failed to set picture format.\n");
		return rv;
	}
	
	addr = pDCC->rua_malloc(pDCC->pRua, DisplayBlock, pDCC->dram, RUA_DRAM_UNPROTECTED, pic_out[0] * picture_count + surface_size);
	if (addr == 0) {
		fprintf(stderr, "Error: Failed to allocate memory.\n");
		return RM_FATALOUTOFMEMORY;
	}
	pVideoSource->surface = addr;
	pVideoSource->picture_count = picture_count;

	RMuint32 surface_cfg[9];

	surface_cfg[0] = profile->ColorMode;
	surface_cfg[1] = profile->ColorFormat;
	surface_cfg[2] = profile->SamplingMode;
	surface_cfg[3] = pVideoSource->surface;
	surface_cfg[4] = profile->ColorSpace;
	surface_cfg[5] = profile->PixelAspectRatio.X;
	surface_cfg[6] = profile->PixelAspectRatio.Y;
	surface_cfg[7] = picture_count;
	surface_cfg[8] = 0;

	if (pStcSource != NULL) {
		DCCSTCGetModuleId(pStcSource, &surface_cfg[8]);
	}

	rv = RUASetProperty(pDCC->pRua, EMHWLIB_MODULE(DisplayBlock, 0), RMDisplayBlockPropertyID_InitMultiplePictureSurface, &surface_cfg, sizeof(surface_cfg), 0);
	if (rv != RM_OK) {
		RUAFree(pDCC->pRua, pVideoSource->surface);
		pVideoSource->surface = 0;
		fprintf(stderr, "Error: Failed to set surface.\n");
		return rv;
	}

	addr += surface_size;

	RMuint32 frameIn[6];

	memset(frameIn, 0, sizeof(frameIn));
	frameIn[0] = profile->Width;
	frameIn[1] = profile->Height;
	frameIn[3] = pic_out[1]; // LumaSize
	frameIn[4] = pic_out[2]; // ChromaSize
	frameIn[5] = pic_out[3]; // PaletteSize

	for (i = 0; i < picture_count; i++) {
		RMuint32 frameOut[3];
		pic_info_t *pic = &pVideoSource->pic_info[i];

		frameIn[2] = addr;

		memset(frameOut, 0, sizeof(frameOut));
		rv = RUAExchangeProperty(pDCC->pRua, EMHWLIB_MODULE(DisplayBlock, 0), RMDisplayBlockPropertyID_InitPictureX, frameIn, sizeof(frameIn), frameOut, sizeof(frameOut));

		pic->PictureAddr = addr;
		pic->LumaAddress = frameOut[0];
		pic->LumaSize = pic_out[1];
		pic->ChromaAddress = frameOut[1];
		pic->ChromaSize = pic_out[2];
		pic->PaletteAddress = frameOut[2];
		pic->PaletteSize = pic_out[3];
		DPRINTF("LumaAddress 0x%08x\n", frameOut[0]);
		DPRINTF("ChromaAddress 0x%08x\n", frameOut[1]);
		DPRINTF("PaletteAddress 0x%08x\n", frameOut[2]);

		addr += pic_out[0]; // buffer size
	}
	RMuint32 enable[2];
	enable[0] = pVideoSource->surface;
	enable[1] = TRUE;

	rv = RUASetProperty(pDCC->pRua, EMHWLIB_MODULE(DisplayBlock, 0), RMDisplayBlockPropertyID_EnableGFXInteraction, enable, sizeof(enable), 0);
	if (rv != RM_OK) {
		fprintf(stderr, "Error: Failed to enable surface.\n");
		RUAFree(pDCC->pRua, pVideoSource->surface);
		pVideoSource->surface = 0;
		return rv;
	}

	return RM_OK;
}

RMstatus DCCOpenOSDVideoSource(struct DCC *pDCC, struct DCCOSDProfile *profile, struct DCCVideoSource **ppVideoSource)
{
	struct DCCVideoSource *pVideoSource;
	RMstatus rv;
	RMuint32 pic_in[5];
	RMuint32 pic_out[3];
	RMuint32 buffer_init[11];
	RMuint32 result_init[3];
	RMuint32 addr;
	pic_info_t *pic;

	pVideoSource = malloc(sizeof(*pVideoSource));
	if (pVideoSource == NULL) {
		fprintf(stderr, "Error: out of memory\n");

		return RM_FATALOUTOFMEMORY;
	}
	memset(pVideoSource, 0, sizeof(*pVideoSource));
	pVideoSource->pRua = pDCC->pRua;
	pVideoSource->pDCC = pDCC;
	*ppVideoSource = pVideoSource;

	memset(pic_in, 0, sizeof(pic_in));
	memset(pic_out, 0, sizeof(pic_out));

	pic_in[0] = profile->ColorMode;
	pic_in[1] = profile->ColorFormat;
	pic_in[2] = profile->SamplingMode;
	pic_in[3] = profile->Width;
	pic_in[4] = profile->Height;

	rv = RUAExchangeProperty(pDCC->pRua, EMHWLIB_MODULE(DisplayBlock, 0), RMDisplayBlockPropertyID_SurfaceSize, &pic_in, sizeof(pic_in), &pic_out, sizeof(pic_out));
	if (rv != RM_OK) {
		fprintf(stderr, "Error: Failed to get surface size.\n");
		return rv;
	}
	addr = pDCC->rua_malloc(pDCC->pRua, DisplayBlock, pDCC->dram, RUA_DRAM_UNPROTECTED, pic_out[0]);
	if (addr == 0) {
		fprintf(stderr, "Error: Failed to allocate memory.\n");
		return RM_FATALOUTOFMEMORY;
	}
	pVideoSource->surface = addr;
	pVideoSource->picture_count = 1;

	memset(buffer_init, 0, sizeof(buffer_init));
	memset(result_init, 0, sizeof(result_init));
	buffer_init[0] = profile->ColorMode;
	buffer_init[1] = profile->ColorFormat;
	buffer_init[2] = profile->SamplingMode;
	buffer_init[3] = profile->Width;
	buffer_init[4] = profile->Height;
	buffer_init[5] = addr;
	buffer_init[6] = pic_out[1];
	buffer_init[7] = pic_out[2];
	buffer_init[8] = profile->ColorSpace;
	buffer_init[9] = profile->PixelAspectRatio.X;
	buffer_init[10] = profile->PixelAspectRatio.Y;
	rv = RUAExchangeProperty(pDCC->pRua, EMHWLIB_MODULE(DisplayBlock, 0), RMDisplayBlockPropertyID_InitSurface, &buffer_init, sizeof(buffer_init), &result_init, sizeof(result_init));
	if (rv != RM_OK) {
		fprintf(stderr, "Error: Failed to get nitialize surface.\n");
		return rv;
	}

	pVideoSource->pic_info = malloc(sizeof(*pVideoSource->pic_info) * pVideoSource->picture_count);
	if (pVideoSource->pic_info == NULL) {
		free(pVideoSource);

		fprintf(stderr, "Error: out of memory\n");

		return RM_FATALOUTOFMEMORY;
	}
	memset(pVideoSource->pic_info, 0, sizeof(*pVideoSource->pic_info) * pVideoSource->picture_count);

	pic = pVideoSource->pic_info;
	pic->PictureAddr = addr;
	pic->LumaAddress = result_init[0];
	pic->LumaSize = pic_out[1];
	pic->ChromaAddress = result_init[1];
	pic->ChromaSize = pic_out[2];
	pic->PaletteAddress = 0;
	pic->PaletteSize = 0;

	return RM_OK;
}

RMstatus DCCGetOSDSurfaceInfo(struct DCC *pDCC, struct DCCVideoSource *pVideoSource, struct DCCOSDProfile *profile, RMuint32 *SurfaceAddr, RMuint32 *SurfaceSize)
{
	if (pDCC == NULL) {
		return RM_INVALID_PARAMETER;
	}
	if (pDCC->pRua == NULL) {
		return RM_INVALIDMODE;
	}

	if (profile != NULL) {
		RMstatus rv;
		RMuint32 pic_in[5];
		RMuint32 pic_out[3];

		printf("DCCGetOSDSurfaceInfo untested code\n");

		memset(pic_in, 0, sizeof(pic_in));
		memset(pic_out, 0, sizeof(pic_out));

		pic_in[0] = profile->ColorMode;
		pic_in[1] = profile->ColorFormat;
		pic_in[2] = profile->SamplingMode;
		pic_in[3] = profile->Width;
		pic_in[4] = profile->Height;

		rv = RUAExchangeProperty(pDCC->pRua, EMHWLIB_MODULE(DisplayBlock, 0), RMDisplayBlockPropertyID_SurfaceSize, &pic_in, sizeof(pic_in), &pic_out, sizeof(pic_out));
		if (rv != RM_OK) {
			fprintf(stderr, "Error: Failed to get surface size.\n");
			return rv;
		}
		*SurfaceSize = pic_out[0];
	}
	*SurfaceAddr = pVideoSource->surface;
	return RM_OK;
}

RMstatus DCCGetOSDPictureInfo(struct DCCVideoSource *pVideoSource, RMuint32 index, RMuint32 *PictureAddr,  RMuint32 *LumaAddr, RMuint32 *LumaSize, RMuint32 *ChromaAddr, RMuint32 *ChromaSize)
{
	if (pVideoSource == NULL) {
		return RM_INVALID_PARAMETER;
	}
	if (pVideoSource->pRua == NULL) {
		return RM_INVALIDMODE;
	}
	if (index >= pVideoSource->picture_count) {
		return RM_ERROR;
	}

	if (PictureAddr != NULL) {
		*PictureAddr = pVideoSource->pic_info[index].PictureAddr;
	}
	if (LumaAddr != NULL) {
		*LumaAddr = pVideoSource->pic_info[index].LumaAddress;
	}
	if (LumaSize != NULL) {
		*LumaSize = pVideoSource->pic_info[index].LumaSize;
	}
	if (ChromaAddr != NULL) {
		*ChromaAddr = pVideoSource->pic_info[index].ChromaAddress;
	}
	if (ChromaSize != NULL) {
		*ChromaSize = pVideoSource->pic_info[index].ChromaSize;
	}
	return RM_OK;
}

RMstatus DCCGetOSDVideoSourceInfo(struct DCCVideoSource *pVideoSource, RMuint32 *LumaAddr, RMuint32 *LumaSize, RMuint32 *ChromaAddr, RMuint32 *ChromaSize)
{
	return DCCGetOSDPictureInfo(pVideoSource, 0 , NULL, LumaAddr, LumaSize, ChromaAddr, ChromaSize);
}

RMstatus DCCGetScalerModuleID(struct DCC *pDCC, enum DCCRoute route, enum DCCSurface surface, RMuint32 index, RMuint32 *scalermoduleid)
{
	if (pDCC == NULL) {
		return RM_INVALID_PARAMETER;
	}
	if (pDCC->pRua == NULL) {
		return RM_INVALIDMODE;
	}

	switch (route) {
		case DCCRoute_Main:
			switch(surface) {
				case DCCSurface_OSD:
					if (index == 0) {
						*scalermoduleid = EMHWLIB_MODULE(DispOSDScaler, 0);
						return RM_OK;
					} else {
						fprintf(stderr, "Error: %s is not implemented.\n", __FUNCTION__);
						return RM_NOTIMPLEMENTED;
					}
					break;

				case DCCSurface_Video:
					if (index == 0) {
						*scalermoduleid = EMHWLIB_MODULE(DispMainVideoScaler, 0);
						return RM_OK;
					} else {
						fprintf(stderr, "Error: %s is not implemented.\n", __FUNCTION__);
						return RM_NOTIMPLEMENTED;
					}
					break;
				default:
					fprintf(stderr, "Error: %s is not implemented.\n", __FUNCTION__);
					return RM_NOTIMPLEMENTED;
			}
			break;
		default:
			fprintf(stderr, "Error: %s is not implemented.\n", __FUNCTION__);
			return RM_NOTIMPLEMENTED;
	}
}

static RMstatus set_memory(struct RUA *pRua, RMuint32 addr, RMuint32 size, RMuint8 *user, RMbool clear, RMint32 initvalue)
{
	while (size > 0) {
		RMstatus rv;
		RMuint32 s;
		RMuint8 *p;

		s = size;
		if (s > 0x100000) {
			s = 0x100000;
		}

		rv = RUALock(pRua, addr, s);
		if (rv != RM_OK) {
			return rv;
		}
		p = RUAMap(pRua, addr, s);
		if (p == NULL) {
			fprintf(stderr, "RUAMap failed for 0x%08x size %d\n", addr, s);
			return RM_ERROR;
		}
		if ((!clear) && (user != NULL)) {
			memcpy(p, user, s);
		} else {
			memset(p, initvalue, s);
		}
		RUAUnMap(pRua, p, s);
		rv = RUAUnLock(pRua, addr, s);
		if (rv != RM_OK) {
			fprintf(stderr, "RUAUnMap failed for 0x%08x size %d\n", addr, s);
			return rv;
		}
		addr += s;
		size -= s;
		if (user != NULL) {
			user += s;
		}

	}
	return RM_OK;
}

RMstatus DCCClearOSDPicture(struct DCCVideoSource *pVideoSource, RMuint32 index)
{
	RMstatus rv;
	RMuint32 LumaAddr;
	RMuint32 LumaSize;
	RMuint32 ChromaAddr;
	RMuint32 ChromaSize;

	if (pVideoSource == NULL) {
		return RM_INVALID_PARAMETER;
	}
	if (pVideoSource->pRua == NULL) {
		return RM_INVALIDMODE;
	}

	rv = DCCGetOSDPictureInfo(pVideoSource, index, NULL,  &LumaAddr, &LumaSize, &ChromaAddr, &ChromaSize);
	if (rv != RM_OK) {
		return rv;
	}
	rv = set_memory(pVideoSource->pRua, ChromaAddr, ChromaSize, NULL, TRUE, 0);
	if (rv != RM_OK) {
		return rv;
	}
	if ((LumaAddr != 0) && (LumaSize != 0)) {
		rv = set_memory(pVideoSource->pRua, LumaAddr, LumaSize, NULL, TRUE, 128);
		if (rv != RM_OK) {
			return rv;
		}
	}

	return RM_OK;
}

RMstatus DCCClearOSDVideoSource(struct DCCVideoSource *pVideoSource)
{
	return DCCClearOSDPicture(pVideoSource, 0);
}

RMstatus DCCInsertPictureInMultiplePictureOSDVideoSource(struct DCCVideoSource *pVideoSource, RMuint32 index, RMuint64 Pts)
{
	RMuint32 buffer[4];
	RMstatus rv;

	if (pVideoSource == NULL) {
		return RM_INVALID_PARAMETER;
	}
	if (pVideoSource->pRua == NULL) {
		return RM_INVALIDMODE;
	}

	memset(buffer, 0, sizeof(buffer));
	buffer[0] = pVideoSource->surface;
	buffer[1] = pVideoSource->pic_info[index].PictureAddr;
	*((RMuint64 *) &buffer[2]) = Pts;

	rv = RUASetProperty(pVideoSource->pRua, EMHWLIB_MODULE(DisplayBlock, 0), RMDisplayBlockPropertyID_InsertPictureInSurfaceFifo, buffer, sizeof(buffer), 0);

	return rv;
}

RMstatus DCCEnableVideoSource(struct DCCVideoSource *pVideoSource, RMbool enable)
{
	RMstatus rv;
	RMuint32 idx;
	RMuint32 state;
	RMuint32 mixer;

	if (pVideoSource == NULL) {
		return RM_INVALID_PARAMETER;
	}
	if (pVideoSource->pRua == NULL) {
		return RM_INVALIDMODE;
	}

	if (pVideoSource->mixermoduleid == 0) {
		fprintf(stderr, "Error: %s no mixermoduleid defined.\n", __FUNCTION__);
		return RM_ERROR;
	}
	if (pVideoSource->scalermoduleid == 0) {
		fprintf(stderr, "Error: %s no scalermoduleid defined.\n", __FUNCTION__);
		return RM_ERROR;
	}

	rv = RUAExchangeProperty(pVideoSource->pRua, pVideoSource->mixermoduleid, RMGenericPropertyID_MixerSourceIndex, &pVideoSource->scalermoduleid, sizeof(pVideoSource->scalermoduleid), &idx, sizeof(idx));
	if (rv != RM_OK) {
		fprintf(stderr, "Error: %s getting index failed.\n", __FUNCTION__);
		return rv;
	}
	if (enable) {
		state = 2;
	} else {
		state = 1;
	}
	mixer = EMHWLIB_TARGET_MODULE(pVideoSource->mixermoduleid, 0, idx);
	rv = set_property(pVideoSource->pRua, mixer, RMGenericPropertyID_MixerSourceState, &state, sizeof(state));
	if (rv != RM_OK) {
		return rv;
	}
	rv = set_property(pVideoSource->pRua, mixer, RMGenericPropertyID_Validate, NULL, 0);
	if (rv != RM_OK) {
		return rv;
	}
	return RM_OK;
}

RMstatus DCCSetMemoryManager(struct DCC *pDCC, RMuint8 dram)
{
	RMstatus rv;
	struct RUA *pRua;
	RMuint32 buffer[1];
	RMuint32 result[1];
	
	if (pDCC == NULL) {
		return RM_INVALID_PARAMETER;
	}
	if (pDCC->pRua == NULL) {
		return RM_INVALIDMODE;
	}
	pRua = pDCC->pRua;

	memset(buffer, 0, sizeof(buffer));
	memset(result, 0, sizeof(result));
	buffer[0] = MM;
	rv = RUAExchangeProperty(pDCC->pRua, EMHWLIB_MODULE(Enumerator, 0), RMEnumeratorPropertyID_CategoryIDToNumberOfInstances, buffer, sizeof(buffer), &result, sizeof(result));
	if (rv != RM_OK) {
		return RM_ERROR;
	}
	if (dram >= result[0]) {
		return RM_ERROR;
	}
	pDCC->dram = dram;
	return RM_OK;

}

RMstatus DCCSTCOpen(struct DCC *pDCC, struct DCCStcProfile *stc_profile, struct DCCSTCSource **ppStcSource)
{
	RMstatus rv;
	RMuint32 buffer[9];
	struct DCCSTCSource *pStcSource;
	
	if (pDCC == NULL) {
		return RM_INVALID_PARAMETER;
	}
	if (pDCC->pRua == NULL) {
		return RM_INVALIDMODE;
	}

	pStcSource = malloc(sizeof(*pStcSource));

	pStcSource->pRua = pDCC->pRua;
	pStcSource->StcModuleId = EMHWLIB_MODULE(STC, stc_profile->STCID);

	memset(buffer, 0, sizeof(buffer));
	buffer[0] = stc_profile->master;
	buffer[1] = stc_profile->stc_timer_id;
	buffer[2] = stc_profile->stc_time_resolution;
	buffer[3] = stc_profile->video_timer_id;
	buffer[4] = stc_profile->video_time_resolution;
	buffer[5] = stc_profile->video_offset;
	buffer[6] = stc_profile->audio_timer_id;
	buffer[7] = stc_profile->audio_time_resolution;
	buffer[8] = stc_profile->audio_offset;

	rv = RUASetProperty(pDCC->pRua, pStcSource->StcModuleId, RMSTCPropertyID_Open, buffer, sizeof(buffer), 0);
	if (rv != RM_OK) {
		free(pStcSource);
		pStcSource = NULL;
		*ppStcSource = NULL;
		return rv;
	}
	*ppStcSource = pStcSource;
	return RM_OK;
}

RMstatus DCCSTCClose(struct DCCSTCSource *pStcSource)
{
	RMstatus rv;
	RMuint32 buffer[9];
	
	if (pStcSource == NULL) {
		return RM_INVALID_PARAMETER;
	}
	if (pStcSource->pRua == NULL) {
		return RM_INVALIDMODE;
	}

	memset(buffer, 0, sizeof(buffer));
	buffer[0] = 0;

	rv = RUASetProperty(pStcSource->pRua, pStcSource->StcModuleId, RMSTCPropertyID_Close, buffer, sizeof(buffer), 0);

	free(pStcSource);
	pStcSource = NULL;

	return rv;
}

RMstatus DCCSTCGetModuleId(struct DCCSTCSource *pStcSource, RMuint32 *stc_id)
{
	*stc_id = pStcSource->StcModuleId;

	return RM_OK;
}

RMstatus DCCSTCSetTimeResolution(struct DCCSTCSource *pStcSource, enum DCCStreamType type, RMuint32 time_resolution)
{
	RMstatus rv = RM_ERROR;

	switch(type) {
		case DCC_Stc:
			rv = RUASetProperty(pStcSource->pRua, pStcSource->StcModuleId, RMSTCPropertyID_StcTimeResolution, &time_resolution, sizeof(time_resolution), 0);
			break;

		case DCC_Video:
			rv = RUASetProperty(pStcSource->pRua, pStcSource->StcModuleId, RMSTCPropertyID_VideoTimeResolution, &time_resolution, sizeof(time_resolution), 0);
			break;

		case DCC_Audio:
			rv = RUASetProperty(pStcSource->pRua, pStcSource->StcModuleId, RMSTCPropertyID_AudioTimeResolution, &time_resolution, sizeof(time_resolution), 0);
			break;
	}
	return rv;
}

RMstatus DCCSTCSetVideoOffset(struct DCCSTCSource *pStcSource, RMint32 time, RMuint32 time_resolution)
{
	RMstatus rv;
	RMuint32 buffer[2];

	buffer[0] = time_resolution;
	buffer[1] = time;

	rv = RUASetProperty(pStcSource->pRua, pStcSource->StcModuleId, RMSTCPropertyID_VideoOffset, &buffer, sizeof(buffer), 0);

	return rv;
}

RMstatus DCCSTCSetAudioOffset(struct DCCSTCSource *pStcSource, RMint32 time, RMuint32 time_resolution)
{
	RMstatus rv;
	RMuint32 buffer[2];

	buffer[0] = time_resolution;
	buffer[1] = time;

	rv = RUASetProperty(pStcSource->pRua, pStcSource->StcModuleId, RMSTCPropertyID_AudioOffset, &buffer, sizeof(buffer), 0);

	return rv;
}

RMstatus DCCSTCSetTime(struct DCCSTCSource *pStcSource, RMuint64 time, RMuint32 time_resolution)
{
	RMstatus rv;
	RMuint32 buffer[4];

	if (pStcSource == NULL) {
		return RM_FATALINVALIDPOINTER;
	}
	if (time_resolution == 0) {
		return RM_ERROR;
	}

	memset(buffer, 0, sizeof(buffer));
	buffer[0] = time_resolution;
	*((RMuint64 *) &buffer[2]) = time;

	rv = RUASetProperty(pStcSource->pRua, pStcSource->StcModuleId, RMSTCPropertyID_Time, &buffer, sizeof(buffer), 0);

	return rv;
}

RMstatus DCCSTCSetSpeed(struct DCCSTCSource *pStcSource, RMint32 numerator, RMuint32 denominator)
{
	RMstatus rv;
	RMuint32 buffer[6];

	memset(buffer, 0, sizeof(buffer));
	buffer[0] = numerator;
	buffer[1] = denominator;
	rv = RUASetProperty(pStcSource->pRua, pStcSource->StcModuleId, RMSTCPropertyID_Speed, &buffer, sizeof(buffer), 0);

	return rv;
}

RMstatus DCCSTCGetTime(struct DCCSTCSource *pStcSource, RMuint64 *ptime, RMuint32 time_resolution)
{
	RMstatus rv;

	if (pStcSource == NULL) {
		return RM_FATALINVALIDPOINTER;
	}

	rv = RUAExchangeProperty(pStcSource->pRua, pStcSource->StcModuleId, RMSTCPropertyID_TimeInfo, &time_resolution, sizeof(time_resolution), ptime, sizeof(*ptime));

	return rv;
}

RMstatus DCCSTCPlay(struct DCCSTCSource *pStcSource)
{
	RMstatus rv;

	if (pStcSource == NULL) {
		return RM_FATALINVALIDPOINTER;
	}

	rv = RUASetProperty(pStcSource->pRua, pStcSource->StcModuleId, RMSTCPropertyID_Play, NULL, 0, 0);

	return rv;
}

RMstatus DCCSTCStop(struct DCCSTCSource *pStcSource)
{
	RMstatus rv;

	if (pStcSource == NULL) {
		return RM_FATALINVALIDPOINTER;
	}

	rv = RUASetProperty(pStcSource->pRua, pStcSource->StcModuleId, RMSTCPropertyID_Stop, NULL, 0, 0);

	return rv;
}

RMstatus DCCGetVideoModuleIDsFromIndexes(struct DCC *pDCC, RMuint32 MpegEngineID, RMuint32 VideoDecoderID, RMuint32 *MpegModuleID, RMuint32 *DecoderModuleID)
{
	RMuint32 buffer[1];
	RMuint32 number_of_engines;
	RMuint32 number_of_decoders;
	RMstatus rv;

	number_of_engines = 0;
	buffer[0] = MpegEngine;
	rv = RUAExchangeProperty(pDCC->pRua, EMHWLIB_MODULE(Enumerator, 0), RMEnumeratorPropertyID_CategoryIDToNumberOfInstances, buffer, sizeof(buffer), &number_of_engines, sizeof(number_of_engines));
	if (rv != RM_OK) {
		return rv;
	}
	if (MpegEngineID >= number_of_engines) {
		EPRINTF("MpegEngineID %u is larger or equal to %u.\n", MpegEngineID, number_of_engines);
		return RM_PARAMETER_OUT_OF_RANGE;
	}

	buffer[0] = VideoDecoder;
	rv = RUAExchangeProperty(pDCC->pRua, EMHWLIB_MODULE(Enumerator, 0), RMEnumeratorPropertyID_CategoryIDToNumberOfInstances, buffer, sizeof(buffer), &number_of_decoders, sizeof(number_of_decoders));
	if (rv != RM_OK) {
		return rv;
	}
	if (VideoDecoderID >= (number_of_decoders / number_of_engines)) {
		EPRINTF("VideoDecoderID %u is larger or equal to %u.\n", VideoDecoderID, (number_of_decoders / number_of_engines));
		return RM_PARAMETER_OUT_OF_RANGE;
	}
	if (MpegModuleID != NULL) {
		*MpegModuleID = EMHWLIB_MODULE(MpegEngine, MpegEngineID);
	}
	if (DecoderModuleID != NULL) {
		*DecoderModuleID = EMHWLIB_MODULE(VideoDecoder, (MpegEngineID * (number_of_decoders / number_of_engines)) + VideoDecoderID);
	}
	return RM_OK;
}

RMstatus DCCGetVideoSourceRequired(struct DCC *pDCC, struct DCCXVideoProfile *dcc_profile, struct DCCResource *resource)
{
	RMstatus rv;
	RMuint32 MpegModuleID;
	RMuint32 DecoderModuleID;
	RMuint32 result_shm[2];
	RMuint32 buffer_mem[6];
	RMuint32 result_mem[3];
	RMuint32 result_decmem[2];
	RMuint32 buffer_dramx[8];
	RMuint32 result_dramx[3];

	memset(resource, 0, sizeof(*resource));

	rv = DCCGetVideoModuleIDsFromIndexes(pDCC, dcc_profile->MpegEngineID, dcc_profile->VideoDecoderID, &MpegModuleID, &DecoderModuleID);
	if (rv != RM_OK) {
		return rv;
	}
	rv = RUAGetProperty(pDCC->pRua, MpegModuleID, RMMpegEnginePropertyID_SchedulerSharedMemory, &result_shm, sizeof(result_shm));
	if (rv != RM_OK) {
		return rv;
	}
	if (result_shm[0] == 0) {
		resource->schedmemsize = result_shm[1];
	} else {
		resource->schedmemsize = 0;
	}

	buffer_mem[0] = dcc_profile->Codec;
	buffer_mem[1] = dcc_profile->Profile;
	buffer_mem[2] = dcc_profile->Level;
	buffer_mem[3] = dcc_profile->ExtraPictureBufferCount;
	buffer_mem[4] = dcc_profile->MaxWidth;
	buffer_mem[5] = dcc_profile->MaxHeight;
	memset(&result_mem, 0, sizeof(result_mem));
	rv = RUAExchangeProperty(pDCC->pRua, DecoderModuleID, RMVideoDecoderPropertyID_DecoderDataMemory, buffer_mem, sizeof(buffer_mem), &result_mem, sizeof(result_mem));
	if (rv != RM_OK) {
		return rv;
	}

	rv = RUAGetProperty(pDCC->pRua, MpegModuleID, RMMpegEnginePropertyID_DecoderSharedMemory, &result_decmem, sizeof(result_decmem));
	if (rv != RM_OK) {
		return rv;
	}

	if (result_mem[0] == 0) {
		resource->decodershmemsize = result_mem[2];
	} else {
		if (result_mem[1] < result_mem[2]) {
			return RM_ERROR;
		}
	}

	memset(&result_dramx, 0, sizeof(result_dramx));
	buffer_dramx[0] = dcc_profile->ProtectedFlags;
	buffer_dramx[1] = dcc_profile->BitstreamFIFOSize;
	buffer_dramx[2] = USER_DATA_SIZE;
	buffer_dramx[3] = result_mem[0];
	buffer_dramx[4] = dcc_profile->XferFIFOCount;
	buffer_dramx[5] = dcc_profile->PtsFIFOCount;
	buffer_dramx[6] = dcc_profile->InbandFIFOCount;
	buffer_dramx[7] = dcc_profile->XtaskInbandFIFOCount;
	rv = RUAExchangeProperty(pDCC->pRua, DecoderModuleID, RMVideoDecoderPropertyID_DRAMSizeX, buffer_dramx, sizeof(buffer_dramx), &result_dramx, sizeof(result_dramx));
	if (rv != RM_OK) {
		return rv;
	}
	resource->picprotsize = result_dramx[0];
	resource->bitprotsize = result_dramx[1];
	resource->unprotsize = result_dramx[2];

	if (dcc_profile->reserved1 != 0) {
		return RM_NOT_SUPPORTED;
	}
	resource->reservedsize = 0;

	return RM_OK;
}

RMstatus DCCXOpenVideoDecoderSourceWithResources(struct DCC *pDCC, struct DCCXVideoProfile *dcc_profile, struct DCCResource *resource, struct DCCVideoSource **ppVideoSource)
{
	RMstatus rv;
	RMuint32 MpegModuleID;
	RMuint32 DecoderModuleID;
	struct DCCVideoSource *pVideoSource;
	RMuint32 buffer_mem[6];
	RMuint32 result_mem[3];
	RMuint32 buffer_open[18];
	RMuint32 buffer_surface[1];

	pVideoSource = malloc(sizeof(*pVideoSource));
	if (pVideoSource == NULL) {
		fprintf(stderr, "Error: out of memory\n");

		return RM_FATALOUTOFMEMORY;
	}
	memset(pVideoSource, 0, sizeof(*pVideoSource));
	pVideoSource->pRua = pDCC->pRua;
	pVideoSource->pDCC = pDCC;
	*ppVideoSource = pVideoSource;

	rv = DCCGetVideoModuleIDsFromIndexes(pDCC, dcc_profile->MpegEngineID, dcc_profile->VideoDecoderID, &MpegModuleID, &DecoderModuleID);
	if (rv != RM_OK) {
		return rv;
	}
	pVideoSource->enginemoduleid = MpegModuleID;
	pVideoSource->decodermoduleid = DecoderModuleID;

	pVideoSource->STCID = dcc_profile->STCID;

	if (resource->schedmemsize != 0) {
		RMuint32 buffer[2];

		buffer[0] = resource->schedmem;
		buffer[1] = resource->schedmemsize;
		rv = set_property(pDCC->pRua, MpegModuleID, RMMpegEnginePropertyID_SchedulerSharedMemory, &buffer, sizeof(buffer));
		if (rv != RM_OK) {
			return rv;
		}
	}

	if (resource->decodershmemsize != 0) {
		RMuint32 buffer[2];

		buffer[0] = resource->decodershmem;
		buffer[1] = resource->decodershmemsize;
		rv = set_property(pDCC->pRua, MpegModuleID, RMMpegEnginePropertyID_DecoderSharedMemory, &buffer, sizeof(buffer));
		if (rv != RM_OK) {
			return rv;
		}
	}

	pVideoSource->picprot = resource->picprot;
	pVideoSource->bitprot = resource->bitprot;
	pVideoSource->unprot = resource->unprot;

	buffer_mem[0] = dcc_profile->Codec;
	buffer_mem[1] = dcc_profile->Profile;
	buffer_mem[2] = dcc_profile->Level;
	buffer_mem[3] = dcc_profile->ExtraPictureBufferCount;
	buffer_mem[4] = dcc_profile->MaxWidth;
	buffer_mem[5] = dcc_profile->MaxHeight;
	memset(&result_mem, 0, sizeof(result_mem));
	rv = RUAExchangeProperty(pDCC->pRua, DecoderModuleID, RMVideoDecoderPropertyID_DecoderDataMemory, buffer_mem, sizeof(buffer_mem), &result_mem, sizeof(result_mem));
	if (rv != RM_OK) {
		return rv;
	}

	memset(buffer_open, 0, sizeof(*buffer_open));
	buffer_open[0] = dcc_profile->ProtectedFlags;
	buffer_open[1] = dcc_profile->BitstreamFIFOSize;
	buffer_open[2] = USER_DATA_SIZE;
	buffer_open[3] = result_mem[0]; /* decoder data size. */
	buffer_open[4] = result_mem[1]; /* decoder context size. */
	buffer_open[5] = dcc_profile->ExtraPictureBufferCount;
	buffer_open[6] = dcc_profile->XferFIFOCount;
	buffer_open[7] = dcc_profile->PtsFIFOCount;
	buffer_open[8] = dcc_profile->InbandFIFOCount;
	buffer_open[9] = dcc_profile->XtaskInbandFIFOCount;
	buffer_open[10] = dcc_profile->STCID;
	buffer_open[11] = dcc_profile->XtaskID;
	buffer_open[12] = resource->picprot;
	buffer_open[13] = resource->picprotsize;
	buffer_open[14] = resource->bitprot;
	buffer_open[15] = resource->bitprotsize;
	buffer_open[16] = resource->unprot;
	buffer_open[17] = resource->unprotsize;
	rv = set_property(pDCC->pRua, DecoderModuleID, RMVideoDecoderPropertyID_OpenX, &buffer_open, sizeof(buffer_open));
	if (rv != RM_OK) {
		return rv;
	}

	rv = RUAGetProperty(pDCC->pRua, DecoderModuleID, RMGenericPropertyID_Surface, &buffer_surface, sizeof(buffer_surface));
	if (rv != RM_OK) {
		return rv;
	}
	pVideoSource->surface = buffer_surface[0];
	return rv;
}

RMstatus DCCXOpenVideoDecoderSource(struct DCC *pDCC, struct DCCXVideoProfile *dcc_profile, struct DCCVideoSource **ppVideoSource)
{
	RMstatus rv;
	struct DCCResource resource;

	if (pDCC == NULL) {
		return RM_INVALID_PARAMETER;
	}
	if (pDCC->pRua == NULL) {
		return RM_INVALIDMODE;
	}

	if (pDCC->dram != dcc_profile->MpegEngineID) {
		return RM_ERROR;
	}

	rv = DCCGetVideoSourceRequired(pDCC, dcc_profile, &resource);
	if (rv != RM_OK) {
		return rv;
	}
	if (resource.schedmemsize != 0) {
		resource.schedmem = pDCC->rua_malloc(pDCC->pRua, 0, pDCC->dram, RUA_DRAM_UNPROTECTED, resource.schedmemsize);
		if (resource.schedmem == 0) {
			return RM_FATALOUTOFMEMORY;
		}
	}
	if (resource.decodershmemsize != 0) {
		resource.decodershmem = pDCC->rua_malloc(pDCC->pRua, 0, pDCC->dram, RUA_DRAM_UNPROTECTED, resource.decodershmemsize);
		if (resource.decodershmem == 0) {
			return RM_FATALOUTOFMEMORY;
		}
	}
	if (resource.picprotsize != 0) {
		resource.picprot = pDCC->rua_malloc(pDCC->pRua, 0, pDCC->dram, RUA_DRAM_UNPROTECTED, resource.picprotsize);
		if (resource.picprot == 0) {
			return RM_FATALOUTOFMEMORY;
		}
	}
	if (resource.bitprotsize != 0) {
		resource.bitprot = pDCC->rua_malloc(pDCC->pRua, 0, pDCC->dram, RUA_DRAM_UNPROTECTED, resource.bitprotsize);
		if (resource.bitprot == 0) {
			return RM_FATALOUTOFMEMORY;
		}
	}
	if (resource.unprotsize != 0) {
		resource.unprot = pDCC->rua_malloc(pDCC->pRua, 0, pDCC->dram, RUA_DRAM_UNPROTECTED, resource.unprotsize);
		if (resource.unprot == 0) {
			return RM_FATALOUTOFMEMORY;
		}
	}
	if (resource.reservedsize != 0) {
		resource.reserveddata = pDCC->rua_malloc(pDCC->pRua, 0, pDCC->dram, RUA_DRAM_UNPROTECTED, resource.reservedsize);
		if (resource.reserveddata == 0) {
			return RM_FATALOUTOFMEMORY;
		}
	}

	rv = DCCXOpenVideoDecoderSourceWithResources(pDCC, dcc_profile, &resource, ppVideoSource);
	if (rv != RM_OK) {
		return rv;
	}

	if (dcc_profile->SPUBitstreamFIFOSize != 0) {
		EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

		/* TBD: Implement DCCOpenSPUDecoderSource. */

		return RM_NOTIMPLEMENTED;
	}
	return RM_OK;
}

RMstatus DCCCloseVideoSource(struct DCCVideoSource *pVideoSource)
{
	RMstatus rv;

	if (pVideoSource->spu_decoder != NULL) {
		EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

		/* TBD: Implement SPUDisconnectSPUFromScaler(). */
		return RM_NOTIMPLEMENTED;
	} else if (pVideoSource->spudecodermoduleid != 0) {
		EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

		return RM_NOTIMPLEMENTED;
	}
	if (pVideoSource->scalermoduleid != 0) {
		RMuint32 surface;
		rv = RUAGetProperty(pVideoSource->pRua, pVideoSource->scalermoduleid, RMGenericPropertyID_Surface, &surface, sizeof(surface));
		if (rv != RM_OK) {
			return rv;
		}
		if (surface != 0) {
			surface = 0;
			rv = set_property(pVideoSource->pRua, pVideoSource->scalermoduleid, RMGenericPropertyID_Surface, &surface, sizeof(surface));
			if (rv != RM_OK) {
				return rv;
			}
		}
	}

	// TBD: Implement SPU stuff.

	if (pVideoSource->decodermoduleid != 0) {
		RMuint32 buffer[1];
		memset(buffer, 0, sizeof(buffer));

		switch(pVideoSource->decodermoduleid & 0xFF) {
			case DispVideoInput:
				rv = set_property(pVideoSource->pRua, pVideoSource->decodermoduleid, RMDispVideoInputPropertyID_Close, &buffer, sizeof(buffer));
				break;

			case DispGraphicInput:
				rv = set_property(pVideoSource->pRua, pVideoSource->decodermoduleid, RMDispGraphicInputPropertyID_Close, &buffer, sizeof(buffer));
				break;

			default:
				rv = set_property(pVideoSource->pRua, pVideoSource->decodermoduleid, RMVideoDecoderPropertyID_Close, &buffer, sizeof(buffer));
				break;
		}
		if (rv != RM_OK) {
			return rv;
		}
	}

	if (pVideoSource->picprot != 0) {
		pVideoSource->pDCC->rua_free(pVideoSource->pRua, pVideoSource->picprot);
		pVideoSource->picprot = 0;
	}
	if (pVideoSource->bitprot != 0) {
		pVideoSource->pDCC->rua_free(pVideoSource->pRua, pVideoSource->bitprot);
		pVideoSource->bitprot = 0;
	}
	if (pVideoSource->unprot != 0) {
		pVideoSource->pDCC->rua_free(pVideoSource->pRua, pVideoSource->unprot);
		pVideoSource->unprot = 0;
	}
	if (pVideoSource->enginemoduleid != 0) {
		RMuint32 count;

		rv = RUAGetProperty(pVideoSource->pRua, pVideoSource->enginemoduleid, RMMpegEnginePropertyID_ConnectedTaskCount, &count, sizeof(count));
		if (rv != RM_OK) {
			return rv;
		}
		if (count == 0) {
			RMuint32 result_decmem[2];

			rv = RUAGetProperty(pVideoSource->pRua, pVideoSource->enginemoduleid, RMMpegEnginePropertyID_DecoderSharedMemory, &result_decmem, sizeof(result_decmem));
			if (rv != RM_OK) {
				return rv;
			}
			if (result_decmem[0] != 0) {
				RMuint32 address;

				address = result_decmem[0];
				result_decmem[0] = 0;
				result_decmem[1] = 0;

				rv = set_property(pVideoSource->pRua, pVideoSource->enginemoduleid, RMMpegEnginePropertyID_DecoderSharedMemory, &result_decmem, sizeof(result_decmem));
				if (rv != RM_OK) {
					return rv;
				}

				RUAFree(pVideoSource->pRua, address);
			}

			rv = RUAGetProperty(pVideoSource->pRua, pVideoSource->enginemoduleid, RMMpegEnginePropertyID_SchedulerSharedMemory, &result_decmem, sizeof(result_decmem));
			if (rv != RM_OK) {
				return rv;
			}
			if (result_decmem[0] != 0) {
				RMuint32 address;

				address = result_decmem[0];
				result_decmem[0] = 0;
				result_decmem[1] = 0;

				rv = set_property(pVideoSource->pRua, pVideoSource->enginemoduleid, RMMpegEnginePropertyID_SchedulerSharedMemory, &result_decmem, sizeof(result_decmem));
				if (rv != RM_OK) {
					return rv;
				}

				rv = RUASetAddressID(pVideoSource->pRua, address, 0);
				if (rv != RM_OK) {
					return rv;
				}

				RUAFree(pVideoSource->pRua, address);
			}
		}
	}
	free(pVideoSource);
	pVideoSource = NULL;

	return RM_OK;
}

RMstatus DCCXSetVideoDecoderSourceCodec(struct DCCVideoSource *pVideoSource, enum EMhwlibVideoCodec Codec)
{
	RMstatus rv;

	rv = send_video_command(pVideoSource->pRua, pVideoSource->decodermoduleid, 0);
	if (rv != RM_OK) {
		return rv;
	}
	rv = set_property(pVideoSource->pRua, pVideoSource->decodermoduleid, RMVideoDecoderPropertyID_CodecX, &Codec, sizeof(Codec));
	if (rv != RM_OK) {
		return rv;
	}
	rv = send_video_command(pVideoSource->pRua, pVideoSource->decodermoduleid, 1);
	if (rv != RM_OK) {
		return rv;
	}
	if (pVideoSource->spu_decoder != NULL) {
		EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

		/* TBD: Implement DCCResetSPUDecoderSource(). */

		return RM_NOTIMPLEMENTED;
	} else if (pVideoSource->spudecodermoduleid != 0) {
		rv = send_video_command(pVideoSource->pRua, pVideoSource->spudecodermoduleid, 0);
		if (rv != RM_OK) {
			return rv;
		}
		Codec = EMhwlibDVDSpuCodec;
		rv = set_property(pVideoSource->pRua, pVideoSource->spudecodermoduleid, RMSpuDecoderPropertyID_CodecX, &Codec, sizeof(Codec));
		if (rv != RM_OK) {
			return rv;
		}
	}
	return RM_OK;
}

RMstatus DCCGetVideoDecoderSourceInfo(struct DCCVideoSource *pVideoSource, RMuint32 *video_decoder, RMuint32 *spu_decoder, RMuint32 *timer)
{
	if (video_decoder != NULL) {
		*video_decoder = pVideoSource->decodermoduleid;
	}
	if (timer != NULL) {
		*timer = pVideoSource->STCID;
	}
	if (spu_decoder != NULL) {
		*spu_decoder = pVideoSource->spudecodermoduleid;
	}
	return RM_OK;
}

RMstatus DCCPlayVideoSource(struct DCCVideoSource *pVideoSource, enum DCCVideoPlayCommand cmd)
{
	RMstatus rv;

	if (cmd == DCCVideoPlayNextFrame) {
		if (pVideoSource->scalermoduleid != 0) {
			rv = set_property(pVideoSource->pRua, pVideoSource->scalermoduleid, RMGenericPropertyID_Step, NULL, 0);
			if (rv != RM_OK) {
				return rv;
			}
		}
	} else {
		rv = send_video_command(pVideoSource->pRua, pVideoSource->decodermoduleid, cmd);
		if (rv != RM_OK) {
			return rv;
		}
	}
	if (pVideoSource->spu_decoder != NULL) {
		EPRINTF("Function %s is not implemented.\n", __FUNCTION__);
		/* TBD: Implement DCCPlaySPUSource(). */

		return RM_NOTIMPLEMENTED;
	} else {
		if (pVideoSource->spudecodermoduleid != 0) {
			EPRINTF("Function %s is not implemented.\n", __FUNCTION__);
			/* TBD: Implement send_spu_command(pVideoSource->pRua, pVideoSource->spudecodermoduleid, 3). */

			return RM_NOTIMPLEMENTED;
		}
	}
	return RM_OK;
}

RMstatus DCCStopVideoSource(struct DCCVideoSource *pVideoSource, enum DCCStopMode stop_mode)
{
	RMstatus rv;

	rv = send_video_command(pVideoSource->pRua, pVideoSource->decodermoduleid, 2);
	if (rv != RM_OK) {
		return RM_OK;
	}
	if (pVideoSource->spu_decoder != NULL) {
		EPRINTF("Function %s is not implemented.\n", __FUNCTION__);
		/* TBD: Implement DCCPlaySPUSource(). */

		return RM_NOTIMPLEMENTED;
	} else {
		if (pVideoSource->spudecodermoduleid != 0) {
			EPRINTF("Function %s is not implemented.\n", __FUNCTION__);
			/* TBD: Implement send_spu_command(pVideoSource->pRua, pVideoSource->spudecodermoduleid, 3). */

			return RM_NOTIMPLEMENTED;
		}
	}
	if (pVideoSource->scalermoduleid != 0) {
		struct RUAEvent evt;
		RMuint32 ModuleID;
		RMuint32 index;

		switch (stop_mode) {
			case DCCStopMode_BlackFrame:
				rv = set_property(pVideoSource->pRua, pVideoSource->scalermoduleid, RMGenericPropertyID_Stop, NULL, 0);
				if (rv != RM_OK) {
					return rv;
				}
				break;

			case DCCStopMode_LastFrame:
				rv = set_property(pVideoSource->pRua, pVideoSource->scalermoduleid, RMGenericPropertyID_Flush, NULL, 0);
				if (rv != RM_OK) {
					return rv;
				}
				break;
		}
		ModuleID = EMHWLIB_MODULE(DisplayBlock, 0);
		get_event_mask(pVideoSource->scalermoduleid, &evt);
		if (evt.Mask != 0) {
			rv = RUAResetEvent(pVideoSource->pRua, &evt);
			if (rv != RM_OK) {
				fprintf(stderr, "reset_event failed for ModuleID %d\n", ModuleID & 0xFF);
				return rv;
			}
		}
		rv = RUAWaitForMultipleEvents(pVideoSource->pRua, &evt, 1, 1000000, &index);
		if (rv != RM_OK) {
			return rv;
		}
	}

	return RM_OK;
}

RMstatus DCCOpenAudioDecoderSource(struct DCC *pDCC, struct DCCAudioProfile *dcc_profile, struct DCCAudioSource **ppAudioSource)
{
	struct DCCAudioSource *pAudioSource;
	RMstatus rv;
	RMuint32 audioengineid = AudioEngine;
	RMuint32 audiodecoderid = AudioDecoder;
	RMuint32 number_of_engines;
	RMuint32 number_of_decoders;
	RMuint32 buffer_info[2];
	RMuint32 result_info[1];
	RMuint32 result_shm[2];
	RMuint32 buffer_dram[4];
	RMuint32 result_dram[2];
	RMuint32 buffer_shared[10];

	pAudioSource = malloc(sizeof(*pAudioSource));
	if (pAudioSource == NULL) {
		fprintf(stderr, "Error: out of memory\n");

		return RM_FATALOUTOFMEMORY;
	}
	memset(pAudioSource, 0, sizeof(*pAudioSource));
	pAudioSource->pRua = pDCC->pRua;
	pAudioSource->pDCC = pDCC;
	pAudioSource->STCID = dcc_profile->STCID;

	rv = RUAExchangeProperty(pDCC->pRua, EMHWLIB_MODULE(Enumerator, 0), RMEnumeratorPropertyID_CategoryIDToNumberOfInstances, &audioengineid, sizeof(audioengineid), &number_of_engines, sizeof(number_of_engines));
	if (rv != RM_OK) {
		return rv;
	}
	if (dcc_profile->AudioEngineID >= number_of_engines) {
		return RM_PARAMETER_OUT_OF_RANGE;
	}
	rv = RUAExchangeProperty(pDCC->pRua, EMHWLIB_MODULE(Enumerator, 0), RMEnumeratorPropertyID_CategoryIDToNumberOfInstances, &audiodecoderid, sizeof(audiodecoderid), &number_of_decoders, sizeof(number_of_decoders));
	if (rv != RM_OK) {
		return rv;
	}
	if (dcc_profile->AudioDecoderID >= number_of_decoders/number_of_engines) {
		return RM_PARAMETER_OUT_OF_RANGE;
	}
	pAudioSource->enginemoduleid = EMHWLIB_MODULE(AudioEngine, dcc_profile->AudioEngineID);
	pAudioSource->decodermoduleid = EMHWLIB_MODULE(AudioDecoder, dcc_profile->AudioDecoderID);
	memset(buffer_info, 0, sizeof(buffer_info));
	rv = RUAExchangeProperty(pDCC->pRua, pAudioSource->enginemoduleid, RMAudioEnginePropertyID_DecoderSharedMemoryInfo, buffer_info, sizeof(buffer_info), result_info, sizeof(result_info));
	if (rv != RM_OK) {
		return rv;
	}
	rv = RUAGetProperty(pDCC->pRua, pAudioSource->enginemoduleid, RMAudioEnginePropertyID_DecoderSharedMemory, &result_shm, sizeof(result_shm));
	if (rv != RM_OK) {
		return rv;
	}
	if (result_shm[0] == 0) {
		result_shm[0] = pDCC->rua_malloc(pDCC->pRua, pAudioSource->enginemoduleid, pDCC->dram, RUA_DRAM_UNPROTECTED, result_info[0]);
		result_shm[1] = result_info[0];
		rv = set_property(pDCC->pRua, pAudioSource->enginemoduleid, RMAudioEnginePropertyID_DecoderSharedMemory, &result_shm, sizeof(result_shm));
		if (rv != RM_OK) {
			return rv;
		}
		pAudioSource->reserved1C = 1;
	} else {
		pAudioSource->reserved1C = 1;
	}
	memset(buffer_dram, 0, sizeof(buffer_dram));
	buffer_dram[0] = 12;
	buffer_dram[1] = 0x00000F00;
	buffer_dram[2] = dcc_profile->BitstreamFIFOSize;
	buffer_dram[3] = dcc_profile->XferFIFOCount;
	rv = RUAExchangeProperty(pDCC->pRua, pAudioSource->decodermoduleid, RMAudioDecoderPropertyID_DRAMSize, buffer_dram, sizeof(buffer_dram), result_dram, sizeof(result_dram));
	if (rv != RM_OK) {
		return rv;
	}
	memset(buffer_shared, 0, sizeof(buffer_shared));
	buffer_shared[0] = buffer_dram[0]; // 0x00
	buffer_shared[1] = buffer_dram[1]; // 0x04
	buffer_shared[2] = dcc_profile->BitstreamFIFOSize; // 0x08
	buffer_shared[3] = dcc_profile->XferFIFOCount; // 0x0c
	buffer_shared[4] = 0; // 0x10
	buffer_shared[5] = result_dram[0]; // 0x14
	buffer_shared[6] = 0; // 0x18
	buffer_shared[7] = result_dram[1]; // 0x1c
	buffer_shared[8] = dcc_profile->DemuxProgramID; // 0x20
	buffer_shared[9] = dcc_profile->STCID; //0x24
	if (result_dram[0] != 0) {
		buffer_shared[4] = pDCC->rua_malloc(pDCC->pRua, pAudioSource->enginemoduleid, pDCC->dram, RUA_DRAM_UNPROTECTED, result_dram[0]);
		if (buffer_shared[4] == 0) {
			return RM_FATALOUTOFMEMORY;
		}
		pAudioSource->mem1 = buffer_shared[4];
	}
	if (result_dram[1] != 0) {
		buffer_shared[6] = pDCC->rua_malloc(pDCC->pRua, pAudioSource->enginemoduleid, pDCC->dram, RUA_DRAM_UNPROTECTED, result_dram[1]);
		if (buffer_shared[6] == 0) {
			return RM_FATALOUTOFMEMORY;
		}
		pAudioSource->mem2 = buffer_shared[6];
	}
	rv = set_property(pDCC->pRua, pAudioSource->decodermoduleid, RMAudioDecoderPropertyID_Open, &buffer_shared, sizeof(buffer_shared));
	if (rv != RM_OK) {
		return rv;
	}
	*ppAudioSource = pAudioSource;

	return RM_OK;
}

RMstatus DCCCloseAudioSource(struct DCCAudioSource *pAudioSource)
{
	RMstatus rv;
	RMuint32 buffer[1];
	RMuint32 taskcount;
	RMuint32 result_shm[2];

	memset(buffer, 0, sizeof(buffer));
	rv = set_property(pAudioSource->pRua, pAudioSource->decodermoduleid, RMAudioDecoderPropertyID_Close, &buffer, sizeof(buffer));
	if (rv != RM_OK) {
		return rv;
	}
	rv = RUAGetProperty(pAudioSource->pRua, pAudioSource->enginemoduleid, RMAudioEnginePropertyID_ConnectedTaskCount, &taskcount, sizeof(taskcount));
	if (rv != RM_OK) {
		return rv;
	}
	rv = RUAGetProperty(pAudioSource->pRua, pAudioSource->enginemoduleid, RMAudioEnginePropertyID_DecoderSharedMemory, &result_shm, sizeof(result_shm));
	if (rv != RM_OK) {
		return rv;
	}
	if ((result_shm[0] != 0) && (pAudioSource->reserved1C == 1)) {
		RMuint32 address;

		address = result_shm[0];

		result_shm[0] = 0;
		result_shm[1] = 0;
		rv = set_property(pAudioSource->pRua, pAudioSource->enginemoduleid, RMAudioEnginePropertyID_DecoderSharedMemory, &result_shm, sizeof(result_shm));
		if (rv != RM_OK) {
			return rv;
		}

		rv = RUASetAddressID(pAudioSource->pRua, address, 0);
		if (rv != RM_OK) {
			return rv;
		}
		pAudioSource->pDCC->rua_free(pAudioSource->pRua, address);
		pAudioSource->reserved1C = 0;
	}
	if (pAudioSource->mem1 != 0) {
		pAudioSource->pDCC->rua_free(pAudioSource->pRua, pAudioSource->mem1);
		pAudioSource->mem1 = 0;
	}
	if (pAudioSource->mem2 != 0) {
		pAudioSource->pDCC->rua_free(pAudioSource->pRua, pAudioSource->mem2);
		pAudioSource->mem2 = 0;
	}

	return RM_OK;
}

RMstatus DCCGetAudioDecoderSourceInfo(struct DCCAudioSource *pAudioSource, RMuint32 *decoder, RMuint32 *engine, RMuint32 *timer)
{
	if (pAudioSource == NULL) {
		return RM_INVALID_PARAMETER;
	}

	if (decoder != NULL) {
		*decoder = pAudioSource->decodermoduleid;
	}
	if (timer != NULL) {
		*timer = pAudioSource->STCID;
	}
	if (engine != NULL) {
		*engine = pAudioSource->enginemoduleid;
	}

	return RM_OK;
}

RMstatus DCCSetAudioAACFormat(struct DCCAudioSource *pAudioSource, struct AudioDecoder_AACParameters_type *pFormat)
{
	RMstatus rv;
	RMuint32 buffer[1];
	
	rv = send_audio_command(pAudioSource->pRua, pAudioSource->decodermoduleid, 7);
	if (rv != RM_OK) {
		return rv;
	}
	/* Set codec to AAC. */
	buffer[0] = 3;
	rv = set_property(pAudioSource->pRua, pAudioSource->decodermoduleid, RMAudioDecoderPropertyID_Codec, &buffer, sizeof(buffer));
	if (rv != RM_OK) {
		return rv;
	}
	rv = set_property(pAudioSource->pRua, pAudioSource->decodermoduleid, RMAudioDecoderPropertyID_AACParameters, pFormat, sizeof(*pFormat));
	if (rv != RM_OK) {
		return rv;
	}
	rv = send_audio_command(pAudioSource->pRua, pAudioSource->decodermoduleid, 6);
	if (rv != RM_OK) {
		return rv;
	}

	return RM_OK;
}

RMstatus DCCSetAudioSourceVolume(struct DCCAudioSource *pAudioSource, RMuint32 volume)
{
	RMstatus rv;
	int i;

	for (i = 0; i < 12; i++) {
		RMuint32 buffervol[2];

		buffervol[0] = i;
		buffervol[1] = volume;
		rv = set_property(pAudioSource->pRua, pAudioSource->enginemoduleid, RMAudioEnginePropertyID_Volume, &buffervol, sizeof(buffervol));
		if (rv != RM_OK) {
			return rv;
		}
	}

	return RM_OK;
}

RMstatus DCCPlayAudioSource(struct DCCAudioSource *pAudioSource)
{
	RMstatus rv;
	RMuint32 codec;
	RMuint32 level = 256;

	rv = RUAGetProperty(pAudioSource->pRua, pAudioSource->decodermoduleid, RMAudioDecoderPropertyID_Codec, &codec, sizeof(codec));
	if (rv != RM_OK) {
		return rv;
	}

	switch (codec) {
		case 2:
			level = 512;
			break;

		case 7:
			level = 1024;
			break;

		case 14:
			level = 0;
			break;
	}
	rv = DCCSetAudioBtsThreshold(pAudioSource, level);
	if (rv != RM_OK) {
		EPRINTF("DCCSetAudioBtsThreshold() failed with rv = %u.\n", rv);
		return rv;
	}
	
	return send_audio_command(pAudioSource->pRua, pAudioSource->decodermoduleid, 1);
}

RMstatus DCCPauseAudioSource(struct DCCAudioSource *pAudioSource)
{
	return send_audio_command(pAudioSource->pRua, pAudioSource->decodermoduleid, 2);
}

RMstatus DCCStopAudioSource(struct DCCAudioSource *pAudioSource)
{
	return send_audio_command(pAudioSource->pRua, pAudioSource->decodermoduleid, 3);
}

RMstatus DCCSetAudioBtsThreshold(struct DCCAudioSource *pAudioSource, RMuint32 level)
{
	return set_property(pAudioSource->pRua, pAudioSource->decodermoduleid, RMAudioDecoderPropertyID_AudioBtsThreshold, &level, sizeof(level));
}

RMstatus DCCOpenDemuxTask(struct DCC *pDCC, struct DCCDemuxTaskProfile *dcc_profile, struct DCCDemuxTask **ppDemuxTask)
{
	fprintf(stderr, "Error: %s is not implemented.\n", __FUNCTION__);
	return RM_NOTIMPLEMENTED;
}

RMstatus DCCCloseDemuxTask(struct DCCDemuxTask *pDemuxTask)
{
	fprintf(stderr, "Error: %s is not implemented.\n", __FUNCTION__);
	return RM_NOTIMPLEMENTED;
}

RMstatus DCCSetAudioMpegFormat(struct DCCAudioSource *pAudioSource, struct AudioDecoder_MpegParameters_type *pFormat)
{
	fprintf(stderr, "Error: %s is not implemented.\n", __FUNCTION__);
	return RM_NOTIMPLEMENTED;
}

RMstatus DCCPlayDemuxTask(struct DCCDemuxTask *pDemuxTask)
{
	fprintf(stderr, "Error: %s is not implemented.\n", __FUNCTION__);
	return RM_NOTIMPLEMENTED;
}

RMstatus DCCStopDemuxTask(struct DCCDemuxTask *pDemuxTask)
{
	fprintf(stderr, "Error: %s is not implemented.\n", __FUNCTION__);
	return RM_NOTIMPLEMENTED;
}

RMstatus DCCGetDemuxTaskInfo(struct DCCDemuxTask *pDemuxTask, RMuint32 *demux_task)
{
	fprintf(stderr, "Error: %s is not implemented.\n", __FUNCTION__);
	return RM_NOTIMPLEMENTED;
}

RMstatus DCCSetRouteDisplayAspectRatio(struct DCC *pDCC, enum DCCRoute route, RMuint8 ar_x, RMuint8 ar_y)
{
	fprintf(stderr, "Error: %s is not implemented.\n", __FUNCTION__);
	return RM_NOTIMPLEMENTED;
}

void dontremovefunction(void)
{
	void *p;

	/* Call some function so that the function will not be removed by the linker. */
	p = RMMalloc(32);
	if (p != NULL) {
		RMMemset(p, 0, 32);

		RMMemcpy(p, p, 32);

		RMFree(p);
	} else {
		printf("RMMalloc() failed verbose_stderr %d.\n", verbose_stderr);
	}
}
