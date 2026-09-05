// SPDX-License-Identifier: GPL-2.0
/* MISC 蜂鸣器用户态测试程序。 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BEEPOFF 0
#define BEEPON 1

/**
 * main() - 向 MISC 蜂鸣器写入开关状态
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

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <device> <0|1>\n", argv[0]);
		return 1;
	}
	databuf = atoi(argv[2]);
	if (databuf != BEEPON && databuf != BEEPOFF) {
		fprintf(stderr, "Invalid buzzer state: %u\n", databuf);
		return 1;
	}
	/* 打开 MISC 蜂鸣器设备。 */
	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", argv[1], strerror(errno));
		return 1;
	}
	/* 按单字节协议写入蜂鸣器状态。 */
	retvalue = write(fd, &databuf, sizeof(databuf));
	if (retvalue != (int)sizeof(databuf)) {
		fprintf(stderr, "write %s failed: %s\n", argv[1],
			retvalue < 0 ? strerror(errno) : "short write");
		close(fd);
		return 1;
	}
	printf("PASS: buzzer turned %s\n", databuf == BEEPON ? "on" : "off");
	if (close(fd) < 0) {
		fprintf(stderr, "close %s failed: %s\n", argv[1], strerror(errno));
		return 1;
	}
	return 0;
}
