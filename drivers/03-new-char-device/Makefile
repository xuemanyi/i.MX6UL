# SPDX-License-Identifier: GPL-2.0

# Architecture and cross compiler (can be overridden)
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-

# Kernel source directory
KDIR ?= $(CURDIR)/../../kernel

# Module object
obj-m := new-char-led.o

.PHONY: all clean

# Build module
all:
	$(MAKE) -C $(KDIR) \
		ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
		M=$(CURDIR) modules

# Clean module
clean:
	$(MAKE) -C $(KDIR) \
		ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
		M=$(CURDIR) clean
