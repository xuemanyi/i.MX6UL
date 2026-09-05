// SPDX-License-Identifier: GPL-2.0
/* 基于 platform_driver 的设备树 GPIO LED 字符设备驱动。 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

#define PLATFORM_LED_COUNT	1
#define PLATFORM_LED_NAME	"platform_led"
#define PLATFORM_LED_PROPERTY	"led-gpio"
#define LEDOFF			0
#define LEDON			1

/**
 * struct platform_led - platform LED 字符设备状态
 * @devid: 动态分配的字符设备号
 * @cdev: 字符设备对象
 * @class: 设备类
 * @device: 设备节点
 * @pdev: 匹配到的 platform 设备
 * @gpio: LED 使用的 GPIO 编号
 * @active_low: LED 是否低电平有效
 * @state: 当前 LED 逻辑状态
 * @lock: 保护 GPIO 和状态访问
 */
struct platform_led {
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct platform_device *pdev;
	int gpio;
	bool active_low;
	u8 state;
	struct mutex lock;
};

static struct platform_led platform_led;

/* 将逻辑状态转换为物理电平并写入 LED GPIO，调用方必须持有 lock。 */
static void platform_led_set(struct platform_led *led, u8 state)
{
	int value = state == LEDON;

	if (led->active_low)
		value = !value;
	gpio_set_value(led->gpio, value);
	led->state = state;
}

/**
 * platform_led_open() - 打开 platform LED 字符设备
 * @inode: 字符设备 inode
 * @filp: 文件对象
 *
 * Context: 进程上下文。
 * Return: 成功返回 0。
 */
static int platform_led_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &platform_led;
	return 0;
}

/**
 * platform_led_write() - 根据用户态数据控制 LED
 * @filp: 打开的文件对象
 * @buf: 用户态 LED 状态缓冲区
 * @count: 缓冲区长度
 * @offp: 文件偏移，本驱动不使用
 *
 * 用户态首字节必须为 LEDON 或 LEDOFF。
 * Context: 进程上下文，可以睡眠。
 * Return: 成功返回 1，参数非法返回 -EINVAL，复制失败返回 -EFAULT。
 */
static ssize_t platform_led_write(struct file *filp, const char __user *buf,
				   size_t count, loff_t *offp)
{
	struct platform_led *led = filp->private_data;
	u8 state;

	(void)offp;
	if (count < sizeof(state))
		return -EINVAL;
	if (copy_from_user(&state, buf, sizeof(state)))
		return -EFAULT;
	if (state != LEDON && state != LEDOFF)
		return -EINVAL;

	mutex_lock(&led->lock);
	platform_led_set(led, state);
	mutex_unlock(&led->lock);
	pr_info("%s: LED turned %s\n", PLATFORM_LED_NAME,
		state == LEDON ? "on" : "off");
	return sizeof(state);
}

static const struct file_operations platform_led_fops = {
	.owner = THIS_MODULE,
	.open = platform_led_open,
	.write = platform_led_write,
	.llseek = no_llseek,
};

/**
 * platform_led_probe() - 绑定设备树 LED platform 设备
 * @pdev: platform 设备
 *
 * 从设备树获取 led-gpio，申请 GPIO 并注册字符设备。
 * Context: 进程上下文，可以睡眠。
 * Return: 成功返回 0，否则返回负错误码。
 */
static int platform_led_probe(struct platform_device *pdev)
{
	enum of_gpio_flags flags;
	struct device_node *node = pdev->dev.of_node;
	int ret;
	int inactive_value;

	if (!node) {
		dev_err(&pdev->dev, "device tree node is missing\n");
		return -ENODEV;
	}
	platform_led.gpio = of_get_named_gpio_flags(node, PLATFORM_LED_PROPERTY,
						    0, &flags);
	if (!gpio_is_valid(platform_led.gpio)) {
		ret = platform_led.gpio < 0 ? platform_led.gpio : -EINVAL;
		dev_err(&pdev->dev, "failed to get %s: %d\n",
			PLATFORM_LED_PROPERTY, ret);
		return ret;
	}
	platform_led.active_low = flags & OF_GPIO_ACTIVE_LOW;
	ret = gpio_request(platform_led.gpio, PLATFORM_LED_NAME);
	if (ret) {
		dev_err(&pdev->dev, "failed to request GPIO %d: %d\n",
			platform_led.gpio, ret);
		return ret;
	}
	inactive_value = platform_led.active_low ? 1 : 0;
	ret = gpio_direction_output(platform_led.gpio, inactive_value);
	if (ret)
		goto err_free_gpio;

	mutex_init(&platform_led.lock);
	platform_led.state = LEDOFF;
	platform_led.pdev = pdev;
	ret = alloc_chrdev_region(&platform_led.devid, 0, PLATFORM_LED_COUNT,
				  PLATFORM_LED_NAME);
	if (ret)
		goto err_free_gpio;
	cdev_init(&platform_led.cdev, &platform_led_fops);
	platform_led.cdev.owner = THIS_MODULE;
	ret = cdev_add(&platform_led.cdev, platform_led.devid,
			       PLATFORM_LED_COUNT);
	if (ret)
		goto err_unregister;
	platform_led.class = class_create(THIS_MODULE, PLATFORM_LED_NAME);
	if (IS_ERR(platform_led.class)) {
		ret = PTR_ERR(platform_led.class);
		goto err_del_cdev;
	}
	platform_led.device = device_create(platform_led.class, &pdev->dev,
					    platform_led.devid, NULL,
					    PLATFORM_LED_NAME);
	if (IS_ERR(platform_led.device)) {
		ret = PTR_ERR(platform_led.device);
		goto err_destroy_class;
	}
	platform_set_drvdata(pdev, &platform_led);
	dev_info(&pdev->dev, "registered, GPIO=%d major=%u minor=%u\n",
		 platform_led.gpio, MAJOR(platform_led.devid), MINOR(platform_led.devid));
	return 0;

err_destroy_class:
	class_destroy(platform_led.class);
err_del_cdev:
	cdev_del(&platform_led.cdev);
err_unregister:
	unregister_chrdev_region(platform_led.devid, PLATFORM_LED_COUNT);
err_free_gpio:
	gpio_free(platform_led.gpio);
	return ret;
}

/**
 * platform_led_remove() - 移除 platform LED 设备
 * @pdev: platform 设备
 *
 * Context: 进程上下文，可以睡眠。
 */
static int platform_led_remove(struct platform_device *pdev)
{
	struct platform_led *led = platform_get_drvdata(pdev);

	device_destroy(led->class, led->devid);
	class_destroy(led->class);
	cdev_del(&led->cdev);
	unregister_chrdev_region(led->devid, PLATFORM_LED_COUNT);
	mutex_lock(&led->lock);
	platform_led_set(led, LEDOFF);
	mutex_unlock(&led->lock);
	gpio_free(led->gpio);
	platform_set_drvdata(pdev, NULL);
	dev_info(&pdev->dev, "unregistered\n");
	return 0;
}

static const struct of_device_id platform_led_of_match[] = {
	{ .compatible = "atkalpha-gpioled" },
	{ }
};
MODULE_DEVICE_TABLE(of, platform_led_of_match);

static struct platform_driver platform_led_driver = {
	.probe = platform_led_probe,
	.remove = platform_led_remove,
	.driver = {
		.name = PLATFORM_LED_NAME,
		.of_match_table = platform_led_of_match,
	},
};

module_platform_driver(platform_led_driver);

MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("Platform GPIO LED character driver");
MODULE_LICENSE("GPL");
