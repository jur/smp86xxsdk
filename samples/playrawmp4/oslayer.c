#include <stdlib.h>
#include <string.h>

#include "rua.h"

int verbose_stderr = 1;

void *RMMalloc(RMuint32 size)
{
	return malloc(size);
}

void RMFree(void *addr)
{
	free(addr);
}

void *RMMemset(void *addr, RMuint8 c, RMuint32 size)
{
	return memset(addr, c, size);
}

void *RMMemcpy(void *dst, const void *src, RMuint32 size)
{
	return memcpy(dst, src, size);
}
