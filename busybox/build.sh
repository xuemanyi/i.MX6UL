#!/bin/bash
# =============================================================================
# ARM Toolchain Environment
# Usage:
#   source env.sh
# =============================================================================

# Toolchain 根目录
export TOOLCHAIN_DIR="/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf"

# 检查工具链是否存在
if [ ! -d "${TOOLCHAIN_DIR}" ]; then
    echo "Error: Toolchain not found!"
    echo "       ${TOOLCHAIN_DIR}"
    return 1 2>/dev/null || exit 1
fi

# 检查主机构建工具，避免清理源码后才发现依赖缺失
for tool in make lzop; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "Error: Required host tool not found: ${tool}"
        echo "       Install it before building (for Ubuntu/Debian: sudo apt-get install ${tool})."
        exit 1
    fi
done

# 添加到 PATH（避免重复添加）
export PATH="${TOOLCHAIN_DIR}/bin:$PATH"

# 编译目标
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-

# 常用变量
export CC=${CROSS_COMPILE}gcc
export CXX=${CROSS_COMPILE}g++
export LD=${CROSS_COMPILE}ld
export AS=${CROSS_COMPILE}as
export AR=${CROSS_COMPILE}ar
export NM=${CROSS_COMPILE}nm
export OBJCOPY=${CROSS_COMPILE}objcopy
export OBJDUMP=${CROSS_COMPILE}objdump
export STRIP=${CROSS_COMPILE}strip

# 打印当前环境
echo "========================================"
echo " ARM Cross Compile Environment"
echo "========================================"
echo "TOOLCHAIN_DIR : ${TOOLCHAIN_DIR}"
echo "ARCH          : ${ARCH}"
echo "CROSS_COMPILE : ${CROSS_COMPILE}"
echo "CC            : $(which ${CC})"
echo "GCC Version   :"
${CC} --version | head -n 1
echo "========================================"

# make distclean
# make clean
# make install CONFIG_PREFIX=/home/gs/linux/nfs/rootfs/ -j12

make