// SPDX-License-Identifier: GPL-2.0
#include <linux/printk.h>

#include "multi_file.h"

void multi_file_print_status(void)
{
	pr_info("multi_file: module loaded\n");
}
