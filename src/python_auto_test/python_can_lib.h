#ifndef PYTHON_CAN_LIB_H__
#define PYTHON_CAN_LIB_H__
/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2015  AS <parai@foxmail.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
/* ============================ [ INCLUDES  ] ====================================================== */
#ifdef LINUX_PLATFORM__
#include <unistd.h>
#endif

#ifdef WIN_PLATFORM__
#include <windows.h>
#endif

#include <stdbool.h>
#include <stdint.h>
/* ============================ [ MACROS    ] ====================================================== */
#define CAN_DEVICE_NAME_SIZE 32

#ifdef LINUX_PLATFORM__
#define TRUE (true)
#define FALSE (false)

/* ============================ [ TYPES     ] ====================================================== */
typedef bool boolean; 
#endif

typedef void (*can_device_rx_notification_t)(uint32_t busid, uint32_t canid, 
        uint32_t dlc, uint8_t* data);

typedef boolean (*can_device_probe_t)(uint32_t busid, uint32_t port, 
        uint32_t baudrate, can_device_rx_notification_t rx_notification);
typedef boolean (*can_device_write_t)(uint32_t port, uint32_t canid, 
        uint32_t dlc, uint8_t* data);
typedef void (*can_device_close_t)(uint32_t port);

typedef struct
{
	char name[CAN_DEVICE_NAME_SIZE];
	can_device_probe_t probe;
	can_device_write_t write;
	can_device_close_t close;
}Can_DeviceOpsType;

typedef struct
{
	uint32_t busid; /* busid of this driver */
	uint32_t port;  /* busid --> port of this device */
	const Can_DeviceOpsType* ops;
}Can_DeviceType;

// can open 
extern int can_open(unsigned long busid, const char* device_name, 
        unsigned long port, unsigned long baudrate);
// can write
extern int can_write(unsigned long busid, unsigned long canid,
        unsigned long dlc, unsigned char* data);
// can read
extern int can_read(unsigned long busid, unsigned long canid, 
        unsigned long* p_canid, unsigned long *dlc,unsigned char* data);

// can lib open
extern void python_can_lib_open(void);
// can lib close
extern void python_can_lib_close(void);

#endif /* PYTHON_CAN_LIB_H__ */
