#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>

#include <asm/uaccess.h>
#include <asm/io.h>


/* Device information */
#define LED_MAJOR 200
#define LED_NAME  "led"

/* LED control commands */
#define LED_OFF   0
#define LED_ON    1

/* Physical register base addresses */
#define CCM_CCGR1_BASE         0x020C406C
#define SW_MUX_GPIO1_IO03_BASE 0x020E0068
#define SW_PAD_GPIO1_IO03_BASE 0x020E02F4
#define GPIO1_DR_BASE          0x0209C000
#define GPIO1_GDIR_BASE        0x0209C004

/* Virtual address pointers after ioremap */
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

/**
 * led_switch() - Turn LED on or off
 * @sta: LED_ON or LED_OFF
 *
 * This function controls the LED state by writing
 * to the GPIO data register.
 */
static void led_switch(u8 sta)
{
	u32 val;

	val = readl(GPIO1_DR);

	if (sta == LED_ON)
		val &= ~(1 << 3);   /* LED ON (low level) */
	else
		val |= (1 << 3);    /* LED OFF (high level) */

	writel(val, GPIO1_DR);
}

/**
 * led_open() - Open LED device
 * @inode: inode pointer
 * @filp:  file pointer
 *
 * Return: 0 on success
 */
static int led_open(struct inode *inode, struct file *filp)
{
	return 0;
}

/**
 * led_read() - Read from LED device
 * @filp: file pointer
 * @buf:  user buffer
 * @cnt:  bytes to read
 * @offt: file offset
 *
 * This driver does not support read operation.
 *
 * Return: 0
 */
static ssize_t led_read(struct file *filp,
			char __user *buf,
			size_t cnt,
			loff_t *offt)
{
	return 0;
}

/**
 * led_write() - Write to LED device
 * @filp: file pointer
 * @buf:  user buffer
 * @cnt:  bytes to write
 * @offt: file offset
 *
 * User writes one byte to control LED state:
 * 0 - LED_OFF
 * 1 - LED_ON
 *
 * Return: number of bytes written or negative error code
 */
static ssize_t led_write(struct file *filp,
			 const char __user *buf,
			 size_t cnt,
			 loff_t *offt)
{
	u8 databuf;
	int ret;

	ret = copy_from_user(&databuf, buf, 1);
	if (ret) {
		pr_err("led: failed to copy data from user\n");
		return -EFAULT;
	}

	if (databuf == LED_ON)
		led_switch(LED_ON);
	else
		led_switch(LED_OFF);

	return cnt;
}

/**
 * led_release() - Release LED device
 * @inode: inode pointer
 * @filp:  file pointer
 *
 * Return: 0
 */
static int led_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* File operations structure */
static const struct file_operations led_fops = {
	.owner   = THIS_MODULE,
	.open    = led_open,
	.read    = led_read,
	.write   = led_write,
	.release = led_release,
};

/**
 * led_init() - LED driver initialization
 *
 * Maps hardware registers, configures GPIO,
 * and registers character device.
 *
 * Return: 0 on success, negative error code on failure
 */
static int __init led_init(void)
{
	u32 val;
	int ret;

	/* Map registers */
	IMX6U_CCM_CCGR1     = ioremap(CCM_CCGR1_BASE, 4);
	SW_MUX_GPIO1_IO03   = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
	SW_PAD_GPIO1_IO03   = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
	GPIO1_DR            = ioremap(GPIO1_DR_BASE, 4);
	GPIO1_GDIR          = ioremap(GPIO1_GDIR_BASE, 4);

	/* Enable GPIO1 clock */
	val = readl(IMX6U_CCM_CCGR1);
	val &= ~(3 << 26);
	val |=  (3 << 26);
	writel(val, IMX6U_CCM_CCGR1);

	/* Configure pin mux and pad */
	writel(5, SW_MUX_GPIO1_IO03);
	writel(0x10B0, SW_PAD_GPIO1_IO03);

	/* Configure GPIO as output */
	val = readl(GPIO1_GDIR);
	val |= (1 << 3);
	writel(val, GPIO1_GDIR);

	/* Turn off LED by default */
	val = readl(GPIO1_DR);
	val |= (1 << 3);
	writel(val, GPIO1_DR);

	/* Register character device */
	ret = register_chrdev(LED_MAJOR, LED_NAME, &led_fops);
	if (ret < 0) {
		pr_err("led: failed to register char device\n");
		return ret;
	}

	pr_info("led: driver initialized\n");
	return 0;
}

/**
 * led_exit() - LED driver exit
 *
 * Unmaps registers and unregisters device.
 */
static void __exit led_exit(void)
{
	iounmap(IMX6U_CCM_CCGR1);
	iounmap(SW_MUX_GPIO1_IO03);
	iounmap(SW_PAD_GPIO1_IO03);
	iounmap(GPIO1_DR);
	iounmap(GPIO1_GDIR);

	unregister_chrdev(LED_MAJOR, LED_NAME);

	pr_info("led: driver removed\n");
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("snowclad");
MODULE_DESCRIPTION("Simple LED character device driver for i.MX6UL");
