# keyinput Linux input 驱动重启后测试结果分析

## 1. 测试目标

验证目标板重启后 `keyinput.ko` 的设备树匹配、GPIO/IRQ 初始化、input 设备注册、
KEY0 消抖以及用户态 `input_event` 读取流程。

## 2. 驱动注册结果

目标板 `dmesg` 显示：

```text
[   42.295008] input: keyinput as /devices/virtual/input/input3
[   42.301591] keyinput key: registered, GPIO=18 IRQ=48 input=keyinput
```

这说明重启后 GPIO18 和 IRQ48 mapping 已恢复，设备树 `/key` 与驱动匹配成功，
input 核心创建了名为 `keyinput` 的输入设备。

## 3. 输入设备节点判断

测试时目标板存在：

```text
/dev/input/event0
/dev/input/event1
/dev/input/event2
```

`keyinput_test` 应使用 `keyinput` 对应的 `eventX`，不能仅凭编号猜测。可以通过：

```bash
cat /proc/bus/input/devices
```

查找 `N: Name="keyinput"`，并读取该条目 `Handlers=... eventX` 中的编号。

本次使用 `/dev/input/event2` 时获得了正确的 KEY0 事件；`event0` 和 `event1` 属于
其他输入设备，读取它们没有打印 KEY0 事件是正常的。内核仍会在所有按键操作时打印
`keyinput: KEY0 pressed/released`，因为这些日志来自驱动上报路径，与用户态打开哪个
event 节点无关。

## 4. 用户态测试结果

使用 `/dev/input/event2` 读取到多组完整事件：

```text
[ 1039.243421] keyinput: KEY0 pressed
KEY0 press, value=1
[ 1039.423412] keyinput: KEY0 released
KEY0 release, value=0
[ 1039.783404] keyinput: KEY0 pressed
KEY0 press, value=1
[ 1039.953402] keyinput: KEY0 released
KEY0 release, value=0
[ 1040.103403] keyinput: KEY0 pressed
KEY0 press, value=1
[ 1040.243395] keyinput: KEY0 released
KEY0 release, value=0
[ 1040.383396] keyinput: KEY0 pressed
KEY0 press, value=1
[ 1040.563404] keyinput: KEY0 released
KEY0 release, value=0
```

驱动侧还记录了更早的多组按键事件，证明双边沿中断和消抖定时器持续工作：

```text
keyinput: KEY0 pressed
keyinput: KEY0 released
```

每次按键均产生一个 `EV_KEY` 按下事件（`value=1`）和一个释放事件（`value=0`），
测试程序能够正确解析 `code=KEY_0`。

## 5. hexdump 验证说明

对 `/dev/input/event0` 和 `/dev/input/event1` 执行 `hexdump` 没有输出，是因为这两个
节点不是 `keyinput` 对应的输入设备，或者测试期间没有对应设备事件。应先根据
`/proc/bus/input/devices` 找到 `event2`，再执行：

```bash
hexdump /dev/input/event2
```

按键后应能看到 `input_event` 原始数据，其中 `type=EV_KEY`、`code=KEY_0`，随后通常
还会有 `EV_SYN` 同步事件。

## 6. 测试结果

| 测试项目 | 实际结果 | 状态 |
| --- | --- | --- |
| 设备树和 platform 匹配 | 注册 `keyinput`，GPIO18、IRQ48 | PASS |
| input 设备创建 | 创建 `/dev/input/event2` | PASS |
| KEY0 按下上报 | `KEY0 press, value=1` | PASS |
| KEY0 释放上报 | `KEY0 release, value=0` | PASS |
| 多次按键 | 多组按下/释放事件均正确 | PASS |
| 消抖处理 | 内核持续输出稳定 press/release | PASS |

## 7. 结论

重启后 `keyinput` 驱动已成功注册并通过功能测试。正确的输入设备节点为
`/dev/input/event2`，用户态能够收到完整的 KEY0 按下和释放事件，测试结果为 PASS。
此前对 `event0`、`event1` 的无输出现象属于选择了其他输入设备，并非驱动故障。
