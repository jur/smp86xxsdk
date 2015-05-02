#ifndef _ZYXEL_DMA2500_H
#define _ZYXEL_DMA2500_H

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

/**
 * The IDs depend on the version of the Sigma SDK for SMP8634/SMP8635.
 * The firmware 1.00.07b1 for the Zyxel DMA-2500 uses the following
 * mapping to numbers.
 * To be compatible the names are the same as example code which is freely
 * available.
 * The base used by Zyxel for the DMA-2500 seems to be a package that is maybe
 * called mrua_SMP8634_2.8.4.1_GCC4_dev.mips (I found this information in the
 * kernel driver, so I am not sure).
 *
 * TBD: Maybe some IDs are not correct. Need to check them.
 */
typedef enum {
	RMEnumeratorPropertyID_CategoryIDToNumberOfInstances = 1,

	RMDemuxEnginePropertyID_TimerInit = 1064,

	RMMpegEnginePropertyID_InitMicrocodeSymbols = 1067,

	RMMMPropertyID_Malloc = 1080,
	RMMMPropertyID_Free = 1081,

	RMGenericPropertyID_Error = 4018,

	RMDisplayBlockPropertyID_SurfaceSize = 4023,
	RMDisplayBlockPropertyID_PictureSize = 4024,
	RMDisplayBlockPropertyID_MultiplePictureSurfaceSize = 4025,
	RMDisplayBlockPropertyID_InitPictureX = 4027,
	RMDisplayBlockPropertyID_InsertPictureInSurfaceFifo = 4028,

	RMDispVideoInputPropertyID_Close = 4055,

	RMMpegEnginePropertyID_DecoderSharedMemory = 4101,
	RMMpegEnginePropertyID_SchedulerSharedMemory = 4102,
	RMMpegEnginePropertyID_ConnectedTaskCount = 4103,

	RMVideoDecoderPropertyID_DRAMSize = 4104,
	RMVideoDecoderPropertyID_Open = 4105,
	RMVideoDecoderPropertyID_Close = 4106,
	RMVideoDecoderPropertyID_Codec = 4107,
	RMVideoDecoderPropertyID_Command = 4108,
	RMVideoDecoderPropertyID_State = 4109,
	RMVideoDecoderPropertyID_DecoderDataMemory = 4133,
	RMVideoDecoderPropertyID_DRAMSizeX = 4134,
	RMVideoDecoderPropertyID_OpenX = 4135,
	RMVideoDecoderPropertyID_CodecX = 4136,
	RMVideoDecoderPropertyID_BtsFIFO = 4137,
	RMVideoDecoderPropertyID_CommandStatus = 4140,
	RMGenericPropertyID_ReadBufferCompletion = 4148,

	RMAudioEnginePropertyID_Volume = 4162,
	RMAudioEnginePropertyID_DecoderSharedMemoryInfo = 4175,
	RMAudioEnginePropertyID_DecoderSharedMemory = 4176,
	RMAudioDecoderPropertyID_DRAMSize = 4187,
	RMAudioDecoderPropertyID_Open = 4188,
	RMAudioDecoderPropertyID_Command = 4193,
	RMAudioDecoderPropertyID_State = 4194,
	RMAudioDecoderPropertyID_Codec = 4196,
	RMAudioDecoderPropertyID_AudioBtsThreshold = 4197,
	RMAudioDecoderPropertyID_AACParameters = 4201,

	RMGFXEnginePropertyID_ColorFormat = 4297,

	RMSpuDecoderPropertyID_DRAMSize = 4247,
	RMSpuDecoderPropertyID_Open = 4348,
	RMSpuDecoderPropertyID_Close = 4349,
	RMSpuDecoderPropertyID_Command = 4350,
	RMSpuDecoderPropertyID_State = 4351,
	RMSpuDecoderPropertyID_CodecX = 4355,

	RMSTCPropertyID_Open = 4386,
	RMSTCPropertyID_Close = 4389,
	RMSTCPropertyID_StcTimeResolution = 4390,
	RMSTCPropertyID_VideoTimeResolution = 4391,
	RMSTCPropertyID_AudioTimeResolution = 4392,
	RMSTCPropertyID_VideoOffset = 4393,
	RMSTCPropertyID_AudioOffset = 4394,
	RMSTCPropertyID_Time = 4395,
	RMSTCPropertyID_TimeInfo = 4396,
	RMSTCPropertyID_Speed = 4399,
	RMSTCPropertyID_Play = 4397,
	RMSTCPropertyID_Stop = 4398,
	RMSTCPropertyID_Discontinuity = 4400,

	RMDemuxTaskPropertyID_MatchSectionEntry = 4457,
	RMDemuxTaskPropertyID_PidEntryEnable = 4464,
	RMDemuxTaskPropertyID_PidEntryDisable = 4465,

	RMDemuxOutputPropertyID_DRAMSize = 4539,
	RMDemuxOutputPropertyID_Close = 4543,
	RMDemuxOutputPropertyID_Disconnect = 4545,
	RMDemuxOutputPropertyID_Flush = 4548,
	RMDemuxOutputPropertyID_BtsFIFO = 4554,

	RMVCXOPropertyID_Speed = 4591,

	RMAudioEnginePropertyID_BandwidthInit = 4640,

	RMDisplayBlockPropertyID_InitMultiplePictureSurface = 6002,
	RMDisplayBlockPropertyID_EnableGFXInteraction = 6009,

	RMGenericPropertyID_Enable = 6016,
	RMGenericPropertyID_Validate = 6018,
	RMGenericPropertyID_Surface = 6019,
	RMGenericPropertyID_PersistentSurface = 6021,
	RMGenericPropertyID_Stop = 6042,
	RMGenericPropertyID_Flush = 6043,
	RMGenericPropertyID_Step = 6044,
	RMGenericPropertyID_SubPictureSurface = 6046,
	RMGenericPropertyID_DisplayAspectRatio = 6065,
	RMGenericPropertyID_MixerSourceIndex = 6067,
	RMGenericPropertyID_MixerSourceState = 6069,

	RMSpuDecoderPropertyID_VSyncSurface = 6209,
} RMPropertyID;

#endif
