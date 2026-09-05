#!/usr/bin/env bash
# 公共构建函数。调用方必须在 source 本文件前完成 envsetup.sh 初始化。

set -Eeuo pipefail
IFS=$'\n\t'

OUTPUT_DIR="${PROJECT_ROOT}/output"
BUILD_DIR="${OUTPUT_DIR}/build"
LOG_ROOT="${PROJECT_ROOT}/logs"
KERNEL_SRC="${PROJECT_ROOT}/kernel"
KERNEL_OUT_OF_TREE="${BUILD_DIR}/kernel"
UBOOT_SRC="${PROJECT_ROOT}/bootloader"
UBOOT_OUT_OF_TREE="${BUILD_DIR}/uboot"
BUSYBOX_SRC="${PROJECT_ROOT}/busybox"
BUSYBOX_OUT_OF_TREE="${BUILD_DIR}/busybox"
DRIVERS_DIR="${PROJECT_ROOT}/drivers"
PLATFORM_DIR="${PROJECT_ROOT}/platform"
ROOTFS_STAGING="${OUTPUT_DIR}/rootfs/staging"

# Linux 4.1 的 O= 构建要求源码树没有历史配置和生成文件。自动模式绝不擅自
# 执行 mrproper，而是暂时复用现有源码树，保护用户当前可工作的构建状态。
case "${KERNEL_BUILD_MODE:-auto}" in
	auto)
		if [[ -f "${KERNEL_SRC}/.config" || -d "${KERNEL_SRC}/include/generated" ]]; then
			KERNEL_OUT="${KERNEL_SRC}"
			KERNEL_IN_SOURCE=1
		else
			KERNEL_OUT="${KERNEL_OUT_OF_TREE}"
			KERNEL_IN_SOURCE=0
		fi
		;;
	out) KERNEL_OUT="${KERNEL_OUT_OF_TREE}"; KERNEL_IN_SOURCE=0 ;;
	source) KERNEL_OUT="${KERNEL_SRC}"; KERNEL_IN_SOURCE=1 ;;
	*) die "Invalid KERNEL_BUILD_MODE: ${KERNEL_BUILD_MODE}" ;;
esac

# U-Boot 2016.03 发现源码树含历史构建文件时会拒绝 O= 构建。
case "${UBOOT_BUILD_MODE:-auto}" in
	auto)
		if [[ -f "${UBOOT_SRC}/.config" || -d "${UBOOT_SRC}/include/config" ]]; then
			UBOOT_OUT="${UBOOT_SRC}"
			UBOOT_IN_SOURCE=1
		else
			UBOOT_OUT="${UBOOT_OUT_OF_TREE}"
			UBOOT_IN_SOURCE=0
		fi
		;;
	out) UBOOT_OUT="${UBOOT_OUT_OF_TREE}"; UBOOT_IN_SOURCE=0 ;;
	source) UBOOT_OUT="${UBOOT_SRC}"; UBOOT_IN_SOURCE=1 ;;
	*) die "Invalid UBOOT_BUILD_MODE: ${UBOOT_BUILD_MODE}" ;;
esac

# BusyBox 1.29 的 O= 构建也要求源码树洁净，自动模式优先保护已有配置。
case "${BUSYBOX_BUILD_MODE:-auto}" in
	auto)
		if [[ -f "${BUSYBOX_SRC}/.config" || -d "${BUSYBOX_SRC}/include/config" ]]; then
			BUSYBOX_OUT="${BUSYBOX_SRC}"
			BUSYBOX_IN_SOURCE=1
		else
			BUSYBOX_OUT="${BUSYBOX_OUT_OF_TREE}"
			BUSYBOX_IN_SOURCE=0
		fi
		;;
	out) BUSYBOX_OUT="${BUSYBOX_OUT_OF_TREE}"; BUSYBOX_IN_SOURCE=0 ;;
	source) BUSYBOX_OUT="${BUSYBOX_SRC}"; BUSYBOX_IN_SOURCE=1 ;;
	*) die "Invalid BUSYBOX_BUILD_MODE: ${BUSYBOX_BUILD_MODE}" ;;
esac

LOG_DIR="${LOG_DIR:-${LOG_ROOT}/$(date +%Y%m%d-%H%M%S)}"

log_info() { printf 'INFO: %s\n' "$*"; }
log_error() { printf 'ERROR: %s\n' "$*" >&2; }

die() {
	log_error "$*"
	exit 1
}

on_error() {
	local status="$1"
	local line="$2"
	local command="$3"
	log_error "Build failed at line ${line}: ${command} (status ${status})"
	log_error "Logs: ${LOG_DIR}"
	exit "${status}"
}

trap 'on_error "$?" "$LINENO" "$BASH_COMMAND"' ERR

require_dir() { [[ -d "$1" ]] || die "Directory not found: $1"; }
require_file() { [[ -f "$1" ]] || die "File not found: $1"; }

require_toolchain() {
	require_dir "${TOOLCHAIN_DIR}"
	[[ -x "${CROSS_COMPILE}gcc" ]] || die "Cross compiler not found: ${CROSS_COMPILE}gcc"
	command -v make >/dev/null 2>&1 || die "Host tool not found: make"
	[[ "${JOBS}" =~ ^[1-9][0-9]*$ ]] || die "JOBS must be a positive integer: ${JOBS}"
}

safe_output_dir() {
	local target="$1"
	[[ -n "${target}" && "${target}" == "${OUTPUT_DIR}"/* ]] || die "Unsafe output path: ${target}"
}

run_logged() {
	local stage="$1"
	shift
	local log_file="${LOG_DIR}/${stage}.log"
	mkdir -p -- "${LOG_DIR}"
	log_info "Stage: ${stage}"
	"$@" 2>&1 | tee "${log_file}"
	local status="${PIPESTATUS[0]}"
	if (( status != 0 )); then
		log_error "Stage failed: ${stage}; see ${log_file}"
		return "${status}"
	fi
}

kernel_make() {
	if (( KERNEL_IN_SOURCE )); then
		make -C "${KERNEL_SRC}" "$@"
	else
		make -C "${KERNEL_SRC}" O="${KERNEL_OUT}" "$@"
	fi
}

uboot_make() {
	if (( UBOOT_IN_SOURCE )); then
		make -C "${UBOOT_SRC}" "$@"
	else
		make -C "${UBOOT_SRC}" O="${UBOOT_OUT}" "$@"
	fi
}

busybox_make() {
	if (( BUSYBOX_IN_SOURCE )); then
		make -C "${BUSYBOX_SRC}" "$@"
	else
		make -C "${BUSYBOX_SRC}" O="${BUSYBOX_OUT}" "$@"
	fi
}

list_driver_names() {
	"${DRIVERS_DIR}/build.sh" list
}

list_app_names() {
	"${PLATFORM_DIR}/build.sh" list
}

validate_component_name() {
	local name="$1"
	[[ "${name}" =~ ^[A-Za-z0-9][A-Za-z0-9_.-]*$ ]] || die "Invalid component name: ${name}"
}

kernel_release() {
	make -s -C "${KERNEL_OUT}" kernelrelease
}
