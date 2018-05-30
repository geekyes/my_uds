/*
 * TODO 只支持一对一通讯，如果要一对多个节点发送，或是多对一个节点接收需要增加
 * 数据接收发送的容器，如FIFO
 */
#ifndef CAN_TP_H__
#define CAN_TP_H__

// TODO C99 only
#include <stdint.h>


#define MAIN_NETWORK_LAYER_PERIOD ((uint16_t)2) // 周期函数调度的周期 单位: ms
// 单位: ms
#define TIMER_A_S (1000 / MAIN_NETWORK_LAYER_PERIOD)
#define TIMER_B_S (120 / MAIN_NETWORK_LAYER_PERIOD)
// #define TIMER_C_S

// #define TIMER_A_R (1000 / MAIN_NETWORK_LAYER_PERIOD)
#define TIMER_B_R ((TIMER_A_S / 10) * 9)
#define TIMER_C_R (120 / MAIN_NETWORK_LAYER_PERIOD)

#define TIMER_FC_STMIN (60 / MAIN_NETWORK_LAYER_PERIOD)

#define FC_BS ((uint8_t)4)
#define CAN_FRAME_SIZE ((uint8_t)8)
#define N_DATA_MAX ((uint32_t)120)
#define CAN_FRAME_BYTE_FILL ((uint8_t)0)

// 数据有效性，有效就处理
typedef enum {DATA_INVALID, DATA_VALID}validity_t;

// 发送数据的结果
typedef enum {
    N_INVALID, // 自己添加的默认发送结果
    N_OK,
    N_TIMEOUT_A,
    N_TIMEOUT_Bs, 
    N_TIMEOUT_Cr, 
    N_WRONG_SN,
    N_INVALID_FS,
    N_UNEXP_PDU,
    N_WFT_OVRN,
    N_BUFFER_OVFLW,
    N_ERROR
} N_result_t;

// 目标地址的类型
typedef enum {PHYSICAL_ADDR, FUNCTIONAL_ADDR}N_TAtype_t;

// 地址信息
typedef struct {
    uint32_t id; // 直接使用id
    N_TAtype_t N_TAtype;
}N_AI_t; // network address information

typedef struct {
    N_result_t N_result;
    uint32_t length;
}rcv_result_t; // 接收的结果

typedef struct {
    validity_t is_valid; // 当前N_PDU的数据有效，需要处理(发送或接收)
    N_AI_t N_AI;      // 地址信息
    uint8_t N_PCI_Data[CAN_FRAME_SIZE]; // N_PCI加上N_Data
}N_PDU_t; // network protocol data unit

typedef void (*fp_link_layer_t)(N_PDU_t* p_pdu);

// network_layer初始化
extern void init_network_layer(fp_link_layer_t send, fp_link_layer_t rcv);
// 需要周期调用，主要是对时间参数和数据流的控制
extern void main_network_layer(void);

// 将要发送的数据缓存到数据缓存区
extern void send_network_layer(N_AI_t addr, const uint8_t *p_data,\
        uint32_t size);
// 返回发送数据的状态
/*
 * 可能返回的值
 * N_INVALID
 * N_TIMEOUT_Bs
 * N_BUFFER_OVFLW
 * N_INVALID_FS
 * N_TIMEOUT_A
 * N_OK
 */
extern N_result_t confirm_network_layer(void);

/*
 * 先用receive_FF_network_layer判断是否接收到首帧，然后等待receive_network_layer
 */
// 提供接收到的所有数据
// 返回{.N_result == N_OK}，
// 返回{.N_result == N_INVALID}表示在接收中或是没有接收到数据
/*
 * rev_result_t.N_result可能返回的值
 * N_INVALID
 * N_ERROR
 * N_WRONG_S
 * N_TIMEOUT_Cr
 * N_OK
 */
extern rcv_result_t receive_network_layer(N_AI_t addr, uint8_t *p_data,\
        uint32_t size_max);
// 通知上层验证N_AI，自己测试没用使用这个函数
extern validity_t receive_FF_network_layer(N_AI_t *p_addr);

#endif /*CAN_TP_H__*/
