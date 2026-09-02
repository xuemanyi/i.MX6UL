/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LED_FOPS_H
#define LED_FOPS_H

#include <linux/fs.h>
#include <linux/uaccess.h>

#include "imx6ull_led_hw.h"

static int led_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t led_read(struct file *filp, char __user *buf,
                        size_t count, loff_t *ppos)
{
    u8 state;

    if (!count)
        return 0;

    if (*ppos != 0)
        return 0;

    state = imx6ull_led_get();

    if (copy_to_user(buf, &state, sizeof(state)))
        return -EFAULT;

    *ppos += sizeof(state);

    return sizeof(state);
}

static ssize_t led_write(struct file *filp, const char __user *buf,
                         size_t count, loff_t *ppos)
{
    u8 state;
    int ret;

    if (!count)
        return 0;

    if (copy_from_user(&state, buf, sizeof(state)))
        return -EFAULT;

    ret = imx6ull_led_set(state);
    if (ret)
        return ret;

    return sizeof(state);
}

static int led_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static const struct file_operations led_fops = {
    .owner          = THIS_MODULE,
    .open           = led_open,
    .read           = led_read,
    .write          = led_write,
    .release        = led_release,
    .llseek         = no_llseek,
};

#endif