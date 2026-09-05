# SPDX-License-Identifier: GPL-2.0

ifneq ($(KERNELRELEASE),)
obj-m += keyinput.o
else
ARCH := arm
TOOLCHAIN_DIR := /usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf
CROSS_COMPILE := $(TOOLCHAIN_DIR)/bin/arm-linux-gnueabihf-
KDIR ?= $(CURDIR)/../../kernel
KCFLAGS := -march=armv7-a
.PHONY: all modules app clean
all: modules app
modules:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) KCFLAGS=$(KCFLAGS) M=$(CURDIR) modules
app:
	$(MAKE) -C $(CURDIR)/app CROSS_COMPILE=$(CROSS_COMPILE)
clean:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) KCFLAGS=$(KCFLAGS) M=$(CURDIR) clean
	$(MAKE) -C $(CURDIR)/app clean
endif
