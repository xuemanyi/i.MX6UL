// SPDX-License-Identifier: GPL-2.0
/* 基于设备树、GPIO 和中断的 i.MX6ULL 按键字符设备驱动。 */

#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#define KEY_COUNT		1
#define KEY_NAME		"key"
#define KEY_NODE_PATH		"/key"
#define KEY_GPIO_PROPERTY	"key-gpio"
#define KEY0_VALUE		0xf0
#define INVALID_KEY		0x00

/**
 * struct key_device - GPIO 按键字符设备
 * @devid: 动态分配的字符设备号
 * @cdev: 字符设备对象
 * @class: sysfs 设备类
 * @device: sysfs 设备对象
 * @node: key 设备树节点
 * @gpio: 按键使用的 Linux GPIO 编号
 * @irq: GPIO 对应的 Linux IRQ
 * @active_low: 按键按下是否为低电平
 * @keyvalue: 最近一次按键事件值
 * @event: 是否存在尚未读取的按键事件
 * @wait: 等待按键事件的等待队列
 * @lock: 保护事件状态和 GPIO 读取的互斥锁
 */
struct key_device {
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *node;
	int gpio;
	int irq;
	bool active_low;
	atomic_t keyvalue;
	atomic_t event;
	wait_queue_head_t wait;
	struct mutex lock;
};

static struct key_device keydev;

/**
 * key_irq_handler() - 处理 GPIO 按键边沿中断
 * @irq: 中断号，本驱动不使用
 * @data: 指向 struct key_device
 *
 * 按下时记录 KEY0_VALUE 并唤醒阻塞读取；释放时清除当前按键状态。
 *
 * Context: hardirq 上下文，不允许睡眠。
 * Return: 始终返回 IRQ_HANDLED。
 */
static irqreturn_t key_irq_handler(int irq, void *data)
{
	struct key_device *key = data;
	bool pressed;

	(void)irq;
	pressed = !!gpio_get_value(key->gpio);
	if (key->active_low)
		pressed = !pressed;
	if (pressed) {
		atomic_set(&key->keyvalue, KEY0_VALUE);
		atomic_set(&key->event, 1);
		wake_up_interruptible(&key->wait);
	} else {
		atomic_set(&key->keyvalue, INVALID_KEY);
	}
	return IRQ_HANDLED;
}

/**
 * key_open() - 打开按键字符设备
 * @inode: 字符设备 inode
 * @filp: 打开的文件对象
 *
 * Context: 仅允许进程上下文调用。
 * Return: 始终返回 0。
 */
static int key_open(struct inode *inode, struct file *filp)
{
	struct key_device *key;

	key = container_of(inode->i_cdev, struct key_device, cdev);
	filp->private_data = key;
	return 0;
}

/**
 * key_read() - 阻塞读取一次按键按下事件
 * @filp: 打开的文件对象
 * @buf: 用户态接收缓冲区
 * @count: 用户态缓冲区长度
 * @offp: 文件偏移，本驱动不使用
 *
 * 非阻塞文件返回 -EAGAIN；普通文件在没有事件时睡眠，按下后返回 KEY0_VALUE。
 *
 * Context: 仅允许进程上下文调用，可以睡眠。
 * Return: 成功返回 1，参数非法返回 -EINVAL，被信号中断返回 -ERESTARTSYS，
 * 复制失败返回 -EFAULT，无事件的非阻塞读取返回 -EAGAIN。
 */
static ssize_t key_read(struct file *filp, char __user *buf, size_t count,
			loff_t *offp)
{
	struct key_device *key = filp->private_data;
	u8 value;
	int ret;

	(void)offp;
	if (count < sizeof(value))
		return -EINVAL;
	if (!(filp->f_flags & O_NONBLOCK)) {
		ret = wait_event_interruptible(key->wait,
					       atomic_read(&key->event));
		if (ret)
			return -ERESTARTSYS;
	} else if (!atomic_read(&key->event)) {
		return -EAGAIN;
	}

	mutex_lock(&key->lock);
	if (!atomic_xchg(&key->event, 0)) {
		mutex_unlock(&key->lock);
		return -EAGAIN;
	}
	value = atomic_read(&key->keyvalue);
	mutex_unlock(&key->lock);
	if (copy_to_user(buf, &value, sizeof(value)))
		return -EFAULT;
	return sizeof(value);
}

static const struct file_operations key_fops = {
	.owner = THIS_MODULE,
	.open = key_open,
	.read = key_read,
	.llseek = no_llseek,
};

/**
 * key_module_init() - 初始化按键 GPIO、中断和字符设备
 *
 * 解析 /key 的 key-gpio，申请 GPIO 和双边沿 IRQ，再创建 /dev/key。
 *
 * Context: 模块加载期间在进程上下文调用，可以睡眠。
 * Return: 成功返回 0，失败返回对应的负错误码。
 */
static int __init key_module_init(void)
{
	enum of_gpio_flags flags;
	int ret;

	atomic_set(&keydev.keyvalue, INVALID_KEY);
	atomic_set(&keydev.event, 0);
	init_waitqueue_head(&keydev.wait);
	mutex_init(&keydev.lock);

	keydev.node = of_find_node_by_path(KEY_NODE_PATH);
	if (!keydev.node) {
		pr_err("%s: device tree node %s not found\n", KEY_NAME,
		       KEY_NODE_PATH);
		return -ENODEV;
	}
	if (!of_device_is_compatible(keydev.node, "atkalpha-key") ||
	    !of_device_is_available(keydev.node)) {
		pr_err("%s: device tree node is unavailable or incompatible\n",
		       KEY_NAME);
		ret = -ENODEV;
		goto err_put_node;
	}

	keydev.gpio = of_get_named_gpio_flags(keydev.node, KEY_GPIO_PROPERTY,
					      0, &flags);
	if (!gpio_is_valid(keydev.gpio)) {
		ret = keydev.gpio < 0 ? keydev.gpio : -EINVAL;
		pr_err("%s: failed to get %s: %d\n", KEY_NAME,
		       KEY_GPIO_PROPERTY, ret);
		goto err_put_node;
	}
	keydev.active_low = flags & OF_GPIO_ACTIVE_LOW;
	ret = gpio_request(keydev.gpio, KEY_NAME);
	if (ret) {
		pr_err("%s: failed to request GPIO %d: %d\n", KEY_NAME,
		       keydev.gpio, ret);
		goto err_put_node;
	}
	ret = gpio_direction_input(keydev.gpio);
	if (ret) {
		pr_err("%s: failed to configure GPIO %d as input: %d\n",
		       KEY_NAME, keydev.gpio, ret);
		goto err_free_gpio;
	}
	keydev.irq = gpio_to_irq(keydev.gpio);
	if (keydev.irq < 0) {
		ret = keydev.irq;
		pr_err("%s: failed to map GPIO %d to IRQ: %d\n", KEY_NAME,
		       keydev.gpio, ret);
		goto err_free_gpio;
	}
	ret = request_irq(keydev.irq, key_irq_handler,
			  IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			  KEY_NAME, &keydev);
	if (ret) {
		pr_err("%s: failed to request IRQ %d: %d\n", KEY_NAME,
		       keydev.irq, ret);
		goto err_free_gpio;
	}

	ret = alloc_chrdev_region(&keydev.devid, 0, KEY_COUNT, KEY_NAME);
	if (ret)
		goto err_free_irq;
	cdev_init(&keydev.cdev, &key_fops);
	keydev.cdev.owner = THIS_MODULE;
	ret = cdev_add(&keydev.cdev, keydev.devid, KEY_COUNT);
	if (ret)
		goto err_unregister;
	keydev.class = class_create(THIS_MODULE, KEY_NAME);
	if (IS_ERR(keydev.class)) {
		ret = PTR_ERR(keydev.class);
		goto err_del_cdev;
	}
	keydev.device = device_create(keydev.class, NULL, keydev.devid, NULL,
				      KEY_NAME);
	if (IS_ERR(keydev.device)) {
		ret = PTR_ERR(keydev.device);
		goto err_destroy_class;
	}
	pr_info("%s: registered, GPIO=%d IRQ=%d major=%u minor=%u\n", KEY_NAME,
		keydev.gpio, keydev.irq, MAJOR(keydev.devid), MINOR(keydev.devid));
	return 0;

err_destroy_class:
	class_destroy(keydev.class);
err_del_cdev:
	cdev_del(&keydev.cdev);
err_unregister:
	unregister_chrdev_region(keydev.devid, KEY_COUNT);
err_free_irq:
	free_irq(keydev.irq, &keydev);
err_free_gpio:
	gpio_free(keydev.gpio);
err_put_node:
	of_node_put(keydev.node);
	keydev.node = NULL;
	return ret;
}

/**
 * key_module_exit() - 注销按键字符设备并释放资源
 *
 * Context: 模块卸载期间在进程上下文调用，可以睡眠。
 */
static void __exit key_module_exit(void)
{
	device_destroy(keydev.class, keydev.devid);
	class_destroy(keydev.class);
	cdev_del(&keydev.cdev);
	unregister_chrdev_region(keydev.devid, KEY_COUNT);
	free_irq(keydev.irq, &keydev);
	gpio_free(keydev.gpio);
	of_node_put(keydev.node);
	pr_info("%s: unregistered\n", KEY_NAME);
}

module_init(key_module_init);
module_exit(key_module_exit);

MODULE_AUTHOR("ALIENTEK");
MODULE_DESCRIPTION("Device-tree GPIO key character driver");
MODULE_LICENSE("GPL");
