// SPDX-License-Identifier: GPL-2.0
/*
 * basic_consumer.c - Basic exported-symbol consumer module
 */

#include <linux/init.h>
#include <linux/module.h>

#include "basic_api.h"

enum basic_operation {
    BASIC_OPERATION_ADD,
    BASIC_OPERATION_SUBTRACT,
};

static long value_a = 1;
module_param(value_a, long, 0444);
MODULE_PARM_DESC(value_a, "First operand");

static long value_b = 1;
module_param(value_b, long, 0444);
MODULE_PARM_DESC(value_b, "Second operand");

static char *operation = "add";
module_param(operation, charp, 0444);
MODULE_PARM_DESC(operation, "Operation: add or subtract");

static int parse_operation(enum basic_operation *selected)
{
    if (!operation)
        return -EINVAL;

    if (!strcmp(operation, "add")) {
        *selected = BASIC_OPERATION_ADD;
        return 0;
    }

    if (!strcmp(operation, "subtract")) {
        *selected = BASIC_OPERATION_SUBTRACT;
        return 0;
    }

        return -EINVAL;
}

static int __init basic_consumer_init(void)
{
    enum basic_operation selected;
    long result;
    int ret;

    ret = parse_operation(&selected);
    if (ret) {
        pr_err("basic_consumer: unsupported operation \"%s\"\n",
                operation ?: "<null>");
        return ret;
    }

    switch (selected) {
    case BASIC_OPERATION_ADD:
        result = basic_add(value_a, value_b);
        break;

    case BASIC_OPERATION_SUBTRACT:
        result = basic_subtract(value_a, value_b);
        break;

    default:
        return -EINVAL;
    }

    pr_info("basic_consumer: %ld %s %ld = %ld\n",
        value_a, operation, value_b, result);

    return 0;
}

static void __exit basic_consumer_exit(void)
{
    pr_info("basic_consumer: unloaded\n");
}

module_init(basic_consumer_init);
module_exit(basic_consumer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("Basic exported-symbol consumer module");
MODULE_VERSION("1.0");
