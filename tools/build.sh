#!/usr/bin/env bash
# i.MX6UL 统一构建入口。路径始终从本文件位置推导，与调用时工作目录无关。

set -Eeuo pipefail
IFS=$'\n\t'

TOOLS_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck disable=SC1091
source "${TOOLS_DIR}/envsetup.sh"
# shellcheck disable=SC1091
source "${TOOLS_DIR}/scripts/common.sh"
source "${TOOLS_DIR}/scripts/build_uboot.sh"
source "${TOOLS_DIR}/scripts/build_kernel.sh"
source "${TOOLS_DIR}/scripts/build_drivers.sh"
source "${TOOLS_DIR}/scripts/build_busybox.sh"
source "${TOOLS_DIR}/scripts/build_platform.sh"
source "${TOOLS_DIR}/scripts/build_rootfs.sh"

show_help() {
	cat <<'EOF'
Usage: ./tools/build.sh <command> [component]

Commands:
  all                     Build and package U-Boot, kernel, RootFS and release files
  uboot | kernel | busybox Build one core component
  drivers                 Build all external kernel drivers
  driver <name>           Build one driver under drivers/
  platform                Build configured platform applications
  app <name>              Build one application under platform/
  rootfs                  Build the RootFS archive and staging tree
  image                   Package u-boot.imx, zImage, DTB and rootfs.tar
  menuconfig <component>  Open uboot, kernel or busybox menuconfig
  clean [component]       Clean build objects; default is all components
  distclean [component]   Remove isolated output/build objects; default is all
  list <drivers|apps>     List discoverable components
  env                     Print active cross-build environment
  help                    Show this help

Without arguments, an interactive Bash menu is displayed.
EOF
}

show_env() {
	printf 'PROJECT_ROOT=%s\nTOOLCHAIN_DIR=%s\nARCH=%s\nCROSS_COMPILE=%s\nJOBS=%s\n' \
		"${PROJECT_ROOT}" "${TOOLCHAIN_DIR}" "${ARCH}" "${CROSS_COMPILE}" "${JOBS}"
}

remove_build_dir() {
	local component="$1"
	local path="${BUILD_DIR}/${component}"
	safe_output_dir "${path}"
	rm -rf -- "${path}"
}

clean_all() {
	clean_uboot
	clean_kernel
	clean_busybox
	clean_drivers
	clean_platform
}

distclean_all() {
	remove_build_dir uboot
	remove_build_dir kernel
	remove_build_dir busybox
}

interactive_menu() {
	PS3='Select build group: '
	select choice in boot busybox driver platform all quit; do
		case "${choice}" in
			boot) build_uboot; build_kernel; break ;;
			busybox) build_busybox; break ;;
			driver) build_drivers; break ;;
			platform) build_platform; break ;;
			all) package_image; break ;;
			quit) return 0 ;;
			*) echo "Invalid selection" >&2 ;;
		esac
	done
}

command="${1:-}"
case "${command}" in
	'') interactive_menu ;;
	help|-h|--help) show_help ;;
	env) show_env ;;
	all|image) package_image ;;
	uboot) build_uboot ;;
	kernel) build_kernel ;;
	busybox) build_busybox ;;
	drivers) build_drivers ;;
	driver) [[ $# -eq 2 ]] || die 'Usage: ./tools/build.sh driver <name>'; build_driver "$2" ;;
	platform) build_platform ;;
	app) [[ $# -eq 2 ]] || die 'Usage: ./tools/build.sh app <name>'; build_app "$2" ;;
	rootfs) build_rootfs; log_info "RootFS release directory: ${RELEASE_DIR}" ;;
	menuconfig)
		[[ $# -eq 2 ]] || die 'Usage: ./tools/build.sh menuconfig <uboot|kernel|busybox>'
		case "$2" in
			uboot) menuconfig_uboot ;;
			kernel) menuconfig_kernel ;;
			busybox) menuconfig_busybox ;;
			*) die "Unknown menuconfig component: $2" ;;
		esac
		;;
	list)
		[[ $# -eq 2 ]] || die 'Usage: ./tools/build.sh list <drivers|apps>'
		case "$2" in drivers) list_driver_names ;; apps) list_app_names ;; *) die "Unknown list target: $2" ;; esac
		;;
	clean)
		case "${2:-all}" in
			all) clean_all ;; uboot) clean_uboot ;; kernel) clean_kernel ;;
			busybox) clean_busybox ;; drivers) clean_drivers ;; platform) clean_platform ;;
			rootfs) safe_output_dir "${ROOTFS_STAGING}"; rm -rf -- "${ROOTFS_STAGING}" ;;
			*) die "Unknown clean component: ${2:-}" ;;
		esac
		;;
	distclean)
		case "${2:-all}" in
			all) distclean_all ;; uboot|kernel|busybox) remove_build_dir "${2}" ;;
			*) die "Unknown distclean component: ${2:-}" ;;
		esac
		;;
	*) die "Unknown command: ${command}. Run ./tools/build.sh help" ;;
esac
