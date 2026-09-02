// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define DEFAULT_DEVICE	"/dev/concurrency_led"
#define LED_OFF		0
#define LED_ON		1

/* 输出命令行使用方法。 */
static void print_usage(const char *program)
{
	fprintf(stderr, "Usage: %s <on|off> [device]\n", program);
	fprintf(stderr, "Example: %s on %s\n", program, DEFAULT_DEVICE);
}

/* 子进程验证设备在首次打开期间不能被再次打开。 */
static int verify_second_open_is_busy(int inherited_fd, const char *device)
{
	int fd;

	if (close(inherited_fd) < 0) {
		fprintf(stderr, "child close failed: %s\n", strerror(errno));
		return 1;
	}
	fd = open(device, O_RDWR);
	if (fd >= 0) {
		fprintf(stderr, "FAIL: second open unexpectedly succeeded\n");
		close(fd);
		return 1;
	}
	if (errno != EBUSY) {
		fprintf(stderr, "FAIL: second open returned %s instead of EBUSY\n",
			strerror(errno));
		return 1;
	}
	printf("PASS: second open was rejected with EBUSY\n");
	fflush(stdout);
	return 0;
}

int main(int argc, char *argv[])
{
	const char *device = DEFAULT_DEVICE;
	uint8_t state;
	uint8_t readback;
	pid_t child;
	ssize_t length;
	int status;
	int fd;

	if (argc < 2 || argc > 3) {
		print_usage(argv[0]);
		return 1;
	}
	if (!strcmp(argv[1], "on"))
		state = LED_ON;
	else if (!strcmp(argv[1], "off"))
		state = LED_OFF;
	else {
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
	length = write(fd, &state, sizeof(state));
	if (length != (ssize_t)sizeof(state)) {
		fprintf(stderr, "write failed: %s\n",
			length < 0 ? strerror(errno) : "short write");
		close(fd);
		return 1;
	}
	length = read(fd, &readback, sizeof(readback));
	if (length != (ssize_t)sizeof(readback) || readback != state) {
		fprintf(stderr, "FAIL: LED readback mismatch\n");
		close(fd);
		return 1;
	}
	printf("PASS: LED state is %s\n", state == LED_ON ? "on" : "off");

	child = fork();
	if (child < 0) {
		fprintf(stderr, "fork failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}
	if (!child)
		_exit(verify_second_open_is_busy(fd, device));
	if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0) {
		fprintf(stderr, "FAIL: exclusive-open verification failed\n");
		close(fd);
		return 1;
	}
	if (close(fd) < 0) {
		fprintf(stderr, "close failed: %s\n", strerror(errno));
		return 1;
	}

	fd = open(device, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "FAIL: open after release failed: %s\n",
			strerror(errno));
		return 1;
	}
	close(fd);
	printf("PASS: open succeeded after release\n");
	return 0;
}
