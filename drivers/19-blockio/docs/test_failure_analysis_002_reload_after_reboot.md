# blockio 重启后重复加载失败分析与修复记录

## 1. 测试目标

验证目标板重启后 `blockio` 的完整生命周期：首次加载、阻塞读取、KEY0 按键事件、模块卸载以及再次加载。

## 2. 实际测试日志

首次加载和按键测试成功：

```text
/home # insmod blockio.ko
[  722.195295] blockio: registered, GPIO=18 IRQ=48 major=248 minor=0
/home # ./blockio_test 1 /dev/blockio
[  746.931063] blockio: device opened
Waiting for KEY0 event...[  746.935297] blockio: waiting for a key event
[  751.693419] blockio: KEY0 press confirmed after debounce
[  752.083411] blockio: KEY0 release confirmed, waking readers
[  752.089025] blockio: key event delivered, value=0x01
PASS: KEY0 released, value=0x01
/home # rmmod blockio.ko
[  759.544930] blockio: unregistered
```

重启后的第二次加载失败：

```text
/home # insmod blockio.ko
[  771.854961] blockio: failed to request IRQ 48: -38
[  771.924784] blockio: failed to request IRQ 48: -38
insmod: can't insert 'blockio.ko': kernel does not support requested operation
```

## 3. 现象判断

首次加载、设备创建、按键消抖、阻塞唤醒和键值传递均成功，说明设备树节点、GPIO、IRQ
解析和中断处理函数本身有效。卸载也成功完成，问题只在卸载后再次加载时出现。

`-38` 是 `-ENOSYS`。Linux 4.1.15 的 `kernel/irq/manage.c::__setup_irq()` 在 IRQ
descriptor 使用 `no_irq_chip` 时返回该错误。因此这不是常见的 IRQ 占用冲突；IRQ 被
其他驱动占用时通常返回 `-EBUSY`。

## 4. 根因

`19-blockio/blockio.c` 原先在两个路径调用：

```c
irq_dispose_mapping(blockio.key.irq);
```

调用位置是：

1. `request_irq()` 失败后的错误回滚路径；
2. `blockio_module_exit()` 模块卸载路径。

`irq_of_parse_and_map()` 返回的 IRQ 48 属于 i.MX GPIO 控制器在 probe 阶段建立的
legacy IRQ domain。该 mapping 和 irqchip 由 `gpio-mxc` 控制器拥有，不属于
`blockio` consumer。`irq_dispose_mapping()` 会解除 domain 关联、清除 irqchip 并释放
descriptor。于是第一次 `rmmod` 后，第二次 `insmod` 虽然仍解析出 IRQ 48，但
`request_irq()` 发现其 irqchip 已是 `no_irq_chip`，返回 `-ENOSYS`。

## 5. 永久修复

已修改 `19-blockio/blockio.c`：

- 将 `request_irq()` 失败后的跳转目标从 `err_dispose_irq` 改为 `err_free_gpio`；
- 删除错误回滚路径中的 `irq_dispose_mapping()`；
- 删除模块退出路径中的 `irq_dispose_mapping()`；
- 保留 `free_irq()`、`del_timer_sync()`、`gpio_free()` 和 `of_node_put()`，只释放
  `blockio` 自身申请或注册的资源。

修复后的资源生命周期为：

```text
irq_of_parse_and_map()  → 获取 controller-owned mapping，不销毁
request_irq()           → 注册 blockio handler
free_irq()              → 释放 blockio handler
del_timer_sync()        → 等待并删除消抖定时器
gpio_free()             → 释放 GPIO
of_node_put()           → 释放设备树节点引用
```

## 6. 编译与验证

在主机重新编译：

```bash
cd /home/gs/code/i.MX6UL/drivers/19-blockio
make clean
make modules
make app
```

部署新的 `blockio.ko` 和 `app/blockio_test` 后，先重启目标板一次，清除旧版本运行时已
破坏的 IRQ descriptor，再执行：

```bash
dmesg -c >/dev/null
insmod blockio.ko
./blockio_test 1 /dev/blockio
rmmod blockio.ko
insmod blockio.ko
rmmod blockio.ko
```

预期结果：首次加载和第二次加载均打印 `registered`，按键测试输出 `PASS`，且不再出现
`failed to request IRQ 48: -38`。

## 7. 影响范围与限制

`17-interrupt`、`18-noblockio` 和 `20-asyncnoti` 也复用了 `/key` 节点；如果这些模块
仍保留同样的 `irq_dispose_mapping()`，它们卸载后仍可能破坏共享 GPIO IRQ mapping。
因此应对这些 consumer 驱动执行同样的所有权检查，不能只依赖 `blockio` 单独修复。

本次修复已完成源码修改和静态检查；目标板上的重新编译、部署及重复加载验证待执行。

## 8. 结论

测试已经证明：重启后第一次加载成功，`rmmod` 后第二次加载失败，故障与
`blockio_module_exit()` 销毁 GPIO controller-owned IRQ mapping 的时序完全一致。
删除 consumer 对 `irq_dispose_mapping()` 的调用，只释放自身 IRQ handler，是该问题的
永久修复方案。
