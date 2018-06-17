
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
        
        #  һ��Ŀ¼���������ֶ�����һ����Ŀ¼�������ļ�
        #  TODO Ӧ����Ҫ���Ŀ¼������ȵĿ���
        for elem in os.listdir(curr_folder):
            #  ˵�����Ŀ¼�»�����Ŀ¼����ӵ���ʱ�����б���ȥ
            if (os.path.isdir(os.path.join(curr_folder, elem))):
                tmp_folder.append(os.path.join(sub_folder, elem))
            elif (os.path.isfile(os.path.join(curr_folder, elem))):
                if ('c' == elem.split('.')[-1]): #  ˵����Դ�ļ�
                    src_file_list.append(os.path.join(sub_folder, elem))
                elif ('h' == elem.split('.')[-1]): #  ˵�����Ŀ¼��ͷ�ļ�Ŀ¼
                    folder_exist_file_flag = True
        #  ���Ŀ¼��ͷ�ļ�Ŀ¼
        if(True == folder_exist_file_flag):
            folder_exist_file_flag = False
            header_file_dir_list.append(curr_folder)
        #  ����ǰĿ¼����Ŀ¼��ӵ������б���
        folder_list += tmp_folder

    #  ���Ժ���
    #  print('src file list: \n' + '\n'.join(src_file_list))
    #  print('header file dir list: \n' + '\n'.join(header_file_dir_list))

    return src_file_list,header_file_dir_list

suffix = '/'
target = 'uds_test'
platform = 'win'
#  ��ȡԴ����Ŀ¼�����Ŀ¼
prjs_dir = os.path.abspath('../../') + suffix
build_dir = os.path.join(prjs_dir, 'build/' + platform) + suffix
srcs, header_file_dir = get_handle_file(prjs_dir)

#  ���ñ��빤�ߺͱ������
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
    # ����ѡ����������
    if ('socketwin_can_driver.c' == obj):
        server_src = src
        continue;
    elif ('test.c' == obj[-6:] and target != obj[:-2]):
        # �ж����Ԫ���Ե��ļ�����Ҫ����
        continue;
    obj = '.'.join(obj.split('.')[:-1])
    obj += '.o'
    client_obj.append(build_dir + obj)
    client_env.Object(build_dir + obj, src)

# ���ӿͻ��˳���
client_env.Program(build_dir + "socket_client_uds", client_obj)

# �������ɷ���˳���
server_env.Object(build_dir + "socket_server_uds.o", server_src)
server_env.Program(build_dir + "socket_server_uds",
        build_dir + "socket_server_uds.o")
