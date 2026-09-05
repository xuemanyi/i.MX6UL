# keyinput probe 返回 EBUSY 测试失败分析

## 1. 测试日志

目标板执行：

```text
/home # insmod keyinput.ko
[26165.500984] keyinput: probe of key failed with error -16
/home # ls /dev/input/event*
/dev/input/event0  /dev/input/event1
/home # ./keyinput_test /dev/input/event1

^C
/home # rmmod keyinput
/home # dmesg
[26165.500984] keyinput: probe of key failed with error -16
```

## 2. 现象判断

`-16` 对应 `-EBUSY`，表示 probe 初始化过程中申请的资源已被占用。当前驱动在
`keyinput_probe()` 中依次申请 `/key` 节点的 `key-gpio`、配置 GPIO 输入、映射 IRQ，
再申请 IRQ 和注册 input 设备；因此该错误发生在这些资源申请阶段之一。

当前日志只包含 platform core 的汇总错误，没有打印具体资源申请点，因此仅凭该行不能
绝对区分 GPIO 申请和 IRQ 申请。结合多个驱动共享 KEY0 GPIO 的测试环境，最可能的
直接失败点是：

```c
ret = gpio_request(dev->gpio, KEYINPUT_NAME);
```

KEY0 使用 GPIO1_IO18（Linux GPIO 18）。`blockio`、`noblockio`、`asyncnoti` 和其他
按键示例也使用同一个 GPIO，同一时刻只能有一个 GPIO consumer 持有它。

如需精确定位，建议在 `gpio_request()`、`request_irq()` 和 `input_register_device()`
失败分支分别打印返回值；后续日志应显示具体失败阶段，而不是只依赖 platform core
的 `probe ... error` 汇总信息。

## 3. `/dev/input/event1` 的含义

`/dev/input/event1` 的存在不能证明 `keyinput.ko` probe 成功。设备节点可能属于：

- 系统原有的触摸屏或其他输入设备；
- 之前成功注册的按键驱动实例；
- 驱动失败后遗留的用户态设备节点（需结合 `dmesg` 和 `/proc/bus/input/devices` 判断）。

本次 `insmod` 已明确收到 platform core 的 `probe of key failed`，说明驱动没有完成
input 设备注册；直接读取 `event1` 阻塞且没有 KEY0 输出是符合该结果的。

## 4.1 后续复测：错误变为 ENOSYS

后续排查显示系统中没有已加载的按键模块：

```text
/home # lsmod
Module                  Size  Used by    Tainted: G
```

`/proc/bus/input/devices` 中只有系统电源键和 Goodix 触摸屏，`/proc/interrupts` 中也不再
出现 IRQ 48。此时再次加载得到：

```text
/home # insmod keyinput.ko
[26353.837660] keyinput: probe of key failed with error -38
```

`-38` 对应 `-ENOSYS`，与前一次的 `-EBUSY` 不是同一个阶段：GPIO 已经不再被模块占用，
但 IRQ 48 的 descriptor 已经没有有效 irqchip。此前日志还记录了：

```text
[26285.392656] asyncnoti: unregistered
[26286.472279] keyinput: probe of key failed with error -38
```

这表明 `asyncnoti` 卸载后，后续 `keyinput` 加载立即失败。`20-asyncnoti/asyncnoti.c`
的 remove 和错误回滚路径仍调用 `irq_dispose_mapping()`，销毁了由 `gpio-mxc` 控制器
拥有的 GPIO legacy IRQ mapping，导致 `request_irq(48)` 返回 `-ENOSYS`。

## 5. 排查步骤

先查看已加载模块和 input 设备归属：

```bash
lsmod
cat /proc/bus/input/devices
cat /proc/interrupts | grep -E 'KEY0|gpio'
```

停止可能持有按键资源的测试程序，并卸载复用 KEY0 的模块：

```bash
killall keyinput_test blockio_test noblockio_test asyncnoti_test 2>/dev/null
rmmod blockio 2>/dev/null
rmmod noblockio 2>/dev/null
rmmod asyncnoti 2>/dev/null
rmmod keyinput 2>/dev/null
```

确认没有其他驱动占用 GPIO18 后，再加载：

```bash
insmod keyinput.ko
cat /proc/bus/input/devices
ls -l /dev/input/event*
```

若仍返回 `-16`，检查设备树中是否有其他节点或驱动申请 GPIO1_IO18；无法确定占用者
时，重启目标板可清除遗留的运行时 GPIO owner。

## 6. 解决方案

### 6.1 测试环境方案

`keyinput` 与 `blockio`、`noblockio`、`asyncnoti` 共享 KEY0 GPIO，测试时必须保证
同一时间只加载一个：

```text
insmod keyinput.ko
运行 keyinput_test
停止 keyinput_test
rmmod keyinput
再测试其他按键驱动
```

不能通过忽略 `gpio_request()` 错误或修改 GPIO 编号强行加载，否则多个驱动会同时操作
同一硬件。

### 6.2 驱动资源释放方案

当前 `keyinput_remove()` 会释放 IRQ、删除消抖定时器、注销 input 设备并释放 GPIO。
如果确认 `rmmod keyinput` 成功，GPIO 应恢复为空闲。若使用其他复用 KEY0 的驱动，
也应检查其退出路径是否正确释放 GPIO 和 IRQ。

## 7. 重新验证

成功加载后，`/proc/bus/input/devices` 应出现名称为 `keyinput` 的设备，并将其对应的
`eventX` 传给测试程序：

```bash
./keyinput_test /dev/input/eventX
```

按下并释放 KEY0，预期输出：

```text
KEY0 press, value=1
KEY0 release, value=0
```

## 8. 结论

本次测试先出现 `-EBUSY`，后续在卸载 `asyncnoti` 后出现 `-ENOSYS`，分别对应 GPIO
占用和 IRQ mapping 被错误销毁两个问题。应保证 KEY0 consumer 不并发运行，并从
`20-asyncnoti/asyncnoti.c`（以及其他同类 consumer）删除 `irq_dispose_mapping()`，
只释放自身注册的 IRQ handler。修复 mapping 后重启目标板，再重新加载 `keyinput.ko`。
`event1` 的存在不是本次 probe 成功的依据。
