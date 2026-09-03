# SPDX-License-Identifier: GPL-2.0

TOOLCHAIN_DIR ?= /usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf
CROSS_COMPILE ?= $(TOOLCHAIN_DIR)/bin/arm-linux-gnueabihf-
CC := $(CROSS_COMPILE)gcc
CPPFLAGS += -I$(CURDIR)/../include
CFLAGS ?= -O2 -Wall -Wextra
LDFLAGS ?= -static
TARGET := timer_test
SOURCES := $(wildcard src/*.c)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $@

clean:
	$(RM) $(TARGET)
