// SPDX-License-Identifier: GPL-2.0
/* 基于内核定时器和 GPIO 子系统的 LED 闪烁字符设备驱动。 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/types.h>

#include "include/timer_ioctl.h"

#define TIMER_COUNT		1
#define TIMER_NAME		"timer"
#define TIMER_NODE_PATH		"/gpioled"
#define TIMER_GPIO_PROPERTY	"led-gpio"
#define TIMER_DEFAULT_PERIOD_MS	1000U

/**
 * struct timer_device - GPIO LED 定时器字符设备
 * @devid: 动态分配的字符设备号
 * @cdev: 字符设备对象
 * @class: sysfs 设备类
 * @device: sysfs 设备对象
 * @node: gpioled 设备树节点
 * @gpio: LED 使用的 Linux GPIO 编号
 * @active_low: LED 是否低电平有效
 * @period_ms: LED 翻转周期，单位为毫秒
 * @led_on: 当前 LED 的逻辑状态
 * @running: 定时器是否应继续周期运行
 * @timer: 周期翻转 LED 的内核定时器
 * @lock: 保护回调与进程上下文共享状态的自旋锁
 * @ioctl_lock: 串行化用户态控制命令
 *
 * 模块加载期间初始化该单实例对象，卸载期间按资源申请的逆序销毁。
 * @lock 可在 timer softirq 和进程上下文获取；持锁期间不得睡眠。
 */
struct timer_device {
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *node;
	int gpio;
	bool active_low;
	unsigned int period_ms;
	bool led_on;
	bool running;
	struct timer_list timer;
	spinlock_t lock;
	struct mutex ioctl_lock;
};

static struct timer_device timerdev;

/**
 * timer_set_led() - 设置 LED 的逻辑状态
 * @dev: 定时器设备
 * @on: true 表示点亮，false 表示熄灭
 *
 * Context: 可在 timer softirq 上下文调用，不允许睡眠。
 */
static void timer_set_led(struct timer_device *dev, bool on)
{
	int value = on;

	if (dev->active_low)
		value = !value;
	gpio_set_value(dev->gpio, value);
}

/**
 * timer_callback() - 翻转 LED 并重新启动定时器
 * @data: 指向 struct timer_device 的无符号长整型参数
 *
 * 仅当 @running 为 true 时重新安排下一周期，避免关闭命令与回调并发时
 * 回调再次激活定时器。
 *
 * Context: timer softirq 上下文，不允许睡眠。
 */
static void timer_callback(unsigned long data)
{
	struct timer_device *dev = (struct timer_device *)data;
	unsigned long flags;
	unsigned int period_ms;

	spin_lock_irqsave(&dev->lock, flags);
	if (!dev->running) {
		spin_unlock_irqrestore(&dev->lock, flags);
		return;
	}
	dev->led_on = !dev->led_on;
	period_ms = dev->period_ms;
	timer_set_led(dev, dev->led_on);
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(period_ms));
	spin_unlock_irqrestore(&dev->lock, flags);
}

/**
 * timer_open() - 打开定时器字符设备
 * @inode: 字符设备 inode
 * @filp: 打开的文件对象
 *
 * Context: 仅允许进程上下文调用。
 * Return: 始终返回 0。
 */
static int timer_open(struct inode *inode, struct file *filp)
{
	struct timer_device *dev;

	dev = container_of(inode->i_cdev, struct timer_device, cdev);
	filp->private_data = dev;
	return 0;
}

/**
 * timer_ioctl() - 处理用户态定时器控制命令
 * @filp: 打开的文件对象
 * @cmd: CLOSE_CMD、OPEN_CMD 或 SETPERIOD_CMD
 * @arg: SETPERIOD_CMD 的周期值，单位为毫秒
 *
 * SETPERIOD_CMD 更新周期并启动或重新安排定时器。周期必须大于 0，且必须能
 * 使用 unsigned int 表示。
 *
 * Context: 仅允许进程上下文调用，可以睡眠。
 * Return: 成功返回 0，命令非法返回 -ENOTTY，周期非法返回 -EINVAL。
 */
static long timer_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct timer_device *dev = filp->private_data;
	unsigned long flags;
	unsigned int period_ms;
	long ret = 0;

	mutex_lock(&dev->ioctl_lock);
	switch (cmd) {
	case CLOSE_CMD:
		spin_lock_irqsave(&dev->lock, flags);
		dev->running = false;
		spin_unlock_irqrestore(&dev->lock, flags);
		del_timer_sync(&dev->timer);
		pr_info("%s: timer stopped\n", TIMER_NAME);
		break;
	case OPEN_CMD:
		spin_lock_irqsave(&dev->lock, flags);
		dev->running = true;
		period_ms = dev->period_ms;
		mod_timer(&dev->timer,
			  jiffies + msecs_to_jiffies(period_ms));
		spin_unlock_irqrestore(&dev->lock, flags);
		pr_info("%s: timer started, period=%u ms\n", TIMER_NAME,
			period_ms);
		break;
	case SETPERIOD_CMD:
		if (!arg || arg > UINT_MAX) {
			pr_warn("%s: invalid timer period: %lu ms\n",
				TIMER_NAME, arg);
			ret = -EINVAL;
			break;
		}
		period_ms = arg;
		spin_lock_irqsave(&dev->lock, flags);
		dev->period_ms = period_ms;
		dev->running = true;
		mod_timer(&dev->timer,
			  jiffies + msecs_to_jiffies(period_ms));
		spin_unlock_irqrestore(&dev->lock, flags);
		pr_info("%s: timer period set to %u ms\n", TIMER_NAME,
			period_ms);
		break;
	default:
		pr_warn("%s: unsupported ioctl command: %#x\n", TIMER_NAME,
			cmd);
		ret = -ENOTTY;
		break;
	}
	mutex_unlock(&dev->ioctl_lock);
	return ret;
}

static const struct file_operations timer_fops = {
	.owner = THIS_MODULE,
	.open = timer_open,
	.unlocked_ioctl = timer_ioctl,
	.llseek = no_llseek,
};

/**
 * timer_module_init() - 初始化 GPIO、内核定时器和字符设备
 *
 * 解析 /gpioled 的 led-gpio，将 LED 默认设置为熄灭，并创建 /dev/timer。
 * 定时器默认周期为 1000 ms，但加载模块时不自动启动。
 *
 * Context: 模块加载期间在进程上下文调用，可以睡眠。
 * Return: 成功返回 0，失败返回对应的负错误码。
 */
static int __init timer_module_init(void)
{
	enum of_gpio_flags flags;
	int inactive_value;
	int ret;

	spin_lock_init(&timerdev.lock);
	mutex_init(&timerdev.ioctl_lock);
	timerdev.period_ms = TIMER_DEFAULT_PERIOD_MS;
	timerdev.node = of_find_node_by_path(TIMER_NODE_PATH);
	if (!timerdev.node) {
		pr_err("%s: device tree node %s not found\n", TIMER_NAME,
		       TIMER_NODE_PATH);
		return -ENODEV;
	}
	if (!of_device_is_available(timerdev.node)) {
		pr_err("%s: device tree node is unavailable\n", TIMER_NAME);
		ret = -ENODEV;
		goto err_put_node;
	}

	timerdev.gpio = of_get_named_gpio_flags(timerdev.node,
					       TIMER_GPIO_PROPERTY, 0, &flags);
	if (!gpio_is_valid(timerdev.gpio)) {
		ret = timerdev.gpio < 0 ? timerdev.gpio : -EINVAL;
		pr_err("%s: failed to get %s: %d\n", TIMER_NAME,
		       TIMER_GPIO_PROPERTY, ret);
		goto err_put_node;
	}
	timerdev.active_low = flags & OF_GPIO_ACTIVE_LOW;
	ret = gpio_request(timerdev.gpio, TIMER_NAME);
	if (ret) {
		pr_err("%s: failed to request GPIO %d: %d\n", TIMER_NAME,
		       timerdev.gpio, ret);
		goto err_put_node;
	}

	inactive_value = timerdev.active_low ? 1 : 0;
	ret = gpio_direction_output(timerdev.gpio, inactive_value);
	if (ret) {
		pr_err("%s: failed to configure GPIO %d as output: %d\n",
		       TIMER_NAME, timerdev.gpio, ret);
		goto err_free_gpio;
	}

	setup_timer(&timerdev.timer, timer_callback,
		    (unsigned long)&timerdev);
	ret = alloc_chrdev_region(&timerdev.devid, 0, TIMER_COUNT, TIMER_NAME);
	if (ret)
		goto err_free_gpio;
	cdev_init(&timerdev.cdev, &timer_fops);
	timerdev.cdev.owner = THIS_MODULE;
	ret = cdev_add(&timerdev.cdev, timerdev.devid, TIMER_COUNT);
	if (ret)
		goto err_unregister;
	timerdev.class = class_create(THIS_MODULE, TIMER_NAME);
	if (IS_ERR(timerdev.class)) {
		ret = PTR_ERR(timerdev.class);
		goto err_del_cdev;
	}
	timerdev.device = device_create(timerdev.class, NULL, timerdev.devid,
					NULL, TIMER_NAME);
	if (IS_ERR(timerdev.device)) {
		ret = PTR_ERR(timerdev.device);
		goto err_destroy_class;
	}

	pr_info("%s: registered, GPIO=%d major=%u minor=%u period=%u ms\n",
		TIMER_NAME, timerdev.gpio, MAJOR(timerdev.devid),
		MINOR(timerdev.devid), timerdev.period_ms);
	return 0;

err_destroy_class:
	class_destroy(timerdev.class);
err_del_cdev:
	cdev_del(&timerdev.cdev);
err_unregister:
	unregister_chrdev_region(timerdev.devid, TIMER_COUNT);
err_free_gpio:
	gpio_free(timerdev.gpio);
err_put_node:
	of_node_put(timerdev.node);
	timerdev.node = NULL;
	return ret;
}

/**
 * timer_module_exit() - 停止定时器并释放驱动资源
 *
 * Context: 模块卸载期间在进程上下文调用，可以睡眠。
 */
static void __exit timer_module_exit(void)
{
	unsigned long flags;

	spin_lock_irqsave(&timerdev.lock, flags);
	timerdev.running = false;
	spin_unlock_irqrestore(&timerdev.lock, flags);
	del_timer_sync(&timerdev.timer);
	timer_set_led(&timerdev, false);
	device_destroy(timerdev.class, timerdev.devid);
	class_destroy(timerdev.class);
	cdev_del(&timerdev.cdev);
	unregister_chrdev_region(timerdev.devid, TIMER_COUNT);
	gpio_free(timerdev.gpio);
	of_node_put(timerdev.node);
	pr_info("%s: unregistered\n", TIMER_NAME);
}

module_init(timer_module_init);
module_exit(timer_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Codex");
MODULE_DESCRIPTION("GPIO LED kernel timer driver");
