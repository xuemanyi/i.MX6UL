# DTS LED 驱动与测试程序

## 目的

该示例通过设备树根节点 `/alphaled` 提供的 `reg` 属性映射 i.MX6ULL
GPIO1_IO03 相关寄存器，注册 `/dev/dtsled` 字符设备，并由用户态程序验证
LED 的打开、关闭和状态读取功能。

## 验证机制

用户态与驱动之间使用一个字节传递 LED 状态：

- `0`：关闭 LED；
- `1`：打开 LED。

LED 为低电平有效。驱动加载后默认关闭 LED。

## 目录结构

```text
11-dts-led/
├── app/
│   ├── Makefile
│   └── dtsled_test.c
├── dtsled.c
├── Makefile
└── README.md
```

## 环境依赖

- 已配置 `alphaled` 节点的 `imx6ull-alientek-emmc.dtb`；
- i.MX6ULL Linux 4.1 内核源码及已完成的内核配置；
- ARM 交叉编译工具链；
- 目标板使用包含上述设备树节点的 DTB 启动。

## 编译

在本目录执行：

```bash
make
```

该命令分别生成：

```text
dtsled.ko
app/dtsled_test
```

也可以分别构建内核模块和测试程序：

```bash
make modules
make app
```

## 运行

将 `dtsled.ko` 和 `app/dtsled_test` 复制到目标板，然后执行：

```bash
insmod dtsled.ko
./dtsled_test on
./dtsled_test get
./dtsled_test off
rmmod dtsled
```

默认访问 `/dev/dtsled`。也可以通过第二个参数指定设备文件：

```bash
./dtsled_test on /dev/dtsled
```

## 预期结果

- `on` 命令点亮 LED；
- `get` 命令输出 `PASS: LED is on`；
- `off` 命令熄灭 LED；
- 每条成功的测试命令输出以 `PASS` 开头的英文结果。

## 清理

```bash
make clean
```

## 已知限制

- 驱动直接映射并操作 SoC 寄存器，仅适用于当前 i.MX6ULL 板级设计；
- `alphaled` 节点中五组 `reg` 资源的顺序必须与驱动定义一致；
- GPIO1_IO03 不应同时由其他驱动占用；
- 当前构建环境只能完成交叉编译，LED 动作需要在目标板上验证。
