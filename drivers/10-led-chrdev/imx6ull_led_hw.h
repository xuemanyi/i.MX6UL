/* SPDX-License-Identifier: GPL-2.0 */
#ifndef IMX6ULL_LED_HW_H
#define IMX6ULL_LED_HW_H

#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/types.h>

#define LED_OFF                         0
#define LED_ON                          1

#define CCM_CCGR1_BASE                  0x020c406c
#define SW_MUX_GPIO1_IO03_BASE          0x020e0068
#define SW_PAD_GPIO1_IO03_BASE          0x020e02f4

#define GPIO1_BASE                      0x0209c000
#define GPIO1_REG_SIZE                  0x20
#define GPIO1_DR_OFFSET                 0x00
#define GPIO1_GDIR_OFFSET               0x04

#define GPIO1_IO03_MASK                 BIT(3)

#define CCM_CCGR1_GPIO1_SHIFT           26
#define CCM_CCGR1_GPIO1_MASK            (0x3U << CCM_CCGR1_GPIO1_SHIFT)
#define CCM_CCGR1_GPIO1_ENABLE          (0x3U << CCM_CCGR1_GPIO1_SHIFT)

#define GPIO1_IO03_MUX_MODE             5
#define GPIO1_IO03_PAD_CONFIG           0x10b0

static void __iomem *ccm_ccgr1;
static void __iomem *sw_mux_gpio1_io03;
static void __iomem *sw_pad_gpio1_io03;
static void __iomem *gpio1_base;

static DEFINE_MUTEX(led_hw_lock);

static inline void __iomem *gpio1_reg(u32 offset)
{
    return (u8 __iomem *)gpio1_base + offset;
}

static void imx6ull_led_unmap(void)
{
    if (gpio1_base) {
        iounmap(gpio1_base);
        gpio1_base = NULL;
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

static int imx6ull_led_set(u8 state)
{
    void __iomem *gpio1_dr;
    u32 value;

    if (state != LED_ON && state != LED_OFF)
        return -EINVAL;

    if (!gpio1_base)
        return -ENODEV;

    gpio1_dr = gpio1_reg(GPIO1_DR_OFFSET);

    mutex_lock(&led_hw_lock);

    value = readl(gpio1_dr);

    if (state == LED_ON)
        value &= ~GPIO1_IO03_MASK;
    else
        value |= GPIO1_IO03_MASK;

    writel(value, gpio1_dr);

    /*
     * 回读可以确认写操作到达设备，同时冲刷posted write。
     */
    readl(gpio1_dr);

    mutex_unlock(&led_hw_lock);

    return 0;
}

static u8 imx6ull_led_get(void)
{
    void __iomem *gpio1_dr;
    u32 value;
    u8 state;

    gpio1_dr = gpio1_reg(GPIO1_DR_OFFSET);

    mutex_lock(&led_hw_lock);

    value = readl(gpio1_dr);
    state = value & GPIO1_IO03_MASK ? LED_OFF : LED_ON;

    mutex_unlock(&led_hw_lock);

    return state;
}

static int imx6ull_led_hw_init(void)
{
    void __iomem *gpio1_dr;
    void __iomem *gpio1_gdir;
    u32 value;

    ccm_ccgr1 = ioremap(CCM_CCGR1_BASE, sizeof(u32));
    if (!ccm_ccgr1)
        goto err_unmap;

    sw_mux_gpio1_io03 =
        ioremap(SW_MUX_GPIO1_IO03_BASE, sizeof(u32));
    if (!sw_mux_gpio1_io03)
        goto err_unmap;

    sw_pad_gpio1_io03 =
        ioremap(SW_PAD_GPIO1_IO03_BASE, sizeof(u32));
    if (!sw_pad_gpio1_io03)
        goto err_unmap;

    gpio1_base = ioremap(GPIO1_BASE, GPIO1_REG_SIZE);
    if (!gpio1_base)
        goto err_unmap;

    gpio1_dr = gpio1_reg(GPIO1_DR_OFFSET);
    gpio1_gdir = gpio1_reg(GPIO1_GDIR_OFFSET);

    /*
     * 使能GPIO1时钟。
     */
    value = readl(ccm_ccgr1);
    value &= ~CCM_CCGR1_GPIO1_MASK;
    value |= CCM_CCGR1_GPIO1_ENABLE;
    writel(value, ccm_ccgr1);

    /*
     * GPIO1_IO03选择ALT5，即GPIO功能。
     */
    writel(GPIO1_IO03_MUX_MODE, sw_mux_gpio1_io03);
    writel(GPIO1_IO03_PAD_CONFIG, sw_pad_gpio1_io03);

    /*
     * 先设置默认输出高电平，避免切换输出方向时LED闪烁。
     * LED低电平点亮，因此高电平为关闭状态。
     */
    value = readl(gpio1_dr);
    value |= GPIO1_IO03_MASK;
    writel(value, gpio1_dr);

    /*
     * GPIO1_IO03设置为输出。
     */
    value = readl(gpio1_gdir);
    value |= GPIO1_IO03_MASK;
    writel(value, gpio1_gdir);

    return 0;

err_unmap:
    imx6ull_led_unmap();

    return -ENOMEM;
}

static void imx6ull_led_hw_exit(void)
{
    if (gpio1_base)
        imx6ull_led_set(LED_OFF);

    imx6ull_led_unmap();
}

#endif