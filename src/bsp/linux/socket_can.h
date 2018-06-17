/*================================================================
*                source for my_uds
*   
*   filename   : socket_can.h
*   author     : chenjiang
*   date       : 2018-06-17
*   description: Add a header file
*
================================================================*/

#ifndef SOCKET_CAN_H__
#define SOCKET_CAN_H__

#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>

#define TRUE (true)
#define FALSE (false)

typedef bool boolean; 

// 接收到数据，通知函数
typedef void (*can_device_rx_notification_t)(uint32_t busid,\
        uint32_t canid, uint32_t dlc, uint8_t* data);

extern boolean socket_probe(uint32_t busid, uint32_t port, uint32_t baudrate,\
        can_device_rx_notification_t rx_notification);

extern boolean socket_write(uint32_t port, uint32_t canid, uint32_t dlc,
        uint8_t* data);

extern void socket_close(uint32_t port);

#endif /* SOCKET_CAN_H__ */


