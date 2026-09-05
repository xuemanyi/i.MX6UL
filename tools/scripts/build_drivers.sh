#!/usr/bin/env bash
# 外部驱动调度入口。所有需要统一构建的驱动由 drivers/build.sh 集中登记。

drivers_script="${DRIVERS_DIR}/build.sh"

build_drivers() {
	require_file "${drivers_script}"
	prepare_kernel_modules
	run_logged drivers env KDIR="${KERNEL_OUT}" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" \
		JOBS="${JOBS}" "${drivers_script}" build
}

build_driver() {
	local name="$1"
	validate_component_name "${name}"
	require_file "${drivers_script}"
	prepare_kernel_modules
	run_logged "driver-${name}" env KDIR="${KERNEL_OUT}" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" \
		JOBS="${JOBS}" "${drivers_script}" build "${name}"
}

install_selected_drivers() {
	require_file "${drivers_script}"
	prepare_kernel_modules
	mkdir -p -- "${ROOTFS_STAGING}"
	run_logged drivers-install env KDIR="${KERNEL_OUT}" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" \
		JOBS="${JOBS}" INSTALL_MOD_PATH="${ROOTFS_STAGING}" "${drivers_script}" install
}

clean_drivers() {
	[[ -f "${drivers_script}" ]] || return 0
	[[ -f "${KERNEL_OUT}/Makefile" ]] || return 0
	env KDIR="${KERNEL_OUT}" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" \
		"${drivers_script}" clean
}
