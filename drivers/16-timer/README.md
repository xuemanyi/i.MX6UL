# GPIO LED 内核定时器驱动与测试程序

## 目的

本示例通过 Linux 内核定时器周期翻转 GPIO LED，并由 `/dev/timer` 的 ioctl 接口
完成定时器关闭、打开和周期调整。

## 验证对象与机制

驱动复用设备树 `/gpioled` 节点的 `led-gpio` 和有效电平配置。模块加载后 LED
保持熄灭，默认周期为 1000 ms；收到打开命令后，定时器回调按当前周期翻转 LED，
并通过 `mod_timer()` 安排下一次回调。

控制命令如下：

- `CLOSE_CMD`：停止定时器；
- `OPEN_CMD`：按当前周期启动或重新安排定时器；
- `SETPERIOD_CMD`：设置非零毫秒周期，并立即启动或重新安排定时器。

## 目录结构

```text
16-timer/
├── app/
│   ├── src/timer_test.c
│   └── Makefile
├── include/timer_ioctl.h
├── timer.c
├── Makefile
└── README.md
```

## 编译

```bash
make modules
make app
```

## 运行

目标板使用包含可用 `/gpioled` 节点的 DTB 启动，且 GPIO 未被其他驱动占用：

```bash
insmod timer.ko
./app/timer_test /dev/timer
```

程序菜单中输入 `1` 关闭、`2` 打开、`3` 设置周期，输入 `0` 退出。完成测试后：

```bash
rmmod timer
```

## 预期结果

打开定时器后 LED 默认每 1000 ms 翻转一次；设置新周期后按新周期闪烁。每个成功
执行的用户态命令输出：

```text
PASS: command N completed
```

## 清理

```bash
make clean
```

## 环境依赖

- Linux 4.1.15 内核源码及 ARM 交叉工具链；
- 设备树包含状态为 `okay` 的 `/gpioled` 节点和 `led-gpio` 属性；
- GPIO1_IO03 未被 `gpioled.ko`、并发实验模块或其他驱动占用；
- 目标板具备模块加载权限和 `/dev` 设备节点管理机制。

## 已知限制

- 本示例仅支持一个 LED 和一个全局定时器实例；
- 多个进程共享同一周期及运行状态，ioctl 命令会被串行处理；
- 周期单位为毫秒，实际触发精度受内核 `HZ` 和系统调度影响；
- 当前环境只能交叉编译，LED 闪烁效果需要在目标板实际验证。
