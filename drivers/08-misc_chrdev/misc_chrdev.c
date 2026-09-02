// SPDX-License-Identifier: GPL-2.0
/*
 * misc_chrdev.c - Character device implemented as a miscellaneous device
 *
 * This module demonstrates how to create a simple character device using
 * the miscdevice framework. The miscellaneous device subsystem manages the
 * character device registration, device number, device object, and device
 * node creation.
 *
 * The device is registered with misc_register() and provides the
 * /dev/misc_chrdev device node.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#define DEVICE_NAME	"misc_chrdev"
#define BUF_SIZE	128

/*
 * struct misc_chrdev_device - Miscellaneous character device context
 * @miscdev: miscellaneous device registration object
 * @buffer: kernel buffer used to store user data
 * @data_len: number of valid bytes currently stored in @buffer
 * @lock: protects @buffer and @data_len
 */
struct misc_chrdev_device {
	struct miscdevice miscdev;

	char buffer[BUF_SIZE];
	size_t data_len;
	struct mutex lock;
};

static struct misc_chrdev_device misc_dev;

/*
 * misc_chrdev_open - Open the miscellaneous character device
 * @inode: inode associated with the opened device node
 * @file: file object created for this open operation
 *
 * Before calling this function, misc_open() stores the address of the
 * corresponding struct miscdevice in file->private_data. Convert that
 * pointer to the complete driver-private structure and save it back in
 * file->private_data for subsequent read and write operations.
 */
static int misc_chrdev_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc;
	struct misc_chrdev_device *dev;

	misc = file->private_data;
	dev = container_of(misc, struct misc_chrdev_device, miscdev);

	file->private_data = dev;

	return 0;
}

/*
 * misc_chrdev_release - Release the miscellaneous character device
 * @inode: inode associated with the device
 * @file: file object being released
 */
static int misc_chrdev_release(struct inode *inode, struct file *file)
{
	return 0;
}

/*
 * misc_chrdev_read - Read data from the kernel buffer
 * @file: opened file object
 * @buf: userspace destination buffer
 * @count: maximum number of bytes requested
 * @ppos: current file position
 *
 * Return: number of bytes copied, zero at end of data, or a negative error.
 */
static ssize_t misc_chrdev_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	struct misc_chrdev_device *dev = file->private_data;
	ssize_t ret;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	ret = simple_read_from_buffer(buf, count, ppos,
				      dev->buffer, dev->data_len);

	mutex_unlock(&dev->lock);

	return ret;
}

/*
 * misc_chrdev_write - Write userspace data into the kernel buffer
 * @file: opened file object
 * @buf: userspace source buffer
 * @count: number of bytes requested
 * @ppos: current file position
 *
 * At most BUF_SIZE bytes are accepted. New data replaces the previously
 * stored data rather than being appended to it.
 *
 * Return: number of bytes copied or a negative error.
 */
static ssize_t misc_chrdev_write(struct file *file,
				 const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct misc_chrdev_device *dev = file->private_data;
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

	/*
	 * The driver uses replacement semantics. Reset the file position so
	 * data written through this descriptor can be read from the beginning.
	 */
	*ppos = 0;
	ret = write_len;

out_unlock:
	mutex_unlock(&dev->lock);

	return ret;
}

static const struct file_operations misc_chrdev_fops = {
	.owner		= THIS_MODULE,
	.open		= misc_chrdev_open,
	.release	= misc_chrdev_release,
	.read		= misc_chrdev_read,
	.write		= misc_chrdev_write,
	.llseek		= no_llseek,
};

/*
 * misc_chrdev_init - Initialize and register the miscellaneous device
 *
 * MISC_DYNAMIC_MINOR instructs the miscdevice subsystem to allocate an
 * available minor device number automatically. The miscdevice subsystem
 * also handles the device class, device object, and /dev node creation.
 */
static int __init misc_chrdev_init(void)
{
	int ret;

	mutex_init(&misc_dev.lock);

	misc_dev.miscdev.minor = MISC_DYNAMIC_MINOR;
	misc_dev.miscdev.name = DEVICE_NAME;
	misc_dev.miscdev.fops = &misc_chrdev_fops;

	ret = misc_register(&misc_dev.miscdev);
	if (ret) {
		pr_err("%s: failed to register misc device: %d\n",
		       DEVICE_NAME, ret);
		return ret;
	}

	pr_info("%s: registered, minor=%d\n",
		DEVICE_NAME, misc_dev.miscdev.minor);

	return 0;
}

/*
 * misc_chrdev_exit - Unregister the miscellaneous device
 */
static void __exit misc_chrdev_exit(void)
{
	misc_deregister(&misc_dev.miscdev);

	pr_info("%s: unregistered\n", DEVICE_NAME);
}

module_init(misc_chrdev_init);
module_exit(misc_chrdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sucre");
MODULE_DESCRIPTION("Miscellaneous character device example");