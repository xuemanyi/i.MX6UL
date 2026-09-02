# SPDX-License-Identifier: GPL-2.0

ifneq ($(KERNELRELEASE),)

# multi_file.ko is linked from multi_file_main.o and multi_file_ops.o.
obj-m += led_register_chrdev.o
obj-m += led_cdev_static.o
obj-m += led_cdev_dynamic.o
obj-m += led_misc.o

else

# i.MX6UL is a 32-bit ARM SoC.  Use board defaults even when the shell has
# ARCH/CROSS_COMPILE values left over from an arm64 build; command-line
# assignments (for example, `make ARCH=...`) can still override these.
ARCH          := arm
TOOLCHAIN_DIR := /usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf
CROSS_COMPILE := $(TOOLCHAIN_DIR)/bin/arm-linux-gnueabihf-
KDIR          ?= $(CURDIR)/../../kernel
# Linux 4.1's compiler-option probe falls back to armv5t with modern GCC.
KCFLAGS       := -march=armv7-a

.PHONY: all clean modules_install

all:
	$(MAKE) -C $(KDIR) \
		ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
		KCFLAGS=$(KCFLAGS) \
		M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) \
		ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
		KCFLAGS=$(KCFLAGS) \
		M=$(CURDIR) clean

modules_install:
	$(MAKE) -C $(KDIR) \
		ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
		KCFLAGS=$(KCFLAGS) \
		M=$(CURDIR) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) \
		modules_install

endif
