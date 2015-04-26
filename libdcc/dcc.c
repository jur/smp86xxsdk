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

struct DCC {
	struct RUA *pRua;
	RMuint32 video_ucode_address;
	RMuint32 audio_ucode_address;
	RMuint32 demux_ucode_address;
	RMuint32 dram;
	void *rua_malloc;
	void *rua_free;
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

struct DCCVideoSource {
	struct DCC *pDCC;
	struct RUA *pRua;
	RMuint32 mixer;
	RMuint32 scaler;
	RMuint32 addr;
	RMuint32 picture_count;
	pic_info_t *pic_info;
};

struct DCCSTCSource {
	struct RUA *pRua;
	RMuint32 StcModuleId;
};

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

static void get_event_mask(struct RUA *pRua, RMuint32 ModuleID, struct RUAEvent *evt)
{
	evt->ModuleID = EMHWLIB_MODULE(DisplayBlock, 0);

	switch(ModuleID & 0xFF) {
		case DispMainVideoScaler:
			evt->Mask = 0x400;
			break;

		case DispHardwareCursor:
			evt->Mask = 0x200;
			break;

		case DispOSDScaler:
			evt->Mask = 0x100;
			break;

		case DispMainMixer:
			evt->Mask = 0x40;
			break;

		case DispRouting:
			evt->Mask = 0x20;
			break;

		case DispColorBars:
			evt->Mask = 0x10;
			break;

		case DispComponentOut:
			evt->Mask = 0x04;
			break;

		case DispMainAnalogOut:
			evt->Mask = 0x02;
			break;

		case DispDigitalOut:
			evt->Mask = 0x01;
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

		get_event_mask(pRua, ModuleID, &evt);
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

		pVideoSource->scaler = surfaceID;

		switch (surfaceID) {
			case DispMainVideoScaler:
			case DispSubPictureScaler:
			case DispOSDScaler:
				pVideoSource->mixer = EMHWLIB_MODULE(DispMainMixer, 0);
				break;
			default:
				pVideoSource->mixer = 0;
				fprintf(stderr, "Error: %s is not implemented for surfaceID %d.\n", __FUNCTION__, surfaceID);
				return RM_NOTIMPLEMENTED;
		}
	
		surface = pVideoSource->addr;
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
	
	addr = RUAMalloc(pDCC->pRua, pDCC->dram, RUA_DRAM_UNPROTECTED, pic_out[0] * picture_count + surface_size);
	if (addr == 0) {
		fprintf(stderr, "Error: Failed to allocate memory.\n");
		return RM_FATALOUTOFMEMORY;
	}
	pVideoSource->addr = addr;
	pVideoSource->picture_count = picture_count;

	RMuint32 surface_cfg[9];

	surface_cfg[0] = profile->ColorMode;
	surface_cfg[1] = profile->ColorFormat;
	surface_cfg[2] = profile->SamplingMode;
	surface_cfg[3] = pVideoSource->addr;
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
		RUAFree(pDCC->pRua, pVideoSource->addr);
		pVideoSource->addr = 0;
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
		printf("LumaAddress 0x%08x\n", frameOut[0]);
		printf("ChromaAddress 0x%08x\n", frameOut[1]);
		printf("PaletteAddress 0x%08x\n", frameOut[2]);

		addr += pic_out[0]; // buffer size
	}
	RMuint32 enable[2];
	enable[0] = pVideoSource->addr;
	enable[1] = TRUE;

	rv = RUASetProperty(pDCC->pRua, EMHWLIB_MODULE(DisplayBlock, 0), RMDisplayBlockPropertyID_EnableGFXInteraction, enable, sizeof(enable), 0);
	if (rv != RM_OK) {
		fprintf(stderr, "Error: Failed to enable surface.\n");
		RUAFree(pDCC->pRua, pVideoSource->addr);
		pVideoSource->addr = 0;
		return rv;
	}

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
			fprintf(stderr, "Error: Failed to set picture format.\n");
			return rv;
		}
		*SurfaceSize = pic_out[0];
	}
	*SurfaceAddr = pVideoSource->addr;
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

RMstatus DCCGetScalerModuleID(struct DCC *pDCC, enum DCCRoute route, enum DCCSurface surface, RMuint32 index, RMuint32 *scaler)
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
						*scaler = EMHWLIB_MODULE(DispOSDScaler, 0);
						return RM_OK;
					} else {
						fprintf(stderr, "Error: %s is not implemented.\n", __FUNCTION__);
						return RM_NOTIMPLEMENTED;
					}
					break;

				case DCCSurface_Video:
					if (index == 0) {
						*scaler = EMHWLIB_MODULE(DispMainVideoScaler, 0);
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
	buffer[0] = pVideoSource->addr;
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

	if (pVideoSource->mixer == 0) {
		fprintf(stderr, "Error: %s no mixer defined.\n", __FUNCTION__);
		return RM_ERROR;
	}
	if (pVideoSource->scaler == 0) {
		fprintf(stderr, "Error: %s no scaler defined.\n", __FUNCTION__);
		return RM_ERROR;
	}

	rv = RUAExchangeProperty(pVideoSource->pRua, pVideoSource->mixer, RMGenericPropertyID_MixerSourceIndex, &pVideoSource->scaler, sizeof(pVideoSource->scaler), &idx, sizeof(idx));
	if (rv != RM_OK) {
		fprintf(stderr, "Error: %s getting index failed.\n", __FUNCTION__);
		return rv;
	}
	if (enable) {
		state = 2;
	} else {
		state = 1;
	}
	mixer = EMHWLIB_TARGET_MODULE(pVideoSource->mixer, 0, idx);
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
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCSTCSetVideoOffset(struct DCCSTCSource *pStcSource, RMint32 time, RMuint32 time_resolution)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCSTCSetAudioOffset(struct DCCSTCSource *pStcSource, RMint32 time, RMuint32 time_resolution)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCSTCSetTime(struct DCCSTCSource *pStcSource, RMuint64 time, RMuint32 time_resolution)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCSTCSetSpeed(struct DCCSTCSource *pStcSource, RMint32 numerator, RMuint32 denominator)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCSTCGetTime(struct DCCSTCSource *pStcSource, RMuint64 *ptime, RMuint32 time_resolution)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCSTCPlay(struct DCCSTCSource *pStcSource)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCSTCStop(struct DCCSTCSource *pStcSource)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCXOpenVideoDecoderSource(struct DCC *pDCC, struct DCCXVideoProfile *dcc_profile, struct DCCVideoSource **ppVideoSource)
{
	struct DCCVideoSource *pVideoSource;
	RMuint32 buffer[1];
	RMuint32 result[1];
	RMstatus rv;

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
	pVideoSource->pRua = pDCC->pRua;
	pVideoSource->pDCC = pDCC;

	memset(buffer, 0, sizeof(buffer));
	memset(result, 0, sizeof(result));
	buffer[0] = MpegEngine;
	rv = RUAExchangeProperty(pDCC->pRua, EMHWLIB_MODULE(Enumerator, 0), RMEnumeratorPropertyID_CategoryIDToNumberOfInstances, buffer, sizeof(buffer), &result, sizeof(result));
	if (rv != RM_OK) {
		return RM_ERROR;
	}
	if (dcc_profile->MpegEngineID >= result[0]) {
		return RM_PARAMETER_OUT_OF_RANGE;
	}

	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCCloseVideoSource(struct DCCVideoSource *pVideoSource)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCXSetVideoDecoderSourceCodec(struct DCCVideoSource *pVideoSource, enum EMhwlibVideoCodec Codec)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCGetVideoDecoderSourceInfo(struct DCCVideoSource *pVideoSource, RMuint32 *video_decoder, RMuint32 *spu_decoder, RMuint32 *timer)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCPlayVideoSource(struct DCCVideoSource *pVideoSource, enum DCCVideoPlayCommand cmd)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCStopVideoSource(struct DCCVideoSource *pVideoSource, enum DCCStopMode stop_mode)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCOpenAudioDecoderSource(struct DCC *pDCC, struct DCCAudioProfile *dcc_profile, struct DCCAudioSource **ppAudioSource)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCCloseAudioSource(struct DCCAudioSource *pAudioSource)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCGetAudioDecoderSourceInfo(struct DCCAudioSource *pAudioSource, RMuint32 *decoder, RMuint32 *engine, RMuint32 *timer)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCSetAudioAACFormat(struct DCCAudioSource *pAudioSource, struct AudioDecoder_AACParameters_type *pFormat)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCSetAudioSourceVolume(struct DCCAudioSource *pAudioSource, RMuint32 volume)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCPlayAudioSource(struct DCCAudioSource *pAudioSource)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus DCCStopAudioSource(struct DCCAudioSource *pAudioSource)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

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
