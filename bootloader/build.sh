#!/usr/bin/env bash
# U-Boot 模块构建入口，默认使用 configs/mytest_defconfig。
set -Eeuo pipefail
IFS=$'\n\t'
UBOOT_SRC="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_ROOT="$(cd -- "${UBOOT_SRC}/.." && pwd -P)"
# shellcheck disable=SC1091
source "${PROJECT_ROOT}/tools/envsetup.sh"
# shellcheck disable=SC1091
source "${PROJECT_ROOT}/tools/scripts/common.sh"

configure() {
	require_toolchain
	require_file "${UBOOT_SRC}/configs/${UBOOT_DEFCONFIG}"
	if [[ ! -f "${UBOOT_OUT}/.config" || "${RECONFIGURE:-0}" == "1" ]]; then
		run_logged uboot-config uboot_make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" "${UBOOT_DEFCONFIG}"
	fi
}

build() {
	configure
	run_logged uboot uboot_make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" -j"${JOBS}"
	require_file "${UBOOT_OUT}/${UBOOT_IMAGE}"
	mkdir -p -- "${OUTPUT_DIR}/boot"
	cp -f -- "${UBOOT_OUT}/${UBOOT_IMAGE}" "${OUTPUT_DIR}/boot/${UBOOT_IMAGE}"
}

case "${1:-build}" in
	build) build ;;
	menuconfig) configure; uboot_make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" menuconfig ;;
	clean) [[ -f "${UBOOT_OUT}/Makefile" ]] && uboot_make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" clean ;;
	*) echo "Usage: $0 {build|menuconfig|clean}" >&2; exit 2 ;;
esac
