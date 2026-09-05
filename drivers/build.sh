#!/usr/bin/env bash
# drivers 外部模块统一构建入口。
# 新增需要统一构建或发布的驱动时，仅在 DRIVER_COMPONENTS 中登记其一级目录名。

set -Eeuo pipefail
IFS=$'\n\t'

DRIVERS_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_ROOT="$(cd -- "${DRIVERS_DIR}/.." && pwd -P)"

# 无论调用者遗留何种交叉编译环境，均强制使用工程内 i.MX6UL 工具链。
# shellcheck disable=SC1091
source "${PROJECT_ROOT}/tools/envsetup.sh"

KDIR="${KDIR:-${PROJECT_ROOT}/kernel}"
INSTALL_MOD_PATH="${INSTALL_MOD_PATH:-}"

# 仅此列表中的驱动会被 build/install 的无参数模式处理。
# 例如：DRIVER_COMPONENTS=("12-gpioled" "15-key")
DRIVER_COMPONENTS=()

die() {
	echo "ERROR: $*" >&2
	exit 1
}

validate_driver() {
	local driver="$1"
	[[ "${driver}" =~ ^[A-Za-z0-9][A-Za-z0-9_.-]*$ ]] || die "Invalid driver name: ${driver}"
	[[ -f "${DRIVERS_DIR}/${driver}/Makefile" ]] || die "Driver Makefile not found: ${driver}"
}

build_one() {
	local driver="$1"
	validate_driver "${driver}"
	make -C "${KDIR}" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" \
		M="${DRIVERS_DIR}/${driver}" -j"${JOBS:-1}" modules
}

install_one() {
	local driver="$1"
	[[ -n "${INSTALL_MOD_PATH}" ]] || die "INSTALL_MOD_PATH is required for install"
	build_one "${driver}"
	make -C "${KDIR}" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" \
		M="${DRIVERS_DIR}/${driver}" INSTALL_MOD_PATH="${INSTALL_MOD_PATH}" modules_install
}

clean_one() {
	local driver="$1"
	validate_driver "${driver}"
	make -C "${KDIR}" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" \
		M="${DRIVERS_DIR}/${driver}" clean
}

run_selected() {
	local action="$1"
	shift
	local driver
	local -a drivers=("$@")
	if (( ${#drivers[@]} == 0 )); then
		drivers=("${DRIVER_COMPONENTS[@]}")
	fi
	for driver in "${drivers[@]}"; do
		case "${action}" in
			build) build_one "${driver}" ;;
			install) install_one "${driver}" ;;
			clean) clean_one "${driver}" ;;
		esac
	done
}

action="${1:-build}"
case "${action}" in
	build|install|clean)
		# 无参数时 action 使用默认 build，不读取未定义的 $1。
		if (( $# > 0 )); then
			shift
		fi
		run_selected "${action}" "$@"
		;;
	list)
		if (( ${#DRIVER_COMPONENTS[@]} )); then
			printf '%s\n' "${DRIVER_COMPONENTS[@]}"
		fi
		;;
	*) echo "Usage: $0 {build|install|clean|list} [driver ...]" >&2; exit 2 ;;
esac
