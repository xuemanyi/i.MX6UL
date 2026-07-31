# SPDX-License-Identifier: GPL-2.0

ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

# These defaults override stale shell environment values, while command-line
# assignments (for example, make ARCH=...) still take precedence.
TOOLCHAIN_DIR := /usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf
ARCH          := arm
CROSS_COMPILE := $(TOOLCHAIN_DIR)/bin/arm-linux-gnueabihf-
KDIR          := $(ROOT_DIR)/../kernel

export ARCH CROSS_COMPILE KDIR TOOLCHAIN_DIR
export PATH := $(TOOLCHAIN_DIR)/bin:$(PATH)

# Find module directories. The root Makefile itself is excluded by -mindepth.
MODULE_MAKEFILES := $(shell find $(ROOT_DIR) -mindepth 2 -name Makefile -print | sort)
MODULE_DIRS := $(patsubst %/Makefile,%,$(MODULE_MAKEFILES))

.DEFAULT_GOAL := all

.PHONY: all clean modules_install list env check $(MODULE_DIRS)

all: check $(MODULE_DIRS)

# Each discovered directory is built independently.
$(MODULE_DIRS):
	$(MAKE) -C $@ \
		KDIR=$(KDIR) \
		ARCH=$(ARCH) \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		all

check:
	@test -d "$(TOOLCHAIN_DIR)" || { \
		echo "Error: Toolchain not found: $(TOOLCHAIN_DIR)"; \
		exit 1; \
	}
	@test -f "$(KDIR)/Makefile" || { \
		echo "Error: Kernel source not found: $(KDIR)"; \
		exit 1; \
	}
	@command -v make >/dev/null 2>&1 || { \
		echo "Error: Required host tool not found: make"; \
		exit 1; \
	}
	@command -v $(CROSS_COMPILE)gcc >/dev/null 2>&1 || { \
		echo "Error: Cross compiler not found: $(CROSS_COMPILE)gcc"; \
		exit 1; \
	}

clean:
	@for dir in $(MODULE_DIRS); do \
		$(MAKE) -C "$$dir" \
			KDIR=$(KDIR) \
			ARCH=$(ARCH) \
			CROSS_COMPILE=$(CROSS_COMPILE) \
			clean || exit $$?; \
	done

# Example:
# make modules_install INSTALL_MOD_PATH=/home/gs/linux/nfs/rootfs/home
modules_install: check
	@test -n "$(INSTALL_MOD_PATH)" || { \
		echo "Error: INSTALL_MOD_PATH is empty"; \
		exit 1; \
	}
	@for dir in $(MODULE_DIRS); do \
		$(MAKE) -C "$$dir" \
			KDIR=$(KDIR) \
			ARCH=$(ARCH) \
			CROSS_COMPILE=$(CROSS_COMPILE) \
			INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) \
			modules_install || exit $$?; \
	done

list:
	@printf '%s\n' $(MODULE_DIRS)

env:
	@echo "========================================"
	@echo " Kernel Module Build Environment"
	@echo "========================================"
	@echo "ROOT_DIR      : $(ROOT_DIR)"
	@echo "KDIR          : $(KDIR)"
	@echo "TOOLCHAIN_DIR : $(TOOLCHAIN_DIR)"
	@echo "ARCH          : $(ARCH)"
	@echo "CROSS_COMPILE : $(CROSS_COMPILE)"
	@echo "CC            : $$(command -v $(CROSS_COMPILE)gcc || true)"
	@echo "========================================"
