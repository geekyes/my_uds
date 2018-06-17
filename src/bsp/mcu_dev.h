#ifndef MCU_DEV_H__
#define MCU_DEV_H__

#include "uds.h"

// mem dev
// 需要检测p_addr的是否是可以访问设备存储的有效地址且大小为可操作块的整数倍
extern uint8_t check_addr_and_size(addr_t *p_addr, size_of_op_t *p_size);
// 存储的边界检测
extern uint8_t check_range(addr_t start_addr, addr_t end_addr);
extern uint8_t mcu_dev_write(addr_t start_addr, size_of_op_t op_size);
extern uint8_t mcu_dev_read(addr_t start_addr, size_of_op_t op_size);
// 退出程序
extern void mcu_dev_reset(reset_type_t type);


#endif /* MCU_DEV_H__ */
