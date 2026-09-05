// SPDX-License-Identifier: GPL-2.0
/* platform LED 用户态测试程序。 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LEDOFF 0
#define LEDON 1

/**
 * main() - 向 platform LED 驱动写入开关命令
 * @argc: 命令行参数个数
 * @argv: 命令行参数，格式为 <device> <0|1>
 *
 * Return: 成功返回 0，参数、打开、写入或关闭失败返回 1。
 */
int main(int argc, char *argv[])
{
	int fd;
	int retvalue;
	char *filename;
	unsigned char databuf;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <device> <0|1>\n", argv[0]);
		return 1;
	}
	filename = argv[1];
	databuf = atoi(argv[2]);
	if (databuf != LEDOFF && databuf != LEDON) {
		fprintf(stderr, "Invalid LED state: %u\n", databuf);
		return 1;
	}

	/* 打开 LED 驱动设备。 */
	fd = open(filename, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", filename, strerror(errno));
		return 1;
	}
	/* 写入一个状态字节，与内核驱动的控制协议保持一致。 */
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
