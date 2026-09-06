#!/usr/bin/env bash
# BusyBox 模块构建入口，默认继承当前工程已验证的 .config。
set -Eeuo pipefail
IFS=$'\n\t'
BUSYBOX_SRC="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_ROOT="$(cd -- "${BUSYBOX_SRC}/.." && pwd -P)"
# shellcheck disable=SC1091
source "${PROJECT_ROOT}/tools/envsetup.sh"
# shellcheck disable=SC1091
source "${PROJECT_ROOT}/tools/scripts/common.sh"

configure() {
	require_toolchain
	if [[ ! -f "${BUSYBOX_OUT}/.config" || "${RECONFIGURE:-0}" == "1" ]]; then
		if [[ -f "${BUSYBOX_SRC}/.config" ]]; then
			cp -f -- "${BUSYBOX_SRC}/.config" "${BUSYBOX_OUT}/.config"
			run_logged busybox-config busybox_make CROSS_COMPILE="${CROSS_COMPILE}" oldconfig
		else
			run_logged busybox-config busybox_make CROSS_COMPILE="${CROSS_COMPILE}" "${BUSYBOX_CONFIG_TARGET}"
		fi
	fi
}

build() {
	configure
	run_logged busybox busybox_make CROSS_COMPILE="${CROSS_COMPILE}" -j"${JOBS}"
	require_file "${BUSYBOX_OUT}/busybox"
}

install_rootfs() {
	build
	[[ -n "${DESTDIR:-}" ]] || die "DESTDIR is required for install"
	run_logged busybox-install busybox_make CROSS_COMPILE="${CROSS_COMPILE}" CONFIG_PREFIX="${DESTDIR}" install
}

case "${1:-build}" in
	build) build ;;
	install) install_rootfs ;;
	menuconfig) configure; busybox_make CROSS_COMPILE="${CROSS_COMPILE}" menuconfig ;;
	clean) [[ -f "${BUSYBOX_OUT}/Makefile" ]] && busybox_make CROSS_COMPILE="${CROSS_COMPILE}" clean ;;
	*) echo "Usage: $0 {build|install|menuconfig|clean}" >&2; exit 2 ;;
esac
