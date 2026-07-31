// SPDX-License-Identifier: GPL-2.0
/*
 * hello_param.c - Hello World module with parameters
 */

#include <linux/init.h>
#include <linux/module.h>

static char *name = "World";
module_param(name, charp, 0644);
MODULE_PARM_DESC(name, "Name printed by the module");

static unsigned int count = 1;
module_param(count, uint, 0644);
MODULE_PARM_DESC(count, "Number of greeting messages");

static int __init hello_param_init(void)
{
    unsigned int i;

    if (!count) {
        pr_err("hello_param: count must be greater than zero\n");
        return -EINVAL;
    }

    for (i = 0; i < count; i++) {
        pr_info("hello_param: Hello, %s\n", name);
    }

    return 0;
}

static void __exit hello_param_exit(void)
{
    pr_info("hello_param: Goodbye, %s\n", name);
}

module_init(hello_param_init);
module_exit(hello_param_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sucre");
MODULE_DESCRIPTION("Hello World kernel module with parameters");
MODULE_VERSION("1.0");