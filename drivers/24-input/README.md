# 24-input

本示例演示 Linux input 子系统按键驱动。驱动匹配设备树 `/key` 节点的
`compatible = "atkalpha-key"`，通过 GPIO1_IO18 的双边沿中断检测 KEY0，使用 10ms
定时器消抖，并上报 `EV_KEY/KEY_0` 事件。

## 编译

```bash
make modules
make app
```

## 运行

```bash
insmod keyinput.ko
ls /dev/input/event*
./app/keyinput_test /dev/input/event1
```

按下并释放 KEY0 时，测试程序预期输出：

```text
KEY0 press, value=1
KEY0 release, value=0
```

卸载驱动：

```bash
rmmod keyinput
```

## 清理

```bash
make clean
```
