// SPDX-License-Identifier: GPL-2.0

#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>

#include "led_fops.h"

#define LED_DRIVER_NAME             "led_misc"
#define LED_NODE_NAME               "led_misc"

static struct miscdevice led_misc_device = {
    .minor      = MISC_DYNAMIC_MINOR,
    .name       = LED_NODE_NAME,
    .fops       = &led_fops,
    .mode       = 0666,
};

static int __init led_misc_init(void)
{
    int ret;

    ret = imx6ull_led_hw_init();
    if (ret) {
        pr_err("%s: hardware initialization failed: %d\n",
               LED_DRIVER_NAME, ret);
        return ret;
    }

    ret = misc_register(&led_misc_device);
    if (ret) {
        pr_err("%s: misc_register failed: %d\n",
               LED_DRIVER_NAME, ret);
        imx6ull_led_hw_exit();
        return ret;
    }

    pr_info("%s: registered, major=%d minor=%d\n",
            LED_DRIVER_NAME, MISC_MAJOR, led_misc_device.minor);

    return 0;
}

static void __exit led_misc_exit(void)
{
    misc_deregister(&led_misc_device);
    imx6ull_led_hw_exit();

    pr_info("%s: unregistered\n", LED_DRIVER_NAME);
}

module_init(led_misc_init);
module_exit(led_misc_exit);

MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("LED driver using miscdevice");
MODULE_LICENSE("GPL");
