// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ASYNCNOTI_DEVICE	"/dev/asyncnoti"
#define KEY0_VALUE		0x01

static volatile sig_atomic_t sigio_pending;

/**
 * sigio_handler() - 记录 SIGIO 已到达
 * @signum: 信号编号，本函数不直接使用
 *
 * 信号处理函数只修改 sig_atomic_t 标志。read() 和 stdio 输出留在主循环执行，
 * 避免在异步信号上下文调用不安全的库函数。
 *
 * Context: 异步信号处理上下文。
 */
static void sigio_handler(int signum)
{
	(void)signum;
	sigio_pending = 1;
}

/**
 * print_usage() - 输出命令行使用方法
 * @program: 当前程序名称，不允许为 NULL
 *
 * Context: 用户进程上下文调用。
 */
static void print_usage(const char *program)
{
	fprintf(stderr, "Usage: %s [count] [device]\n", program);
	fprintf(stderr, "Example: %s 1 %s\n", program, ASYNCNOTI_DEVICE);
}

/**
 * parse_count() - 解析需要验证的异步事件数量
 * @text: 十进制数量字符串，不允许为 NULL
 * @count: 保存解析结果的地址，不允许为 NULL
 *
 * Context: 用户进程上下文调用。
 * Return: 成功返回 0，输入非法、数值为 0 或超出 unsigned int 范围返回 -1。
 */
static int parse_count(const char *text, unsigned int *count)
{
	unsigned long value;
	char *end;

	errno = 0;
	value = strtoul(text, &end, 10);
	if (errno || *text == '\0' || *end != '\0' || !value ||
	    value > UINT_MAX)
		return -1;
	*count = value;
	return 0;
}

/**
 * read_available_events() - 读取当前可用的全部按键事件
 * @fd: 已启用 O_NONBLOCK 和 O_ASYNC 的设备描述符
 * @completed: 已完成事件计数的地址
 * @target: 测试期望的事件总数
 *
 * SIGIO 是状态变化通知，多个信号可能合并，因此主循环收到一次通知后持续读取，
 * 直到驱动返回 EAGAIN 或达到目标数量。
 *
 * Context: 用户进程上下文调用，不会因无事件阻塞。
 * Return: 成功或暂时无事件返回 0，读取或数据校验失败返回 -1。
 */
static int read_available_events(int fd, unsigned int *completed,
				 unsigned int target)
{
	uint8_t value;
	ssize_t ret;

	while (*completed < target) {
		ret = read(fd, &value, sizeof(value));
		if (ret < 0) {
			if (errno == EAGAIN)
				return 0;
			fprintf(stderr, "read failed: %s\n", strerror(errno));
			return -1;
		}
		if (ret != (ssize_t)sizeof(value)) {
			fprintf(stderr, "short read: expected=%zu actual=%zd\n",
				sizeof(value), ret);
			return -1;
		}
		if (value != KEY0_VALUE) {
			fprintf(stderr, "FAIL: invalid key value: 0x%02x\n",
				value);
			return -1;
		}
		(*completed)++;
		printf("PASS: SIGIO key event, value=0x%02x\n", value);
	}
	return 0;
}

/**
 * main() - 配置 SIGIO 并验证异步按键通知
 * @argc: 命令行参数个数
 * @argv: 命令行参数数组，可指定事件数量和设备文件路径
 *
 * 先阻塞 SIGIO，安装 sigaction，再通过 F_SETOWN 和 F_SETFL/O_ASYNC 注册当前
 * 进程。主循环用 sigsuspend() 原子解除屏蔽并等待，避免检查标志与睡眠之间丢信号。
 *
 * Context: 用户进程上下文调用，可以在 sigsuspend() 中阻塞。
 * Return: 全部事件验证成功返回 EXIT_SUCCESS，参数或系统调用失败返回
 * EXIT_FAILURE。
 */
int main(int argc, char *argv[])
{
	const char *device = ASYNCNOTI_DEVICE;
	struct sigaction action;
	sigset_t blocked_mask;
	sigset_t previous_mask;
	sigset_t wait_mask;
	unsigned int completed = 0;
	unsigned int count = 1;
	int original_flags;
	int exit_status = EXIT_FAILURE;
	int fd;

	if (argc > 3) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}
	if (argc >= 2 && parse_count(argv[1], &count)) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}
	if (argc == 3)
		device = argv[2];

	fd = open(device, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", device,
			strerror(errno));
		return EXIT_FAILURE;
	}

	sigemptyset(&blocked_mask);
	sigaddset(&blocked_mask, SIGIO);
	if (sigprocmask(SIG_BLOCK, &blocked_mask, &previous_mask) < 0) {
		fprintf(stderr, "sigprocmask failed: %s\n", strerror(errno));
		goto out_close;
	}

	memset(&action, 0, sizeof(action));
	action.sa_handler = sigio_handler;
	sigemptyset(&action.sa_mask);
	if (sigaction(SIGIO, &action, NULL) < 0) {
		fprintf(stderr, "sigaction failed: %s\n", strerror(errno));
		goto out_restore_mask;
	}
	if (fcntl(fd, F_SETOWN, getpid()) < 0) {
		fprintf(stderr, "F_SETOWN failed: %s\n", strerror(errno));
		goto out_restore_mask;
	}
	original_flags = fcntl(fd, F_GETFL);
	if (original_flags < 0) {
		fprintf(stderr, "F_GETFL failed: %s\n", strerror(errno));
		goto out_restore_mask;
	}
	if (fcntl(fd, F_SETFL, original_flags | O_ASYNC | O_NONBLOCK) < 0) {
		fprintf(stderr, "F_SETFL failed: %s\n", strerror(errno));
		goto out_restore_mask;
	}

	wait_mask = previous_mask;
	sigdelset(&wait_mask, SIGIO);
	printf("Waiting for SIGIO from %s...\n", device);
	/*
	 * open() 到启用 O_ASYNC 之间可能已有事件进入驱动，但该事件不会
	 * 触发 SIGIO。进入信号等待前先读取一次，避免遗漏注册窗口内的事件。
	 */
	if (read_available_events(fd, &completed, count))
		goto out_disable_async;
	while (completed < count) {
		/*
		 * SIGIO 在普通执行期间保持阻塞；sigsuspend() 原子应用解除
		 * SIGIO 屏蔽的掩码并睡眠，避免信号在检查标志后、睡眠前到达。
		 */
		while (!sigio_pending) {
			if (sigsuspend(&wait_mask) < 0 && errno != EINTR) {
				fprintf(stderr, "sigsuspend failed: %s\n",
					strerror(errno));
				goto out_disable_async;
			}
		}
		sigio_pending = 0;
		if (read_available_events(fd, &completed, count))
			goto out_disable_async;
	}
	exit_status = EXIT_SUCCESS;

out_disable_async:
	/* 清除 O_ASYNC，使驱动在 close 前移除当前文件的异步订阅。 */
	if (fcntl(fd, F_SETFL, original_flags) < 0) {
		fprintf(stderr, "failed to disable O_ASYNC: %s\n",
			strerror(errno));
		exit_status = EXIT_FAILURE;
	}
out_restore_mask:
	if (sigprocmask(SIG_SETMASK, &previous_mask, NULL) < 0) {
		fprintf(stderr, "failed to restore signal mask: %s\n",
			strerror(errno));
		exit_status = EXIT_FAILURE;
	}
out_close:
	if (close(fd) < 0) {
		fprintf(stderr, "close failed: %s\n", strerror(errno));
		exit_status = EXIT_FAILURE;
	}
	return exit_status;
}
