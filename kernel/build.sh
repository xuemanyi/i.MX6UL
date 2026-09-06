#!/usr/bin/env bash
# Linux Kernel 模块构建入口，配置和默认 DTB 与 build_all.sh 保持一致。
set -Eeuo pipefail
IFS=$'\n\t'
KERNEL_SRC="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_ROOT="$(cd -- "${KERNEL_SRC}/.." && pwd -P)"
# shellcheck disable=SC1091
source "${PROJECT_ROOT}/tools/envsetup.sh"
# shellcheck disable=SC1091
source "${PROJECT_ROOT}/tools/scripts/common.sh"

configure() {
	require_toolchain
	require_file "${KERNEL_SRC}/arch/arm/configs/${KERNEL_DEFCONFIG}"
	if (( KERNEL_IN_SOURCE )) && [[ ! -w "${KERNEL_SRC}/include/config" ]]; then
		die "Kernel source build directory is not writable: ${KERNEL_SRC}/include/config"
	fi
	if [[ ! -f "${KERNEL_OUT}/.config" || "${RECONFIGURE:-0}" == "1" ]]; then
		run_logged kernel-config kernel_make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" "${KERNEL_DEFCONFIG}"
	fi
}

prepare() {
	configure
	run_logged kernel-prepare kernel_make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" modules_prepare
}

build() {
	configure
	run_logged kernel kernel_make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" -j"${JOBS}" \
		"${KERNEL_IMAGE_TARGET}" "${KERNEL_DTB}" modules
	require_file "${KERNEL_OUT}/arch/arm/boot/${KERNEL_IMAGE_TARGET}"
	require_file "${KERNEL_OUT}/arch/arm/boot/dts/${KERNEL_DTB}"
	mkdir -p -- "${OUTPUT_DIR}/kernel"
	cp -f -- "${KERNEL_OUT}/arch/arm/boot/${KERNEL_IMAGE_TARGET}" "${OUTPUT_DIR}/kernel/zImage"
	cp -f -- "${KERNEL_OUT}/arch/arm/boot/dts/${KERNEL_DTB}" "${OUTPUT_DIR}/kernel/${KERNEL_DTB}"
}

case "${1:-build}" in
	build) build ;;
	prepare) prepare ;;
	menuconfig) configure; kernel_make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" menuconfig ;;
	clean) [[ -f "${KERNEL_OUT}/Makefile" ]] && kernel_make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" clean ;;
	*) echo "Usage: $0 {build|prepare|menuconfig|clean}" >&2; exit 2 ;;
esac
