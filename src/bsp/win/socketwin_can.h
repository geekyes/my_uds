#ifndef SOCKETWIN_CAN_H__
#define SOCKETWIN_CAN_H__

#include <stdint.h>
#include <windows.h>

// 接收到数据，通知函数
typedef void (*can_device_rx_notification_t)(uint32_t busid,\
        uint32_t canid, uint32_t dlc, uint8_t* data);

extern boolean socket_probe(uint32_t busid, uint32_t port, uint32_t baudrate,\
        can_device_rx_notification_t rx_notification);

extern boolean socket_write(uint32_t port, uint32_t canid, uint32_t dlc,
        uint8_t* data);

extern void socket_close(uint32_t port);

#endif /* SOCKETWIN_CAN_H__ */
