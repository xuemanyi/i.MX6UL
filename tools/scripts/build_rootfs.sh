#!/usr/bin/env bash
# RootFS 组装入口。所有安装均写入 staging，绝不使用 sudo 修改宿主机。

copy_rootfs_overlay() {
	local overlay="${PROJECT_ROOT}/configs/rootfs/overlay"
	[[ -d "${overlay}" ]] || return 0
	cp -a -- "${overlay}/." "${ROOTFS_STAGING}/"
}

install_runtime_libraries() {
	local sysroot interpreter library found binary
	sysroot="$("${CROSS_COMPILE}gcc" -print-sysroot)"
	[[ -n "${sysroot}" && -d "${sysroot}" ]] || die "Toolchain sysroot not found: ${sysroot}"
	mkdir -p -- "${ROOTFS_STAGING}/lib"

	# 为已安装的动态 ELF 收集解释器与 DT_NEEDED 库；静态 BusyBox 不会进入此流程。
	while IFS= read -r -d '' binary; do
		interpreter="$("${CROSS_COMPILE}readelf" -l "${binary}" | sed -n 's/.*Requesting program interpreter: \(.*\)]/\1/p')"
		if [[ -n "${interpreter}" ]]; then
			library="${interpreter##*/}"
			found="$(find -L "${sysroot}" -type f -name "${library}" -print -quit)"
			[[ -n "${found}" ]] || die "Runtime loader not found in sysroot: ${library}"
			cp -aL -- "${found}" "${ROOTFS_STAGING}/lib/${library}"
		fi
		while IFS= read -r library; do
			found="$(find -L "${sysroot}" -type f -name "${library}" -print -quit)"
			[[ -n "${found}" ]] || die "Runtime library not found in sysroot: ${library}"
			cp -aL -- "${found}" "${ROOTFS_STAGING}/lib/${library}"
		done < <("${CROSS_COMPILE}readelf" -d "${binary}" | sed -n 's/.*Shared library: \[\(.*\)\].*/\1/p')
	done < <(find "${ROOTFS_STAGING}/usr/bin" -type f -print0 2>/dev/null || true)
}

write_module_startup_script() {
	local script="${ROOTFS_STAGING}/etc/init.d/S20-modules"
	mkdir -p -- "${ROOTFS_STAGING}/etc/init.d"
	{
		echo '#!/bin/sh'
		echo '# 加载由 ROOTFS_EXTERNAL_DRIVERS 选择并安装到本 RootFS 的外部驱动。'
		echo '# 使用 modprobe 以处理模块依赖；单个驱动失败不应阻塞系统启动。'
		find "${ROOTFS_STAGING}/lib/modules" -type f -name '*.ko' -printf '%f\n' 2>/dev/null | \
			sed 's/\.ko$//' | sort -u | while IFS= read -r module; do
			printf 'modprobe %s 2>/dev/null || true\n' "${module}"
		done
	} > "${script}"
	chmod 0755 -- "${script}"
}

build_rootfs() {
	require_toolchain
	safe_output_dir "${ROOTFS_STAGING}"
	rm -rf -- "${ROOTFS_STAGING}"
	mkdir -p -- "${ROOTFS_STAGING}"
	# 先构建内核并安装内核自带模块，确保外部模块版本与内核 release 一致。
	build_kernel
	install_busybox
	copy_rootfs_overlay
	run_logged kernel-modules-install kernel_make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" \
		INSTALL_MOD_PATH="${ROOTFS_STAGING}" modules_install
	install_selected_drivers
	install_platform
	install_runtime_libraries
	write_module_startup_script

	local release_dir release_stamp archive
	release_stamp="$(date +%Y%m%d-%H%M%S)"
	release_dir="${OUTPUT_DIR}/images/${BOARD_NAME}-${RELEASE_VERSION}-${release_stamp}"
	archive="${release_dir}/${ROOTFS_ARCHIVE_NAME}"
	mkdir -p -- "${release_dir}"
	run_logged rootfs-package tar -C "${ROOTFS_STAGING}" -cf "${archive}" .
	printf 'board=%s\nversion=%s\ntimestamp=%s\n' "${BOARD_NAME}" "${RELEASE_VERSION}" "${release_stamp}" > "${release_dir}/manifest.txt"
	sha256sum "${archive}" > "${release_dir}/sha256sum.txt"
	RELEASE_DIR="${release_dir}"
	export RELEASE_DIR
}

package_image() {
	build_rootfs
	[[ -n "${RELEASE_DIR:-}" && -d "${RELEASE_DIR}" ]] || die "Invalid release directory"
	build_uboot
	cp -f -- "${OUTPUT_DIR}/boot/${UBOOT_IMAGE}" "${RELEASE_DIR}/u-boot.imx"
	cp -f -- "${OUTPUT_DIR}/kernel/zImage" "${RELEASE_DIR}/zImage"
	cp -f -- "${OUTPUT_DIR}/kernel/${KERNEL_DTB}" "${RELEASE_DIR}/${KERNEL_DTB}"
	(
		cd -- "${RELEASE_DIR}"
		sha256sum u-boot.imx zImage "${KERNEL_DTB}" "${ROOTFS_ARCHIVE_NAME}" > sha256sum.txt
	)
	log_info "Release image directory: ${RELEASE_DIR}"
}
