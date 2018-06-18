
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#undef SLEEP
#ifdef LINUX_PLATFORM__
#include "socket_can.h"
#define SLEEP(ARG) usleep((unsigned int)(ARG) * 1000)
#endif

#ifdef WIN_PLATFORM__
#include "socketwin_can.h"
#define SLEEP(ARG) Sleep((unsigned int)(ARG))
#endif

#include "can_tp.h"
#include "uds.h"

/* 不同单片机需要不同的驱动 */
#include "mcu_dev.h"

#define BUSID 0
#define PORT 0
#define BAUDRATE 500000

/* 链路层数据缓冲变量 */
static N_PDU_t N_PDU = {.is_valid = DATA_INVALID};
/* N_PDU的互斥量 */
static pthread_mutex_t N_PDU_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef LINUX_PLATFORM__
static const Can_DeviceOpsType * const can_dev = &can_socket_ops;
#endif

#ifdef WIN_PLATFORM__
static const Can_DeviceOpsType * const can_dev = &can_socketwin_ops;
#endif

/* can_tp的守护线程 */
static void* can_tp_daemon(void* param);
/* 链路层接口 */
static void send_link_layer(N_PDU_t* p_pdu);
static void rcv_link_layer(N_PDU_t* p_pdu);
/* 接收到数据处理的函数 */
static void rx_frame_data(uint32_t busid, uint32_t canid, uint32_t dlc,\
        uint8_t* data);

int main(int argc, char **argv)
{
    reset_op_t reset_op = {mcu_dev_reset};
    mem_dev_t dev = {
        check_addr_and_size,
        check_range,
        mcu_dev_write,
        mcu_dev_read
    };
    /* init  */
    init_network_layer(send_link_layer, rcv_link_layer);
    init_uds(dev, reset_op);

    /*
     * 函数原型
     * boolean socket_probe(uint32_t busid, uint32_t port, uint32_t baudrate,\
     *                     can_device_rx_notification_t rx_notification);
     */
    while (!can_dev->probe(BUSID, PORT, BAUDRATE, rx_frame_data)) {
        printf("can_dev->probe return false, socket_can offline!\n");
        SLEEP(500); // 延迟一定时间后再次连接
    }

    /* 创建can_tp的守护线程 */
    pthread_t can_tp_thread;
    if (pthread_create(&can_tp_thread, NULL, can_tp_daemon, NULL)) {
        printf("can_tp_daemon create fail\n");
        exit(1);
    }

    while (1)
    {
        main_uds_polling();
        /* 相当于周期调度 */
        SLEEP(MAIN_UDS_POLLING_PERIOD);
    }

    pthread_mutex_destroy(&N_PDU_mutex);
    can_dev->close(PORT);
    puts("\nnormal exit...");

    return 0;
}

/* can_tp的守护线程 */
static void* can_tp_daemon(void* param)
{
    while (1)
    {
        main_network_layer();
        /* 相当于周期调度 */
        SLEEP(MAIN_NETWORK_LAYER_PERIOD);
    }

    return NULL;
}

/* 接收到数据处理的函数 */
static void rx_frame_data(uint32_t busid, uint32_t canid, uint32_t dlc,\
        uint8_t* data)
{
    pthread_mutex_lock(&N_PDU_mutex);
    if (DATA_INVALID == N_PDU.is_valid) {
        /* 把接收到的数据直接保存到指定的缓冲区 */
        N_PDU.N_AI.id = canid;
        N_PDU.N_AI.N_TAtype = PHYSICAL_ADDR;
        
        for (int i = 0;\
                i < sizeof(N_PDU.N_PCI_Data) / sizeof(N_PDU.N_PCI_Data[0]); i++) {
            N_PDU.N_PCI_Data[i] = data[i];
        }
        
        N_PDU.is_valid = DATA_VALID;
    }
    pthread_mutex_unlock(&N_PDU_mutex);
    SLEEP(2);
}

/* 发送链路层接口 */
static void send_link_layer(N_PDU_t* p_pdu)
{
    /*
     * 函数原型
     * boolean socket_write(uint32_t port, uint32_t canid, uint32_t dlc,\
     * uint8_t* data);
     */
    if (!can_dev->write(PORT, send_addr.id, CAN_FRAME_SIZE, p_pdu->N_PCI_Data)) {
        printf("can_dev->write return false\n");
        /*
         * 函数原型
         * void socket_close(uint32_t port);
         */
        can_dev->close(PORT);
        exit(1);
    }
}

/* 接收链路层接口 */
static void rcv_link_layer(N_PDU_t* p_pdu)
{
    pthread_mutex_lock(&N_PDU_mutex);
    if (DATA_VALID == N_PDU.is_valid) {
        p_pdu->N_AI = N_PDU.N_AI;
        p_pdu->is_valid = N_PDU.is_valid;
        for (int i = 0;\
                i < sizeof(N_PDU.N_PCI_Data) / sizeof(N_PDU.N_PCI_Data[0]); i++) {
            p_pdu->N_PCI_Data[i] = N_PDU.N_PCI_Data[i];
        }
        N_PDU.is_valid = DATA_INVALID;
    }
    pthread_mutex_unlock(&N_PDU_mutex);
}

