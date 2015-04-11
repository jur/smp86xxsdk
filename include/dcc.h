#ifndef _DCC_H
#define _DCC_H

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

#include <rua.h>

#define EMhwlibSamplingMode_444 1

#define EMhwlibColorMode_TrueColor 5

#define EMhwlibColorFormat_32BPP 4

#define EMhwlibColorSpace_RGB_0_255 3

struct DCC;
struct DCCVideoSource;

enum DCCRoute {
	DCCRoute_Main = 0,
	DCCRoute_Secondary = 1,
};

enum DCCSurface {
	DCCSurface_Video = 0,
	DCCSurface_OSD = 1,
};

enum DCCInitMode {
	DCCInitMode_InitDisplay = 0,
	DCCInitMode_LeaveDisplay
};

struct DCCSTCSource;

struct EMhwlibAspectRatio {
	RMuint32 X;
	RMuint32 Y;
};

struct DCCOSDProfile {
	RMuint32 SamplingMode;
	RMuint32 ColorMode;
	RMuint32 ColorFormat;
	RMuint32 Width;
	RMuint32 Height;
	RMuint32 ColorSpace;
	struct EMhwlibAspectRatio PixelAspectRatio;
};

RMstatus DCCOpen(struct RUA *pRUA, struct DCC **ppDCC);
RMstatus DCCClose(struct DCC *pDCC);
RMstatus DCCInitMicroCodeEx(struct DCC *pDCC, enum DCCInitMode init_mode);
RMstatus DCCSetSurfaceSource(struct DCC *pDCC, RMuint32 surfaceID, struct DCCVideoSource *pVideoSource);
RMstatus DCCOpenMultiplePictureOSDVideoSource(struct DCC *pDCC, struct DCCOSDProfile *profile, RMuint32 picture_count, struct DCCVideoSource **ppVideoSource, struct DCCSTCSource *pStcSource);
RMstatus DCCGetOSDSurfaceInfo(struct DCC *pDCC, struct DCCVideoSource *pVideoSource, struct DCCOSDProfile *profile, RMuint32 *SurfaceAddr, RMuint32 *SurfaceSize);
RMstatus DCCGetOSDPictureInfo(struct DCCVideoSource *pVideoSource, RMuint32 index, RMuint32 *PictureAddr,  RMuint32 *LumaAddr, RMuint32 *LumaSize, RMuint32 *ChromaAddr, RMuint32 *ChromaSize);
RMstatus DCCGetScalerModuleID(struct DCC *pDCC, enum DCCRoute route, enum DCCSurface surface, RMuint32 index, RMuint32 *scaler);
RMstatus DCCClearOSDPicture(struct DCCVideoSource *pVideoSource, RMuint32 index);
RMstatus DCCInsertPictureInMultiplePictureOSDVideoSource(struct DCCVideoSource *pVideoSource, RMuint32 index, RMuint64 Pts);
RMstatus DCCEnableVideoSource(struct DCCVideoSource *pVideoSource, RMbool enable);

#endif
