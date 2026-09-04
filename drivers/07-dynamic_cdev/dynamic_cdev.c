// SPDX-License-Identifier: GPL-2.0
/*
 * dynamic_cdev.c - Character device using a dynamic device number and cdev
 *
 * This module demonstrates how to dynamically allocate a device number with
 * alloc_chrdev_region(), initialize a cdev object, and add the character
 * device to the kernel with cdev_add().
 *
 * A device class and device object are created to provide the
 * /dev/dynamic_cdev device node.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#define DEVICE_NAME	"dynamic_cdev"
#define CLASS_NAME	"dynamic_cdev_class"
#define DEVICE_COUNT	1
#define BUF_SIZE	128

struct dynamic_cdev_device {
	dev_t devt;
	struct cdev cdev;
	struct class *class;
	struct device *device;

	char buffer[BUF_SIZE];
	size_t data_len;
	struct mutex lock;
};

static struct dynamic_cdev_device dynamic_dev;

static int dynamic_cdev_open(struct inode *inode, struct file *file)
{
	struct dynamic_cdev_device *dev;

	dev = container_of(inode->i_cdev,
			   struct dynamic_cdev_device, cdev);
	file->private_data = dev;

	return 0;
}

static int dynamic_cdev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t dynamic_cdev_read(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct dynamic_cdev_device *dev = file->private_data;
	ssize_t ret;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	ret = simple_read_from_buffer(buf, count, ppos,
				      dev->buffer, dev->data_len);

	mutex_unlock(&dev->lock);

	return ret;
}

static ssize_t dynamic_cdev_write(struct file *file, const char __user *buf,
				  size_t count, loff_t *ppos)
{
	struct dynamic_cdev_device *dev = file->private_data;
	size_t write_len;
	ssize_t ret;

	write_len = min(count, (size_t)BUF_SIZE);

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	if (copy_from_user(dev->buffer, buf, write_len)) {
		ret = -EFAULT;
		goto out_unlock;
	}

	dev->data_len = write_len;
	ret = write_len;

out_unlock:
	mutex_unlock(&dev->lock);
	return ret;
}

static const struct file_operations dynamic_cdev_fops = {
	.owner		= THIS_MODULE,
	.open		= dynamic_cdev_open,
	.release	= dynamic_cdev_release,
	.read		= dynamic_cdev_read,
	.write		= dynamic_cdev_write,
	.llseek		= no_llseek,
};

static int __init dynamic_cdev_init(void)
{
	int ret;

	mutex_init(&dynamic_dev.lock);

	/* 由内核动态分配主、次设备号。 */
	ret = alloc_chrdev_region(&dynamic_dev.devt, 0, DEVICE_COUNT,
				  DEVICE_NAME);
	if (ret)
		return ret;

	cdev_init(&dynamic_dev.cdev, &dynamic_cdev_fops);
	dynamic_dev.cdev.owner = THIS_MODULE;

	ret = cdev_add(&dynamic_dev.cdev, dynamic_dev.devt, DEVICE_COUNT);
	if (ret)
		goto err_unregister_region;

	dynamic_dev.class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(dynamic_dev.class)) {
		ret = PTR_ERR(dynamic_dev.class);
		goto err_del_cdev;
	}

	dynamic_dev.device = device_create(dynamic_dev.class, NULL,
					   dynamic_dev.devt, NULL,
					   DEVICE_NAME);
	if (IS_ERR(dynamic_dev.device)) {
		ret = PTR_ERR(dynamic_dev.device);
		goto err_destroy_class;
	}

	pr_info("%s: registered, major=%u minor=%u\n",
		DEVICE_NAME, MAJOR(dynamic_dev.devt),
		MINOR(dynamic_dev.devt));

	return 0;

err_destroy_class:
	class_destroy(dynamic_dev.class);
err_del_cdev:
	cdev_del(&dynamic_dev.cdev);
err_unregister_region:
	unregister_chrdev_region(dynamic_dev.devt, DEVICE_COUNT);
	return ret;
}

static void __exit dynamic_cdev_exit(void)
{
	device_destroy(dynamic_dev.class, dynamic_dev.devt);
	class_destroy(dynamic_dev.class);
	cdev_del(&dynamic_dev.cdev);
	unregister_chrdev_region(dynamic_dev.devt, DEVICE_COUNT);
}

module_init(dynamic_cdev_init);
module_exit(dynamic_cdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("Dynamic device number and cdev example");
