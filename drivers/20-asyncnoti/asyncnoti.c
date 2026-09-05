// SPDX-License-Identifier: GPL-2.0
/* 支持 SIGIO 异步通知的 GPIO 中断按键字符设备驱动。 */

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
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#define ASYNCNOTI_COUNT			1
#define ASYNCNOTI_NAME			"asyncnoti"
#define ASYNCNOTI_NODE_PATH		"/key"
#define ASYNCNOTI_GPIO_PROPERTY		"key-gpio"
#define ASYNCNOTI_KEY0_VALUE		0x01
#define ASYNCNOTI_DEBOUNCE_MS		10U

/**
 * struct asyncnoti_key - 异步通知按键硬件描述
 * @gpio: 按键使用的 Linux GPIO 编号
 * @irq: GPIO 对应的 Linux IRQ
 * @value: 按键对用户态报告的键值
 * @active_low: 按键是否为低电平有效
 * @name: GPIO 和 IRQ consumer 名称
 */
struct asyncnoti_key {
	int gpio;
	int irq;
	u8 value;
	bool active_low;
	const char *name;
};

/**
 * struct asyncnoti_device - 支持 SIGIO 的 GPIO 中断按键设备
 * @devid: 动态分配的字符设备号
 * @cdev: 字符设备对象
 * @class: sysfs 设备类
 * @device: sysfs 设备对象
 * @node: key 设备树节点
 * @key: KEY0 的 GPIO、中断和键值描述
 * @timer: 过滤机械抖动的内核定时器
 * @lock: 保护按键状态和单事件槽的自旋锁
 * @read_wait: read、poll 和 select 共用的等待队列
 * @async_queue: 通过 fasync_helper 管理的异步通知订阅链表
 * @pressed: 消抖后是否已经确认按下
 * @event_pending: 单事件槽中是否存在尚未读取的释放事件
 * @key_value: 单事件槽中的按键值
 *
 * timer softirq 发布事件后，同时唤醒 @read_wait 并通过 @async_queue 发送 SIGIO。
 * @async_queue 只允许通过 fasync_helper() 更新，通过 kill_fasync() 遍历通知。
 */
struct asyncnoti_device {
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *node;
	struct asyncnoti_key key;
	struct timer_list timer;
	spinlock_t lock;
	wait_queue_head_t read_wait;
	struct fasync_struct *async_queue;
	bool pressed;
	bool event_pending;
	u8 key_value;
};

static struct asyncnoti_device asyncnoti;

/**
 * asyncnoti_irq_handler() - 延迟处理 KEY0 的 GPIO 边沿
 * @irq: KEY0 对应的中断号，本函数不直接使用
 * @data: 指向 struct asyncnoti_device
 *
 * 每个边沿都重新安排定时器，只在最后一次抖动边沿之后稳定
 * ASYNCNOTI_DEBOUNCE_MS 毫秒时采样 GPIO。
 *
 * Context: hardirq 上下文，不允许睡眠。
 * Return: 始终返回 IRQ_HANDLED。
 */
static irqreturn_t asyncnoti_irq_handler(int irq, void *data)
{
	struct asyncnoti_device *dev = data;

	(void)irq;
	mod_timer(&dev->timer,
		  jiffies + msecs_to_jiffies(ASYNCNOTI_DEBOUNCE_MS));
	return IRQ_HANDLED;
}

/**
 * asyncnoti_timer_callback() - 发布按键事件并发送 SIGIO
 * @data: 指向 struct asyncnoti_device 的无符号长整型参数
 *
 * 稳定按下只记录状态；之后确认稳定释放时写入 ASYNCNOTI_KEY0_VALUE，唤醒
 * 等待队列，并向已经启用 FASYNC 的文件发送 SIGIO/POLL_IN。
 *
 * Context: timer softirq 上下文，不允许睡眠。
 */
static void asyncnoti_timer_callback(unsigned long data)
{
	struct asyncnoti_device *dev = (struct asyncnoti_device *)data;
	unsigned long flags;
	bool pressed;
	bool overwrite = false;
	bool notify = false;

	pressed = !!gpio_get_value(dev->key.gpio);
	if (dev->key.active_low)
		pressed = !pressed;

	/*
	 * 键值和就绪标志必须在同一临界区发布，确保收到 SIGIO 后执行
	 * read() 的进程不会观察到半更新事件。
	 */
	spin_lock_irqsave(&dev->lock, flags);
	if (pressed) {
		dev->pressed = true;
	} else if (dev->pressed) {
		dev->pressed = false;
		overwrite = dev->event_pending;
		dev->key_value = dev->key.value;
		dev->event_pending = true;
		notify = true;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	if (pressed) {
		pr_info("%s: KEY0 press confirmed after debounce\n",
			ASYNCNOTI_NAME);
	} else if (notify) {
		if (overwrite)
			pr_warn("%s: unread key event overwritten\n",
				ASYNCNOTI_NAME);
		pr_info("%s: KEY0 release confirmed, notifying readers\n",
			ASYNCNOTI_NAME);
		/*
		 * 事件对外可见后再唤醒并发出 SIGIO。等待队列服务阻塞读取和
		 * poll/select，fasync 链表服务设置了 FASYNC 的进程。
		 */
		wake_up_interruptible(&dev->read_wait);
		kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
	}
}

/**
 * asyncnoti_open() - 打开异步通知按键字符设备
 * @inode: 字符设备 inode
 * @filp: 打开的文件对象
 *
 * Context: 仅允许进程上下文调用。
 * Return: 始终返回 0。
 */
static int asyncnoti_open(struct inode *inode, struct file *filp)
{
	struct asyncnoti_device *dev;

	dev = container_of(inode->i_cdev, struct asyncnoti_device, cdev);
	filp->private_data = dev;
	pr_info("%s: device opened, nonblock=%u\n", ASYNCNOTI_NAME,
		!!(filp->f_flags & O_NONBLOCK));
	return 0;
}

/**
 * asyncnoti_read() - 阻塞或非阻塞读取一个 KEY0 释放事件
 * @filp: 打开的文件对象
 * @buf: 用户态接收缓冲区
 * @count: 用户态缓冲区长度
 * @offp: 文件偏移，本驱动不使用
 *
 * O_NONBLOCK 文件在无事件时返回 -EAGAIN；普通文件进入可中断睡眠。SIGIO
 * 只表示状态可能就绪，实际事件仍由 read() 持锁竞争并消费。
 *
 * Context: 仅允许进程上下文调用，可以睡眠。
 * Return: 成功返回 1，缓冲区过小返回 -EINVAL，复制失败返回 -EFAULT，等待被
 * 信号中断返回 -ERESTARTSYS，非阻塞且无事件返回 -EAGAIN。
 */
static ssize_t asyncnoti_read(struct file *filp, char __user *buf,
			      size_t count, loff_t *offp)
{
	struct asyncnoti_device *dev = filp->private_data;
	unsigned long flags;
	u8 value;
	int ret;

	(void)offp;
	if (count < sizeof(value)) {
		pr_warn("%s: read buffer too small: %zu\n", ASYNCNOTI_NAME,
			count);
		return -EINVAL;
	}

	for (;;) {
		if (filp->f_flags & O_NONBLOCK) {
			if (!READ_ONCE(dev->event_pending)) {
				pr_debug("%s: no key event available\n",
					 ASYNCNOTI_NAME);
				return -EAGAIN;
			}
		} else {
			ret = wait_event_interruptible(dev->read_wait,
						       READ_ONCE(dev->event_pending));
			if (ret) {
				pr_warn("%s: read wait interrupted: %d\n",
					ASYNCNOTI_NAME, ret);
				return -ERESTARTSYS;
			}
		}

		/*
		 * 异步信号和 poll 就绪都是瞬时通知，不能代替这里的持锁复查；
		 * 多个订阅者可能同时被通知，但一个事件只能由一个 read 消费。
		 */
		spin_lock_irqsave(&dev->lock, flags);
		if (dev->event_pending) {
			dev->event_pending = false;
			value = dev->key_value;
			spin_unlock_irqrestore(&dev->lock, flags);
			break;
		}
		spin_unlock_irqrestore(&dev->lock, flags);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
	}

	if (copy_to_user(buf, &value, sizeof(value))) {
		pr_err("%s: failed to copy key event to user space\n",
		       ASYNCNOTI_NAME);
		return -EFAULT;
	}
	pr_info("%s: key event delivered, value=0x%02x\n",
		ASYNCNOTI_NAME, value);
	return sizeof(value);
}

/**
 * asyncnoti_poll() - 查询按键事件是否可读
 * @filp: 打开的文件对象
 * @wait: VFS 提供的 poll table
 *
 * Context: 仅允许进程上下文调用。
 * Return: 有事件返回 POLLIN | POLLRDNORM，否则返回 0。
 */
static unsigned int asyncnoti_poll(struct file *filp, poll_table *wait)
{
	struct asyncnoti_device *dev = filp->private_data;
	unsigned long flags;
	unsigned int mask = 0;

	/*
	 * 必须先登记等待队列再检查状态，避免事件在检查后、登记前到达
	 * 而造成 poll/select 丢失本次唤醒。
	 */
	poll_wait(filp, &dev->read_wait, wait);
	spin_lock_irqsave(&dev->lock, flags);
	if (dev->event_pending)
		mask = POLLIN | POLLRDNORM;
	spin_unlock_irqrestore(&dev->lock, flags);
	return mask;
}

/**
 * asyncnoti_fasync() - 更新文件的 SIGIO 异步通知订阅状态
 * @fd: 用户态文件描述符
 * @filp: 打开的文件对象
 * @on: 非零表示加入异步通知链表，零表示移除
 *
 * fasync_helper() 管理 struct fasync_struct 的分配、更新和释放，调用方不得
 * 直接修改 async_queue 链表。
 *
 * Context: 仅允许进程上下文调用，可以睡眠。
 * Return: 成功返回非负值，失败返回 fasync_helper() 的负错误码。
 */
static int asyncnoti_fasync(int fd, struct file *filp, int on)
{
	struct asyncnoti_device *dev = filp->private_data;
	int ret;

	ret = fasync_helper(fd, filp, on, &dev->async_queue);
	if (ret < 0) {
		pr_err("%s: failed to update async notification: %d\n",
		       ASYNCNOTI_NAME, ret);
	} else {
		pr_info("%s: async notification %s\n", ASYNCNOTI_NAME,
			on ? "enabled" : "disabled");
	}
	return ret;
}

/**
 * asyncnoti_release() - 关闭文件并移除异步通知订阅
 * @inode: 字符设备 inode，本函数不直接使用
 * @filp: 即将关闭的文件对象
 *
 * 必须在文件生命周期结束前从 fasync 链表移除 filp，避免后续 kill_fasync()
 * 访问已经释放的文件对象。
 *
 * Context: 仅允许进程上下文调用，可以睡眠。
 * Return: 成功返回非负值，失败返回 fasync_helper() 的负错误码。
 */
static int asyncnoti_release(struct inode *inode, struct file *filp)
{
	int ret;

	(void)inode;
	ret = asyncnoti_fasync(-1, filp, 0);
	if (ret >= 0)
		pr_info("%s: device closed\n", ASYNCNOTI_NAME);
	return ret;
}

static const struct file_operations asyncnoti_fops = {
	.owner = THIS_MODULE,
	.open = asyncnoti_open,
	.read = asyncnoti_read,
	.poll = asyncnoti_poll,
	.fasync = asyncnoti_fasync,
	.release = asyncnoti_release,
	.llseek = no_llseek,
};

/**
 * asyncnoti_module_init() - 初始化异步通知按键字符设备
 *
 * 从 /key 解析 GPIO 和 IRQ，初始化等待队列及消抖定时器，申请双边沿中断，
 * 最后创建 /dev/asyncnoti。
 *
 * Context: 模块加载期间在进程上下文调用，可以睡眠。
 * Return: 成功返回 0，失败返回对应的负错误码。
 */
static int __init asyncnoti_module_init(void)
{
	enum of_gpio_flags gpio_flags;
	unsigned long irq_flags;
	int ret;

	spin_lock_init(&asyncnoti.lock);
	init_waitqueue_head(&asyncnoti.read_wait);
	asyncnoti.key.value = ASYNCNOTI_KEY0_VALUE;
	asyncnoti.key.name = "KEY0";

	asyncnoti.node = of_find_node_by_path(ASYNCNOTI_NODE_PATH);
	if (!asyncnoti.node) {
		pr_err("%s: device tree node %s not found\n", ASYNCNOTI_NAME,
		       ASYNCNOTI_NODE_PATH);
		return -ENODEV;
	}
	if (!of_device_is_compatible(asyncnoti.node, "atkalpha-key") ||
	    !of_device_is_available(asyncnoti.node)) {
		pr_err("%s: device tree node is unavailable or incompatible\n",
		       ASYNCNOTI_NAME);
		ret = -ENODEV;
		goto err_put_node;
	}

	asyncnoti.key.gpio = of_get_named_gpio_flags(asyncnoti.node,
						    ASYNCNOTI_GPIO_PROPERTY,
						    0, &gpio_flags);
	if (!gpio_is_valid(asyncnoti.key.gpio)) {
		ret = asyncnoti.key.gpio < 0 ? asyncnoti.key.gpio : -EINVAL;
		pr_err("%s: failed to get %s: %d\n", ASYNCNOTI_NAME,
		       ASYNCNOTI_GPIO_PROPERTY, ret);
		goto err_put_node;
	}
	asyncnoti.key.active_low = gpio_flags & OF_GPIO_ACTIVE_LOW;
	ret = gpio_request(asyncnoti.key.gpio, asyncnoti.key.name);
	if (ret) {
		pr_err("%s: failed to request GPIO %d: %d\n", ASYNCNOTI_NAME,
		       asyncnoti.key.gpio, ret);
		goto err_put_node;
	}
	ret = gpio_direction_input(asyncnoti.key.gpio);
	if (ret) {
		pr_err("%s: failed to configure GPIO %d as input: %d\n",
		       ASYNCNOTI_NAME, asyncnoti.key.gpio, ret);
		goto err_free_gpio;
	}

	asyncnoti.key.irq = irq_of_parse_and_map(asyncnoti.node, 0);
	if (!asyncnoti.key.irq) {
		pr_err("%s: failed to map KEY0 interrupt\n", ASYNCNOTI_NAME);
		ret = -EINVAL;
		goto err_free_gpio;
	}
	setup_timer(&asyncnoti.timer, asyncnoti_timer_callback,
		    (unsigned long)&asyncnoti);
	irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;
	ret = request_irq(asyncnoti.key.irq, asyncnoti_irq_handler, irq_flags,
			  asyncnoti.key.name, &asyncnoti);
	if (ret) {
		pr_err("%s: failed to request IRQ %d: %d\n", ASYNCNOTI_NAME,
		       asyncnoti.key.irq, ret);
		goto err_dispose_irq;
	}

	ret = alloc_chrdev_region(&asyncnoti.devid, 0, ASYNCNOTI_COUNT,
				  ASYNCNOTI_NAME);
	if (ret) {
		pr_err("%s: failed to allocate device number: %d\n",
		       ASYNCNOTI_NAME, ret);
		goto err_free_irq;
	}
	cdev_init(&asyncnoti.cdev, &asyncnoti_fops);
	asyncnoti.cdev.owner = THIS_MODULE;
	ret = cdev_add(&asyncnoti.cdev, asyncnoti.devid, ASYNCNOTI_COUNT);
	if (ret) {
		pr_err("%s: failed to add cdev: %d\n", ASYNCNOTI_NAME, ret);
		goto err_unregister;
	}
	asyncnoti.class = class_create(THIS_MODULE, ASYNCNOTI_NAME);
	if (IS_ERR(asyncnoti.class)) {
		ret = PTR_ERR(asyncnoti.class);
		pr_err("%s: failed to create class: %d\n", ASYNCNOTI_NAME, ret);
		goto err_del_cdev;
	}
	asyncnoti.device = device_create(asyncnoti.class, NULL,
					 asyncnoti.devid, NULL, ASYNCNOTI_NAME);
	if (IS_ERR(asyncnoti.device)) {
		ret = PTR_ERR(asyncnoti.device);
		pr_err("%s: failed to create device: %d\n", ASYNCNOTI_NAME,
		       ret);
		goto err_destroy_class;
	}

	pr_info("%s: registered, GPIO=%d IRQ=%d major=%u minor=%u\n",
		ASYNCNOTI_NAME, asyncnoti.key.gpio, asyncnoti.key.irq,
		MAJOR(asyncnoti.devid), MINOR(asyncnoti.devid));
	return 0;

err_destroy_class:
	class_destroy(asyncnoti.class);
err_del_cdev:
	cdev_del(&asyncnoti.cdev);
err_unregister:
	unregister_chrdev_region(asyncnoti.devid, ASYNCNOTI_COUNT);
err_free_irq:
	/*
	 * 先阻止新 IRQ，再同步删除 timer，避免清理期间重新安排回调并在
	 * GPIO 释放后访问失效资源。
	 */
	free_irq(asyncnoti.key.irq, &asyncnoti);
	del_timer_sync(&asyncnoti.timer);
err_dispose_irq:
	irq_dispose_mapping(asyncnoti.key.irq);
err_free_gpio:
	gpio_free(asyncnoti.key.gpio);
err_put_node:
	of_node_put(asyncnoti.node);
	asyncnoti.node = NULL;
	return ret;
}

/**
 * asyncnoti_module_exit() - 注销设备并释放异步通知按键资源
 *
 * Context: 模块卸载期间在进程上下文调用，可以睡眠。
 */
static void __exit asyncnoti_module_exit(void)
{
	device_destroy(asyncnoti.class, asyncnoti.devid);
	class_destroy(asyncnoti.class);
	cdev_del(&asyncnoti.cdev);
	unregister_chrdev_region(asyncnoti.devid, ASYNCNOTI_COUNT);

	/*
	 * 文件打开期间 .owner 阻止模块卸载；先释放 IRQ 再同步删除 timer，
	 * 保证后续释放 GPIO 时没有异步回调仍在运行。
	 */
	free_irq(asyncnoti.key.irq, &asyncnoti);
	del_timer_sync(&asyncnoti.timer);
	irq_dispose_mapping(asyncnoti.key.irq);
	gpio_free(asyncnoti.key.gpio);
	of_node_put(asyncnoti.node);
	pr_info("%s: unregistered\n", ASYNCNOTI_NAME);
}

module_init(asyncnoti_module_init);
module_exit(asyncnoti_module_exit);

MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("GPIO key driver with SIGIO asynchronous notification");
MODULE_LICENSE("GPL");
