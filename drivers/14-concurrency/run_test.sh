#!/bin/sh

set -eu

# 获取脚本目录，使脚本从任意当前工作目录执行时都能找到测试文件。
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# 设置内核模块路径，允许通过环境变量覆盖默认路径。
MODULE_PATH="${MODULE_PATH:-${SCRIPT_DIR}/concurrency_led.ko}"
# 优先使用目标板上的平铺测试程序，源码目录则回退到 app 子目录。
if [ -z "${TEST_APP+x}" ]; then
	if [ -x "${SCRIPT_DIR}/concurrency_test" ]; then
		TEST_APP="${SCRIPT_DIR}/concurrency_test"
	else
		TEST_APP="${SCRIPT_DIR}/app/concurrency_test"
	fi
fi
# 设置字符设备路径，便于测试不同的设备节点。
DEVICE="${DEVICE:-/dev/concurrency_led}"

# 等待设备管理器创建字符设备节点。
wait_for_device()
{
	attempt=0
	# 最多轮询 20 次，避免设备节点异常时无限等待。
	while [ ! -e "${DEVICE}" ] && [ "${attempt}" -lt 20 ]; do
		attempt=$((attempt + 1))
		usleep 50000 2>/dev/null || sleep 1
	done
	[ -e "${DEVICE}" ]
}

# 保证单项测试结束后卸载模块。
cleanup_module()
{
	# 仅卸载本脚本确认已加载的同名模块，重复调用保持安全。
	if grep -q '^concurrency_led ' /proc/modules; then
		rmmod concurrency_led
	fi
}

trap cleanup_module EXIT INT TERM

# 模块加载和设备节点操作需要 root 权限。
if [ "$(id -u)" -ne 0 ]; then
	echo "ERROR: run this test as root"
	exit 1
fi
# 检查模块和用户态测试程序是否已经构建完成。
if [ ! -f "${MODULE_PATH}" ] || [ ! -x "${TEST_APP}" ]; then
	echo "ERROR: build the module and test application first"
	exit 1
fi

# 依次加载四种同步机制并运行同一组独占打开测试。
for mode in atomic spinlock semaphore mutex; do
	echo "Testing ${mode} mode..."
	insmod "${MODULE_PATH}" lock_mode="${mode}"
	if ! wait_for_device; then
		echo "FAIL: device node was not created"
		exit 1
	fi
	# 先点亮 LED，再由测试程序验证第二次打开返回 EBUSY。
	"${TEST_APP}" on "${DEVICE}"
	# 为下一种同步机制释放设备和模块资源。
	cleanup_module
done

# 所有测试完成后解除 trap，避免退出时重复执行卸载逻辑。
trap - EXIT INT TERM
echo "PASS: all concurrency modes completed"
