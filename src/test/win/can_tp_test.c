/*
 * win32下can_tp模块基础测试
 * 当接收到数据，直接显示出来，其他时间输入数据，回车发送
 */

#include <stdio.h>
#include <windows.h> // 提供延时Sleep(ms)
#include <pthread.h> // 提供周期调度的线程
#include <string.h>

#include "can_tp.h"
#include "socketwin_can.h"

#define BUSID 0
#define PORT 0
#define BAUDRATE 500000
#define SEND_DATA_BUF_MAX 100
#define RCV_DATA_BUF_MAX 100

#ifdef CLIENT_A
static N_AI_t send_addr = {.id = 0x700, .N_TAtype = PHYSICAL_ADDR};
static N_AI_t rcv_addr = {.id = 0x701, .N_TAtype = PHYSICAL_ADDR};
#endif

#ifdef CLIENT_B
static N_AI_t send_addr = {.id = 0x701, .N_TAtype = PHYSICAL_ADDR};
static N_AI_t rcv_addr = {.id = 0x700, .N_TAtype = PHYSICAL_ADDR};
#endif
static uint8_t send_data_buf[SEND_DATA_BUF_MAX];
static uint8_t rcv_data_buf[RCV_DATA_BUF_MAX];
/* 链路层数据缓冲变量 */
static N_PDU_t N_PDU = {.is_valid = DATA_INVALID};
/* N_PDU的互斥量 */
static pthread_mutex_t N_PDU_mutex = PTHREAD_MUTEX_INITIALIZER;

/* can_tp的main守护线程 */
static void* can_tp_daemon(void* param);
/* 接收到数据处理的函数 */
static void rx_frame_data(uint32_t busid, uint32_t canid, uint32_t dlc,\
        uint8_t* data);

/* 链路层接口 */
static void send_link_layer(N_PDU_t* p_pdu);
static void rcv_link_layer(N_PDU_t* p_pdu);

int main(int argc, char *argv[])
{
    init_network_layer(send_link_layer, rcv_link_layer);
    pthread_mutex_init(&N_PDU_mutex, NULL); // 初始化互斥量
    /*
     * 函数原型
     * boolean socket_probe(uint32_t busid, uint32_t port, uint32_t baudrate,\
     *                     can_device_rx_notification_t rx_notification);
     */
    while (!socket_probe(BUSID, PORT, BAUDRATE, rx_frame_data)) {
        printf("socket_probe return false, socketwin_can offline!\n");
        Sleep(500); // 延迟一定时间后再次连接
    }

    /* 创建can_tp的守护线程 */
    pthread_t can_tp_thread;
    if (pthread_create(&can_tp_thread, NULL, can_tp_daemon, NULL)) {
        printf("can_tp_daemon create fail\n");
        exit(1);
    }

    /* 输入数据，回车发送 */
    while (1) {
        if (scanf("%s", send_data_buf)) {
            /* 调用can_tp的发送函数 */
            send_network_layer(send_addr, send_data_buf,\
                    strlen((char*)send_data_buf));
            fflush(stdin);
            // strcmp()当字符串相等的时候，返回 0
            if (0 == strcmp((char*)send_data_buf, "exit();")) {
                break;
            }
            /* getchar(); // 读取回车符 */
            /* 等待发送完成，才能进入下次的输入 */
            while (1) {
                // 保证第一次进来可以执行一次
                static N_result_t pre_result = N_ERROR;
                N_result_t result = confirm_network_layer();
                if (pre_result != result) {
                    printf("send result: ");
                    switch (result) {
                    case N_INVALID:
                        puts("can_tp_send ...ing or idle");
                        break;
                    case N_TIMEOUT_Bs:
                        puts("N_TIMEOUT_Bs");
                        break;
                    case N_BUFFER_OVFLW:
                        puts("N_BUFFER_OVFLW");
                        break;
                    case N_INVALID_FS:
                        puts("N_INVALID_FS");
                        break;
                    case N_TIMEOUT_A:
                        puts("N_TIMEOUT_A");
                        break;
                    case N_ERROR:
                        puts("N_ERROR");
                        break;
                    case N_OK:
                        puts("N_OK");
                        break;
                    default:
                        break;
                    }
                    pre_result = result;
                }
                if (result != N_INVALID) {
                    break; // 发送完成或是发送出错
                }
                Sleep(MAIN_NETWORK_LAYER_PERIOD);
            }
            for (int i = 0; i < strlen((char*)send_data_buf); i++) {
                send_data_buf[i] = 0;
            }
        }
        Sleep(MAIN_NETWORK_LAYER_PERIOD);
    }

    pthread_mutex_destroy(&N_PDU_mutex);
    socket_close(PORT);
    puts("\nnormal exit...");
    return 0;
}

/* can_tp的main守护线程 */
static void* can_tp_daemon(void* param)
{
    while (1) {
        main_network_layer();
        static N_result_t pre_result = N_ERROR;
        rcv_result_t rcv_result = receive_network_layer(\
                rcv_addr, rcv_data_buf, RCV_DATA_BUF_MAX);

        if (pre_result != rcv_result.N_result) {
            printf("rcv result: ");
            switch (rcv_result.N_result) {
            case N_INVALID:
                puts("can_tp_rcv ...ing or idle");
                break;
            case N_TIMEOUT_Cr:
                puts("N_TIMEOUT_Cr");
                break;
            case N_WRONG_SN:
                puts("N_WRONG_SN");
                break;
            case N_UNEXP_PDU:
                puts("N_UNEXP_PDU");
                break;
            case N_TIMEOUT_A:
                puts("N_TIMEOUT_A");
                break;
            case N_WFT_OVRN:
                puts("N_WFT_OVRN");
                break;
            case N_ERROR:
                puts("N_ERROR");
                break;
            case N_OK:
                puts("N_OK");
                break;
            default:
                break;
            }
            pre_result = rcv_result.N_result;
        }
         
        if (rcv_result.length) {
            for (int i = 0; i < rcv_result.length; i++) {
                printf("%c '0x%x' ", rcv_data_buf[i], rcv_data_buf[i]);
                if (!((i + 1) % 7)) {
                    putchar('\n');
                }
            }
            putchar('\n');
        }
        /* 相当于周期调度 */
        Sleep(MAIN_NETWORK_LAYER_PERIOD);
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
    Sleep(MAIN_NETWORK_LAYER_PERIOD);
}


/* 发送链路层接口 */
static void send_link_layer(N_PDU_t* p_pdu)
{
    /*
     * 函数原型
     * boolean socket_write(uint32_t port, uint32_t canid, uint32_t dlc,\
     * uint8_t* data);
     */
    if (!socket_write(PORT, send_addr.id, CAN_FRAME_SIZE, p_pdu->N_PCI_Data)) {
        printf("socket_write return false\n");
        /*
         * 函数原型
         * void socket_close(uint32_t port);
         */
        socket_close(PORT);
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


