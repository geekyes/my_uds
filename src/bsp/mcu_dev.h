#ifndef MCU_DEV_H__
#define MCU_DEV_H__

#include "uds.h"

// mem dev
// ��Ҫ���p_addr���Ƿ��ǿ��Է����豸�洢����Ч��ַ�Ҵ�СΪ�ɲ������������
extern uint8_t check_addr_and_size(addr_t *p_addr, size_of_op_t *p_size);
// �洢�ı߽���
extern uint8_t check_range(addr_t start_addr, addr_t end_addr);
extern uint8_t mcu_dev_write(addr_t start_addr, size_of_op_t op_size);
extern uint8_t mcu_dev_read(addr_t start_addr, size_of_op_t op_size);
// �˳�����
extern void mcu_dev_reset(reset_type_t type);


#endif /* MCU_DEV_H__ */
