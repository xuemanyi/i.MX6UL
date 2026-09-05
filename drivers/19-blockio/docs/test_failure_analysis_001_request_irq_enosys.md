# blockio 请求 GPIO IRQ 返回 ENOSYS 测试失败分析

## 1. 测试目标

在 i.MX6ULL 目标板加载 `drivers/19-blockio/blockio.ko`，确认驱动能够取得
GPIO1_IO18 对应的 Linux IRQ，注册 KEY0 双边沿中断，并创建 `/dev/blockio`。

## 2. 测试环境

- 目标平台：i.MX6ULL；
- 内核源码版本：Linux 4.1.15；
- 驱动：`drivers/19-blockio/blockio.c`；
- 设备树节点：`/key`；
- GPIO：GPIO1_IO18；
- 中断类型：`IRQ_TYPE_EDGE_BOTH`；
- 设备名称：`blockio`。

设备树相关配置为：

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

## 3. 测试步骤

目标板执行：

```bash
ls
insmod blockio.ko
```

## 4. 实际结果

目标板首次测试日志：

```text
blockio.ko      blockio_test    noblockio.ko    noblockio_test
/home # insmod blockio.ko
[ 2744.162749] blockio: failed to request IRQ 48: -38
[ 2744.273775] blockio: failed to request IRQ 48: -38
insmod: can't insert 'blockio.ko': kernel does not support requested operation
```

驱动已经成功把设备树 interrupt specifier 解析为 Linux IRQ 48，但
`request_irq(48, ...)` 返回 `-38`。

`38` 对应 `ENOSYS`，BusyBox 将其显示为：

```text
kernel does not support requested operation
```

再次执行 `insmod blockio.ko` 后，目标板仍然得到相同错误：

```text
/home # insmod blockio.ko
[ 4325.327556] blockio: failed to request IRQ 48: -38
[ 4325.440994] blockio: failed to request IRQ 48: -38
insmod: can't insert 'blockio.ko': kernel does not support requested operation
/home # dmesg
[ 4325.327556] blockio: failed to request IRQ 48: -38
[ 4325.440994] blockio: failed to request IRQ 48: -38
```

该复测说明故障可以稳定复现，且仍发生在字符设备注册之前的 IRQ handler 注册阶段。
`dmesg` 只是再次显示内核日志缓冲区中已有的两条记录，不表示执行 `dmesg` 时驱动又
调用了一次 `request_irq()`。

两条相同驱动日志说明日志缓冲区中记录了两次初始化失败；仅根据现有输出无法确定是
命令被执行两次、脚本重试还是历史日志未清理。该重复现象不改变 `-ENOSYS` 的根因
分析，可在复测前使用 `dmesg -c` 清空日志后确认实际尝试次数。

## 5. 预期结果

模块应成功加载，并打印类似：

```text
blockio: registered, GPIO=18 IRQ=48 major=<major> minor=<minor>
```

系统应创建：

```text
/dev/blockio
```

执行 `blockio_test` 后，进程应阻塞等待 KEY0。按下并释放按键后返回键值 `0x01`。

## 6. 错误码源码定位

### 6.1 `request_irq()` 调用关系

当前内核中：

```text
blockio_module_init()
    → request_irq()
    → request_threaded_irq()
    → __setup_irq()
```

`include/linux/interrupt.h` 中的 `request_irq()` 是
`request_threaded_irq()` 的包装函数。

### 6.2 `-ENOSYS` 的直接返回条件

`kernel/irq/manage.c::__setup_irq()` 包含：

```c
if (desc->irq_data.chip == &no_irq_chip)
	return -ENOSYS;
```

对于当前非线程化 `request_irq()` 路径，结合 i.MX GPIO irqchip 实现，日志中的
`-38` 表明 IRQ 48 对应的 descriptor 在注册 handler 时没有有效 irqchip，而是
`no_irq_chip`。

这不是以下常见错误：

- IRQ 已被其他驱动独占通常返回 `-EBUSY`，不是 `-ENOSYS`；
- IRQ 参数或 flag 非法通常返回 `-EINVAL`；
- 内存不足返回 `-ENOMEM`；
- 设备树 interrupt specifier 无法映射时，`irq_of_parse_and_map()` 返回 0，驱动会在
  `request_irq()` 之前返回 `-EINVAL`。

## 7. 根因分析

### 7.1 i.MX GPIO 使用 legacy IRQ domain

`drivers/gpio/gpio-mxc.c::mxc_gpio_probe()` 为每个 GPIO bank 执行：

```c
irq_base = irq_alloc_descs(-1, 0, 32, numa_node_id());
port->domain = irq_domain_add_legacy(np, 32, irq_base, 0,
				     &irq_domain_simple_ops, NULL);
mxc_gpio_init_gc(port, irq_base);
```

其含义是 GPIO controller 初始化时一次性为整个 bank 预分配 32 个 Linux IRQ，
建立 legacy mapping，并通过 `mxc_gpio_init_gc()` 给这些 descriptor 安装
`gpio-mxc` irqchip。

因此，这组映射的生命周期属于 GPIO controller，不属于后续加载的 KEY0 consumer
模块。

### 7.2 consumer 错误销毁 controller 拥有的 mapping

当前 `blockio.c` 在错误回滚和模块退出路径调用：

```c
irq_dispose_mapping(blockio.key.irq);
```

`kernel/irq/irqdomain.c::irq_dispose_mapping()` 会进一步执行：

```text
irq_domain_disassociate()
    → 设置 IRQ_NOREQUEST
    → irq_set_chip_and_handler(irq, NULL, NULL)
    → 清除 reverse mapping
irq_free_desc()
```

`irq_set_chip_and_handler(irq, NULL, NULL)` 最终使 descriptor 不再关联有效
`gpio-mxc` irqchip。对动态、由单个 consumer 创建并拥有的 mapping，适当时可以释放；
但这里的 mapping 是 `gpio-mxc` 在 probe 阶段为整个 legacy domain 预创建的，普通
按键 consumer 不应在卸载时销毁它。

### 7.3 与测试现象的对应关系

若此前加载并卸载过包含同样清理代码的按键模块，典型过程为：

```text
系统启动
    → gpio-mxc 为 GPIO1 bank 创建 IRQ descriptors 并安装 irqchip
    → 17-interrupt、18-noblockio 或 19-blockio 加载成功
    → 模块卸载时调用 irq_dispose_mapping()
    → GPIO1_IO18 对应 descriptor 的 irqchip 被移除
    → 再次加载 blockio
    → irq_of_parse_and_map() 得到 IRQ 编号
    → request_irq() 发现 no_irq_chip
    → 返回 -ENOSYS (-38)
```

这能够同时解释：

- GPIO 解析已经成功；
- IRQ 编号不是 0；
- `request_irq()` 返回的不是资源冲突错误 `-EBUSY`；
- 同一模块或其他 KEY0 模块在卸载后重新加载可能失败；
- 重启后第一次加载有可能恢复正常。

因此，基于当前源码，根因判定为：

> KEY0 consumer 错误调用 `irq_dispose_mapping()`，破坏了由 `gpio-mxc` legacy IRQ
> domain 持有的 GPIO1_IO18 映射，导致后续 `request_irq()` 看到 `no_irq_chip` 并
> 返回 `-ENOSYS`。

## 8. 根因确认方法

### 8.1 推荐复现流程

先重启目标板，恢复 `gpio-mxc` 在启动阶段建立的完整 IRQ domain，然后执行：

```bash
dmesg -c >/dev/null
insmod blockio.ko
cat /proc/interrupts | grep KEY0
rmmod blockio
insmod blockio.ko
dmesg | tail -n 30
```

若第一次加载成功、卸载后的第二次加载返回 `-38`，即可直接确认退出路径破坏 IRQ
mapping。

也可以使用另一个包含 `irq_dispose_mapping()` 的 KEY0 模块交叉验证：

```bash
insmod imx6uirq.ko
rmmod imx6uirq
insmod blockio.ko
```

若最后一步返回 `-38`，说明故障跨模块存在，受影响对象是共享 GPIO irqdomain，
而不是 `blockio` 的字符设备资源。

### 8.2 排除资源占用

在加载前检查：

```bash
cat /proc/interrupts | grep -E 'KEY0|gpio'
lsmod
```

如果是另一个模块仍占用相同 IRQ，正常预期错误是 `-EBUSY`。当前日志返回
`-ENOSYS`，应优先检查 irqchip/mapping 生命周期，而不是添加 `IRQF_SHARED` 规避。

## 9. 解决方案

### 9.1 临时恢复

重启目标板，使 `gpio-mxc` 重新 probe，并重建 GPIO bank 的 legacy IRQ domain：

```bash
reboot
```

重启只能恢复当前运行状态，不能修复驱动。若模块退出时继续调用
`irq_dispose_mapping()`，下一次加载仍会复现。

### 9.2 永久修复

从 `drivers/19-blockio/blockio.c` 的以下两个位置删除
`irq_dispose_mapping(blockio.key.irq)`：

1. `err_dispose_irq` 错误回滚路径；
2. `blockio_module_exit()` 模块退出路径。

修复后的中断资源处理应为：

```text
初始化：
irq_of_parse_and_map()
    → request_irq()

退出或 request_irq() 成功后的回滚：
free_irq()
    → del_timer_sync()
    → gpio_free()
    → of_node_put()
```

不再由 consumer 调用 `irq_dispose_mapping()`，GPIO IRQ mapping 留给拥有它的
`gpio-mxc` controller 管理。

如果 `request_irq()` 本身失败，驱动同样不应销毁该 controller-owned mapping，
应直接继续释放 GPIO 和设备树节点。

对应当前源码，错误标签应调整为：

```c
err_free_irq:
	free_irq(blockio.key.irq, &blockio);
	del_timer_sync(&blockio.timer);
err_free_gpio:
	gpio_free(blockio.key.gpio);
err_put_node:
	of_node_put(blockio.node);
	blockio.node = NULL;
	return ret;
```

`request_irq()` 失败分支应直接跳到 `err_free_gpio`：

```c
if (ret) {
	pr_err("%s: failed to request IRQ %d: %d\n", BLOCKIO_NAME,
	       blockio.key.irq, ret);
	goto err_free_gpio;
}
```

模块退出路径同样删除以下语句：

```c
irq_dispose_mapping(blockio.key.irq);
```

注意：当前目标板中的 IRQ descriptor 已经处于损坏状态，仅替换 `.ko` 不能恢复它。
修改并重新编译驱动后仍需重启目标板一次，再开始加载验证。

### 9.3 同类驱动同步修复

以下教学模块复用同一个 `/key` 节点并采用了相同 mapping 清理方式，也需要同步检查：

```text
drivers/17-interrupt/imx6uirq.c
drivers/18-noblockio/noblockio.c
drivers/19-blockio/blockio.c
drivers/20-asyncnoti/asyncnoti.c
```

只修复 `19-blockio` 可以解决其自身卸载后重载问题，但如果先加载再卸载其他未修复
模块，仍可能破坏 GPIO1_IO18 mapping，使随后加载的 `blockio` 再次返回 `-38`。

### 9.4 不推荐方案

不建议采用以下方式：

- 不应把中断改为 `IRQF_SHARED`，因为当前错误不是 `-EBUSY`；
- 不应把 `-ENOSYS` 忽略后继续创建字符设备，否则驱动永远收不到按键事件；
- 不应只删除失败日志或修改测试程序制造 PASS；
- 不应在每次加载模块时重新初始化 `gpio-mxc` controller；
- 不应用固定 IRQ 48 替代设备树映射，Linux virq 编号不属于稳定 ABI。

## 10. 修复后验证步骤

编译并部署修复后的驱动，然后重启一次目标板清除已经损坏的运行时 mapping。

### 10.1 编译

```bash
cd /home/gs/code/i.MX6UL/drivers/19-blockio
make modules
make app
```

### 10.2 首次加载和按键验证

```bash
insmod blockio.ko
ls -l /dev/blockio
cat /proc/interrupts | grep KEY0
./app/blockio_test 1 /dev/blockio
```

按下并释放 KEY0，预期输出：

```text
Waiting for KEY0 event...
PASS: KEY0 released, value=0x01
```

### 10.3 重复加载验证

至少连续执行三轮：

```bash
rmmod blockio
insmod blockio.ko
```

每轮都必须加载成功，不得再次出现：

```text
failed to request IRQ 48: -38
```

### 10.4 跨模块验证

同步修复同类驱动后，依次执行：

```bash
insmod imx6uirq.ko
rmmod imx6uirq
insmod noblockio.ko
rmmod noblockio
insmod blockio.ko
rmmod blockio
insmod asyncnoti.ko
rmmod asyncnoti
```

各模块不能同时加载，但顺序加载和卸载均应成功。该测试用于确认任何一个 consumer
都不会再次破坏共享 GPIO IRQ mapping。

## 11. 当前验证状态

已完成：

- 根据目标板日志确认错误发生在 `request_irq(48, ...)`；
- 确认 `-38` 为 `-ENOSYS`；
- 定位 Linux 4.1.15 `__setup_irq()` 的 `no_irq_chip` 返回条件；
- 核对 `gpio-mxc` 使用 `irq_domain_add_legacy()` 预创建 bank mapping；
- 核对 `irq_dispose_mapping()` 会解除 domain 关联并移除 irqchip；
- 记录第二次目标板复测结果，确认 `request_irq(48)` 仍稳定返回 `-ENOSYS`；
- 检查 `20-asyncnoti` 的源文件仍完整存在。

未完成：

- 本次只生成分析文档，尚未修改 `blockio.c`；
- 尚未在目标板执行“重启后首次加载、卸载后再次加载”的确认实验；
- 尚未在目标板验证永久修复；
- 尚未同步修改其他 KEY0 consumer 驱动。

## 12. 结论

`request_irq(48)` 返回 `-38` 不是普通 IRQ 占用冲突，而是 IRQ 48 的 descriptor
缺少有效 irqchip。当前源码中，按键 consumer 在卸载和错误回滚时调用
`irq_dispose_mapping()`，与 i.MX GPIO legacy IRQ domain 的所有权不匹配，是最符合
源码和测试现象的根因。

短期可通过重启恢复 GPIO irqdomain；永久方案是删除 consumer 中的
`irq_dispose_mapping()`，只用 `free_irq()` 释放本模块注册的 handler，并同步修复
所有复用 `/key` 的教学驱动。修复后必须验证同一模块多次卸载/加载以及不同 KEY0
模块顺序切换。
