# -*- coding: utf-8 -*-


#================================================================
#                source for my_uds
#   
#   filename   : scons_can_tp_test.py
#   author     : chenjiang
#   date       : 2018-06-17
#   description: use scons build can_tp_test
#
#================================================================

import os, sys

#  获取目录下的 C 文件
def get_compile_list(path):
    compile_list = []

    for elem in os.listdir(path):
        filter_list = ['uds.c', 'mcu_dev.c']
        if (not elem in filter_list and 'c' == elem.split('.')[-1]):
            compile_list.append(path + elem)

    return compile_list

def compile_objs(objs_path, compile_list, compiler):
    for src in compile_list:
        obj = objs_path
        obj += ''.join(src.split('/')[-1].split('.')[:-1]) + '.o'
        compiler.Object(obj, src)

#  设置工程的目录
project_path = '../'
if ('msys' == sys.platform):
    can_driver_api_path = project_path + 'src/bsp/win/'
elif('posix' == os.name):
    can_driver_api_path = project_path + 'src/bsp/linux/'
bsp_path = project_path + 'src/bsp/'
app_path = project_path + 'src/'
test_path = project_path + 'src/test/'
python_auto_test_path = project_path + 'src/python_auto_test/'
script_path = './'

#  建立 can_tp_test 文件夹
objs_path = script_path + 'can_tp_test/'
if (not os.path.exists(objs_path)):
    os.makedirs(objs_path)
#  获取需要编译的源文件
compile_list = get_compile_list(can_driver_api_path)
compile_list += get_compile_list(bsp_path)
compile_list += get_compile_list(app_path)
compile_list += get_compile_list(python_auto_test_path)
compile_list.append(test_path + 'can_tp_test.c')

#  设置 client scons 的环境
client_a = Environment()
client_b = Environment()
client_a['CC'] = 'gcc'
client_a['CFLAGS'] = '-Wall -g -O2'
client_a['CPPPATH'] = [can_driver_api_path, bsp_path, app_path,
        python_auto_test_path]
client_b['CC'] = 'gcc'
client_b['CFLAGS'] = '-Wall -g -O2'
client_b['CPPPATH'] = [can_driver_api_path, bsp_path, app_path,
        python_auto_test_path]
if ('msys' == sys.platform):
    client_a['LIBS'] = 'wsock32'
    client_a['CPPDEFINES'] = ['WIN_PLATFORM__', 'CLIENT_A']
    client_b['LIBS'] = 'wsock32'
    client_b['CPPDEFINES'] = ['WIN_PLATFORM__', 'CLIENT_B']
elif ('posix' == os.name):
    client_a['LIBS'] = 'pthread'
    client_a['CPPDEFINES'] = ['LINUX_PLATFORM__', 'CLIENT_A']
    client_b['LIBS'] = 'pthread'
    client_b['CPPDEFINES'] = ['LINUX_PLATFORM__', 'CLIENT_B']

#  建立 client 的 objs 目录
client_a_objs = objs_path + 'client_a_objs/'
client_b_objs = objs_path + 'client_b_objs/'
if (not os.path.exists(objs_path)):
    os.makedirs(client_a_objs)
    os.makedirs(client_b_objs)

#  编译 client a  的所有 objs
compile_objs(client_a_objs, compile_list, client_a)
#  链接 client a
client_a.Program(objs_path + 'can_tp_test_a', Glob(client_a_objs + '*.o'))

#  编译 client b 的所有 objs
compile_objs(client_b_objs, compile_list, client_b)
#  链接 client b
client_b.Program(objs_path + 'can_tp_test_b', Glob(client_b_objs + '*.o'))

