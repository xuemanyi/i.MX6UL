// SPDX-License-Identifier: GPL-2.0
/*
 * hello.c - Simple Hello World kernel module
 *
 * The module prints a message when loaded and another message when unloaded.
 */

#include <linux/init.h>
#include <linux/module.h>

static int __init hello_init(void)
{
        pr_info("hello: Hello, World\n");

        return 0;
}

static void __exit hello_exit(void)
{
        pr_info("hello: Goodbye, World\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("A simple Hello World kernel module");
MODULE_VERSION("1.0");
