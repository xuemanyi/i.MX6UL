// SPDX-License-Identifier: GPL-2.0

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>

#include "led_fops.h"

#define LED_MAJOR                   200
#define LED_DRIVER_NAME             "led_register_chrdev"

static int __init led_register_chrdev_init(void)
{
    int ret;

    ret = imx6ull_led_hw_init();
    if (ret) {
        pr_err("%s: hardware initialization failed: %d\n",
               LED_DRIVER_NAME, ret);
        return ret;
    }

    ret = register_chrdev(LED_MAJOR, LED_DRIVER_NAME, &led_fops);
    if (ret < 0) {
        pr_err("%s: register_chrdev failed: %d\n",
               LED_DRIVER_NAME, ret);
        imx6ull_led_hw_exit();
        return ret;
    }

    pr_info("%s: registered, major=%d minor=0\n",
            LED_DRIVER_NAME, LED_MAJOR);

    return 0;
}

static void __exit led_register_chrdev_exit(void)
{
    unregister_chrdev(LED_MAJOR, LED_DRIVER_NAME);
    imx6ull_led_hw_exit();

    pr_info("%s: unregistered\n", LED_DRIVER_NAME);
}

module_init(led_register_chrdev_init);
module_exit(led_register_chrdev_exit);

MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("LED driver using register_chrdev");
MODULE_LICENSE("GPL");
