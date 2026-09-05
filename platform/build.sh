#!/usr/bin/env bash
# platform 用户态程序统一构建入口。
# 新增应用时，在 PLATFORM_APPS 中登记名称；每个应用目录必须提供 Makefile。

set -Eeuo pipefail
IFS=$'\n\t'

PLATFORM_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_ROOT="$(cd -- "${PLATFORM_DIR}/.." && pwd -P)"

# 无论调用者遗留何种交叉编译环境，均强制使用工程内 i.MX6UL 工具链。
# shellcheck disable=SC1091
source "${PROJECT_ROOT}/tools/envsetup.sh"

BUILD_ROOT="${BUILD_ROOT:-${PROJECT_ROOT}/output/platform}"
DESTDIR="${DESTDIR:-}"

# 仅此列表中的应用会被 build/install 的无参数模式处理。
PLATFORM_APPS=(
	"01-chrdev_test"
	"02-led-test"
)

die() {
	echo "ERROR: $*" >&2
	exit 1
}

build_one() {
	local app="$1"
	local output_dir="${BUILD_ROOT}/${app}"
	[[ -f "${PLATFORM_DIR}/${app}/Makefile" ]] || die "Application Makefile not found: ${app}"
	make -C "${PLATFORM_DIR}/${app}" CROSS_COMPILE="${CROSS_COMPILE}" \
		BUILD_DIR="${output_dir}" all
}

install_one() {
	local app="$1"
	[[ -n "${DESTDIR}" ]] || die "DESTDIR is required for install"
	[[ -f "${PLATFORM_DIR}/${app}/Makefile" ]] || die "Application Makefile not found: ${app}"
	make -C "${PLATFORM_DIR}/${app}" CROSS_COMPILE="${CROSS_COMPILE}" \
		BUILD_DIR="${BUILD_ROOT}/${app}" DESTDIR="${DESTDIR}" install
}

clean_one() {
	local app="$1"
	[[ -f "${PLATFORM_DIR}/${app}/Makefile" ]] || die "Application Makefile not found: ${app}"
	make -C "${PLATFORM_DIR}/${app}" BUILD_DIR="${BUILD_ROOT}/${app}" clean
}

run_selected() {
	local action="$1"
	shift
	local app
	local -a apps=("$@")
	if (( ${#apps[@]} == 0 )); then
		apps=("${PLATFORM_APPS[@]}")
	fi
	for app in "${apps[@]}"; do
		case "${action}" in
			build) build_one "${app}" ;;
			install) install_one "${app}" ;;
			clean) clean_one "${app}" ;;
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
	list) printf '%s\n' "${PLATFORM_APPS[@]}" ;;
	*) echo "Usage: $0 {build|install|clean|list} [app ...]" >&2; exit 2 ;;
esac
