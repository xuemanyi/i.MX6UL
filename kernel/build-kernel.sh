#!/bin/sh

# Clean the kernel source tree
# This removes all generated files and restores the tree to a pristine state
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- distclean

# Load the default configuration for i.MX v7 based ARM platforms
# This provides a baseline kernel configuration for i.MX6UL
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- imx_v7_defconfig

# Open the interactive kernel configuration menu
# Used to enable/disable kernel features manually
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- menuconfig

# Build the entire kernel
# -j16 enables parallel build with 16 jobs to speed up compilation
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- all -j16
