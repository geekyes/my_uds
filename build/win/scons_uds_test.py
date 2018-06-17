
#! -*- coding: utf-8 -*-

import os

def get_handle_file(curr_path):
    curr_abspath = os.path.abspath(curr_path)
    folder_list = [curr_abspath]
    header_file_dir_list = []
    src_file_list = []

    for sub_folder in folder_list:
        curr_folder = os.path.join(curr_abspath, sub_folder)
        tmp_folder = []
        folder_exist_file_flag = False
        
        #  一个目录里面有两种东西，一是子目录，二是文件
        #  TODO 应该需要添加目录遍临深度的控制
        for elem in os.listdir(curr_folder):
            #  说明这个目录下还有子目录，添加到临时遍临列表中去
            if (os.path.isdir(os.path.join(curr_folder, elem))):
                tmp_folder.append(os.path.join(sub_folder, elem))
            elif (os.path.isfile(os.path.join(curr_folder, elem))):
                if ('c' == elem.split('.')[-1]): #  说明是源文件
                    src_file_list.append(os.path.join(sub_folder, elem))
                elif ('h' == elem.split('.')[-1]): #  说明这个目录是头文件目录
                    folder_exist_file_flag = True
        #  这个目录是头文件目录
        if(True == folder_exist_file_flag):
            folder_exist_file_flag = False
            header_file_dir_list.append(curr_folder)
        #  将当前目录的子目录添加到遍临列表中
        folder_list += tmp_folder

    #  调试函数
    #  print('src file list: \n' + '\n'.join(src_file_list))
    #  print('header file dir list: \n' + '\n'.join(header_file_dir_list))

    return src_file_list,header_file_dir_list

suffix = '/'
target = 'uds_test'
platform = 'win'
#  获取源代码目录和输出目录
prjs_dir = os.path.abspath('../../') + suffix
build_dir = os.path.join(prjs_dir, 'build/' + platform) + suffix
srcs, header_file_dir = get_handle_file(prjs_dir)

#  设置编译工具和编译参数
server_env = Environment()
server_env["CC"] = "gcc"
server_env["LIBS"] = "wsock32"
server_env["CPPDEFINES"] = "__SOCKET_WIN_CAN_DRIVER__"
server_env["CFLAGS"] = "-Wall -g -O2"
server_src = ''

client_env = Environment()
client_env["CC"] = "gcc"
client_env["LIBS"] = "wsock32"
client_env["CFLAGS"] = "-Wall -g -O2"
client_env["CPPPATH"] = header_file_dir
client_obj = []


for src in srcs:
    obj = os.path.basename(src)
    # 增加选择编译的条件
    if ('socketwin_can_driver.c' == obj):
        server_src = src
        continue;
    elif ('test.c' == obj[-6:] and target != obj[:-2]):
        # 有多个单元测试的文件，需要过滤
        continue;
    obj = '.'.join(obj.split('.')[:-1])
    obj += '.o'
    client_obj.append(build_dir + obj)
    client_env.Object(build_dir + obj, src)

# 链接客户端程序
client_env.Program(build_dir + "socket_client_uds", client_obj)

# 编译生成服务端程序
server_env.Object(build_dir + "socket_server_uds.o", server_src)
server_env.Program(build_dir + "socket_server_uds",
        build_dir + "socket_server_uds.o")
