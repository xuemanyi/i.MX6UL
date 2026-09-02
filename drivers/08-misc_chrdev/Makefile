# SPDX-License-Identifier: GPL-2.0

ifneq ($(KERNELRELEASE),)

obj-m += misc_chrdev.o

else

# Standalone wrapper section: permits running make in this directory.
TOOLCHAIN_DIR := /usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf
ARCH          := arm
CROSS_COMPILE := $(TOOLCHAIN_DIR)/bin/arm-linux-gnueabihf-
KDIR          := $(abspath $(CURDIR)/../../kernel)
BEAR          ?= bear
COMPDB        := $(CURDIR)/compile_commands.json

.PHONY: all clean modules_install

all:
	$(MAKE) clean
	$(BEAR) --output $(COMPDB) -- \
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
