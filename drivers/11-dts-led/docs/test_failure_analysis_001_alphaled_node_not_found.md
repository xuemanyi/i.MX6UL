# `/alphaled` 设备树节点缺失测试失败分析

## 1. 测试目标

验证 `dtsled.ko` 能从当前运行内核的设备树中找到根节点 `/alphaled`，映射
五组 `reg` 资源，并创建 `/dev/dtsled`。

相关文件：

- 驱动：`drivers/11-dts-led/dtsled.c`；
- 实际设备树：`kernel/arch/arm/boot/dts/imx6ull-14x14-nand-4.3-800x480-c.dts`；
- 构建脚本：`kernel/build_all.sh`；
- 内核模块：`drivers/11-dts-led/dtsled.ko`。

## 2. 测试环境与步骤

- 目标平台：i.MX6ULL；
- 驱动节点路径：`/alphaled`；
- 驱动匹配字符串：`atkalpha-led`。

目标板执行：

```bash
insmod dtsled.ko
dmesg | tail -n 20
```

## 3. 实际结果

```text
dtsled: device tree node /alphaled not found
insmod: can't insert 'dtsled.ko': No such device
```

模块加载失败，`/dev/dtsled` 未创建。

## 4. 预期结果

内核日志应包含类似信息：

```text
dtsled: registered, major=<major> minor=0
```

同时应生成 `/dev/dtsled`。

## 5. 源码调用路径

`dtsled_init()` 使用固定绝对路径查找节点：

```c
dtsled.node = of_find_node_by_path("/alphaled");
```

当前运行内核展开后的设备树不存在该节点时，函数返回 `NULL`。驱动随后返回
`-ENODEV`，用户态将该错误显示为 `No such device`。错误发生在寄存器映射和
字符设备注册之前，与 `/dev/dtsled` 权限、主设备号和测试程序无关。

## 6. 根因分析

### 6.1 直接原因

目标板当前运行的设备树中不存在根节点 `/alphaled`。

### 6.2 已确认的工程根因

`build_all.sh` 固定设置：

```bash
DTB="imx6ull-14x14-nand-4.3-800x480-c.dtb"
```

该 DTB 对应的顶层源文件包含：

```dts
#include "imx6ull-14x14-evk-gpmi-weim.dts"
```

实际继承关系为：

```text
imx6ull-14x14-nand-4.3-800x480-c.dts
└── imx6ull-14x14-evk-gpmi-weim.dts
    └── imx6ull-14x14-evk.dts
```

此前添加 `alphaled` 的 `imx6ull-alientek-emmc.dts` 不在这条继承链中。虽然
`imx6ull-alientek-nand.dts` 会包含 `imx6ull-alientek-emmc.dts`，但
`build_all.sh` 没有编译 `imx6ull-alientek-nand.dtb`，因此该关系对当前 NAND
镜像不生效。

现有输出文件 `tmp/imx6ull-14x14-nand-4.3-800x480-c.dtb` 已通过 `strings` 和
反编译检查，确认不包含 `alphaled` 或 `atkalpha-led`。因此，本次失败不是推测
的 DTB 部署问题，而是节点添加到了不会被 `build_all.sh` 编译的 DTS 文件。

设备树在启动期间由 bootloader 传递给内核。运行中替换 `.dtb` 文件不会改变
当前内核已经展开的设备树，必须重新启动目标板。

### 6.3 修正源码后仍需排除的问题

- U-Boot 的 `fdtfile` 或 `bootcmd` 指向另一个 DTB；
- DTB 被复制到了错误的分区或目录；
- 实际从另一块 SD、eMMC 或 NAND 分区启动；
- 启动介质中仍是旧 DTB；
- 产品实际构建和加载的不是 `imx6ull-alientek-emmc.dts`。

## 7. 本地验证证据

对 `imx6ull-alientek-emmc.dts` 预处理并使用仓库内 `dtc` 编译后，反编译结果
包含：

```dts
alphaled {
	#address-cells = <0x1>;
	#size-cells = <0x1>;
	compatible = "atkalpha-led";
	status = "okay";
	reg = <0x20c406c 0x4 0x20e0068 0x4 0x20e02f4 0x4
	       0x209c000 0x4 0x209c004 0x4>;
};
```

这只能证明节点本身语法正确，不能证明 `build_all.sh` 的 NAND 输出包含该节点。
对实际输出执行：

```bash
strings tmp/imx6ull-14x14-nand-4.3-800x480-c.dtb | \
	grep -E 'alphaled|atkalpha-led'
```

没有任何输出，证实 NAND DTB 缺少该节点。

## 8. 解决方案

### 8.1 节点的最终放置位置

实际 NAND DTS 最终继承公共板级文件：

```text
imx6ull-14x14-nand-4.3-800x480-c.dts
└── imx6ull-14x14-evk-gpmi-weim.dts
    └── imx6ull-14x14-evk.dts
```

因此，可以把节点添加到 `imx6ull-14x14-evk.dts` 的根节点中。本次已按指定
要求添加：

```dts
/ {
	alphaled {
		#address-cells = <1>;
		#size-cells = <1>;
		compatible = "atkalpha-led";
		status = "okay";
		reg = <0x020c406c 0x04
		       0x020e0068 0x04
		       0x020e02f4 0x04
		       0x0209c000 0x04
		       0x0209c004 0x04>;
	};
};
```

该文件同时被 eMMC、BT/Wi-Fi、USB 认证以及所有 NAND 显示配置直接或间接
包含，因此这些 DTB 都会继承 `/alphaled`。若节点只应用于当前 4.3 英寸 NAND
产品，放到 `imx6ull-14x14-nand-4.3-800x480-c.dts` 的影响范围会更小。

### 8.2 GPIO1_IO03 资源冲突

公共 DTS 已有 `gpio-leds` 的 `led1` 使用 GPIO1_IO03，并配置了 `heartbeat`
触发器。`dtsled.ko` 直接映射寄存器，没有通过 GPIO descriptor 申请引脚，内核
不会阻止两个驱动同时修改该 GPIO。可能表现为执行 `on` 或 `off` 后，LED 又被
心跳触发器改变。

本次未擅自禁用现有系统 LED。测试前可在目标板临时关闭 heartbeat：

```bash
echo none >/sys/class/leds/sys-led/trigger
```

如果仍有竞争，应在产品专用 DTS 中禁用或移除 `led1`。这会改变现有系统心跳灯
行为，应作为单独功能修改处理。

### 8.3 使用实际脚本重新编译

```bash
cd /home/gs/code/i.MX6UL/kernel
./build_all.sh
```

预期生成：

```text
tmp/imx6ull-14x14-nand-4.3-800x480-c.dtb
```

当前主机构建树另有权限问题：部分 `scripts/mod/` 生成文件属于
`nobody:nogroup`，普通用户构建可能出现：

```text
cannot create scripts/mod/elfconfig.h: Permission denied
```

应由源码树所有者修复这些生成文件的所有权，或使用权限正常的干净构建目录。
不要长期使用 root 编译来掩盖所有权问题。

### 8.4 复制前验证新 NAND DTB

```bash
scripts/dtc/dtc -I dtb -O dts \
	tmp/imx6ull-14x14-nand-4.3-800x480-c.dtb | \
	sed -n '/alphaled {/,/};/p'
```

输出必须包含 `compatible = "atkalpha-led"`、`status = "okay"` 和五组寄存器
资源。也可快速检查：

```bash
strings tmp/imx6ull-14x14-nand-4.3-800x480-c.dtb | \
	grep -E 'alphaled|atkalpha-led'
```

### 8.5 确认 NAND 启动流程使用的 DTB

在 U-Boot 命令行检查：

```text
printenv fdtfile
printenv bootcmd
printenv bootargs
```

若环境使用 `fdtfile`，其值应指向 `imx6ull-14x14-nand-4.3-800x480-c.dtb`。
NAND 系统也可能把 DTB 写入固定裸分区并由 `bootcmd` 按偏移读取，此时不能只把
文件复制到根文件系统，必须使用项目对应的 NAND 烧写流程更新设备树分区。

### 8.6 更新 NAND 设备树并重启

使用当前项目的 NAND 烧写工具，把新 DTB 写入 U-Boot 实际读取的 NAND 分区或
偏移。写入前应备份原 DTB，并根据 U-Boot 环境确认目标位置；无法从现有日志
安全推断具体 `nand erase`/`nand write` 地址，因此本文不提供未经确认的裸 NAND
写命令。更新后执行：

```bash
sync
reboot
```

仅重新执行 `insmod` 不会加载磁盘上的新 DTB。

### 8.7 重启后验证运行中的设备树

加载模块前执行：

```bash
test -d /proc/device-tree/alphaled && echo "PASS" || echo "FAIL"
tr -d '\000' </proc/device-tree/alphaled/compatible
tr -d '\000' </proc/device-tree/alphaled/status
od -An -tx4 /proc/device-tree/alphaled/reg
```

预期结果包含：

```text
PASS
atkalpha-led
okay
```

若第一条仍输出 `FAIL`，不要继续修改驱动，应重新检查 U-Boot 加载文件和 DTB
部署位置。

### 8.8 加载驱动并测试

```bash
insmod dtsled.ko
ls -l /dev/dtsled
./app/dtsled_test on
./app/dtsled_test get
./app/dtsled_test off
rmmod dtsled
```

测试程序应依次输出以 `PASS` 开头的结果，LED 应能够点亮和熄灭。

## 9. 排查决策表

| 检查结果 | 结论 | 后续动作 |
| --- | --- | --- |
| NAND DTB 无 `alphaled` | 节点未加入实际 NAND DTS | 修改显示屏型号对应的顶层 DTS |
| 本地 DTB 有节点，目标板无节点 | 部署位置或 U-Boot 选择错误 | 检查 `fdtfile`、`bootcmd` 和启动介质 |
| 节点存在但 `compatible` 不匹配 | 节点内容错误 | 修正 DTS，重新部署并重启 |
| 节点正确但寄存器映射失败 | `reg` 内容或地址单元错误 | 反编译运行 DTB 并核对五组资源 |
| 模块加载成功但 LED 无动作 | 引脚占用、极性或寄存器配置问题 | 检查 GPIO1_IO03 是否被其他驱动占用 |

## 10. 结论

本次失败的直接原因是目标板当前运行设备树缺少 `/alphaled`。根因已经确认：
`build_all.sh` 编译的是 `imx6ull-14x14-nand-4.3-800x480-c.dts`，此前修改的
`imx6ull-alientek-emmc.dts` 不在其继承链中。本次已把节点添加到 NAND DTS
继承的公共文件 `imx6ull-14x14-evk.dts`。下一步应重新运行 `build_all.sh`，验证
`tmp` 中的新 NAND DTB，然后按照板卡现有 NAND 烧写流程更新设备树并重启。
在 `/proc/device-tree/alphaled` 出现之前，不需要修改 `dtsled.c`。

## 11. 当前验证限制

本次已完成主机侧 DTS 预处理、DTB 编译和反编译检查。由于无法访问目标板的
U-Boot 环境、启动分区和运行时 `/proc/device-tree`，未执行目标板 DTB 替换、
重启及 LED 实物验证。
