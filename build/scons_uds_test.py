# -*- coding: utf-8 -*-


#================================================================
#                source for my_uds
#   
#   filename   : scons_uds_test.py
#   author     : chenjiang
#   date       : 2018-06-17
#   description: use scons build can_tp_test
#
#================================================================

import os, sys

#  不需要的源文件
src_filter = ['can_tp_test.c']
#  不需要的目录
dir_filter = ['win']

#  递归目录寻找所有 C 文件，h 文件所在目录
def get_src_files_header_paths(curr_path):
    folder_list = []
    header_file_dir_list = []
    src_file_list = []

    #  获取 curr_path 的目录
    for elem in os.listdir(curr_path):
        if (os.path.isdir(curr_path + elem) and not elem in dir_filter):
            folder_list.append(curr_path + elem)

    for sub_folder in folder_list:
        tmp_folder = []
        dot_h_exist_flag = 'dot_none'
        
        #  一个目录里面有两种东西，一是子目录，二是文件
        #  TODO 应该需要添加目录遍临深度的控制
        for elem in os.listdir(sub_folder):
            #  说明这个目录下还有子目录，添加到临时遍临列表中去
            if (os.path.isdir(sub_folder + '/' + elem)):
                if (not elem in dir_filter):
                    tmp_folder.append(sub_folder + '/' + elem)
            elif (os.path.isfile(sub_folder + '/' + elem)):
                if ('c' == elem.split('.')[-1]): #  说明是源文件
                    if (not elem in src_filter):
                        src_file_list.append(sub_folder + '/' + elem)
                        dot_c_exist_flag = 'dot_c'
                elif ('h' == elem.split('.')[-1]): #  说明这个目录是头文件目录
                    dot_h_exist_flag = 'dot_h'
        
        #  避免头文件目录重复添加
        if ('dot_h' == dot_h_exist_flag):
            dot_h_exist_flag = 'dot_none'
            header_file_dir_list.append(sub_folder)
        
        #  将当前目录的子目录添加到遍临列表中
        folder_list += tmp_folder

    #  调试函数是否执行正确，join是把 list 字符化
    #  print('src file list: \n' + '\n'.join(src_file_list))
    #  print('header file dir list: \n' + '\n'.join(header_file_dir_list))

    return src_file_list, header_file_dir_list 

def compile_objs(objs_path, compile_list, compiler):
    for src in compile_list:
        obj = objs_path
        obj += ''.join(src.split('/')[-1].split('.')[:-1]) + '.o'
        compiler.Object(obj, src)

#  设置工程的目录
project_path = '../'
script_path = './'
uds_test_path = script_path + 'uds_test/'
src_file_list, header_file_dir_list = get_src_files_header_paths(project_path)

#  建立 uds_test 文件夹
if (not os.path.exists(uds_test_path)):
    os.makedirs(uds_test_path)

#  设置 uds scons 的环境
uds = Environment()
uds['CC'] = 'gcc'
uds['CFLAGS'] = '-Wall -g -O2'
uds['LIBS'] = 'pthread'
uds['CPPPATH'] = header_file_dir_list
if ('nt' == os.name):
    uds['LIBS'] = 'wsock32'
    uds['CPPDEFINES'] = ['WIN_PLATFORM__']
elif ('posix' == os.name):
    uds['LIBS'] = 'pthread'
    uds['CPPDEFINES'] = ['LINUX_PLATFORM__']

#  建立 uds 的 objs 目录
uds_objs = uds_test_path + 'objs/'
if (not os.path.exists(uds_objs)):
    os.makedirs(uds_objs)
#  编译 uds 的所有 objs
compile_objs(uds_objs, src_file_list, uds)
#  链接 uds
uds.Program(uds_test_path + 'uds_test', Glob(uds_objs + '*.o'))

