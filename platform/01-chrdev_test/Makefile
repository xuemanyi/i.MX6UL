# 01-chrdev_test 独立构建规则。

CROSS_COMPILE ?= arm-linux-gnueabihf-
CC := $(CROSS_COMPILE)gcc
CFLAGS ?= -O2 -Wall -Wextra
BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/chrdev_test
SOURCE := chrdev_test.c

.PHONY: all install clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -pthread -o $@

install: $(TARGET)
	@test -n "$(DESTDIR)" || { echo "ERROR: DESTDIR is required for install" >&2; exit 1; }
	@mkdir -p $(DESTDIR)/usr/bin
	install -m 0755 $(TARGET) $(DESTDIR)/usr/bin/chrdev_test

clean:
	rm -rf $(BUILD_DIR)
