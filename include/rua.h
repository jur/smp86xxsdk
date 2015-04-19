#ifndef _RUA_H_
#define _RUA_H_

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

#include <rua_common.h>
#include <zyxel_dma2500.h>

#define EMHWLIB_MODULE(category, index) ((category & 0xFF) | ((index << 8) & 0xFF00))

struct RUA;
struct RUABufferPool;

struct RUAEvent {                                                               
	RMuint32 ModuleID;
	RMuint32 Mask;
};

enum RUADramType {
	RUA_DRAM_UNPROTECTED = 57,
	RUA_DRAM_ZONEA,
	RUA_DRAM_ZONEB
};

enum RUAPoolDirection {
	RUA_POOL_DIRECTION_SEND = 54,
	RUA_POOL_DIRECTION_RECEIVE = 55,
};

void *RMMalloc(RMuint32 size);
void RMFree(void *addr);
void *RMMemset(void *addr, RMuint8 c, RMuint32 size);
void *RMMemcpy(void *dst, const void *src, RMuint32 size);

RMstatus RUACreateInstance(struct RUA **rua, RMuint32 chipnr);
RMstatus RUADestroyInstance(struct RUA *pRua);
RMstatus RUASetProperty(struct RUA *pRua, RMuint32 ModuleID, RMuint32 PropertyID, void *pValue, RMuint32 ValueSize, RMuint32 TimeOut_us);
RMstatus RUAGetProperty(struct RUA *pRua, RMuint32 ModuleID, RMuint32 PropertyID, void *pValue, RMuint32 ValueSize);
RMstatus RUAExchangeProperty(struct RUA *pRua, RMuint32 ModuleID, RMuint32 PropertyID, void *pValueIn, RMuint32 ValueInSize, void *pValueOut, RMuint32 ValueOutSize);
RMstatus RUAResetEvent(struct RUA *pRua, struct RUAEvent *pEvent);
RMstatus RUAWaitForMultipleEvents(struct RUA *pRua, struct RUAEvent *pEvents, RMuint32 EventCount, RMuint32 TimeOut_us, RMuint32 *pEventNum);
RMstatus RUALock(struct RUA *pRua, RMuint32 address, RMuint32 size);
RMstatus RUAUnLock(struct RUA *pRua, RMuint32 address, RMuint32 size);
RMuint8 *RUAMap(struct RUA *pRua, RMuint32 address, RMuint32 size);
void RUAUnMap(struct RUA *pRua, RMuint8 *ptr, RMuint32 size);
RMuint32 RUAMalloc(struct RUA *pRua, RMuint32 dramIndex, enum RUADramType dramtype, RMuint32 size);
void RUAFree(struct RUA *pRua, RMuint32 ptr);
RMstatus RUAWaitForMultipleEvents(struct RUA *pRua, struct RUAEvent *pEvents, RMuint32 EventCount, RMuint32 TimeOut_us, RMuint32 *pEventNum);
RMstatus RUAOpenPool(struct RUA *pRua, RMuint32 ModuleID, RMuint32 BufferCount, RMuint32 log2BufferSize, enum RUAPoolDirection direction, struct RUABufferPool **ppBufferPool);
RMstatus RUAClosePool(struct RUABufferPool *pBufferPool);
RMstatus RUASetAddressID(struct RUA *pRua, RMuint32 address, RMuint32 ID);
RMuint32 RUAGetAddressID(struct RUA *pRua, RMuint32 ID);
RMstatus RUAGetBuffer(struct RUABufferPool *pBufferPool, RMuint8 **ppBuffer, RMuint32 TimeOut_us);
RMstatus RUASendData(struct RUA *pRua, RMuint32 ModuleID, struct RUABufferPool *pBufferPool, RMuint8 *pData, RMuint32 DataSize, void *pInfo, RMuint32 InfoSize);
RMstatus RUAReleaseBuffer(struct RUABufferPool *pBufferPool, RMuint8 *pBuffer);
RMuint32 RUAGetAvailableBufferCount(struct RUABufferPool *pBufferPool);

extern int verbose_stderr;
 
#endif
