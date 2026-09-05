# 22-dtsplatform

本示例演示基于设备树的 `platform_driver`。驱动通过
`compatible = "atkalpha-gpioled"` 与 `/gpioled` 节点匹配，读取 `led-gpio` 并注册
`/dev/dtsplatled`，用户态通过单字节 `write()` 控制 LED。

## 设备树

```dts
gpioled {
	compatible = "atkalpha-gpioled";
	pinctrl-names = "default";
	pinctrl-0 = <&pinctrl_led>;
	led-gpio = <&gpio1 3 GPIO_ACTIVE_LOW>;
	status = "okay";
};
```

## 编译和运行

```bash
make modules
make app
insmod dtsplatled.ko
./app/dtsplatled_test /dev/dtsplatled 1
./app/dtsplatled_test /dev/dtsplatled 0
rmmod dtsplatled
```

成功时用户态输出 `PASS: LED turned on/off`，内核日志会显示 LED 状态变化。

## 清理

```bash
make clean
```
