// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define KEY_DEVICE	"/dev/key"
#define KEY0_VALUE	0xf0

/* 输出命令行使用方法。 */
static void print_usage(const char *program)
{
	fprintf(stderr, "Usage: %s [count] [device]\n", program);
	fprintf(stderr, "Example: %s 1 %s\n", program, KEY_DEVICE);
}

int main(int argc, char *argv[])
{
	const char *device = KEY_DEVICE;
	unsigned long count = 1;
	unsigned long index;
	char *end;
	uint8_t value;
	ssize_t ret;
	int fd;

	if (argc > 3) {
		print_usage(argv[0]);
		return 1;
	}
	if (argc >= 2) {
		errno = 0;
		count = strtoul(argv[1], &end, 10);
		if (errno || *argv[1] == '\0' || *end != '\0' || !count) {
			print_usage(argv[0]);
			return 1;
		}
	}
	if (argc == 3)
		device = argv[2];

	fd = open(device, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", device,
			strerror(errno));
		return 1;
	}
	for (index = 0; index < count; index++) {
		ret = read(fd, &value, sizeof(value));
		if (ret < 0) {
			fprintf(stderr, "read failed: %s\n", strerror(errno));
			close(fd);
			return 1;
		}
		if (ret != (ssize_t)sizeof(value) || value != KEY0_VALUE) {
			fprintf(stderr, "FAIL: invalid key event, value=0x%02x\n",
				value);
			close(fd);
			return 1;
		}
		printf("PASS: KEY0 pressed, value=0x%02x\n", value);
	}
	if (close(fd) < 0) {
		fprintf(stderr, "close failed: %s\n", strerror(errno));
		return 1;
	}
	return 0;
}
