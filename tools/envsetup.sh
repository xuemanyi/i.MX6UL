#!/usr/bin/env bash
# 统一交叉编译环境初始化。该文件仅导出环境，不执行编译或清理操作。

if [[ -n "${IMX6UL_ENVSETUP_LOADED:-}" ]]; then
	return 0 2>/dev/null || exit 0
fi

TOOLS_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_ROOT="$(cd -- "${TOOLS_DIR}/.." && pwd -P)"
CONFIG_FILE="${PROJECT_ROOT}/configs/imx6ul.env"

if [[ ! -r "${CONFIG_FILE}" ]]; then
	echo "ERROR: Configuration file not found: ${CONFIG_FILE}" >&2
	return 1 2>/dev/null || exit 1
fi

# shellcheck disable=SC1090
source "${CONFIG_FILE}"

TOOLCHAIN_DIR="${PROJECT_ROOT}/${TOOLCHAIN_REL}"
TOOLCHAIN_BIN="${TOOLCHAIN_DIR}/bin"
export PROJECT_ROOT ARCH TOOLCHAIN_DIR TOOLCHAIN_BIN
export CROSS_COMPILE="${TOOLCHAIN_BIN}/${CROSS_PREFIX}"
export PATH="${TOOLCHAIN_BIN}:${PATH}"
export CC="${CROSS_COMPILE}gcc"
export CXX="${CROSS_COMPILE}g++"
export AR="${CROSS_COMPILE}ar"
export AS="${CROSS_COMPILE}as"
export LD="${CROSS_COMPILE}ld"
export NM="${CROSS_COMPILE}nm"
export OBJCOPY="${CROSS_COMPILE}objcopy"
export OBJDUMP="${CROSS_COMPILE}objdump"
export STRIP="${CROSS_COMPILE}strip"
export JOBS="${JOBS:-${JOBS_DEFAULT}}"
export IMX6UL_ENVSETUP_LOADED=1
