// SPDX-License-Identifier: GPL-2.0

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>

#include "led_fops.h"

#define LED_FIRST_MINOR             0
#define LED_DEVICE_COUNT            1

#define LED_DRIVER_NAME             "led_cdev_dynamic"
#define LED_CLASS_NAME              "led_cdev_dynamic_class"
#define LED_NODE_NAME               "led_cdev_dynamic"

static dev_t led_dev;
static struct cdev led_cdev;
static struct class *led_class;
static struct device *led_device;

static int __init led_cdev_dynamic_init(void)
{
    int ret;

    ret = imx6ull_led_hw_init();
    if (ret) {
        pr_err("%s: hardware initialization failed: %d\n",
               LED_DRIVER_NAME, ret);
        return ret;
    }

    ret = alloc_chrdev_region(&led_dev, LED_FIRST_MINOR,
                              LED_DEVICE_COUNT, LED_DRIVER_NAME);
    if (ret) {
        pr_err("%s: alloc_chrdev_region failed: %d\n",
               LED_DRIVER_NAME, ret);
        goto err_hw;
    }

    cdev_init(&led_cdev, &led_fops);
    led_cdev.owner = THIS_MODULE;

    ret = cdev_add(&led_cdev, led_dev, LED_DEVICE_COUNT);
    if (ret) {
        pr_err("%s: cdev_add failed: %d\n",
               LED_DRIVER_NAME, ret);
        goto err_unregister;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    led_class = class_create(LED_CLASS_NAME);
#else
    led_class = class_create(THIS_MODULE, LED_CLASS_NAME);
#endif
    if (IS_ERR(led_class)) {
        ret = PTR_ERR(led_class);
        led_class = NULL;
        pr_err("%s: class_create failed: %d\n",
               LED_DRIVER_NAME, ret);
        goto err_cdev;
    }

    led_device = device_create(led_class, NULL, led_dev, NULL,
                               LED_NODE_NAME);
    if (IS_ERR(led_device)) {
        ret = PTR_ERR(led_device);
        led_device = NULL;
        pr_err("%s: device_create failed: %d\n",
               LED_DRIVER_NAME, ret);
        goto err_class;
    }

    pr_info("%s: registered, major=%u minor=%u\n",
            LED_DRIVER_NAME, MAJOR(led_dev), MINOR(led_dev));

    return 0;

err_class:
    class_destroy(led_class);
    led_class = NULL;

err_cdev:
    cdev_del(&led_cdev);

err_unregister:
    unregister_chrdev_region(led_dev, LED_DEVICE_COUNT);

err_hw:
    imx6ull_led_hw_exit();

    return ret;
}

static void __exit led_cdev_dynamic_exit(void)
{
    device_destroy(led_class, led_dev);
    class_destroy(led_class);
    cdev_del(&led_cdev);
    unregister_chrdev_region(led_dev, LED_DEVICE_COUNT);
    imx6ull_led_hw_exit();

    pr_info("%s: unregistered\n", LED_DRIVER_NAME);
}

module_init(led_cdev_dynamic_init);
module_exit(led_cdev_dynamic_exit);

MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("LED driver using dynamic device number and cdev");
MODULE_LICENSE("GPL");
