# SPDX-License-Identifier: GPL-2.0

TOOLCHAIN_DIR := /usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf
CROSS_COMPILE := $(TOOLCHAIN_DIR)/bin/arm-linux-gnueabihf-

CC := $(CROSS_COMPILE)gcc

CFLAGS := -O2 -Wall -Wextra
LDFLAGS := -static
LDLIBS := -pthread

SOURCES := $(wildcard *.c)
TARGETS := $(SOURCES:.c=)

.PHONY: all clean

all: $(TARGETS)

%: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $< \
		$(LDFLAGS) $(LDLIBS) \
		-o $@

clean:
	$(RM) $(TARGETS)