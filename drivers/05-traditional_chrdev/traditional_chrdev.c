// SPDX-License-Identifier: GPL-2.0
/*
 * traditional_chrdev.c - Traditional register_chrdev character device
 *
 * This module demonstrates how to create a character device using the
 * traditional register_chrdev() interface. The kernel dynamically allocates
 * a major number and associates it with the device file operations.
 *
 * A device class and device object are created to provide the
 * /dev/traditional_chrdev device node.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEVICE_NAME	"traditional_chrdev"
#define CLASS_NAME	"traditional_class"
#define BUF_SIZE	128

static int major;
static struct class *traditional_class;
static struct device *traditional_device;

static char buffer[BUF_SIZE];
static size_t data_len;

static int traditional_open(struct inode *inode, struct file *file)
{
	pr_info("%s: device opened\n", DEVICE_NAME);
	return 0;
}

static int traditional_release(struct inode *inode, struct file *file)
{
	pr_info("%s: device closed\n", DEVICE_NAME);
	return 0;
}

static ssize_t traditional_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	return simple_read_from_buffer(buf, count, ppos, buffer, data_len);
}

static ssize_t traditional_write(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	size_t write_len;

	write_len = min(count, (size_t)BUF_SIZE);

	if (copy_from_user(buffer, buf, write_len))
		return -EFAULT;

	data_len = write_len;

	return write_len;
}

static const struct file_operations traditional_fops = {
	.owner		= THIS_MODULE,
	.open		= traditional_open,
	.release	= traditional_release,
	.read		= traditional_read,
	.write		= traditional_write,
	.llseek		= no_llseek,
};

static int __init traditional_init(void)
{
	int ret;

	/*
	 * major=0：由内核动态分配主设备号。
	 * 返回值大于0时表示分配到的主设备号。
	 */
	ret = register_chrdev(0, DEVICE_NAME, &traditional_fops);
	if (ret < 0)
		return ret;

	major = ret;

	traditional_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(traditional_class)) {
		ret = PTR_ERR(traditional_class);
		goto err_unregister_chrdev;
	}

	traditional_device = device_create(traditional_class, NULL,
					   MKDEV(major, 0), NULL,
					   DEVICE_NAME);
	if (IS_ERR(traditional_device)) {
		ret = PTR_ERR(traditional_device);
		goto err_destroy_class;
	}

	pr_info("%s: registered, major=%d\n", DEVICE_NAME, major);
	return 0;

err_destroy_class:
	class_destroy(traditional_class);
err_unregister_chrdev:
	unregister_chrdev(major, DEVICE_NAME);
	return ret;
}

static void __exit traditional_exit(void)
{
	device_destroy(traditional_class, MKDEV(major, 0));
	class_destroy(traditional_class);
	unregister_chrdev(major, DEVICE_NAME);

	pr_info("%s: unregistered\n", DEVICE_NAME);
}

module_init(traditional_init);
module_exit(traditional_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("Traditional register_chrdev example");
