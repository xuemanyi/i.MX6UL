// SPDX-License-Identifier: GPL-2.0
/* 使用四种 Linux 同步机制实现 LED 独占打开实验。 */

#include <linux/atomic.h>
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
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#define CONCURRENCY_LED_COUNT		1
#define CONCURRENCY_LED_NAME		"concurrency_led"
#define CONCURRENCY_LED_NODE_PATH	"/gpioled"
#define CONCURRENCY_LED_GPIO_PROPERTY	"led-gpio"
#define LED_OFF				0
#define LED_ON				1

/**
 * enum concurrency_lock_mode - 独占打开使用的同步机制
 * @LOCK_MODE_ATOMIC: 使用 atomic_cmpxchg() 原子操作
 * @LOCK_MODE_SPINLOCK: 使用 spin_lock_irqsave() 保护状态
 * @LOCK_MODE_SEMAPHORE: 使用二值信号量保护状态
 * @LOCK_MODE_MUTEX: 使用 mutex 保护状态
 */
enum concurrency_lock_mode {
	LOCK_MODE_ATOMIC,
	LOCK_MODE_SPINLOCK,
	LOCK_MODE_SEMAPHORE,
	LOCK_MODE_MUTEX,
};

/**
 * struct concurrency_led_device - 并发实验 LED 字符设备
 * @devid: 动态分配的字符设备号
 * @cdev: 字符设备对象
 * @class: sysfs 设备类
 * @device: sysfs 设备对象
 * @node: gpioled 设备树节点
 * @gpio: LED 使用的 Linux GPIO 编号
 * @active_low: LED 是否低电平有效
 * @mode: 本次加载选择的同步机制
 * @atomic_available: 原子模式可用标志，1 表示未打开
 * @spin_opened: 自旋锁模式的已打开状态
 * @spin_lock: 保护 @spin_opened 的自旋锁
 * @semaphore_opened: 信号量模式的已打开状态
 * @semaphore: 保护 @semaphore_opened 的二值信号量
 * @mutex_opened: 互斥体模式的已打开状态
 * @open_mutex: 保护 @mutex_opened 的互斥体
 * @io_mutex: 串行化共享文件描述符上的 LED 读写
 *
 * 每次加载只启用 @mode 指定的独占机制。锁只保护状态检查和更新，不跨越
 * open()/release() 长期持有，避免 mutex 被非所有者任务释放。
 */
struct concurrency_led_device {
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *node;
	int gpio;
	bool active_low;
	enum concurrency_lock_mode mode;
	atomic_t atomic_available;
	bool spin_opened;
	spinlock_t spin_lock;
	bool semaphore_opened;
	struct semaphore semaphore;
	bool mutex_opened;
	struct mutex open_mutex;
	struct mutex io_mutex;
};

static struct concurrency_led_device concurrency_led;
static char *lock_mode = "atomic";
module_param(lock_mode, charp, 0444);
MODULE_PARM_DESC(lock_mode, "Exclusive-open mode: atomic, spinlock, semaphore or mutex");

/**
 * concurrency_mode_name() - 返回同步模式名称
 * @mode: 同步模式
 *
 * Return: 对应的静态字符串。
 */
static const char *concurrency_mode_name(enum concurrency_lock_mode mode)
{
	switch (mode) {
	case LOCK_MODE_ATOMIC:
		return "atomic";
	case LOCK_MODE_SPINLOCK:
		return "spinlock";
	case LOCK_MODE_SEMAPHORE:
		return "semaphore";
	case LOCK_MODE_MUTEX:
		return "mutex";
	default:
		return "unknown";
	}
}

/**
 * concurrency_parse_mode() - 解析模块参数中的同步模式
 * @name: 模式名称
 * @mode: 返回解析后的模式
 *
 * Return: 成功返回 0，名称不支持返回 -EINVAL。
 */
static int concurrency_parse_mode(const char *name,
				  enum concurrency_lock_mode *mode)
{
	if (!strcmp(name, "atomic"))
		*mode = LOCK_MODE_ATOMIC;
	else if (!strcmp(name, "spinlock"))
		*mode = LOCK_MODE_SPINLOCK;
	else if (!strcmp(name, "semaphore"))
		*mode = LOCK_MODE_SEMAPHORE;
	else if (!strcmp(name, "mutex"))
		*mode = LOCK_MODE_MUTEX;
	else
		return -EINVAL;
	return 0;
}

/**
 * concurrency_acquire_open() - 申请设备独占打开权
 * @led: 并发实验 LED 设备
 *
 * Context: 仅允许进程上下文调用；信号量和互斥体模式可以睡眠。
 * Return: 成功返回 0，设备已打开返回 -EBUSY，被信号中断返回 -ERESTARTSYS。
 */
static int concurrency_acquire_open(struct concurrency_led_device *led)
{
	unsigned long flags;
	int ret = 0;

	switch (led->mode) {
	case LOCK_MODE_ATOMIC:
		if (atomic_cmpxchg(&led->atomic_available, 1, 0) != 1)
			ret = -EBUSY;
		break;
	case LOCK_MODE_SPINLOCK:
		spin_lock_irqsave(&led->spin_lock, flags);
		if (led->spin_opened)
			ret = -EBUSY;
		else
			led->spin_opened = true;
		spin_unlock_irqrestore(&led->spin_lock, flags);
		break;
	case LOCK_MODE_SEMAPHORE:
		if (down_interruptible(&led->semaphore))
			return -ERESTARTSYS;
		if (led->semaphore_opened)
			ret = -EBUSY;
		else
			led->semaphore_opened = true;
		up(&led->semaphore);
		break;
	case LOCK_MODE_MUTEX:
		if (mutex_lock_interruptible(&led->open_mutex))
			return -ERESTARTSYS;
		if (led->mutex_opened)
			ret = -EBUSY;
		else
			led->mutex_opened = true;
		mutex_unlock(&led->open_mutex);
		break;
	}
	return ret;
}

/**
 * concurrency_release_open() - 释放设备独占打开权
 * @led: 并发实验 LED 设备
 *
 * Context: 仅允许进程上下文调用，可以睡眠。
 */
static void concurrency_release_open(struct concurrency_led_device *led)
{
	unsigned long flags;

	switch (led->mode) {
	case LOCK_MODE_ATOMIC:
		atomic_set(&led->atomic_available, 1);
		break;
	case LOCK_MODE_SPINLOCK:
		spin_lock_irqsave(&led->spin_lock, flags);
		led->spin_opened = false;
		spin_unlock_irqrestore(&led->spin_lock, flags);
		break;
	case LOCK_MODE_SEMAPHORE:
		down(&led->semaphore);
		led->semaphore_opened = false;
		up(&led->semaphore);
		break;
	case LOCK_MODE_MUTEX:
		mutex_lock(&led->open_mutex);
		led->mutex_opened = false;
		mutex_unlock(&led->open_mutex);
		break;
	}
}

/* 调用方必须持有 io_mutex，并传入 LED_ON 或 LED_OFF。 */
static void concurrency_set_led(struct concurrency_led_device *led, u8 state)
{
	int value = state == LED_ON;

	if (led->active_low)
		value = !value;
	gpio_set_value(led->gpio, value);
}

/* 调用方必须持有 io_mutex。 */
static u8 concurrency_get_led(struct concurrency_led_device *led)
{
	int value = !!gpio_get_value(led->gpio);

	if (led->active_low)
		value = !value;
	return value ? LED_ON : LED_OFF;
}

/**
 * concurrency_led_open() - 独占打开并发实验设备
 * @inode: 字符设备 inode
 * @filp: 打开的文件对象
 *
 * Context: 仅允许进程上下文调用，可以睡眠。
 * Return: 成功返回 0，设备已打开返回 -EBUSY，等待被信号中断返回 -ERESTARTSYS。
 */
static int concurrency_led_open(struct inode *inode, struct file *filp)
{
	struct concurrency_led_device *led;
	int ret;

	led = container_of(inode->i_cdev, struct concurrency_led_device, cdev);
	ret = concurrency_acquire_open(led);
	if (ret)
		return ret;
	filp->private_data = led;
	return 0;
}

/**
 * concurrency_led_read() - 读取当前 LED 状态
 * @filp: 打开的文件对象
 * @buf: 用户态接收缓冲区
 * @count: 用户态缓冲区长度
 * @offp: 文件偏移
 *
 * Context: 仅允许进程上下文调用，可以睡眠。
 * Return: 成功返回 1，已读完返回 0，复制失败返回 -EFAULT。
 */
static ssize_t concurrency_led_read(struct file *filp, char __user *buf,
				    size_t count, loff_t *offp)
{
	struct concurrency_led_device *led = filp->private_data;
	u8 state;

	if (!count || *offp)
		return 0;
	mutex_lock(&led->io_mutex);
	state = concurrency_get_led(led);
	mutex_unlock(&led->io_mutex);
	if (copy_to_user(buf, &state, sizeof(state)))
		return -EFAULT;
	*offp += sizeof(state);
	return sizeof(state);
}

/**
 * concurrency_led_write() - 设置 LED 状态
 * @filp: 打开的文件对象
 * @buf: 保存状态的用户态缓冲区
 * @count: 用户态缓冲区长度
 * @offp: 文件偏移，本驱动不使用
 *
 * Context: 仅允许进程上下文调用，可以睡眠。
 * Return: 成功返回 1，参数非法返回 -EINVAL，复制失败返回 -EFAULT。
 */
static ssize_t concurrency_led_write(struct file *filp,
				     const char __user *buf, size_t count,
				     loff_t *offp)
{
	struct concurrency_led_device *led = filp->private_data;
	u8 state;

	(void)offp;
	if (count < sizeof(state))
		return -EINVAL;
	if (copy_from_user(&state, buf, sizeof(state)))
		return -EFAULT;
	if (state != LED_ON && state != LED_OFF)
		return -EINVAL;
	mutex_lock(&led->io_mutex);
	concurrency_set_led(led, state);
	mutex_unlock(&led->io_mutex);
	return sizeof(state);
}

/**
 * concurrency_led_release() - 关闭设备并释放独占打开权
 * @inode: 字符设备 inode，本函数不使用
 * @filp: 打开的文件对象
 *
 * Context: 仅允许进程上下文调用，可以睡眠。
 * Return: 始终返回 0。
 */
static int concurrency_led_release(struct inode *inode, struct file *filp)
{
	struct concurrency_led_device *led = filp->private_data;

	(void)inode;
	concurrency_release_open(led);
	return 0;
}

static const struct file_operations concurrency_led_fops = {
	.owner = THIS_MODULE,
	.open = concurrency_led_open,
	.read = concurrency_led_read,
	.write = concurrency_led_write,
	.release = concurrency_led_release,
	.llseek = no_llseek,
};

/**
 * concurrency_led_init() - 初始化 LED 并发实验驱动
 *
 * 解析同步模式和 /gpioled 资源，申请 GPIO 并注册 /dev/concurrency_led。
 *
 * Context: 模块加载期间在进程上下文调用，可以睡眠。
 * Return: 成功返回 0，失败返回对应的负错误码。
 */
static int __init concurrency_led_init(void)
{
	enum of_gpio_flags flags;
	int inactive_value;
	int ret;

	ret = concurrency_parse_mode(lock_mode, &concurrency_led.mode);
	if (ret) {
		pr_err("%s: invalid lock_mode '%s'\n",
		       CONCURRENCY_LED_NAME, lock_mode);
		return ret;
	}

	atomic_set(&concurrency_led.atomic_available, 1);
	spin_lock_init(&concurrency_led.spin_lock);
	sema_init(&concurrency_led.semaphore, 1);
	mutex_init(&concurrency_led.open_mutex);
	mutex_init(&concurrency_led.io_mutex);

	concurrency_led.node = of_find_node_by_path(CONCURRENCY_LED_NODE_PATH);
	if (!concurrency_led.node) {
		pr_err("%s: device tree node %s not found\n",
		       CONCURRENCY_LED_NAME, CONCURRENCY_LED_NODE_PATH);
		return -ENODEV;
	}
	if (!of_device_is_available(concurrency_led.node)) {
		pr_err("%s: device tree node is unavailable\n",
		       CONCURRENCY_LED_NAME);
		ret = -ENODEV;
		goto err_put_node;
	}

	concurrency_led.gpio = of_get_named_gpio_flags(concurrency_led.node,
					CONCURRENCY_LED_GPIO_PROPERTY, 0,
					&flags);
	if (!gpio_is_valid(concurrency_led.gpio)) {
		ret = concurrency_led.gpio < 0 ? concurrency_led.gpio : -EINVAL;
		pr_err("%s: failed to get %s: %d\n", CONCURRENCY_LED_NAME,
		       CONCURRENCY_LED_GPIO_PROPERTY, ret);
		goto err_put_node;
	}
	concurrency_led.active_low = flags & OF_GPIO_ACTIVE_LOW;
	ret = gpio_request(concurrency_led.gpio, CONCURRENCY_LED_NAME);
	if (ret) {
		pr_err("%s: failed to request GPIO %d: %d\n",
		       CONCURRENCY_LED_NAME, concurrency_led.gpio, ret);
		goto err_put_node;
	}
	inactive_value = concurrency_led.active_low ? 1 : 0;
	ret = gpio_direction_output(concurrency_led.gpio, inactive_value);
	if (ret) {
		pr_err("%s: failed to configure GPIO %d: %d\n",
		       CONCURRENCY_LED_NAME, concurrency_led.gpio, ret);
		goto err_free_gpio;
	}

	ret = alloc_chrdev_region(&concurrency_led.devid, 0,
				  CONCURRENCY_LED_COUNT,
				  CONCURRENCY_LED_NAME);
	if (ret)
		goto err_free_gpio;
	cdev_init(&concurrency_led.cdev, &concurrency_led_fops);
	concurrency_led.cdev.owner = THIS_MODULE;
	ret = cdev_add(&concurrency_led.cdev, concurrency_led.devid,
		       CONCURRENCY_LED_COUNT);
	if (ret)
		goto err_unregister;
	concurrency_led.class = class_create(THIS_MODULE,
					     CONCURRENCY_LED_NAME);
	if (IS_ERR(concurrency_led.class)) {
		ret = PTR_ERR(concurrency_led.class);
		goto err_del_cdev;
	}
	concurrency_led.device = device_create(concurrency_led.class, NULL,
					       concurrency_led.devid, NULL,
					       CONCURRENCY_LED_NAME);
	if (IS_ERR(concurrency_led.device)) {
		ret = PTR_ERR(concurrency_led.device);
		goto err_destroy_class;
	}

	pr_info("%s: registered with %s mode, major=%u minor=%u\n",
		CONCURRENCY_LED_NAME, concurrency_mode_name(concurrency_led.mode),
		MAJOR(concurrency_led.devid), MINOR(concurrency_led.devid));
	return 0;

err_destroy_class:
	class_destroy(concurrency_led.class);
err_del_cdev:
	cdev_del(&concurrency_led.cdev);
err_unregister:
	unregister_chrdev_region(concurrency_led.devid,
				 CONCURRENCY_LED_COUNT);
err_free_gpio:
	gpio_free(concurrency_led.gpio);
err_put_node:
	of_node_put(concurrency_led.node);
	concurrency_led.node = NULL;
	return ret;
}

/**
 * concurrency_led_exit() - 注销 LED 并发实验驱动
 *
 * Context: 模块卸载期间在进程上下文调用，可以睡眠。
 */
static void __exit concurrency_led_exit(void)
{
	device_destroy(concurrency_led.class, concurrency_led.devid);
	class_destroy(concurrency_led.class);
	cdev_del(&concurrency_led.cdev);
	unregister_chrdev_region(concurrency_led.devid,
				 CONCURRENCY_LED_COUNT);
	mutex_lock(&concurrency_led.io_mutex);
	concurrency_set_led(&concurrency_led, LED_OFF);
	mutex_unlock(&concurrency_led.io_mutex);
	gpio_free(concurrency_led.gpio);
	of_node_put(concurrency_led.node);
	pr_info("%s: unregistered\n", CONCURRENCY_LED_NAME);
}

module_init(concurrency_led_init);
module_exit(concurrency_led_exit);

MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("Atomic, spinlock, semaphore and mutex exclusive-open lab");
MODULE_LICENSE("GPL");
