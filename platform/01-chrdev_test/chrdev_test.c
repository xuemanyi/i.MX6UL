/*
 * Userspace test program for the following character devices:
 *
 *   /dev/traditional_chrdev
 *   /dev/static_cdev
 *   /dev/dynamic_cdev
 *   /dev/misc_chrdev
 *
 * Build:
 *   gcc -Wall -Wextra -O2 -pthread chrdev_test.c -o chrdev_test
 *
 * Run one device:
 *   ./chrdev_test /dev/dynamic_cdev
 *
 * Run all existing devices:
 *   ./chrdev_test --all
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_BUFFER_SIZE	256
#define WRITER_COUNT		4

static const char *const default_devices[] = {
	"/dev/traditional_chrdev",
	"/dev/static_cdev",
	"/dev/dynamic_cdev",
	"/dev/misc_chrdev", /* TODO: */
};

struct writer_arg {
	const char *device_path;
	unsigned int id;
	int result;
};

static int write_device(const char *device_path, const char *data)
{
	size_t data_len = strlen(data);
	ssize_t written;
	int fd;

	fd = open(device_path, O_WRONLY);
	if (fd < 0) {
		fprintf(stderr, "open(%s, O_WRONLY) failed: %s\n",
			device_path, strerror(errno));
		return -1;
	}

	written = write(fd, data, data_len);
	if (written < 0) {
		fprintf(stderr, "write(%s) failed: %s\n",
			device_path, strerror(errno));
		close(fd);
		return -1;
	}

	if ((size_t)written != data_len) {
		fprintf(stderr, "write(%s) incomplete: %zd/%zu bytes\n",
			device_path, written, data_len);
		close(fd);
		return -1;
	}

	if (close(fd) < 0) {
		fprintf(stderr, "close(%s) failed: %s\n",
			device_path, strerror(errno));
		return -1;
	}

	return 0;
}

static ssize_t read_device(const char *device_path, char *buffer,
			   size_t buffer_size)
{
	ssize_t total = 0;
	ssize_t ret;
	int fd;

	fd = open(device_path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open(%s, O_RDONLY) failed: %s\n",
			device_path, strerror(errno));
		return -1;
	}

	while ((size_t)total < buffer_size - 1) {
		ret = read(fd, buffer + total, buffer_size - 1 - total);
		if (ret < 0) {
			if (errno == EINTR)
				continue;

			fprintf(stderr, "read(%s) failed: %s\n",
				device_path, strerror(errno));
			close(fd);
			return -1;
		}

		if (ret == 0)
			break;

		total += ret;
	}

	buffer[total] = '\0';

	if (close(fd) < 0) {
		fprintf(stderr, "close(%s) failed: %s\n",
			device_path, strerror(errno));
		return -1;
	}

	return total;
}

static int test_open_close(const char *device_path)
{
	int fd;

	fd = open(device_path, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "[FAIL] open/close: %s\n", strerror(errno));
		return -1;
	}

	if (close(fd) < 0) {
		fprintf(stderr, "[FAIL] open/close: %s\n", strerror(errno));
		return -1;
	}

	printf("[PASS] open/close\n");
	return 0;
}

static int test_write_read(const char *device_path)
{
	const char *message = "character device write/read test";
	char buffer[TEST_BUFFER_SIZE];
	ssize_t bytes_read;

	if (write_device(device_path, message) < 0)
		return -1;

	bytes_read = read_device(device_path, buffer, sizeof(buffer));
	if (bytes_read < 0)
		return -1;

	if (strcmp(buffer, message) != 0) {
		fprintf(stderr, "[FAIL] write/read: expected=\"%s\", actual=\"%s\"\n",
			message, buffer);
		return -1;
	}

	printf("[PASS] write/read: %zd bytes, data=\"%s\"\n",
	       bytes_read, buffer);
	return 0;
}

static int test_segmented_read(const char *device_path)
{
	const char *message = "segmented read test";
	char first[6] = { 0 };
	char rest[TEST_BUFFER_SIZE] = { 0 };
	char combined[TEST_BUFFER_SIZE];
	ssize_t first_len;
	ssize_t rest_len;
	int fd;

	if (write_device(device_path, message) < 0)
		return -1;

	fd = open(device_path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "[FAIL] segmented read open: %s\n",
			strerror(errno));
		return -1;
	}

	first_len = read(fd, first, sizeof(first) - 1);
	if (first_len < 0) {
		fprintf(stderr, "[FAIL] first segmented read: %s\n",
			strerror(errno));
		close(fd);
		return -1;
	}

	rest_len = read(fd, rest, sizeof(rest) - 1);
	if (rest_len < 0) {
		fprintf(stderr, "[FAIL] second segmented read: %s\n",
			strerror(errno));
		close(fd);
		return -1;
	}

	if (close(fd) < 0) {
		fprintf(stderr, "[FAIL] segmented read close: %s\n",
			strerror(errno));
		return -1;
	}

	first[first_len] = '\0';
	rest[rest_len] = '\0';

	if ((size_t)(first_len + rest_len) >= sizeof(combined)) {
		fprintf(stderr, "[FAIL] segmented read result too large\n");
		return -1;
	}

	memcpy(combined, first, first_len);
	memcpy(combined + first_len, rest, rest_len);
	combined[first_len + rest_len] = '\0';

	if (strcmp(combined, message) != 0) {
		fprintf(stderr,
			"[FAIL] segmented read: first=\"%s\", rest=\"%s\"\n",
			first, rest);
		return -1;
	}

	printf("[PASS] segmented read: first=\"%s\", rest=\"%s\"\n",
	       first, rest);
	return 0;
}

static void *concurrent_writer(void *arg)
{
	struct writer_arg *writer = arg;
	char message[32];

	snprintf(message, sizeof(message), "writer-%u", writer->id);
	writer->result = write_device(writer->device_path, message);
	return NULL;
}

static bool is_writer_message(const char *data)
{
	unsigned int i;
	char expected[32];

	for (i = 1; i <= WRITER_COUNT; i++) {
		snprintf(expected, sizeof(expected), "writer-%u", i);
		if (strcmp(data, expected) == 0)
			return true;
	}

	return false;
}

static int test_concurrent_write(const char *device_path)
{
	struct writer_arg args[WRITER_COUNT];
	pthread_t threads[WRITER_COUNT];
	char buffer[TEST_BUFFER_SIZE];
	unsigned int created = 0;
	unsigned int i;
	ssize_t bytes_read;
	int ret = 0;

	for (i = 0; i < WRITER_COUNT; i++) {
		args[i].device_path = device_path;
		args[i].id = i + 1;
		args[i].result = -1;

		if (pthread_create(&threads[i], NULL, concurrent_writer,
				   &args[i]) != 0) {
			fprintf(stderr, "[FAIL] pthread_create writer-%u\n", i + 1);
			ret = -1;
			break;
		}
		created++;
	}

	for (i = 0; i < created; i++) {
		if (pthread_join(threads[i], NULL) != 0) {
			fprintf(stderr, "[FAIL] pthread_join writer-%u\n", i + 1);
			ret = -1;
		} else if (args[i].result < 0) {
			ret = -1;
		}
	}

	if (ret < 0)
		return -1;

	bytes_read = read_device(device_path, buffer, sizeof(buffer));
	if (bytes_read < 0)
		return -1;

	/*
	 * Thread scheduling is nondeterministic, so any complete writer-N message
	 * is valid. A mixed or partial message indicates broken serialization.
	 */
	if (!is_writer_message(buffer)) {
		fprintf(stderr, "[FAIL] concurrent write: invalid data=\"%s\"\n",
			buffer);
		return -1;
	}

	printf("[PASS] concurrent write: final data=\"%s\"\n", buffer);
	return 0;
}

static int test_device(const char *device_path)
{
	int failures = 0;

	printf("\n=== Testing %s ===\n", device_path);

	if (access(device_path, F_OK) < 0) {
		fprintf(stderr, "[SKIP] device node does not exist: %s\n",
			strerror(errno));
		return -1;
	}

	if (test_open_close(device_path) < 0)
		failures++;
	if (test_write_read(device_path) < 0)
		failures++;
	if (test_segmented_read(device_path) < 0)
		failures++;
	if (test_concurrent_write(device_path) < 0)
		failures++;

	printf("=== %s: %s ===\n", device_path,
	       failures == 0 ? "ALL PASSED" : "FAILED");
	return failures == 0 ? 0 : -1;
}

static void print_usage(const char *program)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  %s <device-node>\n", program);
	fprintf(stderr, "  %s --all\n", program);
}

int main(int argc, char *argv[])
{
	size_t i;
	int failures = 0;

	if (argc != 2) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "--all") != 0)
		return test_device(argv[1]) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

	for (i = 0; i < sizeof(default_devices) / sizeof(default_devices[0]); i++) {
		if (test_device(default_devices[i]) < 0)
			failures++;
	}

	printf("\nSummary: %zu devices, %d failed or skipped\n",
	       sizeof(default_devices) / sizeof(default_devices[0]), failures);
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
