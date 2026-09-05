# 21-platform

本示例使用 `platform_driver` 匹配设备树中的 `atkalpha-gpioled` 节点，申请
`led-gpio`，注册 `/dev/platform_led`，并通过 `write()` 控制 LED。

## 目录结构

```text
21-platform/
├── Makefile
├── platform_led.c
├── README.md
└── app/
    ├── Makefile
    └── src/platform_led_test.c
```

## 编译

```bash
make modules
make app
```

## 运行

确保设备树包含状态为 `okay`、compatible 为 `atkalpha-gpioled` 的 `/gpioled` 节点，
并将 `platform_led.ko` 和 `app/platform_led_test` 复制到目标板：

```bash
insmod platform_led.ko
./platform_led_test /dev/platform_led 1
./platform_led_test /dev/platform_led 0
rmmod platform_led
```

## 预期结果

驱动打印 `registered`，测试程序输出：

```text
PASS: LED turned on
PASS: LED turned off
```

## 清理

```bash
make clean
```
