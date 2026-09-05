# event0、event1 和 event2 输入设备 hexdump 对比分析

## 1. 测试背景

重启后 `keyinput` 驱动注册成功，并创建了一个 input 设备。目标板存在多个
`/dev/input/eventX` 节点，用户态必须打开与目标设备对应的节点。

根据此前 `/proc/bus/input/devices` 的设备信息：

| 节点 | 输入设备 | 典型用途 |
| --- | --- | --- |
| `event0` | `20cc000.snvs:snvs-powerkey` | 系统电源键 |
| `event1` | `goodix-ts` | Goodix 触摸屏 |
| `event2` | `keyinput` | GPIO KEY0 按键 |

事件编号由输入子系统动态分配，不能假设每次启动都固定不变。应始终通过设备名称和
`Handlers=... eventX` 关系确认节点。

## 2. 三个节点的区别

### 2.1 `/dev/input/event0`

`event0` 对应 SNVS 电源键。它上报的是电源键相关的 `EV_KEY` 事件，键值通常不是
`KEY_0`。因此使用 `keyinput_test` 读取该节点时，即使按下开发板 KEY0，也不会打印
`KEY0 press/release`；KEY0 事件实际发送到了 `event2`。

### 2.2 `/dev/input/event1`

`event1` 对应 Goodix 触摸屏。它通常上报 `EV_ABS`（绝对坐标）、`EV_SYN`（同步）以及
触摸按键等事件，数据格式和 GPIO KEY0 的 `EV_KEY/KEY_0` 不同。用
`keyinput_test` 读取该节点时，程序只处理 `type == EV_KEY && code == KEY_0`，因此不会
打印触摸屏的坐标事件。

### 2.3 `/dev/input/event2`

`event2` 对应本次 `keyinput` 驱动创建的虚拟输入设备。它由 GPIO18 的双边沿中断触发，
经过 10ms 消抖后上报 KEY0 按下和释放事件，是本次测试的正确节点。

## 3. event2 hexdump 数据解析

测试命令：

```bash
hexdump /dev/input/event2
```

一次完整的 KEY0 按下事件为：

```text
0000000 83b6 0000 e957 000e 0001 000b 0001 0000
0000010 83b6 0000 e957 000e 0000 0000 0000 0000
```

在 32 位 ARM 用户态中，一个 `struct input_event` 为 16 字节。按小端和字段顺序解释：

```text
time.tv_sec   = 0x000083b6
time.tv_usec  = 0x000ee957
type          = 0x0001  (EV_KEY)
code          = 0x000b  (KEY_0)
value         = 0x0001  (按下)
```

第一行是按键事件，第二行：

```text
type  = 0x0000  (EV_SYN)
code  = 0x0000
value = 0x0000
```

表示该次输入报告完成同步。释放事件对应：

```text
type  = 0x0001  (EV_KEY)
code  = 0x000b  (KEY_0)
value = 0x0000  (释放)
```

因此每次按键会产生四行十六进制输出：按下 `EV_KEY`、按下后的 `EV_SYN`、释放
`EV_KEY`、释放后的 `EV_SYN`。

## 4. 本次测试结果

目标板连续捕获了多组完整事件，例如：

```text
[ 1445.163427] keyinput: KEY0 pressed
0000000 83b6 0000 e957 000e 0001 000b 0001 0000
0000010 83b6 0000 e957 000e 0000 0000 0000 0000
[ 1445.313430] keyinput: KEY0 released
0000020 83b7 0000 f10e 0001 0001 000b 0000 0000
0000030 83b7 0000 f10e 0001 0000 0000 0000 0000
```

内核日志和原始输入数据一致：

- `type=1` 表示 `EV_KEY`；
- `code=11` 表示 `KEY_0`；
- `value=1` 表示按下；
- `value=0` 表示释放；
- 每个 KEY 事件后都有 `EV_SYN`。

这证明 GPIO 中断、定时器消抖、input 事件上报和 `/dev/input/event2` 读取链路均正常。

## 5. 验证方法

先确认设备归属：

```bash
cat /proc/bus/input/devices
```

找到包含以下内容的条目：

```text
N: Name="keyinput"
H: Handlers=event2
```

然后分别测试：

```bash
hexdump /dev/input/event0
hexdump /dev/input/event1
hexdump /dev/input/event2
```

只有与 `keyinput` 对应的节点会显示 KEY0 的 `type=1, code=11` 数据；其他节点显示的
是各自设备类型的事件，或者在没有对应操作时保持阻塞等待。

## 6. 结论

`event0` 是 SNVS 电源键，`event1` 是 Goodix 触摸屏，`event2` 是本次 `keyinput` GPIO
按键设备。只有 `/dev/input/event2` 会输出 KEY0 的 `EV_KEY` 事件。hexdump 数据已经
验证按下、释放及同步事件格式正确，input 按键驱动测试通过。
