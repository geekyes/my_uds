# -*- coding: utf-8 -*-


#================================================================
#                source for my_uds
#   
#   filename   : scons_python_can_lib.py
#   author     : chenjiang
#   date       : 2018-06-17
#   description: use scons build python_can_lib
#
#================================================================

import os, sys

def compile_shared_objs(objs_path, compile_list, compiler):
    for src in compile_list:
        obj = objs_path
        obj += ''.join(src.split('/')[-1].split('.')[:-1]) + '.os'
        compiler.SharedObject(obj, src)

project_path = '../'
lib_path = './dep/'

src_file_list = [project_path + 'src/python_auto_test/python_can_lib.c']


header_file_dir_list = [
        project_path + 'src/bsp/',
        project_path + 'src/python_auto_test/']

if (not os.path.exists(lib_path)):
    os.makedirs(lib_path)

can_lib = Environment()
can_lib['CC'] = 'gcc'
can_lib['CFLAGS'] = '-Wall -g -O2'

if ('msys' == sys.platform):
    can_lib['LIBS'] = 'wsock32'
    can_lib['CPPDEFINES'] = ['WIN_PLATFORM__']
    src_file_list.append(project_path + 'src/bsp/win/socketwin_can.c')
    header_file_dir_list.append(project_path + 'src/bsp/win/')
elif ('posix' == os.name):
    can_lib['LIBS'] = 'pthread'
    can_lib['CPPDEFINES'] = ['LINUX_PLATFORM__']
    src_file_list.append(project_path + 'src/bsp/linux/socket_can.c')
    header_file_dir_list.append(project_path + 'src/bsp/linux/')

can_lib['CPPPATH'] = header_file_dir_list

compile_shared_objs(lib_path, src_file_list, can_lib)

can_lib.SharedLibrary(lib_path + 'python_can_lib', Glob(lib_path + '*.os'))

