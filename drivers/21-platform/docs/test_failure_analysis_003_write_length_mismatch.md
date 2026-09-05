# platform_led LED 操作成功但仍报告 short write 复测分析

## 1. 复测日志

驱动加载成功：

```text
/home # insmod platform_led.ko
[19241.056658] platform_led gpioled: registered, GPIO=3 major=247 minor=0
```

用户态执行开关操作时，LED 已被驱动控制，但测试程序报告写入失败：

```text
/home # ./platform_led_test /dev/platform_led 1
[19270.570471] platform_led: LED turned on
write /dev/platform_led failed: short write
/home # ./platform_led_test /dev/platform_led 0
[19273.638772] platform_led: LED turned off
write /dev/platform_led failed: short write
```

## 2. 结论先行

当前 LED 控制功能是成功的。内核日志中的：

```text
platform_led: LED turned on
platform_led: LED turned off
```

证明驱动已完成用户数据复制、状态判断和 GPIO 电平设置。`short write` 只表示用户态
要求写入的长度与驱动返回的已消费长度不一致，并不表示 GPIO 操作失败。

## 3. 源码对应关系

用户态测试程序当前发送两个字节：

```c
unsigned char databuf[2] = { 0 };
databuf[0] = atoi(argv[2]);
retvalue = write(fd, databuf, sizeof(databuf));
if (retvalue != (int)sizeof(databuf))
	fprintf(stderr, "write %s failed: short write\n", filename);
```

内核 `platform_led_write()` 只读取首字节作为 LED 状态，并返回：

```c
return sizeof(state);
```

`state` 类型为 `u8`，因此驱动返回值为 1。实际过程为：

```text
用户态 write count = 2
        ↓
驱动读取 databuf[0]，设置 LED
        ↓
驱动返回 1
        ↓
用户态比较 1 != 2，打印 short write
```

## 4. 为什么两次 LED 都能正确动作

LED 状态位于用户缓冲区的第一个字节，驱动只需要这个字节即可完成控制。第二个字节
当前没有定义用途，驱动没有消费它。因此即使返回值为 1，LED 仍然可以正常点亮和熄灭。

Linux `write()` 的返回值表示本次实际处理的字节数。用户态发送 2 字节而驱动只处理 1
字节时，返回 1 是合法的部分写入结果；错误在于测试程序把协议长度固定要求为 2。

## 5. 推荐修复方案

统一为单字节 LED 控制协议。修改 `app/src/platform_led_test.c`：

```c
unsigned char databuf;

databuf = atoi(argv[2]);
retvalue = write(fd, &databuf, sizeof(databuf));
if (retvalue != (int)sizeof(databuf)) {
	fprintf(stderr, "write %s failed: short write\n", filename);
	/* 错误处理 */
}
```

这样用户态请求长度和驱动返回值均为 1，成功时将只输出：

```text
PASS: LED turned on
PASS: LED turned off
```

另一种兼容方案是在驱动确认首字节有效后返回 `count`，但这会把第二个字节定义为
“已接受但保留”的协议字段；对于当前简单 LED 接口，不如单字节协议清晰。

## 6. 验证步骤

修改用户态程序后重新编译：

```bash
cd /home/gs/code/i.MX6UL/drivers/21-platform
make app
```

将新的 `app/platform_led_test` 复制到目标板，执行：

```bash
./platform_led_test /dev/platform_led 1
./platform_led_test /dev/platform_led 0
```

预期不再出现 `short write`，且内核继续打印对应的 `LED turned on/off` 日志。

## 7. 当前状态

- platform 驱动加载成功；
- LED 点亮和熄灭均已成功执行；
- `short write` 根因已确认是应用和驱动的写入长度协议不一致；
- 已将用户态测试程序修改为单字节写入，并重新编译部署。

## 8. 修复后目标板验证结果

修改用户态程序为单字节写入并重新部署后，目标板测试结果如下：

```text
/home # ./platform_led_test /dev/platform_led 0
[19480.448698] platform_led: LED turned off
PASS: LED turned off
/home # ./platform_led_test /dev/platform_led 1
[19483.413795] platform_led: LED turned on
PASS: LED turned on
```

对应内核日志为：

```text
[19480.448698] platform_led: LED turned off
[19483.413795] platform_led: LED turned on
```

用户态和内核态均确认 LED 状态切换成功，且日志中不再出现 `write ... failed: short
write`。这证明单字节 LED 控制协议已经解决原有的返回长度误判问题。

## 9. 最终结论

本问题已修复并通过目标板验证：驱动执行 LED 控制成功，用户态 `write()` 返回长度与
发送长度一致，测试程序能够正确输出 `PASS`。
