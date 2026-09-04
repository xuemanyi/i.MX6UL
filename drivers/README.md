# i.MX6UL 外部内核模块编译说明

本目录使用“顶层统一调度 + 模块目录独立 Kbuild”的方式编译外部 Linux
内核模块。每个模块放在自己的子目录中；只要子目录中存在 `Makefile`，顶层
`Makefile` 就会自动发现并参与批量编译，不需要再手工修改模块列表。

## 1. 当前目录结构

```text
drivers/
├── Makefile                    # 统一配置工具链并调度所有模块
├── chardevbase/
│   ├── Makefile                # 单个 C 文件生成一个 ko
│   └── chardevbase.c           # 生成 chardevbase.ko
└── multi_file/
    ├── Makefile                # 多个目标文件链接成一个 ko
    ├── multi_file_main.c
    ├── multi_file_ops.c
    └── multi_file.h            # 生成 multi_file.ko
```

构建调用链如下：

```text
drivers/Makefile
  └─ make -C <模块目录> all
       └─ make -C <内核源码目录> M=<模块绝对路径> modules
            └─ 内核 Kbuild 读取模块目录的 obj-m 和 <模块名>-y
```

### 顶层 Makefile 的职责

- 设置并导出 `ARCH`、`CROSS_COMPILE`、`TOOLCHAIN_DIR` 和 `KDIR`。
- 递归查找 `drivers` 下除顶层以外的所有 `Makefile`，将其所在目录作为模块目录。
- 依次执行每个模块目录的 `all`、`clean` 或 `modules_install` 目标。
- 在编译和安装前通过 `check` 检查工具链、内核源码和必要命令。

当前默认配置为：

```make
TOOLCHAIN_DIR := /usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf
ARCH          := arm
CROSS_COMPILE := $(TOOLCHAIN_DIR)/bin/arm-linux-gnueabihf-
KDIR          := $(ROOT_DIR)/../kernel
```

命令行赋值优先级高于 Makefile 中的默认值，因此可以在不修改文件的情况下
切换内核源码或工具链，例如：

```bash
make KDIR=/path/to/kernel \
     TOOLCHAIN_DIR=/path/to/toolchain \
     CROSS_COMPILE=/path/to/toolchain/bin/arm-linux-gnueabihf-
```

### 模块目录 Makefile 的两种工作状态

模块 Makefile 通过 `KERNELRELEASE` 区分调用阶段：

```make
ifneq ($(KERNELRELEASE),)
# 由内核构建系统再次读取：这里只写 Kbuild 规则
else
# 用户或顶层 Makefile 首次调用：转入内核构建系统
endif
```

首次执行模块目录的 `make` 时，`KERNELRELEASE` 为空，执行 `else` 中的包装
规则。包装规则调用内核源码目录的 Makefile；内核构建系统随后再次读取当前
Makefile，此时 `KERNELRELEASE` 已设置，于是使用 `obj-m` 等 Kbuild 规则。

## 2. 常用命令

以下命令默认在 `drivers` 目录执行。

```bash
# 检查并编译所有模块
make

# 并行编译所有模块；并行参数会传递给递归 make
make -j4

# 查看自动发现的模块目录
make list

# 查看当前构建环境
make env

# 清理所有模块产生的中间文件和 ko
make clean

# 安装所有模块到指定根文件系统
make modules_install INSTALL_MOD_PATH=/path/to/rootfs
```

也可以只编译一个模块：

```bash
make -C chardevbase
```

从模块目录直接构建时，应确认该目录 Makefile 中的 `KDIR` 默认路径正确，或
显式传入顶层使用的配置：

```bash
make -C multi_file \
     KDIR="$(pwd)/../kernel" \
     ARCH=arm \
     CROSS_COMPILE=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
```

> 当前 `multi_file/Makefile` 自带的 `KDIR` 默认值与本仓库目录层级不一致；
> 通过顶层 `make` 构建时，顶层会传入正确的 `KDIR`，不受该默认值影响。

## 3. 新增单文件模块

假设新增模块 `my_driver`，并由一个源文件 `my_driver.c` 生成
`my_driver.ko`。

### 第一步：创建目录和源码

```text
drivers/
└── my_driver/
    ├── Makefile
    └── my_driver.c
```

源码至少应包含模块入口、出口和许可证声明，例如：

```c
#include <linux/init.h>
#include <linux/module.h>

static int __init my_driver_init(void)
{
    pr_info("my_driver: loaded\n");
    return 0;
}

static void __exit my_driver_exit(void)
{
    pr_info("my_driver: unloaded\n");
}

module_init(my_driver_init);
module_exit(my_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("i.MX6UL example driver");
```

### 第二步：添加 Makefile

可复制 `chardevbase/Makefile`，将 Kbuild 部分改为：

```make
ifneq ($(KERNELRELEASE),)

obj-m := my_driver.o

else

ARCH          ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-
KDIR          ?= $(abspath $(CURDIR)/../../kernel)

.PHONY: all clean modules_install

all:
	$(MAKE) -C $(KDIR) \
		ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
		M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) \
		ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
		M=$(CURDIR) clean

modules_install:
	$(MAKE) -C $(KDIR) \
		ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
		M=$(CURDIR) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) \
		modules_install

endif
```

核心规则只有一行：

```make
obj-m := my_driver.o
```

它表示把 `my_driver.c` 编译成 `my_driver.o`，再生成可加载模块
`my_driver.ko`。

### 第三步：确认并编译

```bash
make list                  # 输出中应出现 my_driver
make -C my_driver          # 只编译该模块
# 或
make                       # 编译全部模块
```

顶层使用自动发现机制，因此不需要修改顶层 `Makefile`。

## 4. 新增由多个文件组成的单个 ko

假设 `sensor_core.ko` 由三个源文件组成：

```text
drivers/
└── sensor_core/
    ├── Makefile
    ├── sensor_main.c
    ├── sensor_bus.c
    ├── sensor_ioctl.c
    └── sensor_core.h
```

模块 Makefile 的包装部分与单文件模块相同，Kbuild 部分写成：

```make
ifneq ($(KERNELRELEASE),)

obj-m := sensor_core.o
sensor_core-y := sensor_main.o sensor_bus.o sensor_ioctl.o

else

# all、clean、modules_install 包装规则与上一节相同

endif
```

两行规则含义如下：

- `obj-m := sensor_core.o`：最终生成 `sensor_core.ko`。
- `sensor_core-y := ...`：先编译列出的各个 `.c` 文件，再把它们链接为
  `sensor_core.o`，最终生成 `sensor_core.ko`。

新增源文件时，例如加入 `sensor_debug.c`，只需将对应目标加入列表：

```make
sensor_core-y := sensor_main.o sensor_bus.o sensor_ioctl.o sensor_debug.o
```

头文件不需要加入该列表，由源文件通过 `#include "sensor_core.h"` 引用即可。

### 重要的命名约束

多文件模块不要同时使用与最终模块同名的源文件。例如下面的组合会冲突：

```text
sensor_core.c
sensor_bus.c
```

```make
obj-m := sensor_core.o
sensor_core-y := sensor_core.o sensor_bus.o
```

这里 `sensor_core.o` 既代表最终的复合目标，又代表由 `sensor_core.c` 生成的
成员目标，Kbuild 无法正确区分。应将入口源文件改名为
`sensor_main.c`、`sensor_core_main.c` 等，再把它列入 `sensor_core-y`。

## 5. 一个目录生成多个 ko

如果同一目录需要生成多个独立模块，可以在 `obj-m` 中列出多个目标：

```make
obj-m := first_driver.o second_driver.o
```

如果其中一个模块由多个文件组成，可以混合书写：

```make
obj-m := first_driver.o second_driver.o

# first_driver.ko 由两个文件组成
first_driver-y := first_main.o first_ops.o

# second_driver.c 直接生成 second_driver.ko
```

## 6. 编译产物与装载验证

成功编译后，模块目录中会出现 `.ko`、`.o`、`.mod.c`、`.mod.o`、
`modules.order` 和 `Module.symvers` 等文件。提交代码时通常只提交源码、头文件
和 Makefile，不提交这些构建产物。

将与目标板当前内核版本匹配的 `.ko` 复制到目标板后，可执行：

```bash
insmod my_driver.ko
lsmod
dmesg | tail
rmmod my_driver
```

使用 `modules_install` 安装到根文件系统后，模块通常位于：

```text
<INSTALL_MOD_PATH>/lib/modules/<kernel-release>/extra/
```

模块必须使用与目标板运行内核相同的源码、配置和版本信息编译，否则可能出现
`invalid module format`、符号版本不匹配或缺少符号等错误。

## 7. 常见问题

### 新目录没有被编译

确认模块目录内文件名是严格的 `Makefile`，然后执行 `make list` 检查。顶层
Makefile 查找的是所有二级及更深层级的 `Makefile`；因此模块目录内不要再放
与模块构建无关的嵌套工程 Makefile，否则它也会被当作独立模块目录。

### 找不到交叉编译器

先执行 `make env` 检查 `TOOLCHAIN_DIR` 和 `CROSS_COMPILE`。需要注意，
`CROSS_COMPILE` 是包含工具名前缀的路径，末尾应保留连字符 `-`。

### 内核源码目录错误

显式覆盖 `KDIR`：

```bash
make KDIR=/absolute/path/to/kernel
```

该内核源码树应已完成目标板内核配置，并具备外部模块构建所需的生成文件；
通常应先成功编译一次对应内核。

### 多文件模块链接失败或缺少实现

检查每个源文件对应的 `.o` 是否都已列入 `<最终模块名>-y`，并检查函数声明
是否放在共享头文件中。跨源文件调用的实现不能声明为 `static`。

### 模块之间存在符号依赖

提供符号的模块需要使用 `EXPORT_SYMBOL()` 或 `EXPORT_SYMBOL_GPL()` 导出；
目标板装载时应先装载提供符号的模块，或安装后使用 `modprobe` 按依赖关系
加载。

需要注意，当前顶层 Makefile 是分别进入各模块目录编译，各目录之间不会自动
共享 `Module.symvers`。启用 `CONFIG_MODVERSIONS` 或编译阶段需要解析其他模块
导出的符号时，可采用以下任一方式：

- 把相互依赖的模块放在同一个 Kbuild 目录中一次构建。
- 给依赖方设置 `KBUILD_EXTRA_SYMBOLS`，指向提供方编译生成的
  `Module.symvers`，并确保提供方先完成编译。
