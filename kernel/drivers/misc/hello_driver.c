// SPDX-License-Identifier: GPL-2.0
/*
 * hello_driver.c - Simple in-tree Hello World driver example
 */

#include <linux/init.h>
#include <linux/module.h>

static int __init hello_driver_init(void)
{
        pr_info("hello_driver: Hello, World\n");

        return 0;
}

static void __exit hello_driver_exit(void)
{
        pr_info("hello_driver: Goodbye, World\n");
}

module_init(hello_driver_init);
module_exit(hello_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sucre");
MODULE_DESCRIPTION("Simple in-tree Hello World driver");
MODULE_VERSION("1.0");