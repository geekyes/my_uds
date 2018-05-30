__lic__ = '''
/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2015  AS <parai@foxmail.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
 '''

from ctypes import *

# 加载can读写库
can_rw_lib = cdll.LoadLibrary('./msys-can_rw_lib.dll')

def can_open(busid,device,port,baudrate):
    # lib初始化数据对象(容器)
    can_rw_lib.can_rw_lib_open()
    return can_rw_lib.can_open(c_ulong(busid), c_char_p(device),\
            c_ulong(port), c_ulong(baudrate))

def can_write(busid,canid,data):
    '''can request write on can bus <busid>'''
    CAN_FRAME_LENGTH = 8 # 经典CAN
    send_data = (c_ubyte * CAN_FRAME_LENGTH)()
    # enumerate() --  同时获得索引和值
    for i,c in enumerate(data):
        send_data[i] = c_ubyte(int(c))
    dlc = len(data)
    # list元素直接用什么分割，如有list = ['a', 'b', 'c']
    # '.'.join(list)的结果为：a.b.c
    return can_rw_lib.can_write(c_ulong(busid), c_ulong(canid), c_ulong(dlc),\
            byref(send_data))

# 返回多个值，其实就是返回一个tuple(元组)，就是const list 
def can_read(busid,canid):
    ''' can request read a can frame from <canid> queue of <busid>'''
    CAN_FRAME_LENGTH = 8 # 经典CAN
    # 创建C类型
    rcv_canid = c_ulong()
    rcv_dlc = c_ulong()
    rcv_data = (c_ubyte * CAN_FRAME_LENGTH)()
    
    # byref() 对象的引用(reference)
    result = can_rw_lib.can_read(c_ulong(busid), c_ulong(canid),\
            byref(rcv_canid), byref(rcv_dlc), byref(rcv_data))
    
    return result,rcv_canid.value,rcv_data

# 直接关闭所有bus
def can_close(busid):
    can_rw_lib.can_rw_lib_close()
