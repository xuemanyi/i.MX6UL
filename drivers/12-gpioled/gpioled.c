// SPDX-License-Identifier: GPL-2.0
/* 基于设备树和 GPIO 子系统的 i.MX6ULL LED 字符设备驱动。 */

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

#define GPIOLED_COUNT		1
#define GPIOLED_NAME		"gpioled"
#define GPIOLED_NODE_PATH	"/gpioled"
#define GPIOLED_GPIO_PROPERTY	"led-gpio"
#define LED_OFF			0
#define LED_ON			1

/**
 * struct gpioled_device - GPIO LED 字符设备
 * @devid: 动态分配的字符设备号
 * @cdev: 字符设备对象
 * @class: sysfs 设备类
 * @device: sysfs 设备对象
 * @node: gpioled 设备树节点
 * @gpio: LED 使用的 Linux GPIO 编号
 * @active_low: LED 是否低电平有效
 * @lock: 保护 LED 状态访问的互斥锁
 *
 * 模块加载期间初始化该单实例对象，卸载期间按资源申请的逆序销毁。
 * GPIO 成功申请后由本驱动持有，直至模块退出或初始化失败回滚。
 */
struct gpioled_device {
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *node;
	int gpio;
	bool active_low;
	struct mutex lock;
};

static struct gpioled_device gpioled;

/**
 * gpioled_set_state() - 设置 LED 逻辑状态
 * @led: GPIO LED 设备
 * @state: LED_ON 表示点亮，LED_OFF 表示熄灭
 *
 * 根据设备树中的有效电平标志，将逻辑状态转换为 GPIO 物理电平。
 *
 * Context: 仅允许进程上下文调用，调用方必须持有 @led->lock。
 */
static void gpioled_set_state(struct gpioled_device *led, u8 state)
{
	int value;

	value = state == LED_ON;
	if (led->active_low)
		value = !value;
	gpio_set_value(led->gpio, value);
}

/**
 * gpioled_get_state() - 获取 LED 逻辑状态
 * @led: GPIO LED 设备
 *
 * Context: 仅允许进程上下文调用，调用方必须持有 @led->lock。
 * Return: LED 点亮返回 LED_ON，熄灭返回 LED_OFF。
 */
static u8 gpioled_get_state(struct gpioled_device *led)
{
	int value;

	value = !!gpio_get_value(led->gpio);
	if (led->active_low)
		value = !value;
	return value ? LED_ON : LED_OFF;
}

/**
 * gpioled_open() - 打开 GPIO LED 字符设备
 * @inode: 字符设备 inode
 * @filp: 打开的文件对象
 *
 * 将字符设备对象转换为 GPIO LED 设备并保存到文件私有数据中。
 *
 * Context: 仅允许进程上下文调用。
 * Return: 始终返回 0。
 */
static int gpioled_open(struct inode *inode, struct file *filp)
{
	struct gpioled_device *led;

	led = container_of(inode->i_cdev, struct gpioled_device, cdev);
	filp->private_data = led;
	return 0;
}

/**
 * gpioled_read() - 向用户态返回当前 LED 状态
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
static ssize_t gpioled_read(struct file *filp, char __user *buf,
			    size_t count, loff_t *offp)
{
	struct gpioled_device *led = filp->private_data;
	u8 state;

	if (!count || *offp)
		return 0;

	mutex_lock(&led->lock);
	state = gpioled_get_state(led);
	mutex_unlock(&led->lock);

	if (copy_to_user(buf, &state, sizeof(state)))
		return -EFAULT;
	*offp += sizeof(state);
	return sizeof(state);
}

/**
 * gpioled_write() - 根据用户态数据设置 LED 状态
 * @filp: 打开的文件对象
 * @buf: 保存 LED 状态的用户态缓冲区
 * @count: 用户态缓冲区长度
 * @offp: 文件偏移，本驱动不使用
 *
 * 用户态写入的首字节必须为 LED_ON 或 LED_OFF。
 *
 * Context: 仅允许进程上下文调用，可以睡眠。
 * Return: 成功返回 1，长度或状态非法返回 -EINVAL，复制失败返回 -EFAULT。
 */
static ssize_t gpioled_write(struct file *filp, const char __user *buf,
			     size_t count, loff_t *offp)
{
	struct gpioled_device *led = filp->private_data;
	u8 state;

	(void)offp;
	if (count < sizeof(state))
		return -EINVAL;
	if (copy_from_user(&state, buf, sizeof(state)))
		return -EFAULT;
	if (state != LED_ON && state != LED_OFF)
		return -EINVAL;

	mutex_lock(&led->lock);
	gpioled_set_state(led, state);
	mutex_unlock(&led->lock);
	return sizeof(state);
}

static const struct file_operations gpioled_fops = {
	.owner = THIS_MODULE,
	.open = gpioled_open,
	.read = gpioled_read,
	.write = gpioled_write,
	.llseek = no_llseek,
};

/**
 * gpioled_init() - 初始化 GPIO LED 驱动
 *
 * 查找 /gpioled，获取并申请 led-gpio，将 LED 默认设置为熄灭，随后注册动态
 * 字符设备和 /dev/gpioled。
 *
 * Context: 模块加载期间在进程上下文调用，可以睡眠。
 * Return: 成功返回 0，失败返回对应的负错误码。
 */
static int __init gpioled_init(void)
{
	enum of_gpio_flags flags;
	int inactive_value;
	int ret;

	mutex_init(&gpioled.lock);
	gpioled.node = of_find_node_by_path(GPIOLED_NODE_PATH);
	if (!gpioled.node) {
		pr_err("%s: device tree node %s not found\n",
		       GPIOLED_NAME, GPIOLED_NODE_PATH);
		return -ENODEV;
	}
	if (!of_device_is_compatible(gpioled.node, "atkalpha-gpioled") ||
	    !of_device_is_available(gpioled.node)) {
		pr_err("%s: device tree node is unavailable or incompatible\n",
		       GPIOLED_NAME);
		ret = -ENODEV;
		goto err_put_node;
	}

	gpioled.gpio = of_get_named_gpio_flags(gpioled.node,
						GPIOLED_GPIO_PROPERTY, 0, &flags);
	if (!gpio_is_valid(gpioled.gpio)) {
		ret = gpioled.gpio < 0 ? gpioled.gpio : -EINVAL;
		pr_err("%s: failed to get %s: %d\n", GPIOLED_NAME,
		       GPIOLED_GPIO_PROPERTY, ret);
		goto err_put_node;
	}
	gpioled.active_low = flags & OF_GPIO_ACTIVE_LOW;

	ret = gpio_request(gpioled.gpio, GPIOLED_NAME);
	if (ret) {
		pr_err("%s: failed to request GPIO %d: %d\n",
		       GPIOLED_NAME, gpioled.gpio, ret);
		goto err_put_node;
	}

	inactive_value = gpioled.active_low ? 1 : 0;
	ret = gpio_direction_output(gpioled.gpio, inactive_value);
	if (ret) {
		pr_err("%s: failed to configure GPIO %d as output: %d\n",
		       GPIOLED_NAME, gpioled.gpio, ret);
		goto err_free_gpio;
	}

	ret = alloc_chrdev_region(&gpioled.devid, 0, GPIOLED_COUNT,
				  GPIOLED_NAME);
	if (ret) {
		pr_err("%s: failed to allocate device number: %d\n",
		       GPIOLED_NAME, ret);
		goto err_free_gpio;
	}

	cdev_init(&gpioled.cdev, &gpioled_fops);
	gpioled.cdev.owner = THIS_MODULE;
	ret = cdev_add(&gpioled.cdev, gpioled.devid, GPIOLED_COUNT);
	if (ret) {
		pr_err("%s: failed to add cdev: %d\n", GPIOLED_NAME, ret);
		goto err_unregister;
	}

	gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
	if (IS_ERR(gpioled.class)) {
		ret = PTR_ERR(gpioled.class);
		pr_err("%s: failed to create class: %d\n", GPIOLED_NAME, ret);
		goto err_del_cdev;
	}

	gpioled.device = device_create(gpioled.class, NULL, gpioled.devid,
					   NULL, GPIOLED_NAME);
	if (IS_ERR(gpioled.device)) {
		ret = PTR_ERR(gpioled.device);
		pr_err("%s: failed to create device: %d\n", GPIOLED_NAME, ret);
		goto err_destroy_class;
	}

	pr_info("%s: registered, GPIO=%d major=%u minor=%u\n", GPIOLED_NAME,
		gpioled.gpio, MAJOR(gpioled.devid), MINOR(gpioled.devid));
	return 0;

err_destroy_class:
	class_destroy(gpioled.class);
err_del_cdev:
	cdev_del(&gpioled.cdev);
err_unregister:
	unregister_chrdev_region(gpioled.devid, GPIOLED_COUNT);
err_free_gpio:
	gpio_free(gpioled.gpio);
err_put_node:
	of_node_put(gpioled.node);
	gpioled.node = NULL;
	return ret;
}

/**
 * gpioled_exit() - 注销 GPIO LED 驱动
 *
 * 先移除用户态访问入口，再熄灭 LED 并释放 GPIO 和设备树节点引用。
 *
 * Context: 模块卸载期间在进程上下文调用，可以睡眠。
 */
static void __exit gpioled_exit(void)
{
	device_destroy(gpioled.class, gpioled.devid);
	class_destroy(gpioled.class);
	cdev_del(&gpioled.cdev);
	unregister_chrdev_region(gpioled.devid, GPIOLED_COUNT);

	mutex_lock(&gpioled.lock);
	gpioled_set_state(&gpioled, LED_OFF);
	mutex_unlock(&gpioled.lock);
	gpio_free(gpioled.gpio);
	of_node_put(gpioled.node);
	pr_info("%s: unregistered\n", GPIOLED_NAME);
}

module_init(gpioled_init);
module_exit(gpioled_exit);

MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("Device-tree-based GPIO LED character driver");
MODULE_LICENSE("GPL");
