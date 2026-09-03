// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "timer_ioctl.h"

/* 默认定时器字符设备路径。 */
#define TIMER_DEVICE "/dev/timer"

/**
 * read_uint() - 从标准输入读取一个无符号整数
 * @prompt: 读取前输出的提示字符串，不允许为 NULL
 * @value: 保存解析结果的地址，不允许为 NULL
 *
 * 使用十进制格式解析一整行输入，允许数值后存在空格或制表符。输入为空、
 * 包含其他字符、发生转换错误或者结果超出 unsigned int 范围时均视为失败。
 *
 * Context: 用户进程上下文调用，读取标准输入时可能阻塞。
 * Return: 解析成功返回 0，输入结束或内容非法返回 -1。
 */
static int read_uint(const char *prompt, unsigned int *value)
{
	char buffer[128];
	char *end;
	unsigned long parsed;

	printf("%s", prompt);
	fflush(stdout);
	if (!fgets(buffer, sizeof(buffer), stdin))
		return -1;

	errno = 0;
	parsed = strtoul(buffer, &end, 10);
	if (errno || end == buffer || parsed > UINT_MAX)
		return -1;
	while (*end == ' ' || *end == '\t')
		end++;
	if (*end != '\n' && *end != '\0')
		return -1;
	*value = parsed;
	return 0;
}

/**
 * print_menu() - 输出定时器测试命令菜单
 *
 * Context: 用户进程上下文调用。
 */
static void print_menu(void)
{
	printf("\n1: close timer\n");
	printf("2: open timer\n");
	printf("3: set timer period\n");
	printf("0: quit\n");
}

/**
 * main() - 运行定时器驱动交互式测试程序
 * @argc: 命令行参数个数
 * @argv: 命令行参数数组，第二个参数可指定设备文件路径
 *
 * 打开定时器字符设备，循环读取关闭、打开和设置周期命令，并通过 ioctl 将
 * 命令发送给内核驱动。输入 0 或标准输入结束时关闭设备并退出。
 *
 * Context: 用户进程上下文调用，可以阻塞。
 * Return: 正常退出返回 EXIT_SUCCESS，参数、设备操作或关闭失败返回
 * EXIT_FAILURE。
 */
int main(int argc, char *argv[])
{
	const char *device = TIMER_DEVICE;
	unsigned int selection;
	unsigned int period_ms;
	unsigned long argument;
	unsigned int command;
	int fd;

	if (argc > 2) {
		fprintf(stderr, "Usage: %s [device]\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (argc == 2)
		device = argv[1];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", device,
			strerror(errno));
		return EXIT_FAILURE;
	}

	for (;;) {
		print_menu();
		if (read_uint("Input command: ", &selection)) {
			if (feof(stdin))
				break;
			fprintf(stderr, "Invalid command\n");
			continue;
		}
		argument = 0;
		switch (selection) {
		case 0:
			if (close(fd) < 0) {
				fprintf(stderr, "close failed: %s\n",
					strerror(errno));
				return EXIT_FAILURE;
			}
			return EXIT_SUCCESS;
		case 1:
			command = CLOSE_CMD;
			break;
		case 2:
			command = OPEN_CMD;
			break;
		case 3:
			command = SETPERIOD_CMD;
			if (read_uint("Input timer period (ms): ", &period_ms) ||
			    !period_ms) {
				fprintf(stderr, "Invalid timer period\n");
				continue;
			}
			argument = period_ms;
			break;
		default:
			fprintf(stderr, "Unknown command: %u\n", selection);
			continue;
		}

		if (ioctl(fd, command, argument) < 0) {
			fprintf(stderr, "ioctl failed: %s\n", strerror(errno));
			continue;
		}
		printf("PASS: command %u completed\n", selection);
	}

	if (close(fd) < 0) {
		fprintf(stderr, "close failed: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
