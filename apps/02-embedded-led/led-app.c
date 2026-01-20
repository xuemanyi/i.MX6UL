#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#define LED_OFF 0
#define LED_ON  1

/**
 * main - User-space LED control application
 * @argc: argument count
 * @argv: argument vector
 *
 * Usage:
 *   ./led_app <device_file> <0|1>
 *
 *   0 - LED_OFF
 *   1 - LED_ON
 *
 * Return: EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int main(int argc, char *argv[])
{
	int fd;
	int ret;
	const char *devname;
	unsigned char databuf;

	/* Check arguments */
	if (argc != 3) {
		fprintf(stderr,
			"Usage: %s <device_file> <0|1>\n", argv[0]);
		return EXIT_FAILURE;
	}

	devname = argv[1];

	/* Parse LED state */
	if (strcmp(argv[2], "0") == 0) {
		databuf = LED_OFF;
	} else if (strcmp(argv[2], "1") == 0) {
		databuf = LED_ON;
	} else {
		fprintf(stderr, "Invalid LED state: %s\n", argv[2]);
		return EXIT_FAILURE;
	}

	/* Open device file */
	fd = open(devname, O_RDWR);
	if (fd < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	/* Write LED control command */
	ret = write(fd, &databuf, sizeof(databuf));
	if (ret != sizeof(databuf)) {
		perror("write");
		close(fd);
		return EXIT_FAILURE;
	}

	/* Close device */
	if (close(fd) < 0) {
		perror("close");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
