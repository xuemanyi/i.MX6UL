# 非阻塞 GPIO 中断按键驱动与测试程序

## 目的

本示例在 GPIO 中断按键驱动基础上增加 `O_NONBLOCK`、`poll()` 和 `select()`
支持。用户态可以在不阻塞 `read()` 的情况下等待 `/dev/noblockio` 出现按键事件。

## 验证对象与机制

驱动使用 `/key` 设备树节点的 GPIO1_IO18 和双边沿中断。任一边沿到来时重新安排
10 ms 内核定时器，确认一次稳定按下和稳定释放后，把 `0x01` 写入单事件槽并唤醒
等待读取、`poll()` 或 `select()` 的进程。

访问语义如下：

- 普通 `read()`：没有事件时进入可中断睡眠；
- `O_NONBLOCK` read：没有事件时返回 `-EAGAIN`；
- `poll()`/`select()`：事件就绪时返回可读状态；
- 一个事件只由一个读取进程消费。

## 目录结构

```text
18-noblockio/
├── app/
│   ├── src/noblockio_test.c
│   └── Makefile
├── noblockio.c
├── Makefile
└── README.md
```

## 编译

```bash
make modules
make app
```

## 运行

使用 `poll()` 测试一个事件：

```bash
insmod noblockio.ko
./app/noblockio_test poll 1 /dev/noblockio
```

使用 `select()` 测试三个事件：

```bash
./app/noblockio_test select 3 /dev/noblockio
```

程序每次等待 500 ms，超时会打印等待信息并继续。按下并释放 KEY0 后预期输出：

```text
PASS: KEY0 released, value=0x01
```

测试完成后卸载模块：

```bash
rmmod noblockio
```

## 清理

```bash
make clean
```

## 环境依赖

- Linux 4.1.15 内核源码和 ARM 交叉工具链；
- 目标板使用包含 `/key`、`key-gpio` 和双边沿 `interrupts` 的最新 DTB；
- GPIO1_IO18 未被其他驱动占用；
- 目标板具备模块加载权限和 `/dev` 设备节点管理机制。

## 已知限制

- 仅支持 KEY0，键值固定为 `0x01`；
- 单事件槽不会累计事件，未读事件可能被后续事件覆盖；
- 不提供 `epoll` 专用测试，但内核 `.poll` 接口可被 epoll 使用；
- `15-key`、`17-interrupt` 和本模块使用相同 GPIO，不能同时加载；
- 实际中断、消抖和 poll/select 唤醒效果需要在目标板验证。
