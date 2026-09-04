#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    uint8_t state = 1;
    ssize_t ret;
    int fd;

    fd = open("/dev/led", O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "open failed: %s\n", strerror(errno));
        return 1;
    }

    ret = write(fd, &state, sizeof(state));
    if (ret < 0) {
        fprintf(stderr, "write failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    printf("write returned %zd\n", ret);

    close(fd);
    return 0;
}