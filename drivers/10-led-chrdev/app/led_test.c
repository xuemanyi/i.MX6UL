#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LED_OFF    0
#define LED_ON     1

static void print_usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s <device> <on|off|get>\n"
            "Example:\n"
            "  %s /dev/led_misc on\n"
            "  %s /dev/led_misc off\n"
            "  %s /dev/led_misc get\n",
            program, program, program, program);
}

static int write_led_state(int fd, uint8_t state)
{
    ssize_t ret;

    ret = write(fd, &state, sizeof(state));
    if (ret < 0) {
        fprintf(stderr, "write failed: %s\n",
                strerror(errno));
        return -1;
    }

    if (ret != sizeof(state)) {
        fprintf(stderr,
                "short write: expected=%zu actual=%zd\n",
                sizeof(state), ret);
        return -1;
    }

    printf("LED state written: %s\n",
           state == LED_ON ? "on" : "off");

    return 0;
}

static int read_led_state(int fd)
{
    uint8_t state;
    ssize_t ret;

    ret = read(fd, &state, sizeof(state));
    if (ret < 0) {
        fprintf(stderr, "read failed: %s\n",
                strerror(errno));
        return -1;
    }

    if (ret != sizeof(state)) {
        fprintf(stderr,
                "short read: expected=%zu actual=%zd\n",
                sizeof(state), ret);
        return -1;
    }

    if (state != LED_ON && state != LED_OFF) {
        fprintf(stderr, "invalid state received: %u\n",
                state);
        return -1;
    }

    printf("LED state: %s (%u)\n",
           state == LED_ON ? "on" : "off", state);

    return 0;
}

int main(int argc, char *argv[])
{
    int fd;
    int ret;

    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }

    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n",
                argv[1], strerror(errno));
        return 1;
    }

    if (!strcmp(argv[2], "on"))
        ret = write_led_state(fd, LED_ON);
    else if (!strcmp(argv[2], "off"))
        ret = write_led_state(fd, LED_OFF);
    else if (!strcmp(argv[2], "get"))
        ret = read_led_state(fd);
    else {
        print_usage(argv[0]);
        ret = -1;
    }

    close(fd);

    return ret ? 1 : 0;
}