// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#define NOBLOCKIO_DEVICE	"/dev/noblockio"
#define KEY0_VALUE		0x01
#define WAIT_TIMEOUT_MS		500

/**
 * print_usage() - 输出命令行使用方法
 * @program: 当前程序名称，不允许为 NULL
 *
 * Context: 用户进程上下文调用。
 */
static void print_usage(const char *program)
{
	fprintf(stderr, "Usage: %s <poll|select> [count] [device]\n",
		program);
	fprintf(stderr, "Example: %s poll 1 %s\n", program,
		NOBLOCKIO_DEVICE);
}

/**
 * parse_count() - 解析需要验证的事件数量
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
 * wait_with_poll() - 使用 poll 等待设备可读
 * @fd: 已按 O_NONBLOCK 打开的设备文件描述符
 *
 * Context: 用户进程上下文调用，最多阻塞 WAIT_TIMEOUT_MS 毫秒。
 * Return: 可读返回 1，超时返回 0，失败返回 -1。
 */
static int wait_with_poll(int fd)
{
	struct pollfd descriptor;
	int ret;

	descriptor.fd = fd;
	descriptor.events = POLLIN;
	descriptor.revents = 0;
	ret = poll(&descriptor, 1, WAIT_TIMEOUT_MS);
	if (ret <= 0)
		return ret;
	if (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) {
		fprintf(stderr, "poll returned error events: 0x%x\n",
			descriptor.revents);
		errno = EIO;
		return -1;
	}
	return descriptor.revents & POLLIN ? 1 : 0;
}

/**
 * wait_with_select() - 使用 select 等待设备可读
 * @fd: 已按 O_NONBLOCK 打开的设备文件描述符
 *
 * 每次调用都重新构造 fd_set 和 timeval，因为 select 可能修改二者。
 *
 * Context: 用户进程上下文调用，最多阻塞 WAIT_TIMEOUT_MS 毫秒。
 * Return: 可读返回 1，超时返回 0，失败返回 -1。
 */
static int wait_with_select(int fd)
{
	struct timeval timeout;
	fd_set readfds;
	int ret;

	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	timeout.tv_sec = WAIT_TIMEOUT_MS / 1000;
	timeout.tv_usec = (WAIT_TIMEOUT_MS % 1000) * 1000;
	ret = select(fd + 1, &readfds, NULL, NULL, &timeout);
	if (ret <= 0)
		return ret;
	return FD_ISSET(fd, &readfds) ? 1 : 0;
}

/**
 * read_key_event() - 读取并校验一个非阻塞按键事件
 * @fd: 已按 O_NONBLOCK 打开的设备文件描述符
 *
 * poll/select 返回就绪后，事件仍可能被其他读取者抢先消费，因此 -EAGAIN
 * 不视为致命错误，调用方应重新等待。
 *
 * Context: 用户进程上下文调用，不会因设备无事件而阻塞。
 * Return: 键值验证成功返回 1，竞争导致无事件返回 0，其他失败返回 -1。
 */
static int read_key_event(int fd)
{
	uint8_t value;
	ssize_t ret;

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
		fprintf(stderr, "FAIL: invalid key value: 0x%02x\n", value);
		return -1;
	}
	printf("PASS: KEY0 released, value=0x%02x\n", value);
	return 1;
}

/**
 * main() - 使用 poll 或 select 验证非阻塞按键设备
 * @argc: 命令行参数个数
 * @argv: 命令行参数数组，指定等待方式、事件数量和设备路径
 *
 * 设备始终使用 O_NONBLOCK 打开。poll/select 每次等待 500 ms，超时后继续等待；
 * 就绪时读取并验证一个 KEY0_VALUE，直到达到指定事件数量。
 *
 * Context: 用户进程上下文调用，可以在 poll/select 中阻塞。
 * Return: 完成全部验证返回 EXIT_SUCCESS，参数或系统调用失败返回 EXIT_FAILURE。
 */
int main(int argc, char *argv[])
{
	const char *device = NOBLOCKIO_DEVICE;
	unsigned int count = 1;
	unsigned int completed = 0;
	bool use_poll;
	int fd;
	int ret;

	if (argc < 2 || argc > 4) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}
	if (!strcmp(argv[1], "poll"))
		use_poll = true;
	else if (!strcmp(argv[1], "select"))
		use_poll = false;
	else {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}
	if (argc >= 3 && parse_count(argv[2], &count)) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}
	if (argc == 4)
		device = argv[3];

	fd = open(device, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", device,
			strerror(errno));
		return EXIT_FAILURE;
	}

	while (completed < count) {
		ret = use_poll ? wait_with_poll(fd) : wait_with_select(fd);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "%s failed: %s\n",
				use_poll ? "poll" : "select", strerror(errno));
			close(fd);
			return EXIT_FAILURE;
		}
		if (!ret) {
			printf("Waiting for KEY0 event...\n");
			continue;
		}

		/*
		 * 就绪通知与 read 之间可能存在竞争，read_key_event() 返回 0
		 * 时重新进入等待，不把正常的 -EAGAIN 误判为测试失败。
		 */
		ret = read_key_event(fd);
		if (ret < 0) {
			close(fd);
			return EXIT_FAILURE;
		}
		if (ret > 0)
			completed++;
	}

	if (close(fd) < 0) {
		fprintf(stderr, "close failed: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
