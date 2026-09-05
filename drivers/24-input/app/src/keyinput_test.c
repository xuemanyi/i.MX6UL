// SPDX-License-Identifier: GPL-2.0
/* Linux input 按键用户态测试程序。 */

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/**
 * main() - 读取 input_event 并打印 KEY0 状态
 * @argc: 参数个数
 * @argv: 参数，格式为 <event-device>
 *
 * Return: 成功退出返回 0，参数、打开或读取失败返回 1。
 */
int main(int argc, char *argv[])
{
	struct input_event event;
	ssize_t ret;
	int fd;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <event-device>\n", argv[0]);
		return 1;
	}
	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", argv[1], strerror(errno));
		return 1;
	}
	for (;;) {
		ret = read(fd, &event, sizeof(event));
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "read failed: %s\n", strerror(errno));
			close(fd);
			return 1;
		}
		if (ret != (ssize_t)sizeof(event)) {
			fprintf(stderr, "short input event: %zd\n", ret);
			continue;
		}
		if (event.type == EV_KEY && event.code == KEY_0) {
			printf("KEY0 %s, value=%d\n",
				event.value ? "press" : "release", event.value);
			fflush(stdout);
		}
	}
}
