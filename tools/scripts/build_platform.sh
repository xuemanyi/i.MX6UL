#!/usr/bin/env bash
# platform 组件调度入口。所有用户态程序由 platform/build.sh 集中登记和构建。

platform_script="${PLATFORM_DIR}/build.sh"

build_platform() {
	require_file "${platform_script}"
	run_logged platform env ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" JOBS="${JOBS}" \
		BUILD_ROOT="${OUTPUT_DIR}/platform" "${platform_script}" build
}

build_app() {
	local name="$1"
	validate_component_name "${name}"
	require_file "${platform_script}"
	run_logged "app-${name}" env ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" JOBS="${JOBS}" \
		BUILD_ROOT="${OUTPUT_DIR}/platform" "${platform_script}" build "${name}"
}

install_platform() {
	require_file "${platform_script}"
	run_logged platform-install env ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" \
		JOBS="${JOBS}" BUILD_ROOT="${OUTPUT_DIR}/platform" DESTDIR="${ROOTFS_STAGING}" \
		"${platform_script}" install
}

clean_platform() {
	[[ -f "${platform_script}" ]] || return 0
	env BUILD_ROOT="${OUTPUT_DIR}/platform" "${platform_script}" clean
}
