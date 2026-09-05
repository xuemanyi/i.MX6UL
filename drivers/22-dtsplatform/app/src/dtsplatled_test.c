// SPDX-License-Identifier: GPL-2.0
/* dtsplatled 用户态测试程序。 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LEDOFF 0
#define LEDON 1

/**
 * main() - 向设备写入 LED 开关状态
 * @argc: 参数个数
 * @argv: 参数，格式为 <device> <0|1>
 *
 * Return: 成功返回 0，失败返回 1。
 */
int main(int argc, char *argv[])
{
	int fd;
	int retvalue;
	unsigned char databuf;
	char *filename;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <device> <0|1>\n", argv[0]);
		return 1;
	}
	filename = argv[1];
	databuf = atoi(argv[2]);
	if (databuf != LEDON && databuf != LEDOFF) {
		fprintf(stderr, "Invalid LED state: %u\n", databuf);
		return 1;
	}
	/* 打开 platform LED 设备。 */
	fd = open(filename, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", filename, strerror(errno));
		return 1;
	}
	/* 按单字节协议写入 LED 状态。 */
	retvalue = write(fd, &databuf, sizeof(databuf));
	if (retvalue != (int)sizeof(databuf)) {
		fprintf(stderr, "write %s failed: %s\n", filename,
			retvalue < 0 ? strerror(errno) : "short write");
		close(fd);
		return 1;
	}
	printf("PASS: LED turned %s\n", databuf == LEDON ? "on" : "off");
	if (close(fd) < 0) {
		fprintf(stderr, "close %s failed: %s\n", filename, strerror(errno));
		return 1;
	}
	return 0;
}
