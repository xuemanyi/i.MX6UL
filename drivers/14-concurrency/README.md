# Linux 并发与竞争实验

## 1. 实验目的

本实验基于 `/gpioled` 和 GPIO1_IO03，分别使用以下机制实现字符设备独占打开：

- atomic 原子操作；
- spinlock 自旋锁；
- semaphore 信号量；
- mutex 互斥体。

模块通过只读参数 `lock_mode` 选择一种实现。测试程序保持第一个文件描述符打开，
由子进程尝试第二次打开，预期得到 `EBUSY`；第一个文件描述符关闭后再次打开应成功。

## 2. 目录结构

```text
14-concurrency/
├── app/
│   ├── src/
│   │   └── concurrency_test.c
│   └── Makefile
├── concurrency_led.c
├── Makefile
├── run_test.sh
└── README.md
```

## 3. 机制实现

| 模式 | 状态与操作 | 等待特性 |
| --- | --- | --- |
| `atomic` | `atomic_cmpxchg(1, 0)` | 不睡眠 |
| `spinlock` | 自旋锁保护 `spin_opened` | 不睡眠，临界区必须短小 |
| `semaphore` | 二值信号量保护 `semaphore_opened` | 获取时可以睡眠 |
| `mutex` | mutex 保护 `mutex_opened` | 获取时可以睡眠，有所有者语义 |

锁仅保护“检查并设置 opened”这一原子事务。特别是 mutex 不跨越
`open()`/`release()` 长期持有，因为文件描述符可能被 fork、dup 或传递，最终执行
release 的任务不保证是当初执行 open 的任务，非所有者解锁 mutex 是错误用法。

`io_mutex` 另行保护 LED 状态读写，防止同一已打开文件描述符被多个线程共享时发生
读写竞争。

## 4. 编译

```bash
make modules
make app
```

或执行：

```bash
make
```

预期生成 `concurrency_led.ko` 和 `app/concurrency_test`。部署到目标板时，可以
将两个文件复制到同一目录，或保留 app 子目录结构；`run_test.sh` 会自动识别这两种
布局。

## 5. 自动运行

目标板使用包含 `/gpioled` 的 DTB 启动后执行：

```bash
sudo ./run_test.sh
```

脚本依次加载四种模式，验证 LED 读写、第二次打开失败和关闭后的重新打开。

## 6. 手动运行

```bash
insmod concurrency_led.ko lock_mode=atomic
./app/concurrency_test on /dev/concurrency_led
rmmod concurrency_led
```

将 `atomic` 依次替换为 `spinlock`、`semaphore` 和 `mutex`。测试程序的控制命令为
`on` 或 `off`，数据协议仍是单字节 `1` 或 `0`。

## 7. 预期结果

每种模式应输出：

```text
PASS: LED state is on
PASS: second open was rejected with EBUSY
PASS: open succeeded after release
```

全部模式完成后脚本输出：

```text
PASS: all concurrency modes completed
```

## 8. 环境依赖

- Linux 4.1 i.MX6ULL 内核构建环境；
- ARM 交叉编译工具链；
- 状态为 `okay` 的 `/gpioled` 设备树节点；
- GPIO1_IO03 未被 `gpioled.ko` 或其他驱动占用；
- 目标文件系统提供 `insmod`、`rmmod`、`fork()` 和设备节点管理机制。

## 9. 清理

```bash
make clean
```

## 10. 已知限制

- 四种模式通过模块参数分别运行，不同时加载；
- 实验验证独占打开的正确性，不是性能基准；
- 自旋锁示例使用 `irqsave` 形式，但本设备的 open/release 本身处于进程上下文；
- 信号量模式展示可睡眠锁对状态事务的保护，没有跨整个文件生命周期占有信号量；
- 当前主机只能交叉编译，完整行为需要在目标板验证；
- 实验模块与正式 `gpioled.ko` 使用同一 GPIO，二者不能同时加载。
