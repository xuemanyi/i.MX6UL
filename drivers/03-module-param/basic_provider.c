// SPDX-License-Identifier: GPL-2.0
/*
 * basic_provider.c - Basic exported-symbol provider module
 */

#include <linux/init.h>
#include <linux/module.h>

#include "basic_api.h"

long basic_add(long a, long b)
{
    return a + b;
}
EXPORT_SYMBOL_GPL(basic_add);

long basic_subtract(long a, long b)
{
    return a - b;
}
EXPORT_SYMBOL_GPL(basic_subtract);

static int __init basic_provider_init(void)
{
    pr_info("basic_provider: loaded\n");

    return 0;
}

static void __exit basic_provider_exit(void)
{
    pr_info("basic_provider: unloaded\n");
}

module_init(basic_provider_init);
module_exit(basic_provider_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sucre");
MODULE_DESCRIPTION("Basic exported-symbol provider module");
MODULE_VERSION("1.0");