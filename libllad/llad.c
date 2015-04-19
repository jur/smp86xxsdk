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
#include <sys/mman.h>

#include "llad.h"

#define DEVICE_MUM "/dev/mum%s"

#define LLAD_LOCK_AREA 0x46
#define LLAD_GET_AREA 0x47
#define LLAD_UNLOCK_AREA 0x48
#define LLAD_GET_CONFIG 0x49
#define LLAD_MAP_AREA 0x4B

#define LLAD_MAX_AREAS 512

#define PAGSIZE 0x1000 // TBD: SHould be dynamic

/** Print debug message. */
#if 0
#define DPRINTF(args...) printf(args)
#else
#define DPRINTF(args...) do { } while(0)
#endif

/** Print error message. */
#define EPRINTF(format, args...) fprintf(stderr, "libllad: " __FILE__ ":%d: Error: " format, __LINE__, ## args)

struct LLAD {
	int fd;
};

struct GBUS {
	int fd;
	RMuint8 *areaaddress[LLAD_MAX_AREAS];
	RMuint32 areasize[LLAD_MAX_AREAS];
	RMuint32 numberOfAreas;
	RMuint32 size;
};

typedef struct {
	RMuint32 numberOfAreas;
	RMuint32 size;
} llad_config_t;

struct LLAD *llad_open(const char *chipname)
{
	char device[32];
	struct LLAD *pLlad = NULL;

	pLlad = malloc(sizeof(*pLlad));
	if (pLlad != NULL) {
		snprintf(device, sizeof(device), DEVICE_MUM, chipname);
		pLlad->fd = open(device, O_RDWR);
		if (pLlad->fd == -1) {
			free(pLlad);
			pLlad = NULL;
			EPRINTF("Failed to open '%s'\n", device);
		}
	}
	return pLlad;
}

void llad_close(struct LLAD *pLlad)
{
	if (pLlad != NULL) {
		if (pLlad->fd != -1) {
			close(pLlad->fd);
			pLlad->fd = -1;
		}
		free(pLlad);
		pLlad = NULL;
	}
}

struct GBUS *gbus_open(struct LLAD *pLlad)
{
	struct GBUS *pGbus = NULL;

	pGbus = malloc(sizeof(*pGbus));
	if (pGbus != NULL) {
		llad_config_t cfg;
		int rv;

		memset(pGbus, 0, sizeof(*pGbus));
		pGbus->fd = pLlad->fd;

		memset(&cfg, 0, sizeof(cfg));
		rv = ioctl(pGbus->fd, LLAD_GET_CONFIG, &cfg);
		if (rv == 0) {
			RMuint32 i;

			DPRINTF("LLAD_GET_CONFIG numberOfAreas 0x%08x\n", cfg.numberOfAreas);
			DPRINTF("LLAD_GET_CONFIG size 0x%08x\n", cfg.size);
			if (cfg.numberOfAreas > LLAD_MAX_AREAS) {
				pGbus->numberOfAreas = LLAD_MAX_AREAS;
				EPRINTF("Maximum %d areas supported instead of %d.\n", LLAD_MAX_AREAS, pGbus->numberOfAreas);
			} else {
				pGbus->numberOfAreas = cfg.numberOfAreas;
			}
			pGbus->size = cfg.size;
			for (i = 0; i < pGbus->numberOfAreas; i++) {
				pGbus->areaaddress[i] = NULL;
				pGbus->areasize[i] = 0;
			}
			if (cfg.numberOfAreas == 0) {
				free(pGbus);
				pGbus = NULL;
				EPRINTF("Not enough areas.\n");
			}
		} else {
			free(pGbus);
			pGbus = NULL;
			perror("ioctl LLAD_GET_CONFIG failed");
		}
	}
	return pGbus;
}

RMstatus gbus_lock_area(struct GBUS *pGbus, RMuint32 *index, RMuint32 address, RMuint32 size, RMuint32 *count, RMuint32 *offset)
{
	RMuint32 lock[5];
	int rv;

	if (pGbus == NULL) {
		return RM_FATALINVALIDPOINTER;
	}
	if (pGbus->fd == -1) {
		return RM_FATALINVALIDPOINTER;
	}

	memset(lock, 0, sizeof(lock));
	lock[0] = address;
	lock[1] = size;
	lock[3] = *index;

	rv = ioctl(pGbus->fd, LLAD_LOCK_AREA, lock);

	if (rv != 0) {
		return RM_ERROR;
	}
	*index = lock[3];
	*count = lock[4];
	*offset = lock[2];
	return RM_OK;
}

RMstatus gbus_unlock_region(struct GBUS *pGbus, RMuint32 index)
{
	RMuint32 buffer[1];
	int rv;

	if (pGbus == NULL) {
		return RM_FATALINVALIDPOINTER;
	}
	if (pGbus->fd == -1) {
		return RM_FATALINVALIDPOINTER;
	}

	memset(buffer, 0, sizeof(buffer));
	buffer[0] = index;

	rv = ioctl(pGbus->fd, LLAD_UNLOCK_AREA, buffer);

	if (rv != 0) {
		return RM_ERROR;
	}
	return RM_OK;
}

RMstatus gbus_get_locked_area(struct GBUS *pGbus, RMuint32 address, RMuint32 size, RMuint32 *index, RMuint32 *count, RMuint32 *offset)
{
	RMuint32 buffer[5];
	int rv;

	if (pGbus == NULL) {
		return RM_FATALINVALIDPOINTER;
	}
	if (pGbus->fd == -1) {
		return RM_FATALINVALIDPOINTER;
	}

	memset(buffer, 0, sizeof(buffer));
	buffer[0] = address;
	buffer[1] = size;

	rv = ioctl(pGbus->fd, LLAD_GET_AREA, buffer);

	if (rv != 0) {
		return RM_ERROR;
	}
	*index = buffer[3];
	*count = buffer[4];
	*offset = buffer[2];
	return RM_OK;
}

RMuint8 *gbus_map_region(struct GBUS *pGbus, RMuint32 index, RMuint32 count)
{
	RMuint32 buffer[2];
	int rv;
	RMuint32 i;
	RMuint32 freecount;
	RMuint8 *ptr;

	if (pGbus == NULL) {
		return NULL;
	}
	if (pGbus->fd == -1) {
		return NULL;
	}

	memset(buffer, 0, sizeof(buffer));
	buffer[0] = index;

	rv = ioctl(pGbus->fd, LLAD_MAP_AREA, buffer);

	if (rv != 0) {
		EPRINTF("%s failed LLAD_MAP_AREA.\n", __FUNCTION__);
		perror("ioctl failed\n");
		return NULL;
	}
	if (index >= pGbus->numberOfAreas) {
		EPRINTF("%s index %d to large for %d.\n", __FUNCTION__, index, pGbus->numberOfAreas);
		return NULL;
	}
	if (pGbus->numberOfAreas == 0) {
		EPRINTF("%s number of areas are %d.\n", __FUNCTION__, pGbus->numberOfAreas);
		return NULL;
	}

	freecount = 0;
	for (i = 0; i < pGbus->numberOfAreas; i++) {
		if (pGbus->areaaddress[i] == NULL) {
			freecount = freecount + 1;
		}
	}
	if (freecount == 0) {
		return NULL;
	}
	ptr = mmap(NULL, ((uint64_t) buffer[1]) << 12, PROT_READ | PROT_WRITE, MAP_SHARED, pGbus->fd, 0x3000000ULL | ((uint64_t) index) << 12);
	if (ptr == MAP_FAILED) {
		EPRINTF("%s map failed offset 0x%08x, size 0x%08x.\n", __FUNCTION__, buffer[1] << 12, 0x3000000 + (index << 12));
		perror("mmap failed\n");
		return NULL;
	}
	for (i = 0; i < pGbus->numberOfAreas; i++) {
		if (pGbus->areaaddress[i] == NULL) {
			pGbus->areaaddress[i] = ptr;
			pGbus->areasize[i] = buffer[1];
		}
	}
	return ptr;
}

void gbus_unmap_region(struct GBUS *pGbus, RMuint8 *address, RMuint32 size)
{
	RMuint32 i;

	if (pGbus == NULL) {
		EPRINTF("%s bad parameter pGbus.\n", __FUNCTION__);
		return;
	}
	if (pGbus->fd == -1) {
		EPRINTF("%s not initialized.\n", __FUNCTION__);
		return;
	}
	for (i = 0; i < pGbus->numberOfAreas; i++) {
		if (pGbus->areaaddress[i] < address) {
			if ((address - pGbus->areaaddress[i]) < PAGSIZE) {
				if (((pGbus->areasize[i] << 12) - size) < 0x2000) {
					munmap(pGbus->areaaddress[i], pGbus->areasize[i] << 12);
					pGbus->areaaddress[i] = NULL;
					pGbus->areasize[i] = 0;
					return;
				}
			}
		}
	}
	EPRINTF("%s no unmap for %p size %08x.\n", __FUNCTION__, address, size);
}

void gbus_close(struct GBUS *pGbus)
{
	if (pGbus != NULL) {
		RMuint32 i;

		for (i = 0; i < pGbus->numberOfAreas; i++) {
			if (pGbus->areaaddress[i] != 0) {
				munmap(pGbus->areaaddress[i], pGbus->areasize[i] << 12);
			}
		}
		free(pGbus);
		pGbus = NULL;
	}
}

struct dmapool *dmapool_open(struct LLAD *h, void *area, RMuint32 buffercount, RMuint32 log2_buffersize)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return NULL;
}

void dmapool_close(struct dmapool *h)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);
}

RMuint32 dmapool_get_id(struct dmapool *h)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return 0;
}

RMuint8 *dmapool_get_buffer(struct dmapool *h, RMuint32 *timeout_microsecond)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return NULL;
}

RMuint32 dmapool_get_physical_address(struct dmapool *h, RMuint8 *ptr, RMuint32 size)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return 0;
}

RMstatus dmapool_release(struct dmapool *h, RMuint32 physical_address)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

RMstatus dmapool_acquire(struct dmapool *h, RMuint32 physical_address)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return RM_NOTIMPLEMENTED;
}

void dmapool_flush_cache(struct dmapool *h, RMuint32 physical_address, RMuint32 size)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);
}

RMuint32 dmapool_get_available_buffer_count(struct dmapool *h)
{
	EPRINTF("Function %s is not implemented.\n", __FUNCTION__);

	return 0;
}
