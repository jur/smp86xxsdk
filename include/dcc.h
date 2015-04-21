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

enum Master_type {
	Master_STC = 0,
	Master_Audio = 1,
};

struct DCCStcProfile {
	RMuint32 STCID;
	enum Master_type master;
	RMuint32 stc_timer_id;
	RMuint32 stc_time_resolution;
	RMuint32 video_timer_id;
	RMuint32 video_time_resolution;
	RMint32 video_offset;
	RMuint32 audio_timer_id;
	RMuint32 audio_time_resolution;
	RMint32 audio_offset;
};

enum EMhwlibVideoCodec {
	EMhwlibVideoCodec_MPEG2 = 1,
	EMhwlibVideoCodec_MPEG4 = 2,
	EMhwlibVideoCodec_MPEG4_Padding = 3,
	EMhwlibVideoCodec_DIVX3 = 4,
	EMhwlibVideoCodec_VC1 = 5,
	EMhwlibVideoCodec_WMV = 6,
	EMhwlibVideoCodec_H264 = 7,
	EMhwlibJPEGCodec = 8,
	EMhwlibDVDSpuCodec = 9,
	EMhwlibBDRLECodec = 10,
};

struct DCCXVideoProfile {
	RMuint32 MpegEngineID;
	RMuint32 STCID;
	RMuint32 XtaskID;
	RMuint32 XtaskInbandFIFOCount;
	RMuint32 VideoDecoderID;
	RMuint32 ProtectedFlags;
	RMuint32 BitstreamFIFOSize;
	RMuint32 XferFIFOCount;
	RMuint32 PtsFIFOCount;
	RMuint32 InbandFIFOCount;
	enum EMhwlibVideoCodec Codec;
	RMuint32 Profile;
	RMuint32 Level;
	RMuint32 ExtraPictureBufferCount;
	RMuint32 MaxWidth;
	RMuint32 MaxHeight;
	RMuint32 SPUProtectedFlags;
	RMuint32 SPUBitstreamFIFOSize;
	RMuint32 SPUXferFIFOCount;
	RMuint32 SPUPtsFIFOCount;
	RMuint32 SPUInbandFIFOCount;
	enum EMhwlibVideoCodec SPUCodec;
	RMuint32 SPUProfile;
	RMuint32 SPULevel;
	RMuint32 SPUExtraPictureBufferCount;
	RMuint32 SPUMaxWidth;
	RMuint32 SPUMaxHeight;
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
RMstatus DCCSetMemoryManager(struct DCC *pDCC, RMuint8 dram);
RMstatus DCCSTCOpen(struct DCC *pDCC, struct DCCStcProfile *stc_profile, struct DCCSTCSource **ppStcSource);
RMstatus DCCSTCClose(struct DCCSTCSource *pStcSource);
RMstatus DCCSTCGetModuleId(struct DCCSTCSource *pStcSource, RMuint32 *stc_id);
RMstatus DCCXOpenVideoDecoderSource(struct DCC *pDCC, struct DCCXVideoProfile *dcc_profile, struct DCCVideoSource **ppVideoSource);
RMstatus DCCCloseVideoSource(struct DCCVideoSource *pVideoSource);

#endif
