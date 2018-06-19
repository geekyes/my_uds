### 说明

src/python\_auto\_test : 测试脚步来自：[parai](https://github.com/parai/as "大神的工程")

### 构建环境

#### win7 32bit
**安装 [msys2]**(http://www.msys2.com "官网")
安装完成后执行：
```bash
$ pacman -Sy # 本地的包数据库与远程的仓库同步
$ pacman -S git
$ pacman -S mingw-w64-i686-gcc # i686 代表 32bit 系统
$ pacman -S scons
$ pacman -Syu # 同步并对整个系统更新
```
**注：** **msys2** 建议使用[清华大学源](https://mirrors.tuna.tsinghua.edu.cn/help/msys2/ "msys2镜像使用帮助")

#### ubuntu 18.04 64bit
执行下面的命令：
```bash
$ apt install gcc scons
```

### 构建测试

#### 说明
**注意：如果是 windows 系统，需要进入到 msys2 的 bash 下进行下面的构建操作**

#### 获取工程
```bash
$ git clone https://github.com/geekyes/my_uds.git
$ cd my_uds/build
```

#### 构建依赖
**ubuntu:**
```bash
$ scons -f scons_python_can_lib.py
# 加载 virtual socket can
$ sudo modprobe vcan
$ sudo ip link add dev vcan0 type vcan
$ sudo ip link set up vcan0
# 安装 can-utils 工具，用于嗅探和解析 can 数据
# sudo apt install can-utils
```
**windows:**
```bash
$ scons -f scons_python_can_lib.py
$ scons -f scons_win_can_driver.py
```

#### 构建 can tp test
```bash
$ scons -f scons_can_tp_test.py
```

#### 构建 uds test
```bash
$ scons -f scons_uds_test.py
```

### 运行测试

pass

