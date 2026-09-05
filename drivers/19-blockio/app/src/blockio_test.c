// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BLOCKIO_DEVICE	"/dev/blockio"
#define KEY0_VALUE	0x01

/**
 * print_usage() - 输出命令行使用方法
 * @program: 当前程序名称，不允许为 NULL
 *
 * Context: 用户进程上下文调用。
 */
static void print_usage(const char *program)
{
	fprintf(stderr, "Usage: %s [count] [device]\n", program);
	fprintf(stderr, "Example: %s 1 %s\n", program, BLOCKIO_DEVICE);
}

/**
 * parse_count() - 解析需要读取的事件数量
 * @text: 十进制数量字符串，不允许为 NULL
 * @count: 保存解析结果的地址，不允许为 NULL
 *
 * Context: 用户进程上下文调用。
 * Return: 成功返回 0，字符串非法、数值为 0 或超出 unsigned int 范围返回 -1。
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
 * main() - 阻塞读取并验证 KEY0 事件
 * @argc: 命令行参数个数
 * @argv: 命令行参数数组，可指定事件数量和设备文件路径
 *
 * 使用普通 O_RDONLY 方式打开 /dev/blockio。每次 read() 在没有事件时阻塞，
 * 直到驱动确认完整按下和释放，再验证返回值是否为 KEY0_VALUE。
 *
 * Context: 用户进程上下文调用，read() 可能无限期阻塞。
 * Return: 全部事件验证成功返回 EXIT_SUCCESS，参数或设备操作失败返回
 * EXIT_FAILURE。
 */
int main(int argc, char *argv[])
{
	const char *device = BLOCKIO_DEVICE;
	unsigned int count = 1;
	unsigned int index;
	uint8_t value;
	ssize_t ret;
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

	fd = open(device, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", device,
			strerror(errno));
		return EXIT_FAILURE;
	}

	for (index = 0; index < count; index++) {
		printf("Waiting for KEY0 event...\n");
		ret = read(fd, &value, sizeof(value));
		if (ret < 0) {
			fprintf(stderr, "read failed: %s\n", strerror(errno));
			close(fd);
			return EXIT_FAILURE;
		}
		if (ret != (ssize_t)sizeof(value)) {
			fprintf(stderr, "short read: expected=%zu actual=%zd\n",
				sizeof(value), ret);
			close(fd);
			return EXIT_FAILURE;
		}
		if (value != KEY0_VALUE) {
			fprintf(stderr, "FAIL: invalid key value: 0x%02x\n",
				value);
			close(fd);
			return EXIT_FAILURE;
		}
		printf("PASS: KEY0 released, value=0x%02x\n", value);
	}

	if (close(fd) < 0) {
		fprintf(stderr, "close failed: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
