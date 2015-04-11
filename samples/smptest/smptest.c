/*
 * Copyright (c) 2015, Juergen Urban
 * All rights reserved.
 *
 * The test just shows some colored lines on a black screen.
 */

#include <stdio.h>
#include <string.h>

#include "rua.h"
#include "dcc.h"

#define DEFAULT_OSD_CHIP 0

typedef struct {
	struct RUA *pRUA;
	struct DCC *pDCC;
	RMuint32 osd_scaler;
	RMuint32 LumaAddr;
} app_rua_context_t;

static void cleanup(app_rua_context_t *context)
{
	RMstatus rv;

	if (context == NULL) {
		return;
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

static RMstatus osd_init(app_rua_context_t *context)
{
	RMstatus rv;
	RMuint32 osd_scaler = EMHWLIB_MODULE(DispOSDScaler, 0);

	memset(context, 0, sizeof(*context));

	rv = RUACreateInstance(&context->pRUA, DEFAULT_OSD_CHIP);
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

	rv = DCCSetSurfaceSource(context->pDCC, osd_scaler, NULL);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Cannot reset osd_scaler %d\n", rv);
		cleanup(context);
		return rv;
	}

	return RM_OK;
}

/** The color space seems to be YUV also when we specify RGB. */
static void set_pixel(struct DCCOSDProfile *profile, uint8_t *BaseAddr, uint32_t x, uint32_t line, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	int pos;
	int y;
	int u;
	int v;

	pos = line * profile->Width;
	pos += x;
	pos *= 4;

	y = (int)(0.299 * r + 0.587 * g + 0.114 * b);
	u = (int)((b - y) * 0.565 + 128);
	v = (int)((r - y) * 0.713 + 128);

	if (y > 255) {
		y = 255;
	}
	if (y < 0) {
		y = 0;
	}
	if (u > 255) {
		u = 255;
	}
	if (u < 0) {
		u = 0;
	}
	if (v > 255) {
		v = 255;
	}
	if (v < 0) {
		v = 0;
	}

	BaseAddr[pos + 0] = u;
	BaseAddr[pos + 1] = y;
	BaseAddr[pos + 2] = v;
	BaseAddr[pos + 3] = a;
}

static RMstatus create_osd_buffer(app_rua_context_t *context)
{
	RMstatus rv;
	struct DCCOSDProfile profile;
	struct DCCVideoSource *pVideoSource;
	RMuint32 pic_luma_addr;
	RMuint32 pic_luma_size;
	RMuint32 pic_luma_addr2;
	RMuint32 pic_luma_size2;
	RMuint32 surface_addr;
	uint8_t *BaseAddr;

	profile.SamplingMode = EMhwlibSamplingMode_444;
	profile.ColorMode = EMhwlibColorMode_TrueColor;
	profile.ColorFormat = EMhwlibColorFormat_32BPP;
	profile.Width = 1024;
	profile.Height = 768;
	profile.ColorSpace = EMhwlibColorSpace_RGB_0_255;
	profile.PixelAspectRatio.X = 1;
	profile.PixelAspectRatio.Y = 1;

	rv = DCCOpenMultiplePictureOSDVideoSource(context->pDCC, &profile, 2,
		&pVideoSource, NULL);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error DCCOpenMultiplePictureOSDVideoSource! %d\n", rv);
		return rv;
	}

	surface_addr = 0;
	rv = DCCGetOSDSurfaceInfo(context->pDCC, pVideoSource, NULL,
		&surface_addr, NULL);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error DCCGetOSDSurfaceInfo! %d\n", rv);
		return rv;
	}
	printf("surface_addr 0x%08x\n", surface_addr);

	pic_luma_addr = 0;
	pic_luma_size = 0;
	rv = DCCGetOSDPictureInfo(pVideoSource, 0, NULL, &pic_luma_addr, &pic_luma_size, NULL, NULL);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error DCCGetOSDPictureInfo! %d\n", rv);
		return rv;
	}

	rv = DCCGetOSDPictureInfo(pVideoSource, 1, NULL, &pic_luma_addr2, &pic_luma_size2, NULL, NULL);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error DCCGetOSDPictureInfo! %d\n", rv);
		return rv;
	}
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error DCCGetOSDVideoSourceInfo! %d\n", rv);
		return rv;
	}
	printf("pic_luma_addr 0x%08x\n", pic_luma_addr);
	printf("pic_luma_size 0x%08x\n", pic_luma_size);
	printf("pic_luma_addr2 0x%08x\n", pic_luma_addr2);
	printf("pic_luma_size2 0x%08x\n", pic_luma_size2);

	context->osd_scaler = 0;
	rv = DCCGetScalerModuleID(context->pDCC, DCCRoute_Main,
		DCCSurface_OSD, 0, &context->osd_scaler);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error creating scaler! %d\n", rv);
		return rv;
	}
	printf("osd_scaler 0x%08x\n", context->osd_scaler);

	rv = DCCClearOSDPicture(pVideoSource, 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error DCCClearOSDPicture! %d\n", rv);
		return rv;
	}
	rv = DCCClearOSDPicture(pVideoSource, 1);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error DCCClearOSDPicture! %d\n", rv);
		return rv;
	}

	rv = DCCInsertPictureInMultiplePictureOSDVideoSource(pVideoSource, 0, 0);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error DCCInsertPictureInMultiplePictureOSDVideoSource! %d\n", rv);
		return rv;
	}

	rv = DCCSetSurfaceSource(context->pDCC, context->osd_scaler,
		pVideoSource);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error DCCSetSurfaceSource! %d\n", rv);
		return rv;
	}

	rv = DCCEnableVideoSource(pVideoSource, TRUE);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error DCCEnableVideoSource! %d\n", rv);
		return rv;
	}

	rv = RUALock(context->pRUA, pic_luma_addr, pic_luma_size);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error RUALock! %d\n", rv);
		return rv;
	}
	BaseAddr = RUAMap(context->pRUA, pic_luma_addr, pic_luma_size);
	if (BaseAddr != NULL) {
		unsigned int i;

		printf("BaseAddr %p\n", BaseAddr);
		unsigned int x;
		unsigned int y;

		for (y = 0; y < profile.Height; y++) {
			for (x = 0; x < profile.Width; x++) {
				set_pixel(&profile, BaseAddr, x, y, 0x00, 0x00, 0x00, 0xff);
			}
		}

		for (i = 0; i < profile.Width; i++) {
			set_pixel(&profile, BaseAddr, i, 100, 0xff, 0x00, 0x00, 0xff);
		}
		for (i = 0; i < profile.Width; i++) {
			set_pixel(&profile, BaseAddr, i, 105, 0x00, 0xff, 0x00, 0xff);
		}
		for (i = 0; i < profile.Width; i++) {
			set_pixel(&profile, BaseAddr, i, 110, 0x00, 0x00, 0xff, 0xff);
		}
		for (i = 0; i < profile.Width; i++) {
			set_pixel(&profile, BaseAddr, i, 130, 0xff, 0xff, 0xff, 0xff);
		}

	} else {
		fprintf(stderr, "Error RUAMap failed\n");
	}
	rv = RUAUnLock(context->pRUA, pic_luma_addr, pic_luma_size);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error RUAUnLock! %d\n", rv);
		return rv;
	}
	printf("Waiting\n");
	while(1);

	return RM_OK;
}

int main(void)
{
	app_rua_context_t context;
	RMstatus rv;

	printf("smptest\n");

	rv = osd_init(&context);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error failed osd_init! %d\n", rv);
		return rv;
	}

	printf("Init Success\n");

	rv = create_osd_buffer(&context);
	if (RMFAILED(rv)) {
		fprintf(stderr, "Error failed create_osd_buffer! %d\n", rv);
		return rv;
	}

	cleanup(&context);
	printf("Clean up finished\n");

	return 0;
}
