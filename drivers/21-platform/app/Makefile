# SPDX-License-Identifier: GPL-2.0

TOOLCHAIN_DIR ?= /usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf
# 固定使用 i.MX6ULL 所需的 ARMv7 32 位交叉编译器，命令行变量仍可覆盖该默认值。
CROSS_COMPILE := $(TOOLCHAIN_DIR)/bin/arm-linux-gnueabihf-
CC := $(CROSS_COMPILE)gcc
CFLAGS ?= -O2 -Wall -Wextra
LDFLAGS ?= -static
TARGET := platform_led_test
SOURCE := src/platform_led_test.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDFLAGS) -o $@

clean:
	$(RM) $(TARGET)
