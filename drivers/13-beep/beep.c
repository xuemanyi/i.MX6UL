// SPDX-License-Identifier: GPL-2.0
/* 基于设备树和 GPIO 子系统的 i.MX6ULL 有源蜂鸣器字符设备驱动。 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#define BEEP_COUNT		1
#define BEEP_NAME		"beep"
#define BEEP_NODE_PATH		"/beep"
#define BEEP_GPIO_PROPERTY	"beep-gpio"
#define BEEP_OFF		0
#define BEEP_ON			1

/**
 * struct beep_device - GPIO 蜂鸣器字符设备
 * @devid: 动态分配的字符设备号
 * @cdev: 字符设备对象
 * @class: sysfs 设备类
 * @device: sysfs 设备对象
 * @node: beep 设备树节点
 * @gpio: 蜂鸣器使用的 Linux GPIO 编号
 * @active_low: 蜂鸣器是否低电平有效
 * @lock: 保护蜂鸣器状态访问的互斥锁
 *
 * 模块加载期间初始化该单实例对象，卸载期间按资源申请的逆序销毁。
 * GPIO 成功申请后由本驱动持有，直至模块退出或初始化失败回滚。
 */
struct beep_device {
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *node;
	int gpio;
	bool active_low;
	struct mutex lock;
};

static struct beep_device beep;

/**
 * beep_set_state() - 设置蜂鸣器逻辑状态
 * @bdev: 蜂鸣器设备
 * @state: BEEP_ON 表示鸣响，BEEP_OFF 表示关闭
 *
 * 根据设备树中的有效电平标志，将逻辑状态转换为 GPIO 物理电平。
 *
 * Context: 仅允许进程上下文调用，调用方必须持有 @bdev->lock。
 */
static void beep_set_state(struct beep_device *bdev, u8 state)
{
	int value;

	value = state == BEEP_ON;
	if (bdev->active_low)
		value = !value;
	gpio_set_value(bdev->gpio, value);
}

/**
 * beep_get_state() - 获取蜂鸣器逻辑状态
 * @bdev: 蜂鸣器设备
 *
 * Context: 仅允许进程上下文调用，调用方必须持有 @bdev->lock。
 * Return: 蜂鸣器鸣响返回 BEEP_ON，关闭返回 BEEP_OFF。
 */
static u8 beep_get_state(struct beep_device *bdev)
{
	int value;

	value = !!gpio_get_value(bdev->gpio);
	if (bdev->active_low)
		value = !value;
	return value ? BEEP_ON : BEEP_OFF;
}

/**
 * beep_open() - 打开蜂鸣器字符设备
 * @inode: 字符设备 inode
 * @filp: 打开的文件对象
 *
 * 将字符设备对象转换为蜂鸣器设备并保存到文件私有数据中。
 *
 * Context: 仅允许进程上下文调用。
 * Return: 始终返回 0。
 */
static int beep_open(struct inode *inode, struct file *filp)
{
	struct beep_device *bdev;

	bdev = container_of(inode->i_cdev, struct beep_device, cdev);
	filp->private_data = bdev;
	return 0;
}

/**
 * beep_read() - 向用户态返回当前蜂鸣器状态
 * @filp: 打开的文件对象
 * @buf: 用户态接收缓冲区
 * @count: 用户态缓冲区长度
 * @offp: 文件偏移
 *
 * 每次打开设备只返回一个状态字节，后续读取返回 EOF。
 *
 * Context: 仅允许进程上下文调用，可以睡眠。
 * Return: 成功返回 1，已读完返回 0，复制失败返回 -EFAULT。
 */
static ssize_t beep_read(struct file *filp, char __user *buf,
			 size_t count, loff_t *offp)
{
	struct beep_device *bdev = filp->private_data;
	u8 state;

	if (!count || *offp)
		return 0;

	mutex_lock(&bdev->lock);
	state = beep_get_state(bdev);
	mutex_unlock(&bdev->lock);

	if (copy_to_user(buf, &state, sizeof(state)))
		return -EFAULT;
	*offp += sizeof(state);
	return sizeof(state);
}

/**
 * beep_write() - 根据用户态数据设置蜂鸣器状态
 * @filp: 打开的文件对象
 * @buf: 保存蜂鸣器状态的用户态缓冲区
 * @count: 用户态缓冲区长度
 * @offp: 文件偏移，本驱动不使用
 *
 * 用户态写入的首字节必须为 BEEP_ON 或 BEEP_OFF。
 *
 * Context: 仅允许进程上下文调用，可以睡眠。
 * Return: 成功返回 1，长度或状态非法返回 -EINVAL，复制失败返回 -EFAULT。
 */
static ssize_t beep_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *offp)
{
	struct beep_device *bdev = filp->private_data;
	u8 state;

	(void)offp;
	if (count < sizeof(state))
		return -EINVAL;
	if (copy_from_user(&state, buf, sizeof(state)))
		return -EFAULT;
	if (state != BEEP_ON && state != BEEP_OFF)
		return -EINVAL;

	mutex_lock(&bdev->lock);
	beep_set_state(bdev, state);
	mutex_unlock(&bdev->lock);
	return sizeof(state);
}

static const struct file_operations beep_fops = {
	.owner = THIS_MODULE,
	.open = beep_open,
	.read = beep_read,
	.write = beep_write,
	.llseek = no_llseek,
};

/**
 * beep_init() - 初始化 GPIO 蜂鸣器驱动
 *
 * 查找 /beep，获取并申请 beep-gpio，将蜂鸣器默认关闭，随后注册动态字符设备
 * 和 /dev/beep。
 *
 * Context: 模块加载期间在进程上下文调用，可以睡眠。
 * Return: 成功返回 0，失败返回对应的负错误码。
 */
static int __init beep_init(void)
{
	enum of_gpio_flags flags;
	int inactive_value;
	int ret;

	mutex_init(&beep.lock);
	beep.node = of_find_node_by_path(BEEP_NODE_PATH);
	if (!beep.node) {
		pr_err("%s: device tree node %s not found\n",
		       BEEP_NAME, BEEP_NODE_PATH);
		return -ENODEV;
	}
	if (!of_device_is_compatible(beep.node, "atkalpha-beep") ||
	    !of_device_is_available(beep.node)) {
		pr_err("%s: device tree node is unavailable or incompatible\n",
		       BEEP_NAME);
		ret = -ENODEV;
		goto err_put_node;
	}

	beep.gpio = of_get_named_gpio_flags(beep.node, BEEP_GPIO_PROPERTY, 0,
					    &flags);
	if (!gpio_is_valid(beep.gpio)) {
		ret = beep.gpio < 0 ? beep.gpio : -EINVAL;
		pr_err("%s: failed to get %s: %d\n", BEEP_NAME,
		       BEEP_GPIO_PROPERTY, ret);
		goto err_put_node;
	}
	beep.active_low = flags & OF_GPIO_ACTIVE_LOW;

	ret = gpio_request(beep.gpio, BEEP_NAME);
	if (ret) {
		pr_err("%s: failed to request GPIO %d: %d\n",
		       BEEP_NAME, beep.gpio, ret);
		goto err_put_node;
	}

	inactive_value = beep.active_low ? 1 : 0;
	ret = gpio_direction_output(beep.gpio, inactive_value);
	if (ret) {
		pr_err("%s: failed to configure GPIO %d as output: %d\n",
		       BEEP_NAME, beep.gpio, ret);
		goto err_free_gpio;
	}

	ret = alloc_chrdev_region(&beep.devid, 0, BEEP_COUNT, BEEP_NAME);
	if (ret) {
		pr_err("%s: failed to allocate device number: %d\n",
		       BEEP_NAME, ret);
		goto err_free_gpio;
	}

	cdev_init(&beep.cdev, &beep_fops);
	beep.cdev.owner = THIS_MODULE;
	ret = cdev_add(&beep.cdev, beep.devid, BEEP_COUNT);
	if (ret) {
		pr_err("%s: failed to add cdev: %d\n", BEEP_NAME, ret);
		goto err_unregister;
	}

	beep.class = class_create(THIS_MODULE, BEEP_NAME);
	if (IS_ERR(beep.class)) {
		ret = PTR_ERR(beep.class);
		pr_err("%s: failed to create class: %d\n", BEEP_NAME, ret);
		goto err_del_cdev;
	}

	beep.device = device_create(beep.class, NULL, beep.devid, NULL,
				    BEEP_NAME);
	if (IS_ERR(beep.device)) {
		ret = PTR_ERR(beep.device);
		pr_err("%s: failed to create device: %d\n", BEEP_NAME, ret);
		goto err_destroy_class;
	}

	pr_info("%s: registered, GPIO=%d major=%u minor=%u\n", BEEP_NAME,
		beep.gpio, MAJOR(beep.devid), MINOR(beep.devid));
	return 0;

err_destroy_class:
	class_destroy(beep.class);
err_del_cdev:
	cdev_del(&beep.cdev);
err_unregister:
	unregister_chrdev_region(beep.devid, BEEP_COUNT);
err_free_gpio:
	gpio_free(beep.gpio);
err_put_node:
	of_node_put(beep.node);
	beep.node = NULL;
	return ret;
}

/**
 * beep_exit() - 注销 GPIO 蜂鸣器驱动
 *
 * 先移除用户态访问入口，再关闭蜂鸣器并释放 GPIO 和设备树节点引用。
 *
 * Context: 模块卸载期间在进程上下文调用，可以睡眠。
 */
static void __exit beep_exit(void)
{
	device_destroy(beep.class, beep.devid);
	class_destroy(beep.class);
	cdev_del(&beep.cdev);
	unregister_chrdev_region(beep.devid, BEEP_COUNT);

	mutex_lock(&beep.lock);
	beep_set_state(&beep, BEEP_OFF);
	mutex_unlock(&beep.lock);
	gpio_free(beep.gpio);
	of_node_put(beep.node);
	pr_info("%s: unregistered\n", BEEP_NAME);
}

module_init(beep_init);
module_exit(beep_exit);

MODULE_AUTHOR("ALIENTEK");
MODULE_DESCRIPTION("Device-tree-based GPIO active buzzer character driver");
MODULE_LICENSE("GPL");
