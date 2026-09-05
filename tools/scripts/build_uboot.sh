#!/usr/bin/env bash
# U-Boot 独立输出目录构建入口。

build_uboot() {
	require_toolchain
	require_file "${UBOOT_SRC}/configs/${UBOOT_DEFCONFIG}"
	mkdir -p -- "${UBOOT_OUT}" "${OUTPUT_DIR}/boot"

	if [[ ! -f "${UBOOT_OUT}/.config" || "${RECONFIGURE:-0}" == "1" ]]; then
		run_logged uboot-config uboot_make ARCH="${ARCH}" \
			CROSS_COMPILE="${CROSS_COMPILE}" "${UBOOT_DEFCONFIG}"
	fi
	run_logged uboot uboot_make \
		ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" -j"${JOBS}"
	require_file "${UBOOT_OUT}/${UBOOT_IMAGE}"
	cp -f -- "${UBOOT_OUT}/${UBOOT_IMAGE}" "${OUTPUT_DIR}/boot/${UBOOT_IMAGE}"
}

menuconfig_uboot() {
	require_toolchain
	mkdir -p -- "${UBOOT_OUT}"
	if [[ ! -f "${UBOOT_OUT}/.config" ]]; then
		uboot_make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" "${UBOOT_DEFCONFIG}"
	fi
	uboot_make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" menuconfig
}

clean_uboot() {
	[[ -f "${UBOOT_OUT}/Makefile" ]] || return 0
	uboot_make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" clean
}
