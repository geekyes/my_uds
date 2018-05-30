#ifndef UDS_H__
#define UDS_H__

// c99 only
#include <stdint.h>

#include "can_tp.h"

#define MAIN_UDS_POLLING_PERIOD ((uint16_t)2) // 周期函数调度的周期 单位: ms
#define SEC_TIMER ((uint16_t)500 / MAIN_UDS_POLLING_PERIOD)
// 下载和上传请求一次能处理的块数
#define LOAD_BLOCK_NUMBER ((uint8_t)4)

// 目标机是大端，请取消注解，默认小端
//#define BIG_ENDIAN

typedef uint32_t addr_t;
typedef uint32_t size_of_op_t;
typedef enum {HARD_RESET = 0x01, key_off_on_reset, SOFT_RESET} reset_type_t;

typedef struct {
    void (*reset)(reset_type_t type);
} reset_op_t;

typedef struct {
    /* 操作存储的接口，返回为零表示操作完成 */
    // 需要检测p_addr的是否是可以访问设备存储的有效地址且大小为可操作块的整数倍
    uint8_t (*check_addr_and_size)(addr_t *p_addr, size_of_op_t *p_size);
    // 存储的边界检测
    uint8_t (*check_range)(addr_t start_addr, addr_t end_addr);
    uint8_t (*write)(addr_t start_addr, size_of_op_t op_size);
    uint8_t (*read)(addr_t start_addr, size_of_op_t op_size);
} mem_dev_t;


/* 懒得写A_AI，使用网络层地址的定义 */
extern const N_AI_t rcv_addr;
extern const N_AI_t send_addr;

// 加载储存和系统复位的函数
extern void init_uds(mem_dev_t dev, reset_op_t op);

// 需要周期调用，主要是对会话时间的控制
extern void main_uds_polling(void);

#endif /* UDS_H__ */
