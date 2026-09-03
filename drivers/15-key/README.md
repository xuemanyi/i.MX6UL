# GPIO 按键驱动与测试程序

## 目的

本示例将 GPIO1_IO18 配置为按键输入，使用 GPIO 双边沿中断记录按下事件，并通过
`/dev/key` 字符设备向用户态返回 `0xf0`。

## 设备树

`UART1_CTS_B` 在 `&iomuxc/imx6ul-evk` 下配置为：

```dts
pinctrl_key: keygrp {
	fsl,pins = <
		MX6UL_PAD_UART1_CTS_B__GPIO1_IO18 0xf080 /* KEY0 */
	>;
};
```

根节点 `/key` 通过 `key-gpio = <&gpio1 18 GPIO_ACTIVE_LOW>` 引用该引脚。原有
`gpio-keys/key1` 已移除，避免 GPIO1_IO18 被两个 consumer 同时申请。

## 目录结构

```text
15-key/
├── app/
│   ├── Makefile
│   ├── src/key_test.c
│   └── key_test
├── key.c
├── Makefile
└── README.md
```

## 编译

```bash
make modules
make app
```

## 运行

目标板使用更新后的 DTB 启动，加载模块后执行：

```bash
insmod key.ko
./app/key_test 1
rmmod key
```

`key_test` 默认读取 `/dev/key`，参数 1 表示等待一次按键；也可以指定事件次数和
设备路径：

```bash
./app/key_test 5 /dev/key
```

程序启动后阻塞等待按键，按下 KEY0 时输出 `PASS`。驱动通过 IRQ 唤醒等待队列，
不使用参考代码中的忙等待循环。

## 返回协议

- 按键按下：返回一个字节 `0xf0`；
- 无事件的非阻塞读取：返回 `-EAGAIN`；
- 阻塞读取会睡眠直到产生按键事件；
- 被信号中断的读取返回 `-ERESTARTSYS`。

## 预期结果

按下按键后输出：

```text
PASS: KEY0 pressed, value=0xf0
```

## 已知限制

- 当前实现使用双边沿中断，没有硬件或软件去抖；机械抖动可能产生多个事件；
- 只支持一个按键和固定事件值 `0xf0`；
- 读取的是 GPIO 电平转换后的逻辑事件，不是按键物理波形采样；
- 必须确保 GPIO1_IO18 未被其他 GPIO、UART 或输入设备占用；
- 目标板运行验证需要 root 权限加载模块并具备正确 DTB。
