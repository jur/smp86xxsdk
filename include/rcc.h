#ifndef _RCC_H_
#define _RCC_H_

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

#include <zyxel_dma2500_rcc.h>

/**
 * Open remote control.
 *
 * @¶eturns File descriptor for remote control
 */
int RCCOpen(void);

/**
 * Get control code of pressed button on remote control.
 *
 * @param fd File descriptor returned by RCCOpen()
 * @param usec Time to wait until button is pressed on RC (microseconds).
 *
 * @returns Remote control code
 * @retval 0 when no button was pressed.
 */
int RCCGetKey(int fd, int usec);

/**
 * Close remote control.
 *
 * @param fd File descriptor returned by RCCOpen()
 */
void RCCClose(int fd);

#endif
