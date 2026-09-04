// SPDX-License-Identifier: GPL-2.0
/*
 * static_cdev.c - Character device using a static device number and cdev
 *
 * This module demonstrates how to create a character device using a
 * predefined major and minor number. It registers the device number with
 * register_chrdev_region(), initializes a cdev object, and adds the
 * character device to the kernel with cdev_add().
 *
 * A device class and device object are created to provide the
 * /dev/static_cdev device node.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEVICE_NAME	"static_cdev"
#define CLASS_NAME	"static_cdev_class"
#define STATIC_MAJOR	240
#define STATIC_MINOR	0
#define DEVICE_COUNT	1
#define BUF_SIZE	128

struct static_cdev_device {
	dev_t devt;
	struct cdev cdev;
	struct class *class;
	struct device *device;

	char buffer[BUF_SIZE];
	size_t data_len;
};

static struct static_cdev_device static_dev;

static int static_cdev_open(struct inode *inode, struct file *file)
{
	struct static_cdev_device *dev;

	dev = container_of(inode->i_cdev,
			   struct static_cdev_device, cdev);
	file->private_data = dev;

	return 0;
}

static int static_cdev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t static_cdev_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	struct static_cdev_device *dev = file->private_data;

	return simple_read_from_buffer(buf, count, ppos,
				       dev->buffer, dev->data_len);
}

static ssize_t static_cdev_write(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct static_cdev_device *dev = file->private_data;
	size_t write_len;

	write_len = min(count, (size_t)BUF_SIZE);

	if (copy_from_user(dev->buffer, buf, write_len))
		return -EFAULT;

	dev->data_len = write_len;

	return write_len;
}

static const struct file_operations static_cdev_fops = {
	.owner		= THIS_MODULE,
	.open		= static_cdev_open,
	.release	= static_cdev_release,
	.read		= static_cdev_read,
	.write		= static_cdev_write,
	.llseek		= no_llseek,
};

static int __init static_cdev_init(void)
{
	int ret;

	static_dev.devt = MKDEV(STATIC_MAJOR, STATIC_MINOR);

	/* 注册指定的设备号范围。 */
	ret = register_chrdev_region(static_dev.devt, DEVICE_COUNT,
				     DEVICE_NAME);
	if (ret)
		return ret;

	/* 初始化并添加cdev。 */
	cdev_init(&static_dev.cdev, &static_cdev_fops);
	static_dev.cdev.owner = THIS_MODULE;

	ret = cdev_add(&static_dev.cdev, static_dev.devt, DEVICE_COUNT);
	if (ret)
		goto err_unregister_region;

	static_dev.class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(static_dev.class)) {
		ret = PTR_ERR(static_dev.class);
		goto err_del_cdev;
	}

	static_dev.device = device_create(static_dev.class, NULL,
					  static_dev.devt, NULL,
					  DEVICE_NAME);
	if (IS_ERR(static_dev.device)) {
		ret = PTR_ERR(static_dev.device);
		goto err_destroy_class;
	}

	pr_info("%s: registered, major=%u minor=%u\n",
		DEVICE_NAME, MAJOR(static_dev.devt),
		MINOR(static_dev.devt));

	return 0;

err_destroy_class:
	class_destroy(static_dev.class);
err_del_cdev:
	cdev_del(&static_dev.cdev);
err_unregister_region:
	unregister_chrdev_region(static_dev.devt, DEVICE_COUNT);
	return ret;
}

static void __exit static_cdev_exit(void)
{
	device_destroy(static_dev.class, static_dev.devt);
	class_destroy(static_dev.class);
	cdev_del(&static_dev.cdev);
	unregister_chrdev_region(static_dev.devt, DEVICE_COUNT);
}

module_init(static_cdev_init);
module_exit(static_cdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("Static device number and cdev example");
