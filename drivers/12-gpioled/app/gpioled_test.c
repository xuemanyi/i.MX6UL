// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define GPIOLED_DEVICE	"/dev/gpioled"
#define LED_OFF		0
#define LED_ON		1

/* 输出命令行使用方法。 */
static void print_usage(const char *program)
{
	fprintf(stderr, "Usage: %s <on|off|get> [device]\n", program);
	fprintf(stderr, "Example: %s on %s\n", program, GPIOLED_DEVICE);
}

/* 向设备写入一个 LED 状态字节。 */
static int set_led_state(int fd, uint8_t state)
{
	ssize_t ret;

	ret = write(fd, &state, sizeof(state));
	if (ret < 0) {
		fprintf(stderr, "write failed: %s\n", strerror(errno));
		return -1;
	}
	if (ret != (ssize_t)sizeof(state)) {
		fprintf(stderr, "short write: expected=%zu actual=%zd\n",
			sizeof(state), ret);
		return -1;
	}

	printf("PASS: LED turned %s\n", state == LED_ON ? "on" : "off");
	return 0;
}

/* 从设备读取并输出当前 LED 状态。 */
static int get_led_state(int fd)
{
	uint8_t state;
	ssize_t ret;

	ret = read(fd, &state, sizeof(state));
	if (ret < 0) {
		fprintf(stderr, "read failed: %s\n", strerror(errno));
		return -1;
	}
	if (ret != (ssize_t)sizeof(state)) {
		fprintf(stderr, "short read: expected=%zu actual=%zd\n",
			sizeof(state), ret);
		return -1;
	}
	if (state != LED_ON && state != LED_OFF) {
		fprintf(stderr, "invalid LED state: %u\n", state);
		return -1;
	}

	printf("PASS: LED is %s\n", state == LED_ON ? "on" : "off");
	return 0;
}

int main(int argc, char *argv[])
{
	const char *device = GPIOLED_DEVICE;
	int fd;
	int ret;

	if (argc < 2 || argc > 3) {
		print_usage(argv[0]);
		return 1;
	}
	if (argc == 3)
		device = argv[2];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", device,
			strerror(errno));
		return 1;
	}

	if (!strcmp(argv[1], "on"))
		ret = set_led_state(fd, LED_ON);
	else if (!strcmp(argv[1], "off"))
		ret = set_led_state(fd, LED_OFF);
	else if (!strcmp(argv[1], "get"))
		ret = get_led_state(fd);
	else {
		print_usage(argv[0]);
		ret = -1;
	}

	if (close(fd) < 0) {
		fprintf(stderr, "close failed: %s\n", strerror(errno));
		ret = -1;
	}
	return ret ? 1 : 0;
}
