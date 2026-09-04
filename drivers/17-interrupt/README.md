# GPIO 中断按键驱动与测试程序

## 目的

本示例使用 GPIO1_IO18 的双边沿中断检测 KEY0，通过 10 ms 内核定时器完成按键
消抖，并由 `/dev/imx6uirq` 向用户态报告完整的按下和释放事件。

## 验证对象与机制

设备树 `/key` 节点同时提供 `key-gpio`、`interrupt-parent` 和 `interrupts`。GPIO
任一边沿触发中断后，中断处理函数仅重新安排消抖定时器；GPIO 电平保持稳定
10 ms 后，定时器回调才确认状态。驱动先确认按下，再在确认释放时报告键值
`0x01`，从而过滤机械抖动产生的快速边沿。

没有有效事件时，普通 `read()` 会进入可中断睡眠，不会在用户态忙轮询。

## 目录结构

```text
17-interrupt/
├── app/
│   ├── src/imx6uirq_test.c
│   └── Makefile
├── imx6uirq.c
├── Makefile
└── README.md
```

## 设备树

`/key` 节点需要包含：

```dts
key {
	#address-cells = <1>;
	#size-cells = <1>;
	compatible = "atkalpha-key";
	pinctrl-names = "default";
	pinctrl-0 = <&pinctrl_key>;
	key-gpio = <&gpio1 18 GPIO_ACTIVE_LOW>;
	interrupt-parent = <&gpio1>;
	interrupts = <18 IRQ_TYPE_EDGE_BOTH>;
	status = "okay";
};
```

## 编译

```bash
make modules
make app
```

设备树修改后还需要重新编译并部署对应 DTB。

## 运行

目标板使用更新后的 DTB 启动后执行：

```bash
insmod imx6uirq.ko
./app/imx6uirq_test 1 /dev/imx6uirq
```

按下并释放 KEY0。需要验证多个事件时，将参数 `1` 改为对应次数。测试完成后：

```bash
rmmod imx6uirq
```

## 预期结果

每次完成有效的 KEY0 按下和释放后输出：

```text
PASS: KEY0 released, value=0x01
```

## 清理

```bash
make clean
```

## 环境依赖

- Linux 4.1.15 内核源码及 ARM 交叉工具链；
- 使用包含 `/key` 中断属性的最新 DTB 启动目标板；
- GPIO1_IO18 未被其他 GPIO 或输入驱动占用；
- 目标板具备模块加载权限和 `/dev` 设备节点管理机制。

## 已知限制

- 本示例仅支持 KEY0，一个尚未读取的事件可能被后续事件覆盖；
- 多个读取进程竞争同一个事件槽，每次事件只交付给一个进程；
- `15-key/key.ko` 与本模块使用同一个 GPIO 和设备树节点，不能同时加载；
- 目标板运行验证需要实际按键和正确部署的 DTB。
