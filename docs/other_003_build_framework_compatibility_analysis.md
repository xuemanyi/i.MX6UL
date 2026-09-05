# i.MX6UL 统一编译框架兼容性分析

## 1. 目的与范围

本文记录统一构建框架首次实际执行时出现的 U-Boot、BusyBox 和 Kernel 构建失败，说明
根因、处理策略和后续切换到独立输出目录的条件。本文以工程当前源码、构建日志和
`tools/build.sh` 框架实现为准。

本文不改变 Bootloader、BusyBox 或 Kernel 的源代码与既有配置。

## 2. 实际失败现象

### 2.1 U-Boot 2016.03

执行 boot 组后，`mytest_defconfig` 可以在 `output/build/uboot` 生成配置，但正式编译
停止于：

```text
Using /home/gs/code/i.MX6UL/bootloader as source for U-Boot
/home/gs/code/i.MX6UL/bootloader is not clean, please run 'make mrproper'
```

### 2.2 BusyBox 1.29.0

初始实现调用 `olddefconfig`，但 BusyBox 1.29 的帮助目标只包含 `oldconfig`，不包含
`olddefconfig`，因此出现：

```text
No rule to make target 'olddefconfig'.
```

随后直接编译仍因源码树不洁净而被拒绝：

```text
Using /home/gs/code/i.MX6UL/busybox as source for busybox
/home/gs/code/i.MX6UL/busybox is not clean, please run 'make mrproper'
```

### 2.3 Linux 4.1.15

Kernel 同样要求在使用 `O=` 前清理源码树。更进一步，当前
`kernel/include/config/`、`.config` 等历史生成文件属于 `nobody:nogroup`，当前构建用户
无法创建 `kernel.release.tmp`，因此源码树兼容模式也不能执行 `modules_prepare`。

## 3. 根因

三套旧版本 Kbuild/Kconfig 均会在分离输出目录构建时检测源码树是否含有由历史原地构建
产生的生成文件。当前三个源码树均包含 `.config`、`include/config/` 或其他产物，因此
旧版本的保护逻辑拒绝混合原地与 `O=` 构建。

这不是交叉编译器缺失造成的。Linaro GCC 4.9.4 已迁移到工程内
`tools/toolchain/`，并已验证可以执行。

## 4. 已实现的兼容策略

`configs/imx6ul.env` 为 Kernel、U-Boot 和 BusyBox 分别提供：

```bash
KERNEL_BUILD_MODE="auto"
UBOOT_BUILD_MODE="auto"
BUSYBOX_BUILD_MODE="auto"
```

模式语义如下：

| 模式 | 行为 |
| --- | --- |
| `auto` | 检测到历史构建状态时使用源码树；洁净源码树时使用 `output/build/` 的 `O=` 输出目录。 |
| `source` | 始终使用源码树。适用于保持原有构建方式的过渡期。 |
| `out` | 始终使用独立输出目录；要求调用前人工确认源码树洁净。 |

框架不会自动运行 `mrproper`、`distclean`、`sudo` 或 `chown`，避免删除配置或改变用户
文件所有者。BusyBox 配置同步改为兼容 1.29.0 的 `oldconfig`。

## 5. 建议的迁移步骤

1. 保存并审核 Kernel、U-Boot、BusyBox 当前 `.config`；Kernel 基线应保持
   `imx_v7_defconfig`，U-Boot 基线为 `mytest_defconfig`。
2. 确认是否允许修复 Kernel 历史文件所有者。若需复用源码树，必须使当前构建用户具有
   对 `kernel/include/config/` 的写权限。
3. 确认配置已保存后，在每个源码树中人工执行相应的清理操作，使其满足 `O=` 构建要求。
4. 在 `configs/imx6ul.env` 中将三个 `*_BUILD_MODE` 设为 `out`，先分别验证 U-Boot、
   Kernel 和 BusyBox，再执行完整 RootFS 构建。

## 6. 风险与限制

- 原地构建模式会继续在源码树内写入对象和生成文件，不具备完全隔离性；仅作为安全过渡。
- 直接执行 `mrproper` 会删除 `.config` 和生成文件，必须先保存已验证配置。
- 修改 `nobody:nogroup` 文件所有者会改变工作区元数据，需由工程维护者明确授权。
- BusyBox 的 `oldconfig` 在配置版本不匹配时可能提出交互式选项；应先审查并固定配置快照。
