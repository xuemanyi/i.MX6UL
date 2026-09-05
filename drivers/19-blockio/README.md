# 阻塞 GPIO 中断按键驱动与测试程序

## 目的

本示例使用等待队列实现 `/dev/blockio` 的阻塞读取。没有按键事件时，调用
`read()` 的进程进入可中断睡眠；驱动确认 KEY0 完整按下和释放后唤醒读取进程。

## 验证对象与机制

驱动复用设备树 `/key` 节点中的 GPIO1_IO18 和双边沿中断。中断处理函数将 GPIO
采样推迟 10 ms，以过滤机械抖动。定时器确认稳定释放且此前已确认稳定按下后，
写入固定键值 `0x01`，置位事件标志并调用 `wake_up_interruptible()`。

读取流程如下：

```text
read
    → 检查 event_pending
    → 无事件时进入 TASK_INTERRUPTIBLE 睡眠
    → 按键事件唤醒等待队列
    → 持锁竞争并清除事件
    → copy_to_user 返回 0x01
```

本实现使用 `wait_event_interruptible()` 封装条件检查、等待队列登记、任务状态切换
和调度，不直接使用容易遗漏清理步骤的手工 `DECLARE_WAITQUEUE + schedule()` 流程。

## 目录结构

```text
19-blockio/
├── app/
│   ├── src/blockio_test.c
│   └── Makefile
├── blockio.c
├── Makefile
└── README.md
```

## 编译

```bash
make modules
make app
```

## 运行

加载驱动并等待一个事件：

```bash
insmod blockio.ko
./app/blockio_test 1 /dev/blockio
```

测试程序在 `read()` 中阻塞。按下并释放 KEY0 后预期输出：

```text
Waiting for KEY0 event...
PASS: KEY0 released, value=0x01
```

测试多个事件时修改第一个参数。完成后卸载模块：

```bash
rmmod blockio
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
- 不实现 `.poll`，需要 `poll/select` 时使用 `18-noblockio`；
- 即使文件带有 `O_NONBLOCK`，本教学驱动的 `read()` 仍按阻塞语义等待；
- `15-key`、`17-interrupt`、`18-noblockio` 和本模块不能同时加载；
- 实际中断、消抖和阻塞唤醒效果需要在目标板验证。
