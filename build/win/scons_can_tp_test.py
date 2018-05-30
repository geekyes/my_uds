# -*- coding: utf-8 -*-

import os

def save_env_para_into_file(env):
    client_dict = env.Dictionary()
    dict_keys = client_dict.keys()
    f_out_log = open(build_dir + "/" + "env_para.log", "w")

    for key in dict_keys:
        print >> f_out_log, "env[%s]: %s\n" % (key, client_dict[key])
    f_out_log.close()

client_src_dir = os.path.abspath("../../src")
server_src_dir = os.path.abspath("../../src/bsp/win")
build_dir = os.path.abspath(".")

server_env = Environment()
server_env["CC"] = "gcc"
server_env["LIBS"] = "wsock32"
server_env["CPPDEFINES"] = "__SOCKET_WIN_CAN_DRIVER__"
server_env["CFLAGS"] = "-Wall -g -O2"

client_a_env = Environment()
client_a_env["CC"] = "gcc"
client_a_env["LIBS"] = "wsock32"
client_a_env["CPPDEFINES"] = "CLIENT_A"
client_a_env["CFLAGS"] = "-Wall -g -O2"
client_a_env["CPPPATH"] = [client_src_dir, server_src_dir]

client_b_env = Environment()
client_b_env["CC"] = "gcc"
client_b_env["LIBS"] = "wsock32"
client_b_env["CPPDEFINES"] = "CLIENT_B"
client_b_env["CFLAGS"] = "-Wall -g -O2"
client_b_env["CPPPATH"] = [client_src_dir, server_src_dir]

client_a_env.Object(build_dir + "/" + "can_tp.o",\
                   client_src_dir + "/" + "can_tp.c")
client_a_env.Object(build_dir + "/" + "socketwin_can.o",\
                   server_src_dir + "/" + "socketwin_can.c")

client_a_env.Object(build_dir + "/" + "can_tp_test_a.o",\
                   client_src_dir + "/test/win/" + "can_tp_test.c")

if os.path.exists(build_dir + "/" + "can_tp_test_b.o"):
    os.remove(build_dir + "/" + "can_tp_test_b.o")
client_b_env.Object(build_dir + "/" + "can_tp_test_b.o",\
                   client_src_dir + "/test/win/" + "can_tp_test.c")

server_env.Object(build_dir + "/" + "socketwin_can_driver.o",\
                   server_src_dir + "/" + "socketwin_can_driver.c")

server_env.Program(build_dir + "/" + "socketwin_can_driver",\
                   build_dir + "/" + "socketwin_can_driver.o")

client_a_env.Program(build_dir + "/" + "can_tp_test_a",\
                   [build_dir + "/" + "can_tp.o",
                   build_dir + "/" + "socketwin_can.o",
                   build_dir + "/" + "can_tp_test_a.o"])

client_b_env.Program(build_dir + "/" + "can_tp_test_b",\
                   [build_dir + "/" + "can_tp.o",
                   build_dir + "/" + "socketwin_can.o",
                   build_dir + "/" + "can_tp_test_b.o"])
