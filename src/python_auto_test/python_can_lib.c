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

#include "queue.h"
#include "python_can_lib.h"

#ifdef WIN_PLATFORM__
#include "socketwin_can.h"
#endif

#ifdef LINUX_PLATFORM__
#include "socket_can.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>

#define CAN_BUS_NUM   4
#define CAN_BUS_PDU_NUM   16

typedef struct {
    /* the CAN ID, 29 or 11-bit */
    uint32_t 	id;
    uint8_t     bus;
    /* Length, max 8 bytes */
    uint8_t		length;
    /* data ptr */
    uint8_t 		sdu[64];
} Can_PduType;

struct Can_Pdu_s {
    Can_PduType msg;
    STAILQ_ENTRY(Can_Pdu_s) entry;	/* entry for Can_PduQueue_s, sort by CANID queue*/
    STAILQ_ENTRY(Can_Pdu_s) entry2; /* entry for Can_Bus_s, sort by CANID queue*/
};

struct Can_PduQueue_s {
    uint32_t id;	/* can_id of this list */
    uint32_t size;
    uint32_t warning;
    STAILQ_HEAD(,Can_Pdu_s) head;
    STAILQ_ENTRY(Can_PduQueue_s) entry;
};

struct Can_Bus_s {
    uint32_t busid;
    Can_DeviceType  device;
    STAILQ_HEAD(,Can_PduQueue_s) head;	/* sort message by CANID queue */
    STAILQ_HEAD(,Can_Pdu_s) head2;	/* for all the message received by this bus */
    uint32_t                size2;
    STAILQ_ENTRY(Can_Bus_s) entry;
};

struct Can_BusList_s {
    boolean initialized;
    pthread_mutex_t q_lock;
    STAILQ_HEAD(,Can_Bus_s) head;
};

static struct Can_BusList_s canbusH =
{
    .initialized=FALSE,
    .q_lock=PTHREAD_MUTEX_INITIALIZER
};

static const Can_DeviceOpsType* canOps [] =
{
#ifdef WIN_PLATFORM__
    &can_socketwin_ops,
#endif

#ifdef LINUX_PLATFORM__
    &can_socket_ops,
#endif
    NULL
};

static FILE* canLog = NULL;

static void freeQ(struct Can_PduQueue_s* l)
{
    struct Can_Pdu_s* pdu;
    while(FALSE == STAILQ_EMPTY(&l->head))
    {
        pdu = STAILQ_FIRST(&l->head);
        STAILQ_REMOVE_HEAD(&l->head,entry);
        free(pdu);
    }
}

static void freeB(struct Can_Bus_s* b)
{
    struct Can_PduQueue_s* l;
    while(FALSE == STAILQ_EMPTY(&b->head))
    {
        l = STAILQ_FIRST(&b->head);
        STAILQ_REMOVE_HEAD(&b->head,entry);
        freeQ(l);
        free(l);
    }

}

static void freeH(struct Can_BusList_s *h)
{
    struct Can_Bus_s* b;

    pthread_mutex_lock(&h->q_lock);
    while(FALSE == STAILQ_EMPTY(&h->head))
    {
        b = STAILQ_FIRST(&h->head);
        STAILQ_REMOVE_HEAD(&h->head,entry);
        freeB(b);
        free(b);
    }
    pthread_mutex_unlock(&h->q_lock);
}

static struct Can_Bus_s* getBus(uint32_t busid)
{
    struct Can_Bus_s *handle,*h;
    handle = NULL;
    if(canbusH.initialized)
    {
        (void)pthread_mutex_lock(&canbusH.q_lock);
        STAILQ_FOREACH(h,&canbusH.head,entry)
        {
            if(h->busid == busid)
            {
                handle = h;
                break;
            }
        }
        (void)pthread_mutex_unlock(&canbusH.q_lock);
    }
    return handle;
}

static struct Can_Pdu_s* getPdu(struct Can_Bus_s* b,uint32_t canid)
{
    struct Can_PduQueue_s* L=NULL;
    struct Can_Pdu_s* pdu = NULL;
    struct Can_PduQueue_s* l;
    (void)pthread_mutex_lock(&canbusH.q_lock);

    if((uint32_t)-1 == canid)
    {	/* id is -1, means get the first of queue from b->head2 */
        if(FALSE == STAILQ_EMPTY(&b->head2))
        {
            pdu = STAILQ_FIRST(&b->head2);
            /* get the first message canid, and then search CANID queue L */
            canid = pdu->msg.id;
        }
        else
        {
            /* no message all is empty */
        }
    }
    /* search queue specified by canid */
    STAILQ_FOREACH(l,&b->head,entry)
    {
        if(l->id == canid)
        {
            L = l;
            break;
        }
    }
    if(L && (FALSE == STAILQ_EMPTY(&L->head)))
    {
        pdu = STAILQ_FIRST(&L->head);
        /* when remove, should remove from the both queue */
        STAILQ_REMOVE_HEAD(&L->head,entry);
        STAILQ_REMOVE_HEAD(&b->head2,entry2);
        b->size2 --;
        L->size --;
    }
    (void)pthread_mutex_unlock(&canbusH.q_lock);

    return pdu;
}

static void saveB(struct Can_Bus_s* b,struct Can_Pdu_s* pdu)
{
    struct Can_PduQueue_s* L;
    struct Can_PduQueue_s* l;
    L = NULL;
    (void)pthread_mutex_lock(&canbusH.q_lock);
    STAILQ_FOREACH(l,&b->head,entry)
    {
        if(l->id == pdu->msg.id)
        {
            L = l;
            break;
        }
    }

    if(NULL == L)
    {
        L = malloc(sizeof(struct Can_PduQueue_s));
        if(L)
        {
            L->id = pdu->msg.id;
            L->size = 0;
            L->warning = FALSE;
            STAILQ_INIT(&L->head);
            STAILQ_INSERT_TAIL(&b->head,L,entry);
        }
        else
        {
            printf("LUA CAN Bus List malloc failed\n");
        }
    }

    if(L)
    {
        /* limit by CANID queue is better than the whole bus one */
        if(L->size < CAN_BUS_PDU_NUM)
        {
            STAILQ_INSERT_TAIL(&L->head,pdu,entry);
            STAILQ_INSERT_TAIL(&b->head2,pdu,entry2);
            b->size2 ++;
            L->size ++;
            L->warning = FALSE;
        }
        else
        {
            if(L->warning == FALSE)
            {
                printf("LUA CAN Q[id=%X] List is full with size %d\n", 
                        L->id, L->size);
                L->warning = TRUE;
            }
            free(pdu);
        }
    }

    (void)pthread_mutex_unlock(&canbusH.q_lock);
}

static void logCan(bool isRx, uint32_t busid, uint32_t canid, uint32_t dlc,
        uint8_t* data)
{
    static struct timeval m0 = { -1 , -1 };

    if( (-1 == m0.tv_sec) && (-1 == m0.tv_usec) )
    {
        gettimeofday(&m0,NULL);
    }
    if(NULL != canLog)
    {
        uint32_t i;
        struct timeval m1;
        gettimeofday(&m1,NULL);

        float rtim = m1.tv_sec-m0.tv_sec;

        if(m1.tv_usec > m0.tv_usec)
        {
            rtim += (float)(m1.tv_usec-m0.tv_usec)/1000000.0;
        }
        else
        {
            rtim = rtim - 1 + (float)(1000000.0+m1.tv_usec-m0.tv_usec)/1000000.0;
        }
        /* gettimeofday(&m0,NULL); */ /* use absolute time */
        fprintf(canLog,"busid=%d %s canid=%04X dlc=%d data=[",busid,isRx?"rx":"tx",canid,dlc);
        if(dlc < 8)
        {
            dlc = 8;
        }
        for(i=0; i<dlc; i++)
        {
            fprintf(canLog,"%02X,",data[i]);
        }

        fprintf(canLog,"] [");

        for(i=0; i<dlc; i++)
        {
            if(isprint(data[i]))
            {
                fprintf(canLog,"%c",data[i]);
            }
            else
            {
                fprintf(canLog,".");
            }
        }
        fprintf(canLog,"] @ %f s\n", rtim);
    }
}

static void rx_notification(uint32_t busid, uint32_t canid, uint32_t dlc,
        uint8_t* data)
{
    if((busid < CAN_BUS_NUM) && ((uint32_t)-1 != canid))
    {	/* canid -1 reserved for can_read get the first received CAN message on bus */
        struct Can_Bus_s* b = getBus(busid);
        if(NULL != b)
        {
            struct Can_Pdu_s* pdu = malloc(sizeof(struct Can_Pdu_s));
            if(pdu)
            {
                pdu->msg.bus = busid;
                pdu->msg.id = canid;
                pdu->msg.length = dlc;
                memcpy(&(pdu->msg.sdu),data,dlc);

                saveB(b,pdu);
                logCan(TRUE,busid,canid,dlc,data);
            }
            else
            {
                printf("LUA CAN RX malloc failed\n");
            }
        }
        else
        {
            /* not on-line */
            printf("lua is not on-line now!\n");
        }
    }
    else
    {
        printf("LUA CAN RX bus <%d> out of range, busid < %d is support only\n",busid,CAN_BUS_NUM);
    }

    printf("LUA RX CAN ID=0x%08X LEN=%d \
            DATA=[%02X %02X %02X %02X %02X %02X %02X %02X]\n",
            canid,dlc,
            data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7]);
}

static const Can_DeviceOpsType* search_ops(const char* name)
{
    const Can_DeviceOpsType *ops,**o;
    o = canOps;
    ops = NULL;
    while(*o != NULL)
    {
        if(0 == strcmp((*o)->name,name))
        {
            ops = *o;
            break;
        }
        o++;
    }

    return ops;
}

/* ============================ [ FUNCTIONS ] ====================================================== */

/* can open */
int can_open(unsigned long busid, const char* device_name, unsigned long port, 
        unsigned long baudrate)
{
    int rv;
    const Can_DeviceOpsType* ops;
    ops = search_ops(device_name);
    struct Can_Bus_s* b = getBus(busid);
    rv = FALSE;
    if(NULL != b)
    {
        printf("ERROR :: can bus(%d) is already on-line 'can_open'\n",(int)busid);
    }
    else
    {
        if(NULL != ops)
        {
            b = malloc(sizeof(struct Can_Bus_s));
            b->busid = busid;
            b->device.ops = ops;
            b->device.busid = busid;
            b->device.port = port;

            rv = ops->probe(busid,port,baudrate,rx_notification);

            if(rv)
            {
                STAILQ_INIT(&b->head);
                STAILQ_INIT(&b->head2);
                b->size2 = 0;
                pthread_mutex_lock(&canbusH.q_lock);
                STAILQ_INSERT_TAIL(&canbusH.head,b,entry);
                pthread_mutex_unlock(&canbusH.q_lock);
                /* result OK */
            }
            else
            {
                free(b);
                printf("ERROR :: can_open device <%s> failed!\n",device_name);
            }
        }
        else
        {
            printf("ERROR :: can_open device <%s> is not known by lua!\n",device_name);
        }
    }

    fflush(stdout);
    return rv;
}

/* can write */
int can_write(unsigned long busid, unsigned long canid, unsigned long dlc, 
        unsigned char* data)
{
    int rv;
    struct Can_Bus_s * b = getBus(busid);
    rv = FALSE;
    if(NULL == b)
    {
        printf("ERROR :: can bus(%d) is not on-line 'can_write'\n",(int)busid);
    }
    else if(dlc > 64)
    {
        printf("ERROR :: can bus(%d) 'can_write' with invalid dlc(%d>8)\n",(int)busid,(int)dlc);
    }
    else
    {
        if(b->device.ops->write)
        {
            /*printf("can_write(bus=%d,canid=%X,dlc=%d,data=[%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X]\n",
              busid,canid,dlc,data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7]); */
            rv = b->device.ops->write(b->device.port,canid,dlc,data);

            if(rv)
            {
                /* result OK */
            }
            else
            {
                printf("ERROR :: can_write bus(%d) failed!\n",(int)busid);
            }
        }
        else
        {
            printf("ERROR :: can bus(%d) is read-only 'can_write'\n",(int)busid);
        }
    }
    fflush(stdout);
    return rv;
}

/* can read  */
int can_read(unsigned long busid, unsigned long canid, unsigned long* p_canid, 
        unsigned long *dlc,unsigned char* data)
{
    int rv = FALSE;
    struct Can_Pdu_s* pdu;
    struct Can_Bus_s* b = getBus(busid);

    *dlc = 0;

    if(NULL == b)
    {
        printf("ERROR :: bus(%d) is not on-line 'can_read'\n",(int)busid);
    }
    else
    {
        pdu = getPdu(b,canid);
        if(NULL == pdu)
        {
            /* no data */
        }
        else
        {
            *p_canid = pdu->msg.id;
            *dlc = pdu->msg.length;
            /* asAssert(data); */
            memcpy(data,pdu->msg.sdu,*dlc);
            free(pdu);
            rv = TRUE;
        }
    }

    fflush(stdout);
    return rv;
}

/* can lib init  */
void python_can_lib_open(void)
{
	if(canbusH.initialized)
	{
		freeH(&canbusH);
	}
	canbusH.initialized = TRUE;
	STAILQ_INIT(&canbusH.head);

	canLog=NULL;
}

/* can lib close */
void python_can_lib_close(void)
{
	if(canbusH.initialized)
	{
		struct Can_Bus_s* b;
		STAILQ_FOREACH(b,&canbusH.head,entry)
		{
			b->device.ops->close(b->device.port);
		}
		freeH(&canbusH);
		canbusH.initialized = FALSE;
	}

	if(NULL != canLog)
	{
		fclose(canLog);
	}
}

