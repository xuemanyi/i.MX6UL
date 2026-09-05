#!/usr/bin/env bash
# BusyBox 独立输出目录构建入口。

configure_busybox() {
	require_toolchain
	mkdir -p -- "${BUSYBOX_OUT}"
	if [[ ! -f "${BUSYBOX_OUT}/.config" || "${RECONFIGURE:-0}" == "1" ]]; then
		# 优先继承当前工程已验证的 BusyBox 配置，避免静默回退为通用 defconfig。
		if [[ -f "${BUSYBOX_SRC}/.config" ]]; then
			cp -f -- "${BUSYBOX_SRC}/.config" "${BUSYBOX_OUT}/.config"
			run_logged busybox-config busybox_make CROSS_COMPILE="${CROSS_COMPILE}" oldconfig
		else
			run_logged busybox-config busybox_make \
				CROSS_COMPILE="${CROSS_COMPILE}" "${BUSYBOX_CONFIG_TARGET}"
		fi
	fi
}

build_busybox() {
	configure_busybox
	run_logged busybox busybox_make \
		CROSS_COMPILE="${CROSS_COMPILE}" -j"${JOBS}"
	require_file "${BUSYBOX_OUT}/busybox"
}

install_busybox() {
	build_busybox
	mkdir -p -- "${ROOTFS_STAGING}"
	run_logged busybox-install busybox_make \
		CROSS_COMPILE="${CROSS_COMPILE}" CONFIG_PREFIX="${ROOTFS_STAGING}" install
}

menuconfig_busybox() {
	configure_busybox
	busybox_make CROSS_COMPILE="${CROSS_COMPILE}" menuconfig
}

clean_busybox() {
	[[ -f "${BUSYBOX_OUT}/Makefile" ]] || return 0
	busybox_make CROSS_COMPILE="${CROSS_COMPILE}" clean
}
