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

#define MAX_EVENTS 32

/** Print debug message. */
#if 0
#define DPRINTF(args...) printf(args)
#else
#define DPRINTF(args...) do { } while(0)
#endif

/** Print error message. */
#define EPRINTF(format, args...) fprintf(stderr, "librua: " __FILE__ ":%d: Error: " format, __LINE__, ## args)

struct RUA {
	int fd;
	struct LLAD *pLlad;
	struct GBUS *pGbus;
};

struct RUABufferPool {
	int fd;
	struct dmapool *pDmapool;
	RMuint32 poolid;
	RMuint32 moduleid;
	RMuint32 buffersize;
	enum RUAPoolDirection direction;
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
		EPRINTF("Failed llad_open().\n");
		return RM_ERROR;
	}
	pRua->pGbus = gbus_open(pRua->pLlad);
	if (pRua->pGbus == NULL) {
		llad_close(pRua->pLlad);
		pRua->pLlad = NULL;
		free(pRua);
		pRua = NULL;
		EPRINTF("Failed gbus_open().\n");
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
		EPRINTF("Failed to open '%s'.\n", device);
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
		EPRINTF("Failed ioctl in RUASetProperty(%p, %d, %d, %p, %d, %d) with rv = %d.\n",
			pRua, ModuleID, PropertyID, pValue, ValueSize, TimeOut_us, rv);
		return RM_ERROR;
	}
	rv = buffer[6];
	if (rv != RM_PENDING) {
		if (rv == RM_OK) {
			DPRINTF("RUASetProperty(%p, %d, %d, %p, %d, %d) success with rv = %d.\n",
				pRua, ModuleID, PropertyID, pValue, ValueSize, TimeOut_us, rv);
		} else {
			EPRINTF("Failed RUASetProperty(%p, %d, %d, %p, %d, %d) with rv = %d.\n",
				pRua, ModuleID, PropertyID, pValue, ValueSize, TimeOut_us, rv);
		}
		return rv;
	}
	if (TimeOut_us == 0) {
		return rv;
	}
	/* The original librua also does not implement this. */
	EPRINTF("Timeout not implemented in %s.\n", __FUNCTION__);
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
		EPRINTF("Failed ioctl in RUAGetProperty(%p, %d, %d, %p, %d) with rv = %d.\n",
			pRua, ModuleID, PropertyID, pValue, ValueSize, rv);
	} else {
		rv = buffer[6];
		if (rv == RM_OK) {
			DPRINTF("RUAGetProperty(%p, %d, %d, %p, %d) success with rv = %d.\n",
				pRua, ModuleID, PropertyID, pValue, ValueSize, rv);
		} else {
			EPRINTF("Failed RUAGetProperty(%p, %d, %d, %p, %d) with rv = %d.\n",
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
		EPRINTF("Failed ioctl in RUAExchangeProperty(%p, %d, %d, %p, %d, %p, %d) with rv = %d.\n",
			pRua, ModuleID, PropertyID, pValueIn, ValueInSize, pValueOut, ValueOutSize, rv);
	} else {
		rv = buffer[6];
		if (rv == RM_OK) {
			DPRINTF("RUAExchangeProperty(%p, %d, %d, %p, %d, %p, %d) success with rv = %d.\n",
				pRua, ModuleID, PropertyID, pValueIn, ValueInSize, pValueOut, ValueOutSize, rv);
		} else {
			EPRINTF("Failed RUAExchangeProperty(%p, %d, %d, %p, %d, %p, %d) with rv = %d.\n",
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
		EPRINTF("Failed %s for 0x%08x size 0x%08x.\n", __FUNCTION__, address, size);
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
		EPRINTF("%s failed to free memory at 0x%08x.\n", __FUNCTION__, ptr);
	}
}

RMstatus RUAWaitForMultipleEvents(struct RUA *pRua, struct RUAEvent *pEvents, RMuint32 EventCount, RMuint32 TimeOut_us, RMuint32 *pEventNum)
{
	RMuint32 buffer[2 + MAX_EVENTS + 1];
	RMuint32 i;
	int rv;
	int eventnr;
#if 0
	struct timeval tv;
	gettimeofday(&tv, NULL);
#endif

	if (EventCount >= 32) {
		EPRINTF("RUAWaitForMultipleEvents(%p, %p, %u, %uus, %p) rv = RM_ERROR\n",
		pRua, pEvents, EventCount, TimeOut_us, pEventNum);
		return RM_ERROR;
	}
	memset(buffer, 0, sizeof(buffer));
	buffer[0] = TimeOut_us;
	buffer[1] = EventCount;
	for (i = 0; i < EventCount; i++) {
		buffer[2 + 2 * i] = pEvents[i].ModuleID;
		buffer[3 + 2 * i] = pEvents[i].Mask;
	}
	rv = ioctl(pRua->fd, 0xC10C4507, buffer);
	if (rv < 0) {
		EPRINTF("RUAWaitForMultipleEvents(%p, %p, %u, %uus, %p) rv = RM_ERROR, ioctl failed rv = %d\n",
		pRua, pEvents, EventCount, TimeOut_us, pEventNum, rv);
		return RM_ERROR;
	}
	eventnr = buffer[2 + MAX_EVENTS];
	if (eventnr == -1) {
		return RM_PENDING;
	}
	pEvents[eventnr].Mask = buffer[3 + 2 * eventnr];
	if (pEventNum != NULL) {
		*pEventNum = eventnr;
	}
	DPRINTF("RUAWaitForMultipleEvents(%p, %p, %u, %uus, =%d) rv = RM_OK\n",
		pRua, pEvents, EventCount, TimeOut_us, eventnr);
#if 0
	gettimeofday(&tv, NULL);
#endif
	return RM_OK;
}

static void release_receive_pool(struct RUABufferPool *pBufferPool)
{
	int ret;
	RMuint32 iocmd[4];

	do {
		RMuint8 *buffer;
		RMuint32 timeout_microsecond = 0;
		RMuint32 physical_address;

		buffer = dmapool_get_buffer(pBufferPool->pDmapool, &timeout_microsecond);
		if (buffer == NULL) {
			return;
		}
		physical_address = dmapool_get_physical_address(pBufferPool->pDmapool, buffer, 0);
		iocmd[0] = pBufferPool->moduleid;
		iocmd[1] = pBufferPool->poolid;
		iocmd[2] = physical_address;
		iocmd[3] = pBufferPool->buffersize;
		ret = ioctl(pBufferPool->fd, 0x40184506, iocmd);
	} while (ret >= 0);

	dmapool_release(pBufferPool->pDmapool, iocmd[2]);
}

RMstatus RUAOpenPool(struct RUA *pRua, RMuint32 ModuleID, RMuint32 BufferCount, RMuint32 log2BufferSize, enum RUAPoolDirection direction, struct RUABufferPool **ppBufferPool)
{
	struct RUABufferPool *pBufferPool;
	RMuint32 modid = ModuleID | 0x80000000;

	pBufferPool = malloc(sizeof(*pBufferPool));
	if (pBufferPool == NULL) {
		return RM_FATALOUTOFMEMORY;
	}
	pBufferPool->pDmapool = dmapool_open(pRua->pLlad, NULL, BufferCount, log2BufferSize);
	if (pBufferPool->pDmapool == NULL) {
		free(pBufferPool);
		pBufferPool = NULL;
		EPRINTF("RUAOpenPool(%p, %u, %u, %u, %u, %p) failed with RM_ERROR\n", pRua, ModuleID, BufferCount, log2BufferSize, direction, ppBufferPool);
		return RM_ERROR;
	}
	pBufferPool->poolid = dmapool_get_id(pBufferPool->pDmapool);
	pBufferPool->fd = pRua->fd;
	pBufferPool->buffersize = 1 << log2BufferSize;
	pBufferPool->direction = direction;
	pBufferPool->moduleid = (modid & 0x7FFFFFFF);
	if (pBufferPool->direction != RUA_POOL_DIRECTION_RECEIVE) {
		*ppBufferPool = pBufferPool;
		return RM_OK;
	}
	if (pBufferPool->moduleid == 0) {
		dmapool_close(pBufferPool->pDmapool);
		pBufferPool->pDmapool = NULL;
		free(pBufferPool);
		pBufferPool = NULL;
		EPRINTF("RUAOpenPool(%p, %u, %u, %u, %u, %p) failed with RM_ERROR (moduleid)\n", pRua, ModuleID, BufferCount, log2BufferSize, direction, ppBufferPool);
		return RM_ERROR;
	}
	release_receive_pool(pBufferPool);
	*ppBufferPool = pBufferPool;
	return RM_OK;
}

RMstatus RUAClosePool(struct RUABufferPool *pBufferPool)
{

	dmapool_close(pBufferPool->pDmapool);
	free(pBufferPool);
	pBufferPool = NULL;

	return RM_OK;
}

RMstatus RUASetAddressID(struct RUA *pRua, RMuint32 address, RMuint32 ID)
{
	RMuint32 iocmd[2];
	int ret;

	iocmd[0] = address;
	iocmd[1] = ID;
	ret = ioctl(pRua->fd, 0x4008450c, iocmd);
	if (ret < 0) {
		EPRINTF("RUASetAddressID(%p, 0x%08x, %u) failed with RM_ERROR\n",
			pRua, address, ID);
		return RM_ERROR;
	} else {
		return RM_OK;
	}
}

RMuint32 RUAGetAddressID(struct RUA *pRua, RMuint32 ID)
{
	RMuint32 iocmd[2];
	int ret;

	iocmd[0] = 0;
	iocmd[1] = ID;
	ret = ioctl(pRua->fd, 0x4008450d, iocmd);
	if (ret < 0) {
		EPRINTF("RUAGetAddressID(%p, %u) failed with ret = %d\n",
			pRua, ID, ret);
		return 0;
	} else {
		if (iocmd[0] == 0) {
			EPRINTF("RUAGetAddressID(%p, %u) failed with 0\n",
				pRua, ID);
		}
		return iocmd[0];
	}
#if 0
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);
	return RM_ERROR;
#endif
}

RMstatus RUAGetBuffer(struct RUABufferPool *pBufferPool, RMuint8 **ppBuffer, RMuint32 TimeOut_us)
{
	RMuint8 *buffer;
	
	buffer = dmapool_get_buffer(pBufferPool->pDmapool, &TimeOut_us);
	*ppBuffer = buffer;
	if (buffer == NULL) {
		return RM_PENDING;
	} else {
		return RM_OK;
	}

}

RMstatus RUASendData(struct RUA *pRua, RMuint32 ModuleID, struct RUABufferPool *pBufferPool, RMuint8 *pData, RMuint32 DataSize, void *pInfo, RMuint32 InfoSize)
{
	RMuint32 physical_address;
	RMstatus rv;
	RMuint32 iocmd[6];
	int ret;

	physical_address = dmapool_get_physical_address(pBufferPool->pDmapool, pData, DataSize);
	rv = dmapool_acquire(pBufferPool->pDmapool, physical_address);
	if (rv != RM_OK) {
		return rv;
	}
	dmapool_flush_cache(pBufferPool->pDmapool, physical_address, DataSize);
	iocmd[0] = ModuleID;
	iocmd[1] = pBufferPool->poolid;
	iocmd[2] = physical_address;
	iocmd[3] = DataSize;
	iocmd[4] = (RMuint32) pInfo;
	iocmd[5] = InfoSize;
	ret = ioctl(pRua->fd, 0x40184504, iocmd);
	if (ret >= 0) {
		return RM_OK;
	}
	rv = dmapool_release(pBufferPool->pDmapool, iocmd[2]);
	if (rv == RM_OK) {
		return RM_PENDING;
	}
	return rv;
}

RMstatus RUAReleaseBuffer(struct RUABufferPool *pBufferPool, RMuint8 *pBuffer)
{
	RMuint32 physical_address;
	RMstatus rv;

	physical_address = dmapool_get_physical_address(pBufferPool->pDmapool, pBuffer, 0);
	if (physical_address == 0) {
		return RM_ERROR;
	}
	rv = dmapool_release(pBufferPool->pDmapool, physical_address);
	if (rv == RM_OK) {
		return RM_OK;
	}
	if (pBufferPool->direction != RUA_POOL_DIRECTION_RECEIVE) {
		return rv;
	}
	release_receive_pool(pBufferPool);
	return RM_OK;
}

RMuint32 RUAGetAvailableBufferCount(struct RUABufferPool *pBufferPool)
{
	RMuint32 rv;
	
	rv = dmapool_get_available_buffer_count(pBufferPool->pDmapool);

	return rv;
}
