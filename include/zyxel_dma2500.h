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
 */
typedef enum {
	RMDemuxEnginePropertyID_TimerInit = 1064,
	RMMpegEnginePropertyID_InitMicrocodeSymbols = 1067,
	RMMMPropertyID_Malloc = 1080,
	RMMMPropertyID_Free = 1081,
	RMDisplayBlockPropertyID_SurfaceSize = 4023,
	RMDisplayBlockPropertyID_PictureSize = 4024,
	RMDisplayBlockPropertyID_MultiplePictureSurfaceSize = 4025,
	RMDisplayBlockPropertyID_InitPictureX = 4027,
	RMDisplayBlockPropertyID_InsertPictureInSurfaceFifo = 4028,
	RMDisplayBlockPropertyID_InitMultiplePictureSurface = 6002,
	RMDisplayBlockPropertyID_EnableGFXInteraction = 6009,
	RMGenericPropertyID_Enable = 6016,
	RMGenericPropertyID_Validate = 6018,
	RMGenericPropertyID_Surface = 6019,
	RMGenericPropertyID_PersistentSurface = 6021,
	RMGenericPropertyID_MixerSourceIndex = 6067,
	RMGenericPropertyID_MixerSourceState = 6069,
} RMPropertyID;

#endif
