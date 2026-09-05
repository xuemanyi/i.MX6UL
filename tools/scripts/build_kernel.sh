#!/usr/bin/env bash
# Kernel 构建入口，配置和默认 DTB 与 kernel/build_all.sh 保持一致。

configure_kernel() {
	require_toolchain
	require_file "${KERNEL_SRC}/arch/arm/configs/${KERNEL_DEFCONFIG}"
	if (( KERNEL_IN_SOURCE )) && [[ ! -w "${KERNEL_SRC}/include/config" ]]; then
		die "Kernel source build directory is not writable: ${KERNEL_SRC}/include/config. Fix ownership manually, or clean the source tree and set KERNEL_BUILD_MODE=out."
	fi
	mkdir -p -- "${KERNEL_OUT}"
	if [[ ! -f "${KERNEL_OUT}/.config" || "${RECONFIGURE:-0}" == "1" ]]; then
		run_logged kernel-config kernel_make ARCH="${ARCH}" \
			CROSS_COMPILE="${CROSS_COMPILE}" "${KERNEL_DEFCONFIG}"
	fi
}

build_kernel() {
	configure_kernel
	run_logged kernel kernel_make \
		ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" -j"${JOBS}" \
		"${KERNEL_IMAGE_TARGET}" "${KERNEL_DTB}" modules

	require_file "${KERNEL_OUT}/arch/arm/boot/${KERNEL_IMAGE_TARGET}"
	require_file "${KERNEL_OUT}/arch/arm/boot/dts/${KERNEL_DTB}"
	mkdir -p -- "${OUTPUT_DIR}/kernel"
	cp -f -- "${KERNEL_OUT}/arch/arm/boot/${KERNEL_IMAGE_TARGET}" "${OUTPUT_DIR}/kernel/zImage"
	cp -f -- "${KERNEL_OUT}/arch/arm/boot/dts/${KERNEL_DTB}" "${OUTPUT_DIR}/kernel/${KERNEL_DTB}"
}

prepare_kernel_modules() {
	[[ "${KERNEL_MODULES_PREPARED:-0}" == "1" ]] && return 0
	configure_kernel
	run_logged kernel-prepare kernel_make \
		ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" modules_prepare
	KERNEL_MODULES_PREPARED=1
}

menuconfig_kernel() {
	configure_kernel
	kernel_make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" menuconfig
}

clean_kernel() {
	[[ -f "${KERNEL_OUT}/Makefile" ]] || return 0
	kernel_make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" clean
}
