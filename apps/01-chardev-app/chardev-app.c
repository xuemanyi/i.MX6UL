#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define READ_OP   1
#define WRITE_OP  2
#define BUF_SIZE  128

static const char user_data[] = "user data";

int main(int argc, char *argv[])
{
    int fd;
    int ret;
    int cmd;
    const char *filename;
    char read_buf[BUF_SIZE];
    char write_buf[BUF_SIZE];

    /* Check argument count */
    if (argc != 3) {
        printf("Usage: %s <device_file> <1:read | 2:write>\n", argv[0]);
        return EXIT_FAILURE;
    }

    filename = argv[1];
    cmd = atoi(argv[2]);

    /* Open device file */
    fd = open(filename, O_RDWR);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    /* Read operation */
    if (cmd == READ_OP) {
        memset(read_buf, 0, sizeof(read_buf));

        ret = read(fd, read_buf, sizeof(read_buf) - 1);
        if (ret < 0) {
            perror("read");
        } else if (ret == 0) {
            printf("No data read from device\n");
        } else {
            /* Ensure string termination */
            read_buf[ret] = '\0';
            printf("Read %d bytes: %s\n", ret, read_buf);
        }
    }
    /* Write operation */
    else if (cmd == WRITE_OP) {
        memset(write_buf, 0, sizeof(write_buf));
        strncpy(write_buf, user_data, sizeof(write_buf) - 1);

        ret = write(fd, write_buf, strlen(write_buf));
        if (ret < 0) {
            perror("write");
        } else {
            printf("Write %d bytes to device\n", ret);
        }
    }
    else {
        printf("Invalid command: %d\n", cmd);
        printf("Use 1 for read, 2 for write\n");
    }

    /* Close device file */
    ret = close(fd);
    if (ret < 0) {
        perror("close");
    }

    return EXIT_SUCCESS;
}
