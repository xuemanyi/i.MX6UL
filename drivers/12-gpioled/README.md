# GPIO LED 驱动与测试程序

## 目的

本示例通过设备树 `/gpioled` 节点取得 GPIO1_IO03，注册 `/dev/gpioled` 字符
设备，并使用独立用户态程序验证 LED 打开、关闭和状态读取功能。

## 验证机制

用户态与驱动之间使用一个二进制字节传递 LED 状态：

- `0`：关闭 LED；
- `1`：打开 LED。

设备树将 LED 描述为低电平有效，驱动负责把逻辑状态转换为实际 GPIO 电平。模块
加载后默认关闭 LED。

## 目录结构

```text
12-gpioled/
├── app/
│   ├── Makefile
│   └── gpioled_test.c
├── docs/
├── gpioled.c
├── Makefile
└── README.md
```

## 环境依赖

- 目标 DTB 包含状态为 `okay` 的 `/gpioled` 节点；
- GPIO1_IO03 未被其他设备占用；
- i.MX6ULL Linux 4.1 内核源码及有效的内核配置；
- ARM 交叉编译工具链；
- 目标板使用更新后的 DTB 启动。

## 编译

在本目录执行：

```bash
make
```

该命令分别生成：

```text
gpioled.ko
app/gpioled_test
```

也可以分别构建：

```bash
make modules
make app
```

## 运行

将 `gpioled.ko` 和 `app/gpioled_test` 复制到目标板后执行：

```bash
insmod gpioled.ko
./gpioled_test on
./gpioled_test get
./gpioled_test off
rmmod gpioled
```

默认访问 `/dev/gpioled`，也可以通过第二个参数指定设备文件：

```bash
./gpioled_test on /dev/gpioled
```

## 预期结果

- `on` 命令点亮 LED，并输出 `PASS: LED turned on`；
- `get` 命令输出 `PASS: LED is on`；
- `off` 命令熄灭 LED，并输出 `PASS: LED turned off`；
- 命令失败时向标准错误输出具体原因，并返回非零状态。

## 清理

```bash
make clean
```

## 已知限制

- 驱动和测试程序只支持单个二进制状态字节；
- `get` 读取 GPIO 数据值，不验证 LED 的实际光学状态；
- DTB 更新、模块加载和 LED 动作需要在目标板验证；
- 目标板必须避免其他驱动同时使用 GPIO1_IO03。
