#include <stdio.h>
#include <windows.h>

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
    //Sleep(MAIN_UDS_POLLING_PERIOD);

    return ret;
}

uint8_t write(addr_t start_addr, size_of_op_t op_size)
{
    uint8_t ret = 0;

    puts("exec mcu_dev write!!!!");
    //Sleep(MAIN_UDS_POLLING_PERIOD);

    return ret;
}

uint8_t read(addr_t start_addr, size_of_op_t op_size)
{
    uint8_t ret = 0;

    puts("exec mcu_dev read!!!!");
    //Sleep(MAIN_UDS_POLLING_PERIOD);

    return ret;
}

// 退出程序
void reset(reset_type_t type)
{
    puts("exec mcu_dev reset!!!!");
    Sleep(1000);

    exit(1);
}

