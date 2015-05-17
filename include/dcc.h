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

#define EMhwlibColorFormat_24BPP_565 1
#define EMhwlibColorFormat_24BPP 2
#define EMhwlibColorFormat_32BPP_4444 3
#define EMhwlibColorFormat_32BPP 4
#define EMhwlibColorFormat_16BPP_565 5
#define EMhwlibColorFormat_16BPP_1555 6
#define EMhwlibColorFormat_16BPP_4444 7

#define EMhwlibColorSpace_RGB_0_255 3

struct DCC;
struct DCCVideoSource;
struct DCCAudioSource;
struct DCCDemuxTask;

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
	RMuint32 reserved1;
};

struct DCCAudioProfile {
	RMuint32 BitstreamFIFOSize;
	RMuint32 XferFIFOCount;
	RMuint32 DemuxProgramID;
	RMuint32 AudioEngineID;
	RMuint32 AudioDecoderID;
	RMuint32 STCID;
};

enum AACInputFormat {
	ADIF_header = 0,
	ADTS_header = 1,
	DSI_header = 2
};

enum OutputSpdif_type {
	OutputSpdif_Disable = 0,
	OutputSpdif_Compressed = 1,
	OutputSpdif_Uncompressed = 3,
	OutputSpdif_NoDecodeCompressed = 5,
};

enum OutputDualMode_type {
	DualMode_Stereo = 0,
	DualMode_LeftMono = 1,
	DualMode_RightMono = 2,
	DualMode_MixMono = 3,
};

enum AacOutputChannels_type {
	Aac_C = 1,
	Aac_LR = 2,
	Aac_LCR = 3,
	Aac_LRS = 18,
	Aac_LCRS = 19,
	Aac_LRLsRs = 34,
	Aac_LCRLsRs = 35,
};

enum AudioOutputSurround20_type {
	SurroundAsStream = 0,
	SurroundDisable = 4,
	SurroundEnable = 5,
};

struct AudioDecoder_AACParameters_type {
	enum AACInputFormat InputFormat;
	enum OutputSpdif_type OutputSpdif;
	enum OutputDualMode_type OutputDualMode;
	enum AacOutputChannels_type OutputChannels;
	RMbool Acmod2DualMode;
	RMbool OutputLfe;
	enum AudioOutputSurround20_type OutputSurround20;
	RMuint32 BassMode;
};

enum DCCStreamType {
	DCC_Stc = 0,
	DCC_Video = 1,
	DCC_Audio = 2,
};

struct emhwlib_info {
	RMuint32 ValidFields;
	RMuint64 TimeStamp;
	RMuint32 FirstAccessUnitPointer;
};

enum DCCVideoPlayCommand {
	DCCVideoPlayFwd = 3,
	DCCVideoPlayIFrame = 5,
	DCCVideoPlayNextFrame = 6,
};

enum DCCStopMode {
	DCCStopMode_BlackFrame = 0,
	DCCStopMode_LastFrame = 1,
};

struct DCCDemuxTaskProfile {
	RMuint32 ProtectedFlags; // 0x00
	RMuint32 BitstreamFIFOSize; // 0x04
	RMuint32 XferFIFOCount; // 0x08
	RMuint32 InbandFIFOCount; // 0x0c
	RMuint32 InputPort; // 0x10
	RMuint32 PrimaryMPM; // 0x14
	RMuint32 SecondaryMPM; // 0x18
	RMuint32 DemuxTaskID; // 0x1c
	RMuint32 reserved20; // 0x20
	RMuint32 reserved24; // 0x24
	RMuint32 reserved28; // 0x28
	RMuint32 reserved2c; // 0x2c
};

enum AudioOutputChannels_type {
	Audio_Out_Ch_C = 1,
	Audio_Out_Ch_LR = 2,
	Audio_Out_Ch_LCR = 3,
	Audio_Out_Ch_LRS = 18,
	Audio_Out_Ch_LCRS = 19,
	Audio_Out_Ch_LRLsRs = 34,
	Audio_Out_Ch_LCRLsRs = 35,
	Audio_Out_Ch_LCRLsRsSs = 99,
	Audio_Out_Ch_LRLsRsLssRss = 162,
	Audio_Out_Ch_LCRLsRsLssRss = 163,
};

struct AudioDecoder_MpegParameters_type {
	enum OutputDualMode_type OutputDualMode;
	RMbool Acmod2DualMode;
	enum AudioOutputChannels_type OutputChannels;
	RMbool OutputLfe;
	enum AudioOutputSurround20_type OutputSurround20;
	enum OutputSpdif_type OutputSpdif;
	RMuint32 BassMode;
};


RMstatus DCCOpen(struct RUA *pRUA, struct DCC **ppDCC);
RMstatus DCCClose(struct DCC *pDCC);
RMstatus DCCInitMicroCodeEx(struct DCC *pDCC, enum DCCInitMode init_mode);
RMstatus DCCSetSurfaceSource(struct DCC *pDCC, RMuint32 surfaceID, struct DCCVideoSource *pVideoSource);
RMstatus DCCGetScalerModuleID(struct DCC *pDCC, enum DCCRoute route, enum DCCSurface surface, RMuint32 index, RMuint32 *scaler);

RMstatus DCCOpenMultiplePictureOSDVideoSource(struct DCC *pDCC, struct DCCOSDProfile *profile, RMuint32 picture_count, struct DCCVideoSource **ppVideoSource, struct DCCSTCSource *pStcSource);
RMstatus DCCOpenOSDVideoSource(struct DCC *pDCC, struct DCCOSDProfile *profile, struct DCCVideoSource **ppVideoSource);
RMstatus DCCGetOSDSurfaceInfo(struct DCC *pDCC, struct DCCVideoSource *pVideoSource, struct DCCOSDProfile *profile, RMuint32 *SurfaceAddr, RMuint32 *SurfaceSize);
RMstatus DCCGetOSDPictureInfo(struct DCCVideoSource *pVideoSource, RMuint32 index, RMuint32 *PictureAddr,  RMuint32 *LumaAddr, RMuint32 *LumaSize, RMuint32 *ChromaAddr, RMuint32 *ChromaSize);
RMstatus DCCGetOSDVideoSourceInfo(struct DCCVideoSource *pVideoSource, RMuint32 *LumaAddr, RMuint32 *LumaSize, RMuint32 *ChromaAddr, RMuint32 *ChromaSize);
RMstatus DCCClearOSDPicture(struct DCCVideoSource *pVideoSource, RMuint32 index);
RMstatus DCCClearOSDVideoSource(struct DCCVideoSource *pVideoSource);
RMstatus DCCInsertPictureInMultiplePictureOSDVideoSource(struct DCCVideoSource *pVideoSource, RMuint32 index, RMuint64 Pts);
RMstatus DCCEnableVideoSource(struct DCCVideoSource *pVideoSource, RMbool enable);
RMstatus DCCSetMemoryManager(struct DCC *pDCC, RMuint8 dram);

RMstatus DCCSTCOpen(struct DCC *pDCC, struct DCCStcProfile *stc_profile, struct DCCSTCSource **ppStcSource);
RMstatus DCCSTCClose(struct DCCSTCSource *pStcSource);
RMstatus DCCSTCGetModuleId(struct DCCSTCSource *pStcSource, RMuint32 *stc_id);
RMstatus DCCSTCSetTimeResolution(struct DCCSTCSource *pStcSource, enum DCCStreamType type, RMuint32 time_resolution);
RMstatus DCCSTCSetVideoOffset(struct DCCSTCSource *pStcSource, RMint32 time, RMuint32 time_resolution);
RMstatus DCCSTCSetAudioOffset(struct DCCSTCSource *pStcSource, RMint32 time, RMuint32 time_resolution);
RMstatus DCCSTCSetTime(struct DCCSTCSource *pStcSource, RMuint64 time, RMuint32 time_resolution);
RMstatus DCCSTCSetSpeed(struct DCCSTCSource *pStcSource, RMint32 numerator, RMuint32 denominator);
RMstatus DCCSTCGetTime(struct DCCSTCSource *pStcSource, RMuint64 *ptime, RMuint32 time_resolution);
RMstatus DCCSTCPlay(struct DCCSTCSource *pStcSource);
RMstatus DCCSTCStop(struct DCCSTCSource *pStcSource);

RMstatus DCCXOpenVideoDecoderSource(struct DCC *pDCC, struct DCCXVideoProfile *dcc_profile, struct DCCVideoSource **ppVideoSource);
RMstatus DCCCloseVideoSource(struct DCCVideoSource *pVideoSource);
RMstatus DCCXSetVideoDecoderSourceCodec(struct DCCVideoSource *pVideoSource, enum EMhwlibVideoCodec Codec);
RMstatus DCCGetVideoDecoderSourceInfo(struct DCCVideoSource *pVideoSource, RMuint32 *video_decoder, RMuint32 *spu_decoder, RMuint32 *timer);
RMstatus DCCPlayVideoSource(struct DCCVideoSource *pVideoSource, enum DCCVideoPlayCommand cmd);
RMstatus DCCStopVideoSource(struct DCCVideoSource *pVideoSource, enum DCCStopMode stop_mode);
RMstatus DCCSetRouteDisplayAspectRatio(struct DCC *pDCC, enum DCCRoute route, RMuint8 ar_x, RMuint8 ar_y);

RMstatus DCCOpenAudioDecoderSource(struct DCC *pDCC, struct DCCAudioProfile *dcc_profile, struct DCCAudioSource **ppAudioSource);
RMstatus DCCCloseAudioSource(struct DCCAudioSource *pAudioSource);
RMstatus DCCGetAudioDecoderSourceInfo(struct DCCAudioSource *pAudioSource, RMuint32 *decoder, RMuint32 *engine, RMuint32 *timer);
RMstatus DCCSetAudioAACFormat(struct DCCAudioSource *pAudioSource, struct AudioDecoder_AACParameters_type *pFormat);
RMstatus DCCSetAudioSourceVolume(struct DCCAudioSource *pAudioSource, RMuint32 volume);
RMstatus DCCPlayAudioSource(struct DCCAudioSource *pAudioSource);
RMstatus DCCPauseAudioSource(struct DCCAudioSource *pAudioSource);
RMstatus DCCStopAudioSource(struct DCCAudioSource *pAudioSource);
RMstatus DCCSetAudioBtsThreshold(struct DCCAudioSource *pAudioSource, RMuint32 level);

RMstatus DCCOpenDemuxTask(struct DCC *pDCC, struct DCCDemuxTaskProfile *dcc_profile, struct DCCDemuxTask **ppDemuxTask);
RMstatus DCCCloseDemuxTask(struct DCCDemuxTask *pDemuxTask);
RMstatus DCCSetAudioMpegFormat(struct DCCAudioSource *pAudioSource, struct AudioDecoder_MpegParameters_type *pFormat);
RMstatus DCCPlayDemuxTask(struct DCCDemuxTask *pDemuxTask);
RMstatus DCCStopDemuxTask(struct DCCDemuxTask *pDemuxTask);
RMstatus DCCGetDemuxTaskInfo(struct DCCDemuxTask *pDemuxTask, RMuint32 *demux_task);

#endif
