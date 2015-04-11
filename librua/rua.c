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
#include "llad.h"

#define DEVICE_EM8 "/dev/em8xxx%u"

struct RUA {
	int fd;
	struct LLAD *pLlad;
	struct GBUS *pGbus;
};

RMstatus RUACreateInstance(struct RUA **ppRua, RMuint32 chipnr)
{
	char device[32];
	char chipname[32];
	struct RUA *pRua;

	if (ppRua == NULL) {
		return RM_FATALINVALIDPOINTER;
	}
	pRua = malloc(sizeof(*pRua));
	if (pRua == NULL) {
		return RM_FATALOUTOFMEMORY;
	}
	memset(pRua, 0, sizeof(*pRua));
	pRua->fd = -1;

	snprintf(chipname, sizeof(chipname), "%u", chipnr);
	pRua->pLlad = llad_open(chipname);
	if (pRua->pLlad == NULL) {
		free(pRua);
		pRua = NULL;
		fprintf(stderr, "Error: Failed llad_open().\n");
		return RM_ERROR;
	}
	pRua->pGbus = gbus_open(pRua->pLlad);
	if (pRua->pGbus == NULL) {
		llad_close(pRua->pLlad);
		pRua->pLlad = NULL;
		free(pRua);
		pRua = NULL;
		fprintf(stderr, "Error: Failed gbus_open().\n");
		return RM_ERROR;
	}

	snprintf(device, sizeof(device), DEVICE_EM8, chipnr);
	pRua->fd = open(device, O_RDWR);
	if (pRua->fd < 0) {
		gbus_close(pRua->pGbus);
		pRua->pGbus = NULL;
		llad_close(pRua->pLlad);
		pRua->pLlad = NULL;
		free(pRua);
		pRua = NULL;
		fprintf(stderr, "Error: Failed to open '%s'.\n", device);
		return RM_ERROR;
	}

	*ppRua = pRua;
	return RM_OK;
}

RMstatus RUADestroyInstance(struct RUA *pRua)
{
	if (pRua->fd >= 0) {
		close(pRua->fd);
		pRua->fd = -1;
	}
	if (pRua->pLlad != NULL) {
		llad_close(pRua->pLlad);
		pRua->pLlad = NULL;
	}
	free(pRua);
	pRua = NULL;

	return RM_OK;
}

RMstatus RUASetProperty(struct RUA *pRua, RMuint32 ModuleID, RMuint32 PropertyID, void *pValue, RMuint32 ValueSize, RMuint32 TimeOut_us)
{
	RMuint32 buffer[7];
	int rv;

	memset(buffer, 0, sizeof(buffer));
	buffer[0] = ModuleID;
	buffer[1] = PropertyID;
	buffer[2] = (RMuint32) pValue;
	buffer[3] = ValueSize;
	
	rv = ioctl(pRua->fd, 0xc01c4501, buffer);
	if (rv < 0) {
		fprintf(stderr, "Error: Failed ioctl in RUASetProperty(%p, %d, %d, %p, %d, %d) with rv = %d.\n",
			pRua, ModuleID, PropertyID, pValue, ValueSize, TimeOut_us, rv);
		return RM_ERROR;
	}
	rv = buffer[6];
	if (rv != RM_PENDING) {
		if (rv == RM_OK) {
			printf("RUASetProperty(%p, %d, %d, %p, %d, %d) success with rv = %d.\n",
				pRua, ModuleID, PropertyID, pValue, ValueSize, TimeOut_us, rv);
		} else {
			fprintf(stderr, "Error: Failed RUASetProperty(%p, %d, %d, %p, %d, %d) with rv = %d.\n",
				pRua, ModuleID, PropertyID, pValue, ValueSize, TimeOut_us, rv);
		}
		return rv;
	}
	if (TimeOut_us == 0) {
		return rv;
	}
	/* The original librua also does not implement this. */
	fprintf(stderr, "Error: Timeout not implemented in %s.\n", __FUNCTION__);
	return rv;
}

RMstatus RUAGetProperty(struct RUA *pRua, RMuint32 ModuleID, RMuint32 PropertyID, void *pValue, RMuint32 ValueSize)
{
	RMuint32 buffer[7];
	int rv;
	
	memset(buffer, 0, sizeof(buffer));
	buffer[0] = ModuleID;
	buffer[1] = PropertyID;
	buffer[4] = (RMuint32) pValue;
	buffer[5] = ValueSize;
	
	rv = ioctl(pRua->fd, 0xc01c4502, buffer);
	if (rv < 0) {
		fprintf(stderr, "Error: Failed ioctl in RUAGetProperty(%p, %d, %d, %p, %d) with rv = %d.\n",
			pRua, ModuleID, PropertyID, pValue, ValueSize, rv);
	} else {
		rv = buffer[6];
		if (rv == RM_OK) {
			printf("RUAGetProperty(%p, %d, %d, %p, %d) success with rv = %d.\n",
				pRua, ModuleID, PropertyID, pValue, ValueSize, rv);
		} else {
			fprintf(stderr, "Error: Failed RUAGetProperty(%p, %d, %d, %p, %d) with rv = %d.\n",
				pRua, ModuleID, PropertyID, pValue, ValueSize, rv);
		}
		return rv;
	}
	return RM_ERROR;
}

RMstatus RUAExchangeProperty(struct RUA *pRua, RMuint32 ModuleID, RMuint32 PropertyID, void *pValueIn, RMuint32 ValueInSize, void *pValueOut, RMuint32 ValueOutSize)
{
	RMuint32 buffer[7];
	int rv;
	
	memset(buffer, 0, sizeof(buffer));
	buffer[0] = ModuleID;
	buffer[1] = PropertyID;
	buffer[2] = (RMuint32) pValueIn;
	buffer[3] = ValueInSize;
	buffer[4] = (RMuint32) pValueOut;
	buffer[5] = ValueOutSize;
	
	rv = ioctl(pRua->fd, 0xc01c4503, buffer);
	if (rv < 0) {
		fprintf(stderr, "Error: Failed ioctl in RUAExchangeProperty(%p, %d, %d, %p, %d, %p, %d) with rv = %d.\n",
			pRua, ModuleID, PropertyID, pValueIn, ValueInSize, pValueOut, ValueOutSize, rv);
	} else {
		rv = buffer[6];
		if (rv == RM_OK) {
			printf("RUAExchangeProperty(%p, %d, %d, %p, %d, %p, %d) success with rv = %d.\n",
				pRua, ModuleID, PropertyID, pValueIn, ValueInSize, pValueOut, ValueOutSize, rv);
		} else {
			fprintf(stderr, "Error: Failed RUAExchangeProperty(%p, %d, %d, %p, %d, %p, %d) with rv = %d.\n",
				pRua, ModuleID, PropertyID, pValueIn, ValueInSize, pValueOut, ValueOutSize, rv);
		}
		return rv;
	}
	return RM_ERROR;
}

RMstatus RUAResetEvent(struct RUA *pRua, struct RUAEvent *pEvent)
{
	int rv;
	RMuint32 buffer[2];

	buffer[0] = pEvent->ModuleID;
	buffer[1] = pEvent->Mask;

	rv = ioctl(pRua->fd, 0x40084508, pEvent);
	if (rv < 0) {
		return RM_ERROR;
	} else {
		return RM_OK;
	}

	return rv;
}

RMstatus RUALock(struct RUA *pRua, RMuint32 address, RMuint32 size)
{
	RMstatus rv;
	RMuint32 buffer[2];
	RMuint32 index = 0;
	RMuint32 count = 0;
	RMuint32 offset = 0;
	RMuint32 dramtype = 0;

	rv = gbus_lock_area(pRua->pGbus, &index, address, size, &count, &offset);
	if (rv != RM_OK) {
		return rv;
	}

	if ((address - 0x10000000) < 0x0FFFFFFF) {
		dramtype = 0;
	} else if ((address - 0x20000000) < 0x1FFFFFFF) {
		dramtype = 1;
	} else {
		return RM_FATALMEMORYCORRUPTED;
	}

	memset(buffer, 0, sizeof(buffer));
	rv = RUAGetProperty(pRua, EMHWLIB_MODULE(MM, dramtype), 4343, buffer, sizeof(buffer));
	if (rv != RM_OK) {
		return rv;
	}
	if (address < buffer[0]) {
		return RM_OK;
	}
	if (address < (buffer[0] + buffer[1])) {
		int ret;

		ret = ioctl(pRua->fd, 0x40044509, &address);
		if (ret == 0) {
			return RM_OK;
		}
	} else {
		return RM_OK;
	}
	return RM_ERROR;
}

RMuint8 *RUAMap(struct RUA *pRua, RMuint32 address, RMuint32 size)
{
	RMstatus rv;
	RMuint32 index = 0;
	RMuint32 count = 0;
	RMuint32 offset = 0;
	RMuint8 *p;
	
	rv = gbus_get_locked_area(pRua->pGbus, address, size, &index, &count, &offset);
	if (rv != RM_OK) {
		return NULL;
	}
	p = gbus_map_region(pRua->pGbus, index, count);
	if (p == NULL) {
		fprintf(stderr, "Error: Failed %s for 0x%08x size 0x%08x.\n", __FUNCTION__, address, size);
		return NULL;
	}
	return p + offset;
}

void RUAUnMap(struct RUA *pRua, RMuint8 *ptr, RMuint32 size)
{
	gbus_unmap_region(pRua->pGbus, ptr, size);
}

RMstatus RUAUnLock(struct RUA *pRua, RMuint32 address, RMuint32 size)
{
	RMstatus rv;
	RMuint32 index = 0;
	RMuint32 count = 0;
	RMuint32 offset = 0;
	RMuint32 dramtype = 0;
	RMuint32 i;
	RMuint32 buffer[2];
	
	rv = gbus_get_locked_area(pRua->pGbus, address, size, &index, &count, &offset);
	if (rv != RM_OK) {
		return rv;
	}
	if ((address - 0x10000000) < 0x0FFFFFFF) {
		dramtype = 0;
	} else if ((address - 0x20000000) < 0x1FFFFFFF) {
		dramtype = 1;
	} else {
		return RM_FATALMEMORYCORRUPTED;
	}
	for (i = index; i < (count + index); i++) {
		gbus_unlock_region(pRua->pGbus, i);
	}
	memset(buffer, 0, sizeof(buffer));
	rv = RUAGetProperty(pRua, EMHWLIB_MODULE(MM, dramtype), 4343, buffer, sizeof(buffer));
	if (rv != RM_OK) {
		return rv;
	}
	if (address < buffer[0]) {
		return RM_OK;
	}
	if (address < (buffer[0] + buffer[1])) {
		int ret;

		ret = ioctl(pRua->fd, 0x4004450a, &address);
		if (ret == 0) {
			return RM_OK;
		}
	} else {
		return RM_OK;
	}
	return RM_ERROR;
}

RMuint32 RUAMalloc(struct RUA *pRua, RMuint32 dramIndex, enum RUADramType dramtype, RMuint32 size)
{
	RMstatus rv;
	RMuint32 buffer[2];
	RMuint32 result[1];

	memset(buffer, 0, sizeof(buffer));
	memset(result, 0, sizeof(result));

	switch (dramtype) {
		case RUA_DRAM_UNPROTECTED:
			break;

		case RUA_DRAM_ZONEA:
			dramIndex += 2;
			break;

		case RUA_DRAM_ZONEB:
			dramIndex += 4;
			break;

		default:
			return 0;
	}
	if (size == 0) {
		return 0;
	}
	buffer[0] = dramtype;
	buffer[1] = size;
	rv = RUAExchangeProperty(pRua, EMHWLIB_MODULE(MM, dramIndex), RMMMPropertyID_Malloc, buffer, sizeof(buffer), result, sizeof(result));
	if (rv != RM_OK) {
		return 0;
	}
	return result[0];
}

void RUAFree(struct RUA *pRua, RMuint32 ptr)
{
	RMuint32 buffer[1];
	RMstatus rv;

	buffer[0] = ptr;
	rv = RUASetProperty(pRua, EMHWLIB_MODULE(MM, 0), RMMMPropertyID_Free, buffer, sizeof(buffer), 0);
	if (rv != RM_OK) {
		fprintf(stderr, "Error: %s failed to free memory at 0x%08x.\n", __FUNCTION__, ptr);
	}
}

RMstatus RUAWaitForMultipleEvents(struct RUA *pRua, struct RUAEvent *pEvents, RMuint32 EventCount, RMuint32 TimeOut_us, RMuint32 *pEventNum)
{
	fprintf(stderr, "Error: %s is not implemented.\n", __FUNCTION__);
	return RM_NOTIMPLEMENTED;
}
