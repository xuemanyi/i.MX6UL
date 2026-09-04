// SPDX-License-Identifier: GPL-2.0
/* 基于 GPIO 中断和内核定时器消抖的按键字符设备驱动。 */

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

#define IMX6UIRQ_COUNT		1
#define IMX6UIRQ_NAME		"imx6uirq"
#define IMX6UIRQ_NODE_PATH	"/key"
#define IMX6UIRQ_GPIO_PROPERTY	"key-gpio"
#define KEY0_VALUE		0x01
#define DEBOUNCE_DELAY_MS	10U

/**
 * struct irq_key_desc - 中断按键描述
 * @gpio: 按键使用的 Linux GPIO 编号
 * @irq: GPIO 对应的 Linux IRQ
 * @value: 按键对用户态报告的键值
 * @active_low: 按键是否为低电平有效
 * @name: 中断和 GPIO 资源名称
 */
struct irq_key_desc {
	int gpio;
	int irq;
	u8 value;
	bool active_low;
	const char *name;
};

/**
 * struct imx6uirq_device - GPIO 中断按键字符设备
 * @devid: 动态分配的字符设备号
 * @cdev: 字符设备对象
 * @class: sysfs 设备类
 * @device: sysfs 设备对象
 * @node: key 设备树节点
 * @key: KEY0 的 GPIO、中断和键值描述
 * @timer: 用于按键消抖的内核定时器
 * @lock: 保护按键状态和待读事件的自旋锁
 * @wait: 等待有效按键释放事件的等待队列
 * @pressed: 消抖后是否已经确认按下
 * @event_pending: 是否存在尚未读取的释放事件
 * @key_value: 待读取的按键值
 *
 * @lock 可在 hardirq、timer softirq 和进程上下文获取，持锁期间不得睡眠。
 * 模块持有 GPIO、IRQ 和设备树节点资源，直至初始化失败回滚或模块退出。
 */
struct imx6uirq_device {
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *node;
	struct irq_key_desc key;
	struct timer_list timer;
	spinlock_t lock;
	wait_queue_head_t wait;
	bool pressed;
	bool event_pending;
	u8 key_value;
};

static struct imx6uirq_device imx6uirq;

/**
 * key_irq_handler() - 处理 KEY0 的 GPIO 边沿中断
 * @irq: KEY0 对应的中断号，本函数不直接使用
 * @data: 指向 struct imx6uirq_device
 *
 * 任一边沿到来时重新安排消抖定时器，使 GPIO 电平稳定 10 ms 后再判断按键
 * 状态。连续抖动只会推迟采样时间，不会直接产生用户态事件。
 *
 * Context: hardirq 上下文，不允许睡眠。
 * Return: 始终返回 IRQ_HANDLED。
 */
static irqreturn_t key_irq_handler(int irq, void *data)
{
	struct imx6uirq_device *dev = data;

	(void)irq;
	mod_timer(&dev->timer,
		  jiffies + msecs_to_jiffies(DEBOUNCE_DELAY_MS));
	return IRQ_HANDLED;
}

/**
 * debounce_timer_callback() - 采样稳定按键电平并生成释放事件
 * @data: 指向 struct imx6uirq_device 的无符号长整型参数
 *
 * 稳定按下时记录按下状态；之后检测到稳定释放时，向用户态发布一次 KEY0_VALUE
 * 并唤醒阻塞读取进程。单独的释放边沿不会生成事件。
 *
 * Context: timer softirq 上下文，不允许睡眠。
 */
static void debounce_timer_callback(unsigned long data)
{
	struct imx6uirq_device *dev = (struct imx6uirq_device *)data;
	unsigned long flags;
	bool pressed;
	bool overwrite = false;
	bool wake = false;

	pressed = !!gpio_get_value(dev->key.gpio);
	if (dev->key.active_low)
		pressed = !pressed;

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
			IMX6UIRQ_NAME);
	} else if (wake) {
		if (overwrite)
			pr_warn("%s: unread key event overwritten\n",
				IMX6UIRQ_NAME);
		pr_info("%s: KEY0 release confirmed, event queued\n",
			IMX6UIRQ_NAME);
		wake_up_interruptible(&dev->wait);
	}
}

/**
 * imx6uirq_open() - 打开中断按键字符设备
 * @inode: 字符设备 inode
 * @filp: 打开的文件对象
 *
 * Context: 仅允许进程上下文调用。
 * Return: 始终返回 0。
 */
static int imx6uirq_open(struct inode *inode, struct file *filp)
{
	struct imx6uirq_device *dev;

	dev = container_of(inode->i_cdev, struct imx6uirq_device, cdev);
	filp->private_data = dev;
	pr_info("%s: device opened\n", IMX6UIRQ_NAME);
	return 0;
}

/**
 * imx6uirq_read() - 阻塞读取一次完整的按键按下和释放事件
 * @filp: 打开的文件对象
 * @buf: 用户态接收缓冲区
 * @count: 用户态缓冲区长度
 * @offp: 文件偏移，本驱动不使用
 *
 * 普通文件在没有事件时进入可中断睡眠；使用 O_NONBLOCK 打开且无事件时立即
 * 返回 -EAGAIN。多个读取进程竞争同一个单事件槽，每个事件仅由一个进程取得。
 *
 * Context: 仅允许进程上下文调用，可以睡眠。
 * Return: 成功返回 1，缓冲区过小返回 -EINVAL，复制失败返回 -EFAULT，等待被
 * 信号中断返回 -ERESTARTSYS，非阻塞且无事件返回 -EAGAIN。
 */
static ssize_t imx6uirq_read(struct file *filp, char __user *buf,
			    size_t count, loff_t *offp)
{
	struct imx6uirq_device *dev = filp->private_data;
	unsigned long flags;
	u8 value;
	int ret;

	(void)offp;
	if (count < sizeof(value)) {
		pr_warn("%s: read buffer too small: %zu\n", IMX6UIRQ_NAME,
			count);
		return -EINVAL;
	}

	for (;;) {
		if (filp->f_flags & O_NONBLOCK) {
			if (!READ_ONCE(dev->event_pending)) {
				pr_debug("%s: no key event available\n",
					 IMX6UIRQ_NAME);
				return -EAGAIN;
			}
		} else {
			ret = wait_event_interruptible(dev->wait,
						       READ_ONCE(dev->event_pending));
			if (ret) {
				pr_warn("%s: read wait interrupted: %d\n",
					IMX6UIRQ_NAME, ret);
				return -ERESTARTSYS;
			}
		}

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
				 IMX6UIRQ_NAME);
			return -EAGAIN;
		}
	}

	if (copy_to_user(buf, &value, sizeof(value))) {
		pr_err("%s: failed to copy key event to user space\n",
		       IMX6UIRQ_NAME);
		return -EFAULT;
	}
	pr_info("%s: key event delivered, value=0x%02x\n",
		IMX6UIRQ_NAME, value);
	return sizeof(value);
}

static const struct file_operations imx6uirq_fops = {
	.owner = THIS_MODULE,
	.open = imx6uirq_open,
	.read = imx6uirq_read,
	.llseek = no_llseek,
};

/**
 * imx6uirq_module_init() - 初始化 GPIO 中断、消抖定时器和字符设备
 *
 * 从 /key 节点取得 key-gpio 和 interrupts，申请 KEY0 GPIO 及双边沿中断，
 * 最后创建 /dev/imx6uirq。定时器仅由按键边沿中断启动。
 *
 * Context: 模块加载期间在进程上下文调用，可以睡眠。
 * Return: 成功返回 0，失败返回对应的负错误码。
 */
static int __init imx6uirq_module_init(void)
{
	enum of_gpio_flags gpio_flags;
	unsigned long irq_flags;
	int ret;

	spin_lock_init(&imx6uirq.lock);
	init_waitqueue_head(&imx6uirq.wait);
	imx6uirq.key.value = KEY0_VALUE;
	imx6uirq.key.name = "KEY0";

	imx6uirq.node = of_find_node_by_path(IMX6UIRQ_NODE_PATH);
	if (!imx6uirq.node) {
		pr_err("%s: device tree node %s not found\n", IMX6UIRQ_NAME,
		       IMX6UIRQ_NODE_PATH);
		return -ENODEV;
	}
	if (!of_device_is_compatible(imx6uirq.node, "atkalpha-key") ||
	    !of_device_is_available(imx6uirq.node)) {
		pr_err("%s: device tree node is unavailable or incompatible\n",
		       IMX6UIRQ_NAME);
		ret = -ENODEV;
		goto err_put_node;
	}

	imx6uirq.key.gpio = of_get_named_gpio_flags(imx6uirq.node,
						   IMX6UIRQ_GPIO_PROPERTY, 0,
						   &gpio_flags);
	if (!gpio_is_valid(imx6uirq.key.gpio)) {
		ret = imx6uirq.key.gpio < 0 ? imx6uirq.key.gpio : -EINVAL;
		pr_err("%s: failed to get %s: %d\n", IMX6UIRQ_NAME,
		       IMX6UIRQ_GPIO_PROPERTY, ret);
		goto err_put_node;
	}
	imx6uirq.key.active_low = gpio_flags & OF_GPIO_ACTIVE_LOW;
	ret = gpio_request(imx6uirq.key.gpio, imx6uirq.key.name);
	if (ret) {
		pr_err("%s: failed to request GPIO %d: %d\n", IMX6UIRQ_NAME,
		       imx6uirq.key.gpio, ret);
		goto err_put_node;
	}
	ret = gpio_direction_input(imx6uirq.key.gpio);
	if (ret) {
		pr_err("%s: failed to configure GPIO %d as input: %d\n",
		       IMX6UIRQ_NAME, imx6uirq.key.gpio, ret);
		goto err_free_gpio;
	}

	imx6uirq.key.irq = irq_of_parse_and_map(imx6uirq.node, 0);
	if (!imx6uirq.key.irq) {
		pr_err("%s: failed to map KEY0 interrupt\n", IMX6UIRQ_NAME);
		ret = -EINVAL;
		goto err_free_gpio;
	}
	setup_timer(&imx6uirq.timer, debounce_timer_callback,
		    (unsigned long)&imx6uirq);
	irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;
	ret = request_irq(imx6uirq.key.irq, key_irq_handler, irq_flags,
			  imx6uirq.key.name, &imx6uirq);
	if (ret) {
		pr_err("%s: failed to request IRQ %d: %d\n", IMX6UIRQ_NAME,
		       imx6uirq.key.irq, ret);
		goto err_dispose_irq;
	}

	ret = alloc_chrdev_region(&imx6uirq.devid, 0, IMX6UIRQ_COUNT,
				  IMX6UIRQ_NAME);
	if (ret) {
		pr_err("%s: failed to allocate device number: %d\n",
		       IMX6UIRQ_NAME, ret);
		goto err_free_irq;
	}
	cdev_init(&imx6uirq.cdev, &imx6uirq_fops);
	imx6uirq.cdev.owner = THIS_MODULE;
	ret = cdev_add(&imx6uirq.cdev, imx6uirq.devid, IMX6UIRQ_COUNT);
	if (ret) {
		pr_err("%s: failed to add cdev: %d\n", IMX6UIRQ_NAME, ret);
		goto err_unregister;
	}
	imx6uirq.class = class_create(THIS_MODULE, IMX6UIRQ_NAME);
	if (IS_ERR(imx6uirq.class)) {
		ret = PTR_ERR(imx6uirq.class);
		pr_err("%s: failed to create class: %d\n", IMX6UIRQ_NAME, ret);
		goto err_del_cdev;
	}
	imx6uirq.device = device_create(imx6uirq.class, NULL,
					imx6uirq.devid, NULL, IMX6UIRQ_NAME);
	if (IS_ERR(imx6uirq.device)) {
		ret = PTR_ERR(imx6uirq.device);
		pr_err("%s: failed to create device: %d\n", IMX6UIRQ_NAME,
		       ret);
		goto err_destroy_class;
	}

	pr_info("%s: registered, GPIO=%d IRQ=%d major=%u minor=%u\n",
		IMX6UIRQ_NAME, imx6uirq.key.gpio, imx6uirq.key.irq,
		MAJOR(imx6uirq.devid), MINOR(imx6uirq.devid));
	return 0;

err_destroy_class:
	class_destroy(imx6uirq.class);
err_del_cdev:
	cdev_del(&imx6uirq.cdev);
err_unregister:
	unregister_chrdev_region(imx6uirq.devid, IMX6UIRQ_COUNT);
err_free_irq:
	free_irq(imx6uirq.key.irq, &imx6uirq);
	del_timer_sync(&imx6uirq.timer);
err_dispose_irq:
	irq_dispose_mapping(imx6uirq.key.irq);
err_free_gpio:
	gpio_free(imx6uirq.key.gpio);
err_put_node:
	of_node_put(imx6uirq.node);
	imx6uirq.node = NULL;
	return ret;
}

/**
 * imx6uirq_module_exit() - 注销字符设备并释放中断按键资源
 *
 * 先释放 IRQ 以阻止新的消抖任务，再同步删除定时器，最后释放 GPIO 和设备树
 * 节点，保证回调不会访问已经释放的资源。
 *
 * Context: 模块卸载期间在进程上下文调用，可以睡眠。
 */
static void __exit imx6uirq_module_exit(void)
{
	device_destroy(imx6uirq.class, imx6uirq.devid);
	class_destroy(imx6uirq.class);
	cdev_del(&imx6uirq.cdev);
	unregister_chrdev_region(imx6uirq.devid, IMX6UIRQ_COUNT);
	free_irq(imx6uirq.key.irq, &imx6uirq);
	del_timer_sync(&imx6uirq.timer);
	irq_dispose_mapping(imx6uirq.key.irq);
	gpio_free(imx6uirq.key.gpio);
	of_node_put(imx6uirq.node);
	pr_info("%s: unregistered\n", IMX6UIRQ_NAME);
}

module_init(imx6uirq_module_init);
module_exit(imx6uirq_module_exit);

MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("GPIO interrupt key driver with timer debounce");
MODULE_LICENSE("GPL");
