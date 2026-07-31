# SPDX-License-Identifier: GPL-2.0

ifneq ($(KERNELRELEASE),)

# multi_file.ko is linked from multi_file_main.o and multi_file_ops.o.
obj-m := multi_file.o
multi_file-y := multi_file_main.o multi_file_ops.o

else

ARCH          ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-
KDIR          ?= $(CURDIR)/../../../kernel

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
