// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>

static int __init chardevbase_init(void)
{
	pr_info("chardevbase: module loaded\n");
	return 0;
}

static void __exit chardevbase_exit(void)
{
	pr_info("chardevbase: module unloaded\n");
}

module_init(chardevbase_init);
module_exit(chardevbase_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("Minimal external kernel module example");
