#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define IMAGE_NAME		"load.imx"
#define IMAGE_PAYLOAD_OFFSET	3072U
#define DEVICE_IMAGE_OFFSET	1024U
#define COPY_BUFFER_SIZE	4096U

/*
 * 该 IVT/DCD 数据来自原始 imxdownload 工具。前 1 KiB 保存启动头，
 * 从启动头末尾到 IMAGE_PAYLOAD_OFFSET 之间按零填充。
 */
static const uint32_t imx6_ivtdcd_table[256] = {
    0x402000d1, 0x87800000, 0x00000000, 0x877ff42c, 0x877ff420, 0x877ff400, 0x00000000, 0x00000000,
    0x877ff000, 0x00200000, 0x00000000, 0x40e801d2, 0x04e401cc, 0x68400c02, 0xffffffff, 0x6c400c02,
    0xffffffff, 0x70400c02, 0xffffffff, 0x74400c02, 0xffffffff, 0x78400c02, 0xffffffff, 0x7c400c02,
    0xffffffff, 0x80400c02, 0xffffffff, 0xb4040e02, 0x00000c00, 0xac040e02, 0x00000000, 0x7c020e02,
    0x30000000, 0x50020e02, 0x30000000, 0x4c020e02, 0x30000000, 0x90040e02, 0x30000000, 0x88020e02,
    0x30000c00, 0x70020e02, 0x00000000, 0x60020e02, 0x30000000, 0x64020e02, 0x30000000, 0xa0040e02,
    0x30000000, 0x94040e02, 0x00000200, 0x80020e02, 0x30000000, 0x84020e02, 0x30000000, 0xb0040e02,
    0x00000200, 0x98040e02, 0x30000000, 0xa4040e02, 0x30000000, 0x44020e02, 0x30000000, 0x48020e02,
    0x30000000, 0x1c001b02, 0x00800000, 0x00081b02, 0x030039a1, 0x0c081b02, 0x0b000300, 0x3c081b02,
    0x44014801, 0x48081b02, 0x302c4040, 0x50081b02, 0x343e4040, 0x1c081b02, 0x33333333, 0x20081b02,
    0x33333333, 0x2c081b02, 0x333333f3, 0x30081b02, 0x333333f3, 0xc0081b02, 0x09409400, 0xb8081b02,
    0x00080000, 0x04001b02, 0x2d000200, 0x08001b02, 0x3030331b, 0x0c001b02, 0xf3526b67, 0x10001b02,
    0x630b6db6, 0x14001b02, 0xdb00ff01, 0x18001b02, 0x40172000, 0x1c001b02, 0x00800000, 0x2c001b02,
    0xd2260000, 0x30001b02, 0x23106b00, 0x40001b02, 0x4f000000, 0x00001b02, 0x00001884, 0x90081b02,
    0x00004000, 0x1c001b02, 0x32800002, 0x1c001b02, 0x33800000, 0x1c001b02, 0x31800400, 0x1c001b02,
    0x30802015, 0x1c001b02, 0x40800004, 0x20001b02, 0x00080000, 0x18081b02, 0x27020000, 0x04001b02,
    0x2d550200, 0x04041b02, 0x06100100, 0x1c001b02, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

_Static_assert(sizeof(imx6_ivtdcd_table) <= IMAGE_PAYLOAD_OFFSET,
	       "IVT/DCD table exceeds image header area");

/**
 * write_all() - 完整写入指定缓冲区
 * @fd: 目标文件描述符
 * @buffer: 待写入的数据
 * @length: 数据长度
 *
 * 自动处理被信号中断和短写，避免生成不完整的启动镜像。
 *
 * Return: 成功返回 0，失败返回 -1 并保留 errno。
 */
static int write_all(int fd, const void *buffer, size_t length)
{
	const unsigned char *cursor = buffer;

	while (length > 0U) {
		ssize_t written = write(fd, cursor, length);

		if (written < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (written == 0) {
			errno = EIO;
			return -1;
		}

		cursor += (size_t)written;
		length -= (size_t)written;
	}

	return 0;
}

/**
 * copy_data() - 将源文件剩余内容复制到目标文件
 * @source_fd: 源文件描述符
 * @destination_fd: 目标文件描述符
 *
 * Return: 成功返回 0，读取或写入失败返回 -1。
 */
static int copy_data(int source_fd, int destination_fd)
{
	unsigned char buffer[COPY_BUFFER_SIZE];

	for (;;) {
		ssize_t bytes_read = read(source_fd, buffer, sizeof(buffer));

		if (bytes_read < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (bytes_read == 0)
			return 0;
		if (write_all(destination_fd, buffer, (size_t)bytes_read) != 0)
			return -1;
	}
}

/**
 * create_image() - 构造包含 IVT/DCD 的启动镜像
 * @source_path: 裸机程序 BIN 文件路径
 *
 * Return: 成功返回 0，失败返回 -1。
 */
static int create_image(const char *source_path)
{
	static const unsigned char padding[IMAGE_PAYLOAD_OFFSET -
					   sizeof(imx6_ivtdcd_table)];
	int image_fd = -1;
	int source_fd = -1;
	int saved_errno;
	int result = -1;

	source_fd = open(source_path, O_RDONLY);
	if (source_fd < 0)
		goto out;

	image_fd = open(IMAGE_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (image_fd < 0)
		goto out;

	if (write_all(image_fd, imx6_ivtdcd_table,
		      sizeof(imx6_ivtdcd_table)) != 0 ||
	    write_all(image_fd, padding, sizeof(padding)) != 0 ||
	    copy_data(source_fd, image_fd) != 0 || fsync(image_fd) != 0)
		goto out;

	result = 0;
out:
	saved_errno = errno;
	if (image_fd >= 0)
		close(image_fd);
	if (source_fd >= 0)
		close(source_fd);
	errno = saved_errno;
	return result;
}

/**
 * write_image_to_device() - 将启动镜像写入目标设备
 * @device_path: SD 卡块设备路径
 *
 * 镜像从设备偏移 1 KiB 处开始写入，与 ROM 启动布局保持一致。
 * 调用进程必须具备目标设备的写权限。
 *
 * Return: 成功返回 0，失败返回 -1。
 */
static int write_image_to_device(const char *device_path)
{
	int device_fd = -1;
	int image_fd = -1;
	int saved_errno;
	int result = -1;

	image_fd = open(IMAGE_NAME, O_RDONLY);
	if (image_fd < 0)
		goto out;

	device_fd = open(device_path, O_WRONLY);
	if (device_fd < 0)
		goto out;

	if (lseek(device_fd, DEVICE_IMAGE_OFFSET, SEEK_SET) < 0 ||
	    copy_data(image_fd, device_fd) != 0 || fsync(device_fd) != 0)
		goto out;

	result = 0;
out:
	saved_errno = errno;
	if (device_fd >= 0)
		close(device_fd);
	if (image_fd >= 0)
		close(image_fd);
	errno = saved_errno;
	return result;
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <source_bin> <sd_device>\n", argv[0]);
		return 2;
	}

	printf("i.MX6UL image download tool\n");
	printf("Author: Snowclad\n");

	if (create_image(argv[1]) != 0) {
		fprintf(stderr, "Failed to create %s from %s: %s\n",
			IMAGE_NAME, argv[1], strerror(errno));
		return 1;
	}

	printf("Created %s\n", IMAGE_NAME);
	if (write_image_to_device(argv[2]) != 0) {
		fprintf(stderr, "Failed to write %s to %s: %s\n",
			IMAGE_NAME, argv[2], strerror(errno));
		return 1;
	}

	printf("Wrote %s to %s successfully\n", IMAGE_NAME, argv[2]);
	return 0;
}
