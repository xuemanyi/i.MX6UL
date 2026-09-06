#!/usr/bin/env bash
# RootFS 模块构建入口，负责组装 staging、安装组件并生成发布目录。
set -Eeuo pipefail
IFS=$'\n\t'

ROOTFS_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_ROOT="$(cd -- "${ROOTFS_DIR}/.." && pwd -P)"
# shellcheck disable=SC1091
source "${PROJECT_ROOT}/tools/envsetup.sh"
# shellcheck disable=SC1091
source "${PROJECT_ROOT}/tools/scripts/common.sh"

copy_overlay() {
	local overlay="${PROJECT_ROOT}/configs/rootfs/overlay"
	[[ -d "${overlay}" ]] && cp -a -- "${overlay}/." "${ROOTFS_STAGING}/"
}

write_module_startup() {
	local script="${ROOTFS_STAGING}/etc/init.d/S20-modules"
	mkdir -p -- "${ROOTFS_STAGING}/etc/init.d"
	{
		echo '#!/bin/sh'
		echo '# 加载 drivers/build.sh 中登记并安装的外部驱动。'
		find "${ROOTFS_STAGING}/lib/modules" -type f -name '*.ko' -printf '%f\n' 2>/dev/null | \
			sed 's/\.ko$//' | sort -u | while IFS= read -r module; do
			printf 'modprobe %s 2>/dev/null || true\n' "${module}"
		done
	} > "${script}"
	chmod 0755 -- "${script}"
}

install_runtime_libraries() {
	local sysroot binary library found interpreter
	sysroot="$("${CROSS_COMPILE}gcc" -print-sysroot)"
	[[ -d "${sysroot}" ]] || die "Toolchain sysroot not found: ${sysroot}"
	mkdir -p -- "${ROOTFS_STAGING}/lib"
	while IFS= read -r -d '' binary; do
		interpreter="$("${CROSS_COMPILE}readelf" -l "${binary}" | sed -n 's/.*Requesting program interpreter: \(.*\)]/\1/p')"
		for library in "${interpreter##*/}" $("${CROSS_COMPILE}readelf" -d "${binary}" | sed -n 's/.*Shared library: \[\(.*\)\].*/\1/p'); do
			[[ -n "${library}" ]] || continue
			found="$(find -L "${sysroot}" -type f -name "${library}" -print -quit)"
			[[ -n "${found}" ]] || die "Runtime library not found in sysroot: ${library}"
			cp -aL -- "${found}" "${ROOTFS_STAGING}/lib/${library}"
		done
	done < <(find "${ROOTFS_STAGING}/usr/bin" -type f -print0 2>/dev/null || true)
}

build() {
	safe_output_dir "${ROOTFS_STAGING}"
	"${PROJECT_ROOT}/bootloader/build.sh" build
	"${PROJECT_ROOT}/kernel/build.sh" build
	rm -rf -- "${ROOTFS_STAGING}"
	mkdir -p -- "${ROOTFS_STAGING}"
	DESTDIR="${ROOTFS_STAGING}" "${PROJECT_ROOT}/busybox/build.sh" install
	copy_overlay
	run_logged kernel-modules-install kernel_make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" \
		INSTALL_MOD_PATH="${ROOTFS_STAGING}" modules_install
	KDIR="${KERNEL_OUT}" INSTALL_MOD_PATH="${ROOTFS_STAGING}" "${PROJECT_ROOT}/drivers/build.sh" install
	DESTDIR="${ROOTFS_STAGING}" "${PROJECT_ROOT}/platform/build.sh" install
	install_runtime_libraries
	write_module_startup

	local stamp release_dir
	stamp="$(date +%Y%m%d-%H%M%S)"
	release_dir="${OUTPUT_DIR}/images/${BOARD_NAME}-${RELEASE_VERSION}-${stamp}"
	mkdir -p -- "${release_dir}"
	tar -C "${ROOTFS_STAGING}" -cf "${release_dir}/${ROOTFS_ARCHIVE_NAME}" .
	cp -f -- "${OUTPUT_DIR}/boot/${UBOOT_IMAGE}" "${release_dir}/u-boot.imx"
	cp -f -- "${OUTPUT_DIR}/kernel/zImage" "${release_dir}/zImage"
	cp -f -- "${OUTPUT_DIR}/kernel/${KERNEL_DTB}" "${release_dir}/${KERNEL_DTB}"
	printf 'board=%s\nversion=%s\ntimestamp=%s\n' "${BOARD_NAME}" "${RELEASE_VERSION}" "${stamp}" > "${release_dir}/manifest.txt"
	(
		cd -- "${release_dir}"
		sha256sum u-boot.imx zImage "${KERNEL_DTB}" "${ROOTFS_ARCHIVE_NAME}" > sha256sum.txt
	)
	log_info "Release image directory: ${release_dir}"
}

case "${1:-build}" in
	build) build ;;
	clean) safe_output_dir "${ROOTFS_STAGING}"; rm -rf -- "${ROOTFS_STAGING}" ;;
	*) echo "Usage: $0 {build|clean}" >&2; exit 2 ;;
esac
