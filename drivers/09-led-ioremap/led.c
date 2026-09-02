// SPDX-License-Identifier: GPL-2.0
/*
 * i.MX6U/i.MX6ULL GPIO1_IO03 LED character driver.
 *
 * This driver directly accesses SoC registers and is intended for learning.
 * Production drivers should normally use device tree, pinctrl, GPIO descriptor
 * and the Linux LED subsystem.
 */

#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#define LED_MAJOR                       200
#define LED_NAME                        "led"

#define LEDOFF                          0
#define LEDON                           1

/* i.MX6U/i.MX6ULL physical register addresses. */
#define CCM_CCGR1_BASE                  0x020c406c
#define SW_MUX_GPIO1_IO03_BASE          0x020e0068
#define SW_PAD_GPIO1_IO03_BASE          0x020e02f4
#define GPIO1_DR_BASE                   0x0209c000
#define GPIO1_GDIR_BASE                 0x0209c004

#define REGISTER_SIZE                   sizeof(u32)

#define GPIO1_IO03_BIT                  3
#define GPIO1_IO03_MASK                 BIT(GPIO1_IO03_BIT)

#define CCM_CCGR1_GPIO1_SHIFT           26
#define CCM_CCGR1_GPIO1_MASK            (0x3U << CCM_CCGR1_GPIO1_SHIFT)
#define CCM_CCGR1_GPIO1_ENABLE          (0x3U << CCM_CCGR1_GPIO1_SHIFT)

#define GPIO1_IO03_MUX_MODE             5
#define GPIO1_IO03_PAD_CONFIG           0x10b0

static void __iomem *ccm_ccgr1;
static void __iomem *sw_mux_gpio1_io03;
static void __iomem *sw_pad_gpio1_io03;
static void __iomem *gpio1_dr;
static void __iomem *gpio1_gdir;

static DEFINE_MUTEX(led_lock);

/*
 * The LED is active-low:
 *
 * GPIO output 0: LED on
 * GPIO output 1: LED off
 *
 * The caller must hold led_lock.
 */
static void led_set_state(u8 state)
{
    u32 before;
    u32 after;
    u32 gdir;

    before = readl(gpio1_dr);

    if (state == LEDON)
        writel(before & ~GPIO1_IO03_MASK, gpio1_dr);
    else
        writel(before | GPIO1_IO03_MASK, gpio1_dr);

    /*
     * 回读还可以冲刷可能存在的posted write，
     * 同时用于确认寄存器是否真正改变。
     */
    after = readl(gpio1_dr);
    gdir = readl(gpio1_gdir);

    pr_info("%s: state=%u, dr=0x%08x->0x%08x, gdir=0x%08x\n",
            LED_NAME, state, before, after, gdir);
}

static int led_open(struct inode *inode, struct file *filp)
{
    pr_info("%s: open, major=%u minor=%u, f_op=%px, write=%ps\n",
            LED_NAME, imajor(inode), iminor(inode),
            filp->f_op, filp->f_op->write);

    return 0;
}

static ssize_t led_read(struct file *filp, char __user *buf,
                        size_t count, loff_t *offp)
{
    u8 state;
    u32 value;

    if (!count)
        return 0;

    /*
     * The LED state occupies one byte. After one successful read,
     * subsequent reads return EOF until the device is reopened.
     */
    if (*offp != 0)
        return 0;

    mutex_lock(&led_lock);

    value = readl(gpio1_dr);
    if (value & GPIO1_IO03_MASK)
        state = LEDOFF;
    else
        state = LEDON;

    mutex_unlock(&led_lock);

    if (copy_to_user(buf, &state, sizeof(state)))
        return -EFAULT;

    *offp += sizeof(state);

    return sizeof(state);
}

static ssize_t led_write(struct file *filp, const char __user *buf,
                         size_t count, loff_t *offp)
{
    unsigned long not_copied;
    u8 state;

    pr_info("%s: led_write entered, count=%zu\n",
            LED_NAME, count);

    if (count < sizeof(state)) {
        pr_err("%s: invalid write count=%zu\n",
               LED_NAME, count);
        return -EINVAL;
    }

    not_copied = copy_from_user(&state, buf, sizeof(state));
    if (not_copied) {
        pr_err("%s: copy_from_user failed, not_copied=%lu\n",
               LED_NAME, not_copied);
        return -EFAULT;
    }

    pr_info("%s: received state=%u, raw=0x%02x\n",
            LED_NAME, state, state);

    if (state != LEDON && state != LEDOFF) {
        pr_err("%s: invalid LED state=%u\n",
               LED_NAME, state);
        return -EINVAL;
    }

    mutex_lock(&led_lock);
    led_set_state(state);
    mutex_unlock(&led_lock);

    pr_info("%s: led_write completed, state=%u\n",
            LED_NAME, state);

    return sizeof(state);
}

static int led_release(struct inode *inode, struct file *filp)
{
    pr_info("%s: release, major=%u minor=%u\n",
            LED_NAME, imajor(inode), iminor(inode));

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

static void led_unmap_registers(void)
{
    if (gpio1_gdir) {
        iounmap(gpio1_gdir);
        gpio1_gdir = NULL;
    }

    if (gpio1_dr) {
        iounmap(gpio1_dr);
        gpio1_dr = NULL;
    }

    if (sw_pad_gpio1_io03) {
        iounmap(sw_pad_gpio1_io03);
        sw_pad_gpio1_io03 = NULL;
    }

    if (sw_mux_gpio1_io03) {
        iounmap(sw_mux_gpio1_io03);
        sw_mux_gpio1_io03 = NULL;
    }

    if (ccm_ccgr1) {
        iounmap(ccm_ccgr1);
        ccm_ccgr1 = NULL;
    }
}

static int led_map_registers(void)
{
    ccm_ccgr1 = ioremap(CCM_CCGR1_BASE, REGISTER_SIZE);
    if (!ccm_ccgr1)
        goto err_unmap;

    sw_mux_gpio1_io03 =
        ioremap(SW_MUX_GPIO1_IO03_BASE, REGISTER_SIZE);
    if (!sw_mux_gpio1_io03)
        goto err_unmap;

    sw_pad_gpio1_io03 =
        ioremap(SW_PAD_GPIO1_IO03_BASE, REGISTER_SIZE);
    if (!sw_pad_gpio1_io03)
        goto err_unmap;

    gpio1_dr = ioremap(GPIO1_DR_BASE, REGISTER_SIZE);
    if (!gpio1_dr)
        goto err_unmap;

    gpio1_gdir = ioremap(GPIO1_GDIR_BASE, REGISTER_SIZE);
    if (!gpio1_gdir)
        goto err_unmap;

    return 0;

err_unmap:
    led_unmap_registers();

    return -ENOMEM;
}

static void led_hw_init(void)
{
    u32 value;

    /*
     * Enable the GPIO1 clock.
     *
     * CCM_CCGR1[27:26] = 0b11:
     * clock enabled in all modes.
     */
    value = readl(ccm_ccgr1);
    value &= ~CCM_CCGR1_GPIO1_MASK;
    value |= CCM_CCGR1_GPIO1_ENABLE;
    writel(value, ccm_ccgr1);

    /*
     * Set GPIO1_IO03 mux mode to ALT5, selecting GPIO1_IO03.
     */
    writel(GPIO1_IO03_MUX_MODE, sw_mux_gpio1_io03);

    /*
     * Configure GPIO1_IO03 electrical pad attributes.
     */
    writel(GPIO1_IO03_PAD_CONFIG, sw_pad_gpio1_io03);

    /*
     * Set DR before GDIR.
     *
     * The LED is active-low, so writing 1 turns it off. Setting the
     * inactive level before changing the pin to output avoids a brief flash.
     */
    value = readl(gpio1_dr);
    value |= GPIO1_IO03_MASK;
    writel(value, gpio1_dr);

    /*
     * Set GPIO1_IO03 as output.
     */
    value = readl(gpio1_gdir);
    value |= GPIO1_IO03_MASK;
    writel(value, gpio1_gdir);
}

static int __init led_init(void)
{
    int ret;

    ret = led_map_registers();
    if (ret) {
        pr_err("%s: failed to map registers: %d\n",
               LED_NAME, ret);
        return ret;
    }

    ret = register_chrdev(LED_MAJOR, LED_NAME, &led_fops);
    if (ret < 0) {
        pr_err("%s: failed to register major %d: %d\n",
               LED_NAME, LED_MAJOR, ret);
        led_unmap_registers();
        return ret;
    }

    led_hw_init();

    pr_info("%s: registered, major=%d minor=0\n",
            LED_NAME, LED_MAJOR);

    return 0;
}

static void __exit led_exit(void)
{
    /*
     * Remove the character-device entry before releasing hardware
     * resources so no new operation can enter the driver.
     */
    unregister_chrdev(LED_MAJOR, LED_NAME);

    mutex_lock(&led_lock);
    led_set_state(LEDOFF);
    mutex_unlock(&led_lock);

    led_unmap_registers();

    pr_info("%s: unregistered\n", LED_NAME);
}

module_init(led_init);
module_exit(led_exit);

MODULE_AUTHOR("sucre");
MODULE_DESCRIPTION("i.MX6U/i.MX6ULL GPIO1_IO03 LED driver");
MODULE_LICENSE("GPL");