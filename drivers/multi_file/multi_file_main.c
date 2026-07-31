// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>

#include "multi_file.h"

static int __init multi_file_init(void)
{
	multi_file_print_status();
	return 0;
}

static void __exit multi_file_exit(void)
{
	pr_info("multi_file: module unloaded\n");
}

module_init(multi_file_init);
module_exit(multi_file_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sucre");
MODULE_DESCRIPTION("Multi-file external kernel module example");
