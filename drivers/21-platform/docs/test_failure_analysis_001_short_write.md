# platform_led 写入成功但用户态报告 short write 分析

## 1. 测试日志

驱动加载成功：

```text
/home # insmod platform_led.ko
[18571.725827] platform_led gpioled: registered, GPIO=3 major=247 minor=0
```

LED 开关动作均已执行，但用户态测试程序报告失败：

```text
/home # ./platform_led_test /dev/platform_led 1
[18593.997679] platform_led: LED turned on
write /dev/platform_led failed: short write
/home # ./platform_led_test /dev/platform_led 0
[18597.845020] platform_led: LED turned off
write /dev/platform_led failed: short write
```

## 2. 现象判断

内核日志中的 `LED turned on` 和 `LED turned off` 说明：

- 设备节点打开成功；
- 用户态状态值已复制到内核；
- GPIO 电平已经正确设置；
- 驱动的 LED 控制逻辑执行成功。

失败只发生在用户态对 `write()` 返回值的校验阶段，不是 LED 硬件控制失败。

## 3. 根因分析

用户态程序定义：

```c
unsigned char databuf[2] = { 0 };
databuf[0] = atoi(argv[2]);
retvalue = write(fd, databuf, sizeof(databuf));
if (retvalue != (int)sizeof(databuf))
	fprintf(stderr, "write %s failed: short write\n", filename);
```

该程序发送 2 字节，并要求 `write()` 返回 2。

内核驱动的 `platform_led_write()` 只读取首字节作为 LED 状态，并返回：

```c
return sizeof(state);
```

其中 `state` 类型为 `u8`，因此返回值为 1。实际调用关系是：

```text
用户态发送 count = 2
        ↓
驱动读取首字节 state
        ↓
驱动设置 GPIO，LED 操作成功
        ↓
驱动返回 1（已消费 1 字节）
        ↓
用户态要求返回 2，误判为 short write
```

这符合 POSIX/Linux `write()` 的部分写入语义：返回值表示本次实际消费的字节数，
不等于用户态请求长度时，调用方可以认为仍有数据未被消费。当前测试协议发送了一个
两字节数组，但驱动协议实际只有一个有效状态字节，造成了接口约定不一致。

## 4. 解决方案

### 4.1 推荐方案：用户态只发送一个状态字节

将测试程序改为发送单字节状态：

```c
unsigned char databuf;

databuf = atoi(argv[2]);
retvalue = write(fd, &databuf, sizeof(databuf));
if (retvalue != (int)sizeof(databuf)) {
	fprintf(stderr, "write %s failed: short write\n", filename);
	/* 错误处理 */
}
```

这样用户态发送长度为 1，驱动返回长度也为 1，测试程序将输出：

```text
PASS: LED turned on
PASS: LED turned off
```

### 4.2 兼容现有两字节测试程序

如果希望保留 `databuf[2]` 和 `write(..., sizeof(databuf))`，驱动可以在确认首字节
有效并完成 LED 操作后返回 `count`，表示接受并消费用户态提供的全部 2 字节：

```c
/* 首字节为 LED 状态，额外字节按当前协议忽略。 */
return count;
```

该方案需要明确记录协议：首字节有效，后续字节保留。若未来不允许额外字节，应改为
严格要求 `count == 1` 并让用户态同步使用单字节写入。

## 5. 不建议的处理方式

- 不要因为用户态打印失败就判断 GPIO 控制失败；本次内核日志已经证明 GPIO 操作成功；
- 不要忽略 `write()` 返回值；应让驱动和应用统一数据长度协议；
- 不要通过修改 GPIO 编号或设备树规避该问题；
- 不要同时采用“驱动返回 1”和“用户态强制要求 2”这两套不同协议。

## 6. 验证方法

修复用户态单字节写入或修改驱动返回值后，重新编译并部署：

```bash
cd /home/gs/code/i.MX6UL/drivers/21-platform
make modules
make app
```

目标板执行：

```bash
./platform_led_test /dev/platform_led 1
./platform_led_test /dev/platform_led 0
```

预期结果：

```text
PASS: LED turned on
PASS: LED turned off
```

且内核分别打印：

```text
platform_led: LED turned on
platform_led: LED turned off
```

## 7. 结论

本次测试中 LED 已成功点亮和熄灭，`short write` 是由于用户态发送 2 字节、驱动只
消费并返回 1 字节导致的接口长度不一致。推荐统一为单字节状态协议：用户态发送 1
字节，驱动返回 1 字节。当前仅生成分析文档，尚未修改驱动或测试程序。
