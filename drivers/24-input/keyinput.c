// SPDX-License-Identifier: GPL-2.0
/* 基于 Linux input 子系统的设备树按键驱动。 */

#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/timer.h>

#define KEYINPUT_NAME "keyinput"
#define KEYINPUT_NODE_PATH "/key"
#define KEYINPUT_GPIO_PROPERTY "key-gpio"
#define KEYINPUT_DEBOUNCE_MS 10U
#define KEYINPUT_KEY_CODE KEY_0

/**
 * struct keyinput_device - input 按键设备状态
 * @node: 设备树节点引用
 * @gpio: KEY0 GPIO 编号
 * @irq: KEY0 Linux IRQ 编号
 * @timer: 按键消抖定时器
 * @input: input 设备对象
 * @pressed: 当前按键是否处于按下状态
 * @gpio_requested: GPIO 是否已申请
 * @irq_requested: IRQ 是否已申请
 */
struct keyinput_device {
	struct device_node *node;
	int gpio;
	int irq;
	struct timer_list timer;
	struct input_dev *input;
	bool pressed;
	bool gpio_requested;
	bool irq_requested;
};

static struct keyinput_device keyinput;

/**
 * keyinput_timer_callback() - 消抖后上报 KEY0 状态
 * @data: 指向 struct keyinput_device 的无符号长整型参数
 *
 * Context: timer softirq 上下文，不得睡眠。
 */
static void keyinput_timer_callback(unsigned long data)
{
	struct keyinput_device *dev = (struct keyinput_device *)data;
	bool pressed = gpio_get_value(dev->gpio) == 0;

	if (pressed == dev->pressed)
		return;
	dev->pressed = pressed;
	input_report_key(dev->input, KEYINPUT_KEY_CODE, pressed);
	input_sync(dev->input);
	pr_info("%s: KEY0 %s\n", KEYINPUT_NAME, pressed ? "pressed" : "released");
}

/**
 * keyinput_irq_handler() - 响应 KEY0 边沿并启动消抖定时器
 * @irq: Linux IRQ 编号
 * @data: 指向 struct keyinput_device
 *
 * Context: hardirq 上下文，不得睡眠。
 * Return: 始终返回 IRQ_HANDLED。
 */
static irqreturn_t keyinput_irq_handler(int irq, void *data)
{
	struct keyinput_device *dev = data;

	(void)irq;
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(KEYINPUT_DEBOUNCE_MS));
	return IRQ_HANDLED;
}

/**
 * keyinput_probe() - 初始化设备树按键和 input 设备
 * @pdev: platform 设备
 *
 * Context: 进程上下文，可以睡眠。
 * Return: 成功返回 0，否则返回负错误码。
 */
static int keyinput_probe(struct platform_device *pdev)
{
	struct keyinput_device *dev = &keyinput;
	int ret;

	dev->node = of_find_node_by_path(KEYINPUT_NODE_PATH);
	if (!dev->node)
		return -ENODEV;
	dev->gpio = of_get_named_gpio(dev->node, KEYINPUT_GPIO_PROPERTY, 0);
	if (!gpio_is_valid(dev->gpio)) {
		ret = dev->gpio < 0 ? dev->gpio : -EINVAL;
		goto err_node;
	}
	ret = gpio_request(dev->gpio, KEYINPUT_NAME);
	if (ret)
		goto err_node;
	dev->gpio_requested = true;
	ret = gpio_direction_input(dev->gpio);
	if (ret)
		goto err_gpio;
	dev->irq = irq_of_parse_and_map(dev->node, 0);
	if (!dev->irq) {
		ret = -EINVAL;
		goto err_gpio;
	}
	setup_timer(&dev->timer, keyinput_timer_callback, (unsigned long)dev);
	ret = request_irq(dev->irq, keyinput_irq_handler,
			  IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			  KEYINPUT_NAME, dev);
	if (ret)
		goto err_gpio;
	dev->irq_requested = true;
	dev->input = input_allocate_device();
	if (!dev->input) {
		ret = -ENOMEM;
		goto err_irq;
	}
	dev->input->name = KEYINPUT_NAME;
	input_set_capability(dev->input, EV_KEY, KEYINPUT_KEY_CODE);
	ret = input_register_device(dev->input);
	if (ret)
		goto err_input;
	platform_set_drvdata(pdev, dev);
	dev_info(&pdev->dev, "registered, GPIO=%d IRQ=%d input=%s\n",
		dev->gpio, dev->irq, dev->input->name);
	return 0;

err_input:
	input_free_device(dev->input);
	dev->input = NULL;
err_irq:
	free_irq(dev->irq, dev);
	dev->irq_requested = false;
	del_timer_sync(&dev->timer);
err_gpio:
	gpio_free(dev->gpio);
	dev->gpio_requested = false;
err_node:
	of_node_put(dev->node);
	dev->node = NULL;
	return ret;
}

/**
 * keyinput_remove() - 注销 input 按键设备并释放资源
 * @pdev: platform 设备
 *
 * Context: 进程上下文，可以睡眠。
 * Return: 成功返回 0。
 */
static int keyinput_remove(struct platform_device *pdev)
{
	struct keyinput_device *dev = platform_get_drvdata(pdev);

	if (dev->irq_requested) {
		free_irq(dev->irq, dev);
		dev->irq_requested = false;
	}
	del_timer_sync(&dev->timer);
	if (dev->input) {
		input_unregister_device(dev->input);
		dev->input = NULL;
	}
	if (dev->gpio_requested) {
		gpio_free(dev->gpio);
		dev->gpio_requested = false;
	}
	of_node_put(dev->node);
	dev->node = NULL;
	platform_set_drvdata(pdev, NULL);
	dev_info(&pdev->dev, "unregistered\n");
	return 0;
}

static const struct of_device_id keyinput_of_match[] = {
	{ .compatible = "atkalpha-key" },
	{ }
};
MODULE_DEVICE_TABLE(of, keyinput_of_match);

static struct platform_driver keyinput_driver = {
	.probe = keyinput_probe,
	.remove = keyinput_remove,
	.driver = {
		.name = KEYINPUT_NAME,
		.of_match_table = keyinput_of_match,
	},
};

module_platform_driver(keyinput_driver);

MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("Device-tree GPIO key input driver");
MODULE_LICENSE("GPL");
