# SPDX-License-Identifier: GPL-2.0

TOOLCHAIN_DIR ?= /usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf
# 固定使用 ARMv7 工具链，命令行变量可覆盖该默认值。
CROSS_COMPILE := $(TOOLCHAIN_DIR)/bin/arm-linux-gnueabihf-
CC := $(CROSS_COMPILE)gcc
CFLAGS ?= -O2 -Wall -Wextra
LDFLAGS ?= -static
TARGET := dtsplatled_test

.PHONY: all clean
all: $(TARGET)
$(TARGET): src/dtsplatled_test.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDFLAGS) -o $@
clean:
	$(RM) $(TARGET)
