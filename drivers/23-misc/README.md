# 23-misc

本示例演示 `platform_driver` 与 `miscdevice` 框架结合使用。驱动通过设备树
`compatible = "atkalpha-beep"` 匹配 `/beep` 节点，读取 `beep-gpio`，注册
`/dev/miscbeep`，并使用单字节 `write()` 控制蜂鸣器。

## 设备树

```dts
beep {
	compatible = "atkalpha-beep";
	pinctrl-names = "default";
	pinctrl-0 = <&pinctrl_beep>;
	beep-gpio = <&gpio1 19 GPIO_ACTIVE_LOW>;
	status = "okay";
};
```

## 编译和运行

```bash
make modules
make app
insmod miscbeep.ko
./app/miscbeep_test /dev/miscbeep 1
./app/miscbeep_test /dev/miscbeep 0
rmmod miscbeep
```

成功时用户态输出 `PASS: buzzer turned on/off`，内核日志会记录蜂鸣器状态变化。

## 清理

```bash
make clean
```
