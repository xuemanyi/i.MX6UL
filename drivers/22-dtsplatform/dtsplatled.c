// SPDX-License-Identifier: GPL-2.0
/* 基于设备树的 platform LED 字符设备驱动。 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

#define LEDDEV_CNT 1
#define LEDDEV_NAME "dtsplatled"
#define LEDOFF 0
#define LEDON 1

/**
 * struct leddev_device - 设备树 platform LED 状态
 * @devid: 字符设备号
 * @cdev: 字符设备对象
 * @class: 设备类
 * @device: 设备节点
 * @gpio: LED GPIO 编号
 */
struct leddev_device {
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int gpio;
};

static struct leddev_device leddev;

/* 根据逻辑状态设置低电平有效的 LED。 */
static void led0_switch(u8 state)
{
	if (state == LEDON)
		gpio_set_value(leddev.gpio, 0);
	else
		gpio_set_value(leddev.gpio, 1);
}

/**
 * led_open() - 打开 LED 字符设备
 * @inode: 字符设备 inode
 * @filp: 文件对象
 *
 * Context: 进程上下文。
 * Return: 成功返回 0。
 */
static int led_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &leddev;
	return 0;
}

/**
 * led_write() - 接收用户态 LED 状态
 * @filp: 文件对象
 * @buf: 用户态缓冲区
 * @count: 写入长度，至少为 1
 * @offp: 文件偏移，本驱动不使用
 *
 * Context: 进程上下文，可以睡眠。
 * Return: 成功返回 1，参数非法返回 -EINVAL，复制失败返回 -EFAULT。
 */
static ssize_t led_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *offp)
{
	u8 state;

	(void)filp;
	(void)offp;
	if (count < sizeof(state))
		return -EINVAL;
	if (copy_from_user(&state, buf, sizeof(state)))
		return -EFAULT;
	if (state != LEDON && state != LEDOFF)
		return -EINVAL;
	led0_switch(state);
	pr_info("%s: LED turned %s\n", LEDDEV_NAME,
		state == LEDON ? "on" : "off");
	return sizeof(state);
}

static const struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.write = led_write,
	.llseek = no_llseek,
};

/**
 * led_probe() - 匹配设备树 LED platform 设备
 * @pdev: platform 设备
 *
 * 读取 `led-gpio`，申请并初始化 GPIO，然后注册字符设备。
 * Context: 进程上下文，可以睡眠。
 * Return: 成功返回 0，否则返回负错误码。
 */
static int led_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int ret;

	if (!node) {
		dev_err(&pdev->dev, "device tree node is missing\n");
		return -ENODEV;
	}
	leddev.gpio = of_get_named_gpio(node, "led-gpio", 0);
	if (!gpio_is_valid(leddev.gpio)) {
		ret = leddev.gpio < 0 ? leddev.gpio : -EINVAL;
		dev_err(&pdev->dev, "failed to get led-gpio: %d\n", ret);
		return ret;
	}
	ret = gpio_request(leddev.gpio, LEDDEV_NAME);
	if (ret) {
		dev_err(&pdev->dev, "failed to request GPIO %d: %d\n",
			leddev.gpio, ret);
		return ret;
	}
	ret = gpio_direction_output(leddev.gpio, 1);
	if (ret)
		goto err_gpio;

	ret = alloc_chrdev_region(&leddev.devid, 0, LEDDEV_CNT, LEDDEV_NAME);
	if (ret)
		goto err_gpio;
	cdev_init(&leddev.cdev, &led_fops);
	leddev.cdev.owner = THIS_MODULE;
	ret = cdev_add(&leddev.cdev, leddev.devid, LEDDEV_CNT);
	if (ret)
		goto err_unregister;
	leddev.class = class_create(THIS_MODULE, LEDDEV_NAME);
	if (IS_ERR(leddev.class)) {
		ret = PTR_ERR(leddev.class);
		goto err_cdev;
	}
	leddev.device = device_create(leddev.class, &pdev->dev, leddev.devid,
					NULL, LEDDEV_NAME);
	if (IS_ERR(leddev.device)) {
		ret = PTR_ERR(leddev.device);
		goto err_class;
	}
	platform_set_drvdata(pdev, &leddev);
	dev_info(&pdev->dev, "registered, GPIO=%d major=%u minor=%u\n",
		leddev.gpio, MAJOR(leddev.devid), MINOR(leddev.devid));
	return 0;

err_class:
	class_destroy(leddev.class);
err_cdev:
	cdev_del(&leddev.cdev);
err_unregister:
	unregister_chrdev_region(leddev.devid, LEDDEV_CNT);
err_gpio:
	gpio_free(leddev.gpio);
	return ret;
}

/**
 * led_remove() - 移除 platform LED 设备
 * @pdev: platform 设备
 *
 * Context: 进程上下文，可以睡眠。
 * Return: 成功返回 0。
 */
static int led_remove(struct platform_device *pdev)
{
	struct leddev_device *dev = platform_get_drvdata(pdev);

	led0_switch(LEDOFF);
	device_destroy(dev->class, dev->devid);
	class_destroy(dev->class);
	cdev_del(&dev->cdev);
	unregister_chrdev_region(dev->devid, LEDDEV_CNT);
	gpio_free(dev->gpio);
	platform_set_drvdata(pdev, NULL);
	dev_info(&pdev->dev, "unregistered\n");
	return 0;
}

static const struct of_device_id led_of_match[] = {
	{ .compatible = "atkalpha-gpioled" },
	{ }
};
MODULE_DEVICE_TABLE(of, led_of_match);

static struct platform_driver led_driver = {
	.driver = {
		.name = "imx6ul-led",
		.of_match_table = led_of_match,
	},
	.probe = led_probe,
	.remove = led_remove,
};

module_platform_driver(led_driver);

MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("Device-tree platform GPIO LED driver");
MODULE_LICENSE("GPL");
