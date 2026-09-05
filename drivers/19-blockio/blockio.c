// SPDX-License-Identifier: GPL-2.0
/* 使用等待队列实现阻塞读取的 GPIO 中断按键字符设备驱动。 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#define BLOCKIO_COUNT		1
#define BLOCKIO_NAME		"blockio"
#define BLOCKIO_NODE_PATH	"/key"
#define BLOCKIO_GPIO_PROPERTY	"key-gpio"
#define BLOCKIO_KEY0_VALUE	0x01
#define BLOCKIO_DEBOUNCE_MS	10U

/**
 * struct blockio_key - 阻塞按键硬件描述
 * @gpio: 按键使用的 Linux GPIO 编号
 * @irq: GPIO 对应的 Linux IRQ
 * @value: 按键对用户态报告的键值
 * @active_low: 按键是否为低电平有效
 * @name: GPIO 和 IRQ consumer 名称
 */
struct blockio_key {
	int gpio;
	int irq;
	u8 value;
	bool active_low;
	const char *name;
};

/**
 * struct blockio_device - 支持阻塞读取的 GPIO 中断按键设备
 * @devid: 动态分配的字符设备号
 * @cdev: 字符设备对象
 * @class: sysfs 设备类
 * @device: sysfs 设备对象
 * @node: key 设备树节点
 * @key: KEY0 的 GPIO、中断和键值描述
 * @timer: 过滤机械抖动的内核定时器
 * @lock: 保护按键状态和单事件槽的自旋锁
 * @read_wait: 没有按键事件时供读取进程睡眠的等待队列
 * @pressed: 消抖后是否已经确认按下
 * @event_pending: 单事件槽中是否存在尚未读取的释放事件
 * @key_value: 单事件槽中的按键值
 *
 * timer softirq 发布事件，进程上下文消费事件。@lock 保证状态转换和事件竞争
 * 的原子性；@event_pending 置位后必须唤醒 @read_wait。
 */
struct blockio_device {
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *node;
	struct blockio_key key;
	struct timer_list timer;
	spinlock_t lock;
	wait_queue_head_t read_wait;
	bool pressed;
	bool event_pending;
	u8 key_value;
};

static struct blockio_device blockio;

/**
 * blockio_irq_handler() - 延迟处理 KEY0 的 GPIO 边沿
 * @irq: KEY0 对应的中断号，本函数不直接使用
 * @data: 指向 struct blockio_device
 *
 * 每个边沿都重新安排消抖定时器，使 GPIO 在最后一个抖动边沿之后稳定
 * BLOCKIO_DEBOUNCE_MS 毫秒再被采样。
 *
 * Context: hardirq 上下文，不允许睡眠。
 * Return: 始终返回 IRQ_HANDLED。
 */
static irqreturn_t blockio_irq_handler(int irq, void *data)
{
	struct blockio_device *dev = data;

	(void)irq;
	mod_timer(&dev->timer,
		  jiffies + msecs_to_jiffies(BLOCKIO_DEBOUNCE_MS));
	return IRQ_HANDLED;
}

/**
 * blockio_timer_callback() - 确认按键状态并唤醒阻塞读取
 * @data: 指向 struct blockio_device 的无符号长整型参数
 *
 * 只有先确认稳定按下，再确认稳定释放，才发布 BLOCKIO_KEY0_VALUE。单独出现
 * 的释放电平不会被报告为有效事件。
 *
 * Context: timer softirq 上下文，不允许睡眠。
 */
static void blockio_timer_callback(unsigned long data)
{
	struct blockio_device *dev = (struct blockio_device *)data;
	unsigned long flags;
	bool pressed;
	bool overwrite = false;
	bool wake = false;

	pressed = !!gpio_get_value(dev->key.gpio);
	if (dev->key.active_low)
		pressed = !pressed;

	/*
	 * 在同一临界区完成状态转换和事件发布，保证等待条件为 true 时
	 * key_value 已经写入，读取进程不会观察到半更新事件。
	 */
	spin_lock_irqsave(&dev->lock, flags);
	if (pressed) {
		dev->pressed = true;
	} else if (dev->pressed) {
		dev->pressed = false;
		overwrite = dev->event_pending;
		dev->key_value = dev->key.value;
		dev->event_pending = true;
		wake = true;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	if (pressed) {
		pr_info("%s: KEY0 press confirmed after debounce\n",
			BLOCKIO_NAME);
	} else if (wake) {
		if (overwrite)
			pr_warn("%s: unread key event overwritten\n",
				BLOCKIO_NAME);
		pr_info("%s: KEY0 release confirmed, waking readers\n",
			BLOCKIO_NAME);
		/*
		 * 先发布事件再唤醒，确保从 wait_event_interruptible() 返回的
		 * 进程能够立即看到 event_pending，避免丢失唤醒。
		 */
		wake_up_interruptible(&dev->read_wait);
	}
}

/**
 * blockio_open() - 打开阻塞按键字符设备
 * @inode: 字符设备 inode
 * @filp: 打开的文件对象
 *
 * Context: 仅允许进程上下文调用。
 * Return: 始终返回 0。
 */
static int blockio_open(struct inode *inode, struct file *filp)
{
	struct blockio_device *dev;

	dev = container_of(inode->i_cdev, struct blockio_device, cdev);
	filp->private_data = dev;
	pr_info("%s: device opened\n", BLOCKIO_NAME);
	return 0;
}

/**
 * blockio_read() - 阻塞读取一个完整的 KEY0 按下和释放事件
 * @filp: 打开的文件对象
 * @buf: 用户态接收缓冲区
 * @count: 用户态缓冲区长度
 * @offp: 文件偏移，本驱动不使用
 *
 * 无事件时使用 wait_event_interruptible() 进入可中断睡眠。唤醒后仍在锁内
 * 重新竞争事件，保证多个读取者中只有一个获得当前按键值。
 *
 * Context: 仅允许进程上下文调用，可以睡眠。
 * Return: 成功返回 1，缓冲区过小返回 -EINVAL，复制失败返回 -EFAULT，等待被
 * 信号中断返回 -ERESTARTSYS。
 */
static ssize_t blockio_read(struct file *filp, char __user *buf,
			    size_t count, loff_t *offp)
{
	struct blockio_device *dev = filp->private_data;
	unsigned long flags;
	u8 value;
	int ret;

	(void)offp;
	if (count < sizeof(value)) {
		pr_warn("%s: read buffer too small: %zu\n", BLOCKIO_NAME,
			count);
		return -EINVAL;
	}

	pr_info("%s: waiting for a key event\n", BLOCKIO_NAME);
	for (;;) {
		/*
		 * wait_event_interruptible() 在检查条件、入队和设置任务状态时
		 * 处理必要的竞态，比手工 add_wait_queue()+schedule() 更安全。
		 */
		ret = wait_event_interruptible(dev->read_wait,
					       READ_ONCE(dev->event_pending));
		if (ret) {
			pr_warn("%s: read wait interrupted: %d\n",
				BLOCKIO_NAME, ret);
			return -ERESTARTSYS;
		}

		/*
		 * 多个等待者可能同时被唤醒，必须持锁重新检查并清除事件标志；
		 * 竞争失败的进程重新进入等待，不能重复返回同一个事件。
		 */
		spin_lock_irqsave(&dev->lock, flags);
		if (dev->event_pending) {
			dev->event_pending = false;
			value = dev->key_value;
			spin_unlock_irqrestore(&dev->lock, flags);
			break;
		}
		spin_unlock_irqrestore(&dev->lock, flags);
	}

	if (copy_to_user(buf, &value, sizeof(value))) {
		pr_err("%s: failed to copy key event to user space\n",
		       BLOCKIO_NAME);
		return -EFAULT;
	}
	pr_info("%s: key event delivered, value=0x%02x\n",
		BLOCKIO_NAME, value);
	return sizeof(value);
}

static const struct file_operations blockio_fops = {
	.owner = THIS_MODULE,
	.open = blockio_open,
	.read = blockio_read,
	.llseek = no_llseek,
};

/**
 * blockio_module_init() - 初始化阻塞按键字符设备
 *
 * 从 /key 解析 GPIO 和 IRQ，初始化等待队列及消抖定时器，申请双边沿中断，
 * 最后创建 /dev/blockio。定时器必须在 request_irq() 前初始化。
 *
 * Context: 模块加载期间在进程上下文调用，可以睡眠。
 * Return: 成功返回 0，失败返回对应的负错误码。
 */
static int __init blockio_module_init(void)
{
	enum of_gpio_flags gpio_flags;
	unsigned long irq_flags;
	int ret;

	spin_lock_init(&blockio.lock);
	init_waitqueue_head(&blockio.read_wait);
	blockio.key.value = BLOCKIO_KEY0_VALUE;
	blockio.key.name = "KEY0";

	blockio.node = of_find_node_by_path(BLOCKIO_NODE_PATH);
	if (!blockio.node) {
		pr_err("%s: device tree node %s not found\n", BLOCKIO_NAME,
		       BLOCKIO_NODE_PATH);
		return -ENODEV;
	}
	if (!of_device_is_compatible(blockio.node, "atkalpha-key") ||
	    !of_device_is_available(blockio.node)) {
		pr_err("%s: device tree node is unavailable or incompatible\n",
		       BLOCKIO_NAME);
		ret = -ENODEV;
		goto err_put_node;
	}

	blockio.key.gpio = of_get_named_gpio_flags(blockio.node,
						  BLOCKIO_GPIO_PROPERTY, 0,
						  &gpio_flags);
	if (!gpio_is_valid(blockio.key.gpio)) {
		ret = blockio.key.gpio < 0 ? blockio.key.gpio : -EINVAL;
		pr_err("%s: failed to get %s: %d\n", BLOCKIO_NAME,
		       BLOCKIO_GPIO_PROPERTY, ret);
		goto err_put_node;
	}
	blockio.key.active_low = gpio_flags & OF_GPIO_ACTIVE_LOW;
	ret = gpio_request(blockio.key.gpio, blockio.key.name);
	if (ret) {
		pr_err("%s: failed to request GPIO %d: %d\n", BLOCKIO_NAME,
		       blockio.key.gpio, ret);
		goto err_put_node;
	}
	ret = gpio_direction_input(blockio.key.gpio);
	if (ret) {
		pr_err("%s: failed to configure GPIO %d as input: %d\n",
		       BLOCKIO_NAME, blockio.key.gpio, ret);
		goto err_free_gpio;
	}

	blockio.key.irq = irq_of_parse_and_map(blockio.node, 0);
	if (!blockio.key.irq) {
		pr_err("%s: failed to map KEY0 interrupt\n", BLOCKIO_NAME);
		ret = -EINVAL;
		goto err_free_gpio;
	}
	setup_timer(&blockio.timer, blockio_timer_callback,
		    (unsigned long)&blockio);
	irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;
	ret = request_irq(blockio.key.irq, blockio_irq_handler, irq_flags,
			  blockio.key.name, &blockio);
	if (ret) {
		pr_err("%s: failed to request IRQ %d: %d\n", BLOCKIO_NAME,
		       blockio.key.irq, ret);
		goto err_dispose_irq;
	}

	ret = alloc_chrdev_region(&blockio.devid, 0, BLOCKIO_COUNT,
				  BLOCKIO_NAME);
	if (ret) {
		pr_err("%s: failed to allocate device number: %d\n",
		       BLOCKIO_NAME, ret);
		goto err_free_irq;
	}
	cdev_init(&blockio.cdev, &blockio_fops);
	blockio.cdev.owner = THIS_MODULE;
	ret = cdev_add(&blockio.cdev, blockio.devid, BLOCKIO_COUNT);
	if (ret) {
		pr_err("%s: failed to add cdev: %d\n", BLOCKIO_NAME, ret);
		goto err_unregister;
	}
	blockio.class = class_create(THIS_MODULE, BLOCKIO_NAME);
	if (IS_ERR(blockio.class)) {
		ret = PTR_ERR(blockio.class);
		pr_err("%s: failed to create class: %d\n", BLOCKIO_NAME, ret);
		goto err_del_cdev;
	}
	blockio.device = device_create(blockio.class, NULL, blockio.devid,
				       NULL, BLOCKIO_NAME);
	if (IS_ERR(blockio.device)) {
		ret = PTR_ERR(blockio.device);
		pr_err("%s: failed to create device: %d\n", BLOCKIO_NAME, ret);
		goto err_destroy_class;
	}

	pr_info("%s: registered, GPIO=%d IRQ=%d major=%u minor=%u\n",
		BLOCKIO_NAME, blockio.key.gpio, blockio.key.irq,
		MAJOR(blockio.devid), MINOR(blockio.devid));
	return 0;

err_destroy_class:
	class_destroy(blockio.class);
err_del_cdev:
	cdev_del(&blockio.cdev);
err_unregister:
	unregister_chrdev_region(blockio.devid, BLOCKIO_COUNT);
err_free_irq:
	/*
	 * 先禁止新的 IRQ，再同步删除 timer，避免错误回滚期间中断重新
	 * 安排回调并在 GPIO 释放后访问失效资源。
	 */
	free_irq(blockio.key.irq, &blockio);
	del_timer_sync(&blockio.timer);
err_dispose_irq:
	irq_dispose_mapping(blockio.key.irq);
err_free_gpio:
	gpio_free(blockio.key.gpio);
err_put_node:
	of_node_put(blockio.node);
	blockio.node = NULL;
	return ret;
}

/**
 * blockio_module_exit() - 注销设备并释放阻塞按键资源
 *
 * Context: 模块卸载期间在进程上下文调用，可以睡眠。
 */
static void __exit blockio_module_exit(void)
{
	device_destroy(blockio.class, blockio.devid);
	class_destroy(blockio.class);
	cdev_del(&blockio.cdev);
	unregister_chrdev_region(blockio.devid, BLOCKIO_COUNT);

	/*
	 * 先用 free_irq() 阻止中断再次安排 timer，再同步删除 timer，
	 * 确保释放 GPIO 和设备状态之前不再存在异步访问。
	 */
	free_irq(blockio.key.irq, &blockio);
	del_timer_sync(&blockio.timer);
	irq_dispose_mapping(blockio.key.irq);
	gpio_free(blockio.key.gpio);
	of_node_put(blockio.node);
	pr_info("%s: unregistered\n", BLOCKIO_NAME);
}

module_init(blockio_module_init);
module_exit(blockio_module_exit);

MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("Blocking GPIO interrupt key driver with wait queue");
MODULE_LICENSE("GPL");
