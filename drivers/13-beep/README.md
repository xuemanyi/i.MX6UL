# GPIO 蜂鸣器驱动与测试程序

## 目的

本示例通过设备树根节点 `/beep` 获取 GPIO5_IO01，注册 `/dev/beep` 字符设备，
并使用独立用户态程序验证有源蜂鸣器的打开、关闭和状态读取功能。

## 验证机制

用户态与驱动之间使用一个二进制字节传递状态：

- `0`：关闭蜂鸣器；
- `1`：打开蜂鸣器。

设备树声明蜂鸣器低电平有效，驱动负责逻辑状态和 GPIO 电平之间的转换。模块加载
和卸载时均保持蜂鸣器关闭。

## 目录结构

```text
13-beep/
├── app/
│   ├── Makefile
│   └── beep_test.c
├── beep.c
├── Makefile
└── README.md
```

## 环境依赖

- 目标 DTB 包含状态为 `okay` 的 `/beep` 节点；
- GPIO5_IO01 未被其他设备占用；
- i.MX6ULL Linux 4.1 内核源码和 ARM 交叉编译工具链；
- 目标板使用更新后的 DTB 启动。

## 编译

```bash
make modules
make app
```

也可以使用 `make` 一次生成 `beep.ko` 和 `app/beep_test`。

## 运行

将模块和测试程序复制到目标板后执行：

```bash
insmod beep.ko
./beep_test on
./beep_test get
./beep_test off
rmmod beep
```

默认访问 `/dev/beep`，也可以指定其他设备路径：

```bash
./beep_test on /dev/beep
```

## 预期结果

- `on` 使蜂鸣器鸣响并输出 `PASS: Buzzer turned on`；
- `get` 输出 `PASS: Buzzer is on`；
- `off` 关闭蜂鸣器并输出 `PASS: Buzzer turned off`；
- 失败时程序输出具体错误并返回非零状态。

## 清理

```bash
make clean
```

## 已知限制

- 仅适用于开关控制的有源蜂鸣器，不支持 PWM 音调或频率调节；
- 状态读取反映 GPIO 数据值，不验证实际声学输出；
- DTB 部署和蜂鸣器动作仍需在目标板验证。
