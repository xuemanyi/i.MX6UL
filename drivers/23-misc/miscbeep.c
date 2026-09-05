// SPDX-License-Identifier: GPL-2.0
/* 基于 platform_driver 和 miscdevice 的设备树蜂鸣器驱动。 */

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

#define MISCBEEP_NAME "miscbeep"
#define MISCBEEP_MINOR 144
#define BEEPOFF 0
#define BEEPON 1

/**
 * struct miscbeep_device - MISC 蜂鸣器设备状态
 * @beep_gpio: 蜂鸣器 GPIO 编号
 * @miscdev: MISC 设备对象
 * @gpio_requested: GPIO 是否已申请
 */
struct miscbeep_device {
	int beep_gpio;
	struct miscdevice miscdev;
	bool gpio_requested;
};

static struct miscbeep_device miscbeep;

/* 蜂鸣器低电平有效：BEEPON 输出 0，BEEPOFF 输出 1。 */
static void miscbeep_set_state(struct miscbeep_device *dev, u8 state)
{
	if (state == BEEPON)
		gpio_set_value(dev->beep_gpio, 0);
	else
		gpio_set_value(dev->beep_gpio, 1);
}

/**
 * miscbeep_open() - 打开 MISC 蜂鸣器设备
 * @inode: 字符设备 inode
 * @filp: 文件对象
 *
 * Context: 进程上下文。
 * Return: 成功返回 0。
 */
static int miscbeep_open(struct inode *inode, struct file *filp)
{
	struct miscbeep_device *dev;

	dev = container_of(filp->private_data, struct miscbeep_device, miscdev);
	filp->private_data = dev;
	return 0;
}

/**
 * miscbeep_write() - 设置蜂鸣器开关状态
 * @filp: 文件对象
 * @buf: 用户态状态字节
 * @count: 写入长度，至少为 1
 * @offp: 文件偏移，本驱动不使用
 *
 * Context: 进程上下文，可以睡眠。
 * Return: 成功返回 1，参数非法返回 -EINVAL，复制失败返回 -EFAULT。
 */
static ssize_t miscbeep_write(struct file *filp, const char __user *buf,
				      size_t count, loff_t *offp)
{
	struct miscbeep_device *dev = filp->private_data;
	u8 state;

	(void)offp;
	if (count < sizeof(state))
		return -EINVAL;
	if (copy_from_user(&state, buf, sizeof(state)))
		return -EFAULT;
	if (state != BEEPON && state != BEEPOFF)
		return -EINVAL;
	miscbeep_set_state(dev, state);
	pr_info("%s: buzzer turned %s\n", MISCBEEP_NAME,
		state == BEEPON ? "on" : "off");
	return sizeof(state);
}

static const struct file_operations miscbeep_fops = {
	.owner = THIS_MODULE,
	.open = miscbeep_open,
	.write = miscbeep_write,
	.llseek = no_llseek,
};

/**
 * miscbeep_probe() - 匹配并初始化设备树蜂鸣器
 * @pdev: platform 设备
 *
 * 获取 `beep-gpio`，申请 GPIO，设置默认关闭状态并注册 MISC 设备。
 * Context: 进程上下文，可以睡眠。
 * Return: 成功返回 0，否则返回负错误码。
 */
static int miscbeep_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int ret;

	if (!node) {
		dev_err(&pdev->dev, "device tree node is missing\n");
		return -ENODEV;
	}
	miscbeep.beep_gpio = of_get_named_gpio(node, "beep-gpio", 0);
	if (!gpio_is_valid(miscbeep.beep_gpio)) {
		ret = miscbeep.beep_gpio < 0 ? miscbeep.beep_gpio : -EINVAL;
		dev_err(&pdev->dev, "failed to get beep-gpio: %d\n", ret);
		return ret;
	}
	ret = gpio_request(miscbeep.beep_gpio, MISCBEEP_NAME);
	if (ret) {
		dev_err(&pdev->dev, "failed to request GPIO %d: %d\n",
			miscbeep.beep_gpio, ret);
		return ret;
	}
	miscbeep.gpio_requested = true;
	ret = gpio_direction_output(miscbeep.beep_gpio, 1);
	if (ret)
		goto err_gpio;

	miscbeep.miscdev.minor = MISCBEEP_MINOR;
	miscbeep.miscdev.name = MISCBEEP_NAME;
	miscbeep.miscdev.fops = &miscbeep_fops;
	ret = misc_register(&miscbeep.miscdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to register misc device: %d\n", ret);
		goto err_gpio;
	}
	platform_set_drvdata(pdev, &miscbeep);
	dev_info(&pdev->dev, "registered, GPIO=%d minor=%d\n",
		miscbeep.beep_gpio, MISCBEEP_MINOR);
	return 0;

err_gpio:
	gpio_set_value(miscbeep.beep_gpio, 1);
	gpio_free(miscbeep.beep_gpio);
	miscbeep.gpio_requested = false;
	return ret;
}

/**
 * miscbeep_remove() - 移除 MISC 蜂鸣器设备
 * @pdev: platform 设备
 *
 * Context: 进程上下文，可以睡眠。
 * Return: 成功返回 0。
 */
static int miscbeep_remove(struct platform_device *pdev)
{
	struct miscbeep_device *dev = platform_get_drvdata(pdev);

	misc_deregister(&dev->miscdev);
	miscbeep_set_state(dev, BEEPOFF);
	if (dev->gpio_requested) {
		gpio_free(dev->beep_gpio);
		dev->gpio_requested = false;
	}
	platform_set_drvdata(pdev, NULL);
	dev_info(&pdev->dev, "unregistered\n");
	return 0;
}

static const struct of_device_id miscbeep_of_match[] = {
	{ .compatible = "atkalpha-beep" },
	{ }
};
MODULE_DEVICE_TABLE(of, miscbeep_of_match);

static struct platform_driver miscbeep_driver = {
	.probe = miscbeep_probe,
	.remove = miscbeep_remove,
	.driver = {
		.name = "imx6ul-beep",
		.of_match_table = miscbeep_of_match,
	},
};

module_platform_driver(miscbeep_driver);

MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("Device-tree platform MISC buzzer driver");
MODULE_LICENSE("GPL");
