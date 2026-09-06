#!/usr/bin/env bash
# 完整系统构建入口。具体构建逻辑仅位于各模块目录的 build.sh。
set -Eeuo pipefail
IFS=$'\n\t'

TOOLS_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_ROOT="$(cd -- "${TOOLS_DIR}/.." && pwd -P)"

show_help() {
	cat <<'EOF'
Usage: ./tools/build.sh [build|help]

Without arguments, build the complete i.MX6UL release.

Individual components must be built from their own directories:
  ./bootloader/build.sh
  ./kernel/build.sh
  ./busybox/build.sh
  ./drivers/build.sh
  ./platform/build.sh
  ./rootfs/build.sh
EOF
}

case "${1:-build}" in
	build)
		# RootFS 构建会顺序调用 U-Boot、Kernel、BusyBox、drivers 和 platform 的模块入口。
		exec "${PROJECT_ROOT}/rootfs/build.sh" build
		;;
	help|-h|--help) show_help ;;
	*) echo "ERROR: Unknown command: $1" >&2; show_help >&2; exit 2 ;;
esac
