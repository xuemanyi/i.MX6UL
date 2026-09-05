// SPDX-License-Identifier: GPL-2.0
/* 支持阻塞、非阻塞和 poll/select 的 GPIO 中断按键字符设备驱动。 */

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

#define NOBLOCKIO_COUNT			1
#define NOBLOCKIO_NAME			"noblockio"
#define NOBLOCKIO_NODE_PATH		"/key"
#define NOBLOCKIO_GPIO_PROPERTY		"key-gpio"
#define NOBLOCKIO_KEY0_VALUE		0x01
#define NOBLOCKIO_DEBOUNCE_MS		10U

/**
 * struct noblockio_key - 非阻塞按键硬件描述
 * @gpio: 按键使用的 Linux GPIO 编号
 * @irq: GPIO 对应的 Linux IRQ
 * @value: 按键对用户态报告的键值
 * @active_low: 按键是否为低电平有效
 * @name: GPIO 和 IRQ consumer 名称
 */
struct noblockio_key {
	int gpio;
	int irq;
	u8 value;
	bool active_low;
	const char *name;
};

/**
 * struct noblockio_device - 支持非阻塞访问的 GPIO 中断按键设备
 * @devid: 动态分配的字符设备号
 * @cdev: 字符设备对象
 * @class: sysfs 设备类
 * @device: sysfs 设备对象
 * @node: key 设备树节点
 * @key: KEY0 的 GPIO、中断和键值描述
 * @timer: 过滤机械抖动的内核定时器
 * @lock: 保护按键状态和单事件槽的自旋锁
 * @read_wait: read、poll 和 select 共用的等待队列
 * @pressed: 消抖后是否已经确认按下
 * @event_pending: 单事件槽中是否存在尚未读取的释放事件
 * @key_value: 单事件槽中的按键值
 *
 * @lock 可在 timer softirq 和进程上下文获取，持锁期间不得睡眠。
 * @event_pending 从 false 变为 true 后必须唤醒 @read_wait，使阻塞 read 和
 * poll/select 同时重新检查就绪条件。
 */
struct noblockio_device {
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *node;
	struct noblockio_key key;
	struct timer_list timer;
	spinlock_t lock;
	wait_queue_head_t read_wait;
	bool pressed;
	bool event_pending;
	u8 key_value;
};

static struct noblockio_device noblockio;

/**
 * noblockio_irq_handler() - 延迟处理 KEY0 的 GPIO 边沿
 * @irq: KEY0 对应的中断号，本函数不直接使用
 * @data: 指向 struct noblockio_device
 *
 * 每个边沿都将消抖超时移动到当前时刻之后 10 ms，使定时器只在最后一次
 * 抖动边沿之后采样 GPIO。
 *
 * Context: hardirq 上下文，不允许睡眠。
 * Return: 始终返回 IRQ_HANDLED。
 */
static irqreturn_t noblockio_irq_handler(int irq, void *data)
{
	struct noblockio_device *dev = data;

	(void)irq;
	mod_timer(&dev->timer,
		  jiffies + msecs_to_jiffies(NOBLOCKIO_DEBOUNCE_MS));
	return IRQ_HANDLED;
}

/**
 * noblockio_timer_callback() - 确认稳定按键状态并发布释放事件
 * @data: 指向 struct noblockio_device 的无符号长整型参数
 *
 * 只有先确认稳定按下，再确认稳定释放，才把 NOBLOCKIO_KEY0_VALUE 写入
 * 单事件槽并唤醒等待队列。单独出现的释放电平不会生成事件。
 *
 * Context: timer softirq 上下文，不允许睡眠。
 */
static void noblockio_timer_callback(unsigned long data)
{
	struct noblockio_device *dev = (struct noblockio_device *)data;
	unsigned long flags;
	bool pressed;
	bool overwrite = false;
	bool wake = false;

	pressed = !!gpio_get_value(dev->key.gpio);
	if (dev->key.active_low)
		pressed = !pressed;

	/*
	 * pressed 和事件槽由 read 路径并发访问，必须在同一临界区完成
	 * “确认释放、写入键值、置位就绪标志”，避免 poll 看到半更新状态。
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
			NOBLOCKIO_NAME);
	} else if (wake) {
		if (overwrite)
			pr_warn("%s: unread key event overwritten\n",
				NOBLOCKIO_NAME);
		pr_info("%s: KEY0 release confirmed, event ready\n",
			NOBLOCKIO_NAME);
		/*
		 * 状态对外可见后再唤醒，保证 read、poll 和 select 被唤醒时
		 * 能观察到 event_pending，避免丢失就绪通知。
		 */
		wake_up_interruptible(&dev->read_wait);
	}
}

/**
 * noblockio_open() - 打开非阻塞按键字符设备
 * @inode: 字符设备 inode
 * @filp: 打开的文件对象
 *
 * Context: 仅允许进程上下文调用。
 * Return: 始终返回 0。
 */
static int noblockio_open(struct inode *inode, struct file *filp)
{
	struct noblockio_device *dev;

	dev = container_of(inode->i_cdev, struct noblockio_device, cdev);
	filp->private_data = dev;
	pr_info("%s: device opened, nonblock=%u\n", NOBLOCKIO_NAME,
		!!(filp->f_flags & O_NONBLOCK));
	return 0;
}

/**
 * noblockio_read() - 阻塞或非阻塞读取一个 KEY0 释放事件
 * @filp: 打开的文件对象
 * @buf: 用户态接收缓冲区
 * @count: 用户态缓冲区长度
 * @offp: 文件偏移，本驱动不使用
 *
 * O_NONBLOCK 文件在无事件时返回 -EAGAIN；普通文件在相同等待队列中进入
 * 可中断睡眠。多个读取者竞争单事件槽，一个事件只交付给一个读取者。
 *
 * Context: 仅允许进程上下文调用，可以睡眠。
 * Return: 成功返回 1，缓冲区过小返回 -EINVAL，复制失败返回 -EFAULT，等待被
 * 信号中断返回 -ERESTARTSYS，非阻塞且无事件返回 -EAGAIN。
 */
static ssize_t noblockio_read(struct file *filp, char __user *buf,
			      size_t count, loff_t *offp)
{
	struct noblockio_device *dev = filp->private_data;
	unsigned long flags;
	u8 value;
	int ret;

	(void)offp;
	if (count < sizeof(value)) {
		pr_warn("%s: read buffer too small: %zu\n", NOBLOCKIO_NAME,
			count);
		return -EINVAL;
	}

	for (;;) {
		if (filp->f_flags & O_NONBLOCK) {
			if (!READ_ONCE(dev->event_pending)) {
				pr_debug("%s: no key event available\n",
					 NOBLOCKIO_NAME);
				return -EAGAIN;
			}
		} else {
			ret = wait_event_interruptible(dev->read_wait,
						       READ_ONCE(dev->event_pending));
			if (ret) {
				pr_warn("%s: read wait interrupted: %d\n",
					NOBLOCKIO_NAME, ret);
				return -ERESTARTSYS;
			}
		}

		/*
		 * poll/select 的就绪结果只是瞬时快照。实际读取时必须持锁
		 * 重新竞争事件，防止多个读取者重复取得同一个按键值。
		 */
		spin_lock_irqsave(&dev->lock, flags);
		if (dev->event_pending) {
			dev->event_pending = false;
			value = dev->key_value;
			spin_unlock_irqrestore(&dev->lock, flags);
			break;
		}
		spin_unlock_irqrestore(&dev->lock, flags);
		if (filp->f_flags & O_NONBLOCK) {
			pr_debug("%s: key event consumed by another reader\n",
				 NOBLOCKIO_NAME);
			return -EAGAIN;
		}
	}

	if (copy_to_user(buf, &value, sizeof(value))) {
		pr_err("%s: failed to copy key event to user space\n",
		       NOBLOCKIO_NAME);
		return -EFAULT;
	}
	pr_info("%s: key event delivered, value=0x%02x\n",
		NOBLOCKIO_NAME, value);
	return sizeof(value);
}

/**
 * noblockio_poll() - 查询按键事件是否可读
 * @filp: 打开的文件对象
 * @wait: VFS 提供的 poll table
 *
 * 先使用 poll_wait() 把当前调用者登记到 read_wait，再检查事件槽。该顺序避免
 * 事件恰好在登记与检查之间到来时丢失唤醒。poll_wait() 本身不阻塞。
 *
 * Context: 仅允许进程上下文调用，不得在持有 @dev->lock 时调用。
 * Return: 有事件返回 POLLIN | POLLRDNORM，否则返回 0。
 */
static unsigned int noblockio_poll(struct file *filp, poll_table *wait)
{
	struct noblockio_device *dev = filp->private_data;
	unsigned long flags;
	unsigned int mask = 0;

	/*
	 * 必须先注册等待队列，再读取就绪状态；如果顺序相反，事件可能在
	 * 检查后、注册前到达，导致 poll/select 一直等待到下一次事件。
	 */
	poll_wait(filp, &dev->read_wait, wait);
	spin_lock_irqsave(&dev->lock, flags);
	if (dev->event_pending)
		mask = POLLIN | POLLRDNORM;
	spin_unlock_irqrestore(&dev->lock, flags);

	return mask;
}

static const struct file_operations noblockio_fops = {
	.owner = THIS_MODULE,
	.open = noblockio_open,
	.read = noblockio_read,
	.poll = noblockio_poll,
	.llseek = no_llseek,
};

/**
 * noblockio_module_init() - 初始化非阻塞按键字符设备
 *
 * 从 /key 解析 GPIO 和 IRQ，初始化消抖定时器，申请双边沿中断，最后创建
 * /dev/noblockio。定时器必须在 request_irq() 前初始化。
 *
 * Context: 模块加载期间在进程上下文调用，可以睡眠。
 * Return: 成功返回 0，失败返回对应的负错误码。
 */
static int __init noblockio_module_init(void)
{
	enum of_gpio_flags gpio_flags;
	unsigned long irq_flags;
	int ret;

	spin_lock_init(&noblockio.lock);
	init_waitqueue_head(&noblockio.read_wait);
	noblockio.key.value = NOBLOCKIO_KEY0_VALUE;
	noblockio.key.name = "KEY0";

	noblockio.node = of_find_node_by_path(NOBLOCKIO_NODE_PATH);
	if (!noblockio.node) {
		pr_err("%s: device tree node %s not found\n", NOBLOCKIO_NAME,
		       NOBLOCKIO_NODE_PATH);
		return -ENODEV;
	}
	if (!of_device_is_compatible(noblockio.node, "atkalpha-key") ||
	    !of_device_is_available(noblockio.node)) {
		pr_err("%s: device tree node is unavailable or incompatible\n",
		       NOBLOCKIO_NAME);
		ret = -ENODEV;
		goto err_put_node;
	}

	noblockio.key.gpio = of_get_named_gpio_flags(noblockio.node,
						    NOBLOCKIO_GPIO_PROPERTY,
						    0, &gpio_flags);
	if (!gpio_is_valid(noblockio.key.gpio)) {
		ret = noblockio.key.gpio < 0 ? noblockio.key.gpio : -EINVAL;
		pr_err("%s: failed to get %s: %d\n", NOBLOCKIO_NAME,
		       NOBLOCKIO_GPIO_PROPERTY, ret);
		goto err_put_node;
	}
	noblockio.key.active_low = gpio_flags & OF_GPIO_ACTIVE_LOW;
	ret = gpio_request(noblockio.key.gpio, noblockio.key.name);
	if (ret) {
		pr_err("%s: failed to request GPIO %d: %d\n", NOBLOCKIO_NAME,
		       noblockio.key.gpio, ret);
		goto err_put_node;
	}
	ret = gpio_direction_input(noblockio.key.gpio);
	if (ret) {
		pr_err("%s: failed to configure GPIO %d as input: %d\n",
		       NOBLOCKIO_NAME, noblockio.key.gpio, ret);
		goto err_free_gpio;
	}

	noblockio.key.irq = irq_of_parse_and_map(noblockio.node, 0);
	if (!noblockio.key.irq) {
		pr_err("%s: failed to map KEY0 interrupt\n", NOBLOCKIO_NAME);
		ret = -EINVAL;
		goto err_free_gpio;
	}
	setup_timer(&noblockio.timer, noblockio_timer_callback,
		    (unsigned long)&noblockio);
	irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;
	ret = request_irq(noblockio.key.irq, noblockio_irq_handler, irq_flags,
			  noblockio.key.name, &noblockio);
	if (ret) {
		pr_err("%s: failed to request IRQ %d: %d\n", NOBLOCKIO_NAME,
		       noblockio.key.irq, ret);
		goto err_dispose_irq;
	}

	ret = alloc_chrdev_region(&noblockio.devid, 0, NOBLOCKIO_COUNT,
				  NOBLOCKIO_NAME);
	if (ret) {
		pr_err("%s: failed to allocate device number: %d\n",
		       NOBLOCKIO_NAME, ret);
		goto err_free_irq;
	}
	cdev_init(&noblockio.cdev, &noblockio_fops);
	noblockio.cdev.owner = THIS_MODULE;
	ret = cdev_add(&noblockio.cdev, noblockio.devid, NOBLOCKIO_COUNT);
	if (ret) {
		pr_err("%s: failed to add cdev: %d\n", NOBLOCKIO_NAME, ret);
		goto err_unregister;
	}
	noblockio.class = class_create(THIS_MODULE, NOBLOCKIO_NAME);
	if (IS_ERR(noblockio.class)) {
		ret = PTR_ERR(noblockio.class);
		pr_err("%s: failed to create class: %d\n", NOBLOCKIO_NAME, ret);
		goto err_del_cdev;
	}
	noblockio.device = device_create(noblockio.class, NULL,
					 noblockio.devid, NULL, NOBLOCKIO_NAME);
	if (IS_ERR(noblockio.device)) {
		ret = PTR_ERR(noblockio.device);
		pr_err("%s: failed to create device: %d\n", NOBLOCKIO_NAME,
		       ret);
		goto err_destroy_class;
	}

	pr_info("%s: registered, GPIO=%d IRQ=%d major=%u minor=%u\n",
		NOBLOCKIO_NAME, noblockio.key.gpio, noblockio.key.irq,
		MAJOR(noblockio.devid), MINOR(noblockio.devid));
	return 0;

err_destroy_class:
	class_destroy(noblockio.class);
err_del_cdev:
	cdev_del(&noblockio.cdev);
err_unregister:
	unregister_chrdev_region(noblockio.devid, NOBLOCKIO_COUNT);
err_free_irq:
	/*
	 * 先阻止新的 IRQ 再同步删除定时器，避免清理期间重新激活
	 * timer 并在 GPIO 释放后访问失效资源。
	 */
	free_irq(noblockio.key.irq, &noblockio);
	del_timer_sync(&noblockio.timer);
err_dispose_irq:
	irq_dispose_mapping(noblockio.key.irq);
err_free_gpio:
	gpio_free(noblockio.key.gpio);
err_put_node:
	of_node_put(noblockio.node);
	noblockio.node = NULL;
	return ret;
}

/**
 * noblockio_module_exit() - 注销设备并释放中断按键资源
 *
 * Context: 模块卸载期间在进程上下文调用，可以睡眠。
 */
static void __exit noblockio_module_exit(void)
{
	device_destroy(noblockio.class, noblockio.devid);
	class_destroy(noblockio.class);
	cdev_del(&noblockio.cdev);
	unregister_chrdev_region(noblockio.devid, NOBLOCKIO_COUNT);

	/*
	 * free_irq() 等待正在运行的 handler 结束并禁止新 handler，之后
	 * del_timer_sync() 才能保证没有回调继续访问 GPIO 和设备状态。
	 */
	free_irq(noblockio.key.irq, &noblockio);
	del_timer_sync(&noblockio.timer);
	irq_dispose_mapping(noblockio.key.irq);
	gpio_free(noblockio.key.gpio);
	of_node_put(noblockio.node);
	pr_info("%s: unregistered\n", NOBLOCKIO_NAME);
}

module_init(noblockio_module_init);
module_exit(noblockio_module_exit);

MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("Nonblocking GPIO interrupt key driver with poll support");
MODULE_LICENSE("GPL");
