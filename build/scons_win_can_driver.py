# -*- coding: utf-8 -*-


#================================================================
#                source for my_uds
#   
#   filename   : scons_win_can_driver.py
#   author     : chenjiang
#   date       : 2018-06-18
#   description: use scons build win_can_driver
#
#================================================================

import os

if ('nt' == os.name):
    project_path = '../'
    lib_path = './dep/'

    src_file_list = [ project_path + 'src/bsp/win/socketwin_can_driver.c'] 
    header_file_dir_list = [project_path + 'src/bsp/']

    if (not os.path.exists(lib_path)):
        os.makedirs(lib_path)

    win_can_driver = Environment()
    win_can_driver['CC'] = 'gcc'
    win_can_driver['CFLAGS'] = '-Wall -g -O2'
    win_can_driver['CPPPATH'] = header_file_dir_list
    win_can_driver['LIBS'] = 'wsock32'
    win_can_driver['CPPDEFINES'] = ['__SOCKET_WIN_CAN_DRIVER__']

    win_can_driver.Program(lib_path + 'win_can_driver', src_file_list)
else:
    print('win 下使用！！！\n')
