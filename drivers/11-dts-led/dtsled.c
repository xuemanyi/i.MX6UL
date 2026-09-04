// SPDX-License-Identifier: GPL-2.0
/* 基于设备树寄存器资源的 i.MX6ULL GPIO1_IO03 LED 字符设备驱动。 */

#include <linux/bitops.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#define DTSLED_COUNT			1
#define DTSLED_NAME			"dtsled"
#define DTSLED_NODE_PATH		"/alphaled"
#define LED_OFF				0
#define LED_ON				1
#define GPIO1_IO03_MASK			BIT(3)
#define CCM_CCGR1_GPIO1_SHIFT		26
#define CCM_CCGR1_GPIO1_MASK		(0x3U << CCM_CCGR1_GPIO1_SHIFT)
#define CCM_CCGR1_GPIO1_ENABLE		(0x3U << CCM_CCGR1_GPIO1_SHIFT)
#define GPIO1_IO03_MUX_MODE		5
#define GPIO1_IO03_PAD_CONFIG		0x10b0

enum dtsled_register {
	DTSLED_REG_CCM_CCGR1,
	DTSLED_REG_SW_MUX_GPIO1_IO03,
	DTSLED_REG_SW_PAD_GPIO1_IO03,
	DTSLED_REG_GPIO1_DR,
	DTSLED_REG_GPIO1_GDIR,
	DTSLED_REG_COUNT,
};

/**
 * struct dtsled_device - DTS LED 字符设备
 * @devid: 字符设备号
 * @cdev: 字符设备对象
 * @class: sysfs 设备类
 * @device: sysfs 设备对象
 * @node: alphaled 设备树节点
 * @regs: 按 reg 属性顺序映射的寄存器地址
 * @lock: 保护 GPIO 数据寄存器读改写操作的互斥锁
 *
 * 模块加载期间完成初始化，卸载期间按资源申请的逆序销毁。
 * 用户态访问 GPIO 状态时必须持有 @lock。
 */
struct dtsled_device {
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *node;
	void __iomem *regs[DTSLED_REG_COUNT];
	struct mutex lock;
};

static struct dtsled_device dtsled;

/**
 * dtsled_set_state() - 设置 LED 状态
 * @led: DTS LED 设备
 * @state: LED_ON 表示点亮，LED_OFF 表示熄灭
 *
 * LED 采用低电平有效接法，因此点亮时清除 GPIO1_IO03 输出位。
 *
 * Context: 仅允许进程上下文调用，调用方必须持有 @led->lock。
 */
static void dtsled_set_state(struct dtsled_device *led, u8 state)
{
	u32 value;

	value = readl(led->regs[DTSLED_REG_GPIO1_DR]);
	if (state == LED_ON)
		value &= ~GPIO1_IO03_MASK;
	else
		value |= GPIO1_IO03_MASK;
	writel(value, led->regs[DTSLED_REG_GPIO1_DR]);
}

/**
 * dtsled_get_state() - 获取 LED 状态
 * @led: DTS LED 设备
 *
 * Context: 仅允许进程上下文调用，调用方必须持有 @led->lock。
 * Return: LED 点亮返回 LED_ON，熄灭返回 LED_OFF。
 */
static u8 dtsled_get_state(struct dtsled_device *led)
{
	u32 value;

	value = readl(led->regs[DTSLED_REG_GPIO1_DR]);
	return value & GPIO1_IO03_MASK ? LED_OFF : LED_ON;
}

/**
 * dtsled_open() - 打开 DTS LED 字符设备
 * @inode: 字符设备 inode
 * @filp: 打开的文件对象
 *
 * 将字符设备对象转换为 DTS LED 设备并保存到文件私有数据中。
 *
 * Context: 仅允许进程上下文调用。
 * Return: 始终返回 0。
 */
static int dtsled_open(struct inode *inode, struct file *filp)
{
	struct dtsled_device *led;

	led = container_of(inode->i_cdev, struct dtsled_device, cdev);
	filp->private_data = led;
	return 0;
}

/**
 * dtsled_read() - 向用户态返回当前 LED 状态
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
static ssize_t dtsled_read(struct file *filp, char __user *buf,
			   size_t count, loff_t *offp)
{
	struct dtsled_device *led = filp->private_data;
	u8 state;

	if (!count || *offp)
		return 0;

	mutex_lock(&led->lock);
	state = dtsled_get_state(led);
	mutex_unlock(&led->lock);

	if (copy_to_user(buf, &state, sizeof(state)))
		return -EFAULT;
	*offp += sizeof(state);
	return sizeof(state);
}

/**
 * dtsled_write() - 设置 LED 状态
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
static ssize_t dtsled_write(struct file *filp, const char __user *buf,
			    size_t count, loff_t *offp)
{
	struct dtsled_device *led = filp->private_data;
	u8 state;

	(void)offp;
	if (count < sizeof(state))
		return -EINVAL;
	if (copy_from_user(&state, buf, sizeof(state)))
		return -EFAULT;
	if (state != LED_ON && state != LED_OFF)
		return -EINVAL;

	mutex_lock(&led->lock);
	dtsled_set_state(led, state);
	mutex_unlock(&led->lock);
	return sizeof(state);
}

static const struct file_operations dtsled_fops = {
	.owner = THIS_MODULE,
	.open = dtsled_open,
	.read = dtsled_read,
	.write = dtsled_write,
	.llseek = no_llseek,
};

/**
 * dtsled_unmap_registers() - 取消全部寄存器映射
 * @led: DTS LED 设备
 *
 * 可处理仅完成部分映射的错误路径。
 *
 * Context: 仅允许进程上下文调用。
 */
static void dtsled_unmap_registers(struct dtsled_device *led)
{
	int index;

	for (index = DTSLED_REG_COUNT - 1; index >= 0; index--) {
		if (led->regs[index]) {
			iounmap(led->regs[index]);
			led->regs[index] = NULL;
		}
	}
}

/**
 * dtsled_map_registers() - 映射 alphaled 节点的寄存器资源
 * @led: DTS LED 设备
 *
 * reg 属性必须依次描述 CCM_CCGR1、IOMUXC MUX、IOMUXC PAD、GPIO1_DR
 * 和 GPIO1_GDIR。
 *
 * Context: 仅允许进程上下文调用，可以睡眠。
 * Return: 成功返回 0，映射失败返回 -ENOMEM。
 */
static int dtsled_map_registers(struct dtsled_device *led)
{
	int index;

	for (index = 0; index < DTSLED_REG_COUNT; index++) {
		led->regs[index] = of_iomap(led->node, index);
		if (!led->regs[index]) {
			pr_err("%s: failed to map register resource %d\n",
			       DTSLED_NAME, index);
			dtsled_unmap_registers(led);
			return -ENOMEM;
		}
	}
	return 0;
}

/**
 * dtsled_hw_init() - 初始化 LED 引脚和默认状态
 * @led: DTS LED 设备
 *
 * 使能 GPIO1 时钟，将 GPIO1_IO03 配置为 GPIO 输出并默认熄灭 LED。
 *
 * Context: 仅允许进程上下文调用。
 */
static void dtsled_hw_init(struct dtsled_device *led)
{
	u32 value;

	value = readl(led->regs[DTSLED_REG_CCM_CCGR1]);
	value &= ~CCM_CCGR1_GPIO1_MASK;
	value |= CCM_CCGR1_GPIO1_ENABLE;
	writel(value, led->regs[DTSLED_REG_CCM_CCGR1]);
	writel(GPIO1_IO03_MUX_MODE,
	       led->regs[DTSLED_REG_SW_MUX_GPIO1_IO03]);
	writel(GPIO1_IO03_PAD_CONFIG,
	       led->regs[DTSLED_REG_SW_PAD_GPIO1_IO03]);

	value = readl(led->regs[DTSLED_REG_GPIO1_GDIR]);
	value |= GPIO1_IO03_MASK;
	writel(value, led->regs[DTSLED_REG_GPIO1_GDIR]);

	mutex_lock(&led->lock);
	dtsled_set_state(led, LED_OFF);
	mutex_unlock(&led->lock);
}

/**
 * dtsled_init() - 初始化 DTS LED 驱动
 *
 * 查找 /alphaled，映射寄存器，并注册动态字符设备和 /dev/dtsled。
 *
 * Context: 模块加载期间在进程上下文调用，可以睡眠。
 * Return: 成功返回 0，失败返回对应的负错误码。
 */
static int __init dtsled_init(void)
{
	int ret;

	mutex_init(&dtsled.lock);
	dtsled.node = of_find_node_by_path(DTSLED_NODE_PATH);
	if (!dtsled.node) {
		pr_err("%s: device tree node %s not found\n",
		       DTSLED_NAME, DTSLED_NODE_PATH);
		return -ENODEV;
	}
	if (!of_device_is_compatible(dtsled.node, "atkalpha-led") ||
	    !of_device_is_available(dtsled.node)) {
		pr_err("%s: device tree node is unavailable or incompatible\n",
		       DTSLED_NAME);
		ret = -ENODEV;
		goto err_put_node;
	}

	ret = dtsled_map_registers(&dtsled);
	if (ret)
		goto err_put_node;
	dtsled_hw_init(&dtsled);

	ret = alloc_chrdev_region(&dtsled.devid, 0, DTSLED_COUNT,
				  DTSLED_NAME);
	if (ret) {
		pr_err("%s: failed to allocate device number: %d\n",
		       DTSLED_NAME, ret);
		goto err_unmap;
	}

	cdev_init(&dtsled.cdev, &dtsled_fops);
	dtsled.cdev.owner = THIS_MODULE;
	ret = cdev_add(&dtsled.cdev, dtsled.devid, DTSLED_COUNT);
	if (ret) {
		pr_err("%s: failed to add cdev: %d\n", DTSLED_NAME, ret);
		goto err_unregister;
	}

	dtsled.class = class_create(THIS_MODULE, DTSLED_NAME);
	if (IS_ERR(dtsled.class)) {
		ret = PTR_ERR(dtsled.class);
		pr_err("%s: failed to create class: %d\n", DTSLED_NAME, ret);
		goto err_del_cdev;
	}

	dtsled.device = device_create(dtsled.class, NULL, dtsled.devid,
				       NULL, DTSLED_NAME);
	if (IS_ERR(dtsled.device)) {
		ret = PTR_ERR(dtsled.device);
		pr_err("%s: failed to create device: %d\n", DTSLED_NAME, ret);
		goto err_destroy_class;
	}

	pr_info("%s: registered, major=%u minor=%u\n", DTSLED_NAME,
		MAJOR(dtsled.devid), MINOR(dtsled.devid));
	return 0;

err_destroy_class:
	class_destroy(dtsled.class);
err_del_cdev:
	cdev_del(&dtsled.cdev);
err_unregister:
	unregister_chrdev_region(dtsled.devid, DTSLED_COUNT);
err_unmap:
	dtsled_unmap_registers(&dtsled);
err_put_node:
	of_node_put(dtsled.node);
	dtsled.node = NULL;
	return ret;
}

/**
 * dtsled_exit() - 注销 DTS LED 驱动
 *
 * Context: 模块卸载期间在进程上下文调用，可以睡眠。
 */
static void __exit dtsled_exit(void)
{
	device_destroy(dtsled.class, dtsled.devid);
	class_destroy(dtsled.class);
	cdev_del(&dtsled.cdev);
	unregister_chrdev_region(dtsled.devid, DTSLED_COUNT);
	dtsled_unmap_registers(&dtsled);
	of_node_put(dtsled.node);
	pr_info("%s: unregistered\n", DTSLED_NAME);
}

module_init(dtsled_init);
module_exit(dtsled_exit);

MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("Device-tree-based GPIO1_IO03 LED character driver");
MODULE_LICENSE("GPL");
