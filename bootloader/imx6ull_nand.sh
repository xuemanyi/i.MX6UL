#!/bin/bash
set -e

TOOLCHAIN_DIR=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf

export PATH=${TOOLCHAIN_DIR}/bin:$PATH
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-

which ${CROSS_COMPILE}gcc
${CROSS_COMPILE}gcc -v

make distclean

# make mx6ull_14x14_ddr256_nand_defconfig
make mytest_defconfig
# make mx6ull_14x14_ddr512_emmc_defconfig
# make menuconfig

make V=1 -j12
# make V=1 -j12 2>&1 | tee build.log