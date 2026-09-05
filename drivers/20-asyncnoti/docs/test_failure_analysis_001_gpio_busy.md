# asyncnoti 请求 GPIO 返回 EBUSY 测试失败分析

## 1. 测试目标

在 i.MX6ULL 目标板加载 `drivers/20-asyncnoti/asyncnoti.ko`，验证驱动能独占
KEY0 使用的 GPIO1_IO18，注册 GPIO 中断，并创建 `/dev/asyncnoti`。

## 2. 测试环境

- 目标平台：i.MX6ULL；
- 设备树节点：`/key`；
- GPIO：GPIO1_IO18，Linux GPIO 编号 18；
- 驱动：`drivers/20-asyncnoti/asyncnoti.c`；
- 设备名称：`asyncnoti`。

## 3. 实际结果

目标板执行 `insmod asyncnoti.ko` 时输出：

```text
/home # insmod asyncnoti.ko
[ 2701.715820] asyncnoti: failed to request GPIO 18: -16
[ 2701.815581] asyncnoti: failed to request GPIO 18: -16
insmod: can't insert 'asyncnoti.ko': Device or resource busy
```

错误码 `-16` 是 `-EBUSY`，表示 GPIO 18 已经被其他 GPIO consumer 请求并占用。
失败发生在 `asyncnoti_module_init()` 的 `gpio_request()`，早于 IRQ 映射、
`request_irq()` 和字符设备注册阶段。

## 4. 根因分析

`asyncnoti.c` 从 `/key` 读取 GPIO 18 后执行：

```c
ret = gpio_request(asyncnoti.key.gpio, asyncnoti.key.name);
if (ret) {
	pr_err("%s: failed to request GPIO %d: %d\n", ASYNCNOTI_NAME,
	       asyncnoti.key.gpio, ret);
	goto err_put_node;
}
```

Linux GPIO 框架同一时刻只允许一个普通 consumer 持有 GPIO 18。`blockio`、
`noblockio`、`asyncnoti` 以及其他复用 `/key` 节点的按键示例都请求同一个 GPIO，
因此它们不能同时加载。最常见的原因是：

1. `blockio.ko` 或 `noblockio.ko` 已经加载；
2. 上一次测试程序或其他驱动仍持有 GPIO；
3. 系统设备树中已有其他驱动绑定 GPIO1_IO18；
4. `asyncnoti.ko` 已经加载，重复加载时旧实例仍占用 GPIO；
5. 之前的驱动异常退出，未执行完整资源释放。

本次错误不是 IRQ 冲突。若 GPIO 申请成功而 IRQ 已被独占，通常应在后续
`request_irq()` 阶段看到 `-EBUSY`，而当前日志明确显示失败点为 GPIO 申请。

## 4.1 目标板复测证据

复测时目标板状态如下：

```text
/home # lsmod
Module                  Size  Used by    Tainted: G
blockio                 3629  0
/home # cat /proc/gpio
cat: can't open '/proc/gpio': No such file or directory
/home # cat /sys/kernel/debug/gpio
cat: can't open '/sys/kernel/debug/gpio': No such file or directory
/home # cat /proc/interrupts | grep -E 'KEY0|gpio'
 39:          6  gpio-mxc   9 Edge      gt9xx
 48:         15  gpio-mxc  18 Edge      KEY0
 49:          0  gpio-mxc  19 Edge      2190000.usdhc cd
```

`blockio` 已加载并使用 KEY0，IRQ 48 的中断名称也显示为 `KEY0`。目标板没有启用
`/proc/gpio` 和 `debugfs` GPIO 调试接口，因此无法通过这两个路径直接读取 GPIO owner，
但模块列表和 IRQ 记录已经足以确定当前占用者。

释放占用者后复测成功：

```text
/home # rmmod blockio
[ 3030.429490] blockio: unregistered
/home # insmod asyncnoti.ko
[ 3039.092765] asyncnoti: registered, GPIO=18 IRQ=48 major=248 minor=0
```

这次结果直接验证了：`blockio` 持有 GPIO18 时，`asyncnoti` 的 `gpio_request()` 返回
`-EBUSY`；卸载 `blockio` 释放 GPIO 后，`asyncnoti` 能够正常注册。根因已从推断变为
目标板实测确认。

## 5. 排查步骤

在目标板执行以下命令确认 GPIO owner 和已加载模块：

```bash
lsmod
cat /proc/gpio
cat /sys/kernel/debug/gpio
cat /proc/interrupts | grep -E 'KEY0|gpio'
```

如果存在旧的按键模块，先关闭测试程序，再卸载模块：

```bash
killall asyncnoti_test blockio_test noblockio_test 2>/dev/null
rmmod asyncnoti 2>/dev/null
rmmod blockio 2>/dev/null
rmmod noblockio 2>/dev/null
```

随后确认 GPIO 18 已不再显示为 busy，再重新加载：

```bash
insmod asyncnoti.ko
ls -l /dev/asyncnoti
```

若没有 `debugfs` 或 `/proc/gpio`，可通过 `lsmod`、卸载模块结果和重新加载日志判断
占用者。仍无法释放时，重启目标板可以清理遗留的运行时 GPIO owner。

## 6. 解决方案

### 6.1 测试环境解决方案

三个教学驱动共享同一个 KEY0 GPIO，测试时必须保证同一时间只加载一个：

```text
insmod asyncnoti.ko
运行 asyncnoti_test
停止测试程序
rmmod asyncnoti
再测试 blockio 或 noblockio
```

不要通过修改 GPIO 编号、强制绕过 `gpio_request()` 或忽略 `-EBUSY` 来规避问题；
这样会导致多个驱动同时操作同一硬件，产生不可预测的中断和 GPIO 状态竞争。

### 6.2 驱动资源释放检查

当前驱动在 `gpio_request()` 成功后，错误回滚和退出路径都会调用 `gpio_free()`，
因此单从 GPIO 申请流程看，资源释放是完整的。需要重点确认目标板实际运行的
`asyncnoti.ko` 是否与当前源码匹配，以及卸载命令是否真正成功。

此外，当前 `asyncnoti.c` 仍包含 `irq_dispose_mapping(asyncnoti.key.irq)`。该调用
不应由 GPIO IRQ consumer 销毁 `gpio-mxc` controller-owned legacy mapping；否则可能
在模块卸载后造成下一次加载出现 `request_irq()` 返回 `-ENOSYS`。这与本次
`gpio_request()` 的 `-EBUSY` 是不同阶段的两个问题，应分别修复。

## 7. 验证方法

清理占用者或重启目标板后，执行：

```bash
dmesg -c >/dev/null
insmod asyncnoti.ko
ls -l /dev/asyncnoti
./asyncnoti_test 1 /dev/asyncnoti
```

按下并释放 KEY0，预期用户态输出类似：

```text
SIGIO received, key value=0x01
```

测试结束后执行：

```bash
rmmod asyncnoti
insmod asyncnoti.ko
```

预期第二次加载仍能成功，且不再出现：

```text
failed to request GPIO 18: -16
```

## 7.1 异步通知功能复测结果

释放 `blockio` 后，目标板连续执行三组用户态测试，异步通知、按键消抖、事件读取和
资源释放均正常。

第一次单事件测试：

```text
asyncnoti: device opened, nonblock=1
asyncnoti: async notification enabled
Waiting for SIGIO from /dev/asyncnoti...
asyncnoti: KEY0 press confirmed after debounce
asyncnoti: KEY0 release confirmed, notifying readers
asyncnoti: key event delivered, value=0x01
PASS: SIGIO key event, value=0x01
asyncnoti: async notification disabled
asyncnoti: device closed
```

第二次单事件测试和第三次连续双事件测试同样通过：

```text
PASS: SIGIO key event, value=0x01
PASS: SIGIO key event, value=0x01
```

三次测试均记录了 `async notification enabled`、按键 press/release 消抖日志以及
`async notification disabled` 和 `device closed`，说明 `fasync` 注册、`SIGIO` 发送、
用户态 `read()` 消费和关闭时的异步通知清理流程均已验证。

## 8. 结论

本次 `insmod asyncnoti.ko` 失败的直接原因是 GPIO 18 已被其他 consumer 占用，
`gpio_request()` 返回 `-EBUSY`。应先查明并释放占用 GPIO 的模块或进程，确保
`blockio`、`noblockio` 和 `asyncnoti` 不同时运行。该问题不需要修改设备树 GPIO
编号，也不能通过忽略错误强行加载。

本次仅生成分析文档，未修改 `asyncnoti.c`；文中另行指出的
`irq_dispose_mapping()` 属于后续模块重载稳定性问题。

本次复测的 GPIO 占用问题已通过卸载 `blockio` 解决，异步通知功能测试结果为 PASS。
