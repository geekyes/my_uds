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
#ifdef __LINUX__
/* ============================ [ INCLUDES  ] ====================================================== */
/* #include "Std_Types.h" */
/* #include "lascanlib.h" */
#include <sys/queue.h>
#include <pthread.h>
/* most of the code copy from https://github.com/linux-can/can-utils */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* #include <unistd.h> */

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
/* #include "asdebug.h" */

#include <linux/can.h>
#include <linux/can/raw.h>

#include "socket_can.h"

/* ============================ [ MACROS    ] ====================================================== */
/* virtual socket can
 * sudo modprobe vcan
 * sudo ip link add dev vcan0 type vcan
 * sudo ip link set up vcan0
 */
#ifndef CAN_MTU
#define CAN_MTU sizeof(struct can_frame)
#endif
/* ============================ [ TYPES     ] ====================================================== */
struct Can_SocketHandle_s
{
	uint32_t busid;
	uint32_t port;
	uint32_t baudrate;
	can_device_rx_notification_t rx_notification;
	int s; /* can raw socket */
	struct sockaddr_can addr;
	struct ifreq ifr;
	STAILQ_ENTRY(Can_SocketHandle_s) entry;
};
struct Can_SocketHandleList_s
{
	pthread_t rx_thread;
	volatile boolean   terminated;
	STAILQ_HEAD(,Can_SocketHandle_s) head;
};
/* ============================ [ DECLARES  ] ====================================================== */
/*
 * static boolean socket_probe(uint32_t busid,uint32_t port,uint32_t baudrate,can_device_rx_notification_t rx_notification);
 * static boolean socket_write(uint32_t port,uint32_t canid,uint32_t dlc,uint8_t* data);
 * static void socket_close(uint32_t port);
 */
static void * rx_daemon(void *);
/* ============================ [ DATAS     ] ====================================================== */
/*
 * const Can_DeviceOpsType can_socket_ops =
 * {
 *     .name = "socket",
 *     .probe = socket_probe,
 *     .close = socket_close,
 *     .write = socket_write,
 * };
 */
static struct Can_SocketHandleList_s* socketH = NULL;
/* ============================ [ LOCALS    ] ====================================================== */
static struct Can_SocketHandle_s* getHandle(uint32_t port)
{
	struct Can_SocketHandle_s *handle,*h;
	handle = NULL;
	if(NULL != socketH)
	{
		STAILQ_FOREACH(h,&socketH->head,entry)
		{
			if(h->port == port)
			{
				handle = h;
				break;
			}
		}
	}
	return handle;
}

boolean socket_probe(uint32_t busid,uint32_t port,uint32_t baudrate,can_device_rx_notification_t rx_notification)
{
	boolean rv = TRUE;;
	struct Can_SocketHandle_s* handle;
	if(NULL == socketH)
	{
		socketH = malloc(sizeof(struct Can_SocketHandleList_s));
		/* asAssert(socketH); */
		STAILQ_INIT(&socketH->head);

		socketH->terminated = TRUE;
	}

	handle = getHandle(port);

	if(handle)
	{
		/* ASWARNING("CAN socket port=%d is already on-line, no need to probe it again!\n",port); */
		rv = FALSE;
	}
	else
	{
		int s;
		struct sockaddr_can addr;
		struct ifreq ifr;
		/* open socket */
		if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
			perror("CAN socket : ");
			/* ASWARNING("CAN Socket port=%d open failed!\n",port); */
			rv = FALSE;
		}

		if( rv )
		{
			snprintf(ifr.ifr_name,IFNAMSIZ - 1,"vcan%d", port);
			ifr.ifr_name[IFNAMSIZ - 1] = '\0';
			ifr.ifr_ifindex = if_nametoindex(ifr.ifr_name);
			if (!ifr.ifr_ifindex) {
				perror("CAN socket if_nametoindex");
				/* ASWARNING("CAN Socket port=%d if_nametoindex failed!\n",port); */
				rv = FALSE;
			}
		}

		if( rv )
		{
			addr.can_family = AF_CAN;
			addr.can_ifindex = ifr.ifr_ifindex;

			/* disable default receive filter on this RAW socket */
			/* This is obsolete as we do not read from the socket at all, but for */
			/* this reason we can remove the receive list in the Kernel to save a */
			/* little (really a very little!) CPU usage.                          */
			/* setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0); */

			if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
				perror("CAN socket bind");
				/* ASWARNING("CAN Socket port=%d bind failed!\n",port); */
				rv = FALSE;
			}
		}

		if( rv )
		{	/* open port OK */
			handle = malloc(sizeof(struct Can_SocketHandle_s));
			/* asAssert(handle); */
			handle->busid = busid;
			handle->port = port;
			handle->baudrate = baudrate;
			handle->rx_notification = rx_notification;
			handle->s = s;
			memcpy(&(handle->addr),&addr,sizeof(addr));
			memcpy(&(handle->ifr),&ifr,sizeof(ifr));
			STAILQ_INSERT_TAIL(&socketH->head,handle,entry);
		}
		else
		{
			rv = FALSE;
		}
	}

	if( (TRUE == socketH->terminated) &&
		(FALSE == STAILQ_EMPTY(&socketH->head)) )
	{
		if( 0 == pthread_create(&(socketH->rx_thread),NULL,rx_daemon,NULL))
		{
			socketH->terminated = FALSE;
		}
		else
		{
			/* asAssert(0); */
		}
	}

	return rv;
}
boolean socket_write(uint32_t port,uint32_t canid,uint32_t dlc,uint8_t* data)
{
	boolean rv = TRUE;
	struct Can_SocketHandle_s* handle = getHandle(port);
	if(handle != NULL)
	{
		struct can_frame frame;
		frame.can_id = canid;
		frame.can_dlc = dlc;
		memcpy(frame.data,data,dlc);

		if (write(handle->s, &frame, CAN_MTU) != CAN_MTU) {
			perror("CAN socket write");
			/* ASWARNING("CAN Socket port=%d send message failed!\n",port); */
			rv = FALSE;
		}
	}
	else
	{
		rv = FALSE;
		/* ASWARNING("CAN Socket port=%d is not on-line, not able to send message!\n",port); */
	}

	return rv;
}
void socket_close(uint32_t port)
{
	struct Can_SocketHandle_s* handle = getHandle(port);
	if(NULL != handle)
	{
		close(handle->s);
		STAILQ_REMOVE(&socketH->head,handle,Can_SocketHandle_s,entry);

		free(handle);

		if(FALSE == STAILQ_EMPTY(&socketH->head))
		{
			socketH->terminated = TRUE;
		}
	}
}

static void rx_notifiy(struct Can_SocketHandle_s* handle)
{
	unsigned int nbytes,len;
	struct can_frame frame;
	nbytes = recvfrom(handle->s, &frame, sizeof(frame), 0, (struct sockaddr*)&handle->addr, &len);
	if( -1 == nbytes )
	{
		/* nothing to do */
	}
	else if (nbytes < 0) {
		perror("CAN socket read");
		/* ASWARNING("CAN Socket port=%d read message failed %d!\n",handle->port,nbytes); */
	}
	else
	{
		handle->rx_notification(handle->busid,frame.can_id,frame.can_dlc,frame.data);
	}

}
static void * rx_daemon(void * param)
{
	(void)param;
	struct Can_SocketHandle_s* handle;
	while(FALSE == socketH->terminated)
	{
		STAILQ_FOREACH(handle,&socketH->head,entry)
		{
			rx_notifiy(handle);
		}
	}

	return NULL;
}

/* ============================ [ FUNCTIONS ] ====================================================== */
#endif /* __LINUX__ */

