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

#include <rcc.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>

int RCCOpen(void)
{
	int fd;

	fd = open(IR_DEVICE, O_RDONLY);
	if (fd < 0) {
		perror("Failed to open " IR_DEVICE);
	}
	return fd;
}

int RCCGetKey(int fd, int usec)
{
	unsigned int key = 0;

	if (fd >= 0) {
		fd_set rfds;
		struct timeval tv;
		int ret;

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		tv.tv_sec = 0;
		tv.tv_usec = usec;

		/* Wait until a button is pressed on the remote control. */
		ret = select(fd + 1, &rfds, NULL, NULL, &tv);
		if (ret) {
			/* Read the remote control code of the button. */
			ret = read(fd, &key, sizeof(key));
			if (ret != sizeof(key)) {
				fprintf(stderr, "Failed to read " IR_DEVICE " ret = %d.\n", ret);
				key = 0;
			}
		}
	}

	return key;
}

void RCCClose(int fd)
{
	close(fd);
}
