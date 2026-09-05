# SIGIO 异步通知按键驱动与测试程序

## 目的

本示例通过 `/dev/asyncnoti` 展示字符设备的 SIGIO 异步通知。驱动确认 KEY0
完成稳定按下和释放后，向启用 `O_ASYNC` 的用户进程发送 `SIGIO`；用户态收到通知
后使用非阻塞 `read()` 取得键值。

## 验证对象与机制

驱动复用 `/key` 设备树节点中的 GPIO1_IO18 和双边沿中断，并使用 10 ms 内核
定时器消抖。事件发布顺序如下：

```text
写入 key_value
    → 设置 event_pending
    → 唤醒 read/poll/select 等待者
    → kill_fasync(SIGIO, POLL_IN)
```

用户态通过以下步骤启用异步通知：

1. 使用 `sigaction()` 安装 SIGIO 处理函数；
2. 使用 `F_SETOWN` 将当前 PID 注册为信号接收者；
3. 使用 `F_GETFL` 获取文件状态标志；
4. 使用 `F_SETFL` 增加 `O_ASYNC | O_NONBLOCK`；
5. 非阻塞读取一次，处理启用 `O_ASYNC` 前已经产生的事件；
6. 收到 SIGIO 后在主循环读取事件。

信号处理函数只设置 `sig_atomic_t` 标志，不调用 `printf()`。主循环使用
`sigprocmask()` 和 `sigsuspend()` 避免检查标志与睡眠之间丢失 SIGIO。

## 目录结构

```text
20-asyncnoti/
├── app/
│   ├── src/asyncnoti_test.c
│   └── Makefile
├── asyncnoti.c
├── Makefile
└── README.md
```

## 编译

```bash
make modules
make app
```

## 运行

加载驱动并等待一个异步事件：

```bash
insmod asyncnoti.ko
./app/asyncnoti_test 1 /dev/asyncnoti
```

按下并释放 KEY0 后预期输出：

```text
Waiting for SIGIO from /dev/asyncnoti...
PASS: SIGIO key event, value=0x01
```

完成后卸载模块：

```bash
rmmod asyncnoti
```

## 清理

```bash
make clean
```

## 环境依赖

- Linux 4.1.15 内核源码和 ARM 交叉工具链；
- 目标板使用包含 `/key`、`key-gpio` 和双边沿 `interrupts` 的最新 DTB；
- GPIO1_IO18 未被其他驱动占用；
- 目标板支持 SIGIO 和动态 `/dev` 设备节点创建。

## 已知限制

- 仅支持 KEY0，键值固定为 `0x01`；
- 单事件槽不会累计事件，未读事件可能被后续事件覆盖；
- 标准信号不排队，多次 SIGIO 可能合并，用户态必须读取到 `EAGAIN`；
- 多个异步订阅者都可能收到 SIGIO，但一个事件只由一个读取者消费；
- `15-key`、`17-interrupt`、`18-noblockio`、`19-blockio` 和本模块不能同时加载；
- 实际中断、消抖和 SIGIO 投递需要在目标板验证。
