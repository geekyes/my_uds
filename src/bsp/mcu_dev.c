#include <stdio.h>
#include <stdlib.h>

#undef SLEEP
#ifdef LINUX__
#include "socket_can.h"
#define SLEEP(ARG) usleep((unsigned int)(ARG) * 1000)
#endif

#ifdef WIN__
#include "socketwin_can.h"
#define SLEEP(ARG) Sleep((unsigned int)(ARG))
#endif

#include "mcu_dev.h"

#undef NULL
#define NULL ((void*)0)

// 需要检测p_addr的是否是可以访问设备存储的有效地址且大小为可操作块的整数倍
uint8_t check_addr_and_size(addr_t *p_addr, size_of_op_t *p_size)
{
    uint8_t ret = 0;

    if (NULL == p_addr && NULL == p_size)
    {
        ret = 1;
    }
    else
    {
        //
    }

    return ret;
}

// 存储的边界检测
uint8_t check_range(addr_t start_addr, addr_t end_addr)
{
    uint8_t ret = 0;

    puts("exec mcu_dev check_range!!!!");
    //SLEEP(MAIN_UDS_POLLING_PERIOD);

    return ret;
}

uint8_t mcu_dev_write(addr_t start_addr, size_of_op_t op_size)
{
    uint8_t ret = 0;

    puts("exec mcu_dev write!!!!");
    //SLEEP(MAIN_UDS_POLLING_PERIOD);

    return ret;
}

uint8_t mcu_dev_read(addr_t start_addr, size_of_op_t op_size)
{
    uint8_t ret = 0;

    puts("exec mcu_dev read!!!!");
    //SLEEP(MAIN_UDS_POLLING_PERIOD);

    return ret;
}

// 退出程序
void mcu_dev_reset(reset_type_t type)
{
    puts("exec mcu_dev reset!!!!");
    SLEEP(1000);

    exit(1);
}

