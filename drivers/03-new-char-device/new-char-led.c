#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>

#include <asm/uaccess.h>
#include <asm/io.h>

/* Device information */
#define NEW_CHR_LED_CNT   1
#define NEW_CHR_LED_NAME  "newchrled"

/* LED state */
#define LED_OFF           0
#define LED_ON            1

/* Physical register base addresses */
#define CCM_CCGR1_BASE         0x020C406C
#define SW_MUX_GPIO1_IO03_BASE 0x020E0068
#define SW_PAD_GPIO1_IO03_BASE 0x020E02F4
#define GPIO1_DR_BASE          0x0209C000
#define GPIO1_GDIR_BASE        0x0209C004

/* Mapped register base */
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

/**
 * struct newchrled_dev - LED character device
 */
struct newchrled_dev {
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int major;
	int minor;
};

static struct newchrled_dev newchrled;

/**
 * led_switch - switch LED state
 * @sta: LED_ON or LED_OFF
 */
static void led_switch(u8 sta)
{
	u32 val;

	val = readl(GPIO1_DR);

	if (sta == LED_ON)
		val &= ~(1 << 3);
	else
		val |= (1 << 3);

	writel(val, GPIO1_DR);
}

static int led_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &newchrled;
	return 0;
}

static ssize_t led_read(struct file *filp,
			char __user *buf,
			size_t cnt,
			loff_t *offt)
{
	return 0;
}

static ssize_t led_write(struct file *filp,
			 const char __user *buf,
			 size_t cnt,
			 loff_t *offt)
{
	u8 ledstat;
	int ret;

	if (!cnt)
		return 0;

	/* Only copy one byte from user */
	ret = copy_from_user(&ledstat, buf, 1);
	if (ret) {
		pr_err("newchrled: copy_from_user failed\n");
		return -EFAULT;
	}

	if (ledstat == LED_ON)
		led_switch(LED_ON);
	else
		led_switch(LED_OFF);

	return cnt;
}

static int led_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* File operations */
static const struct file_operations new_chrled_fops = {
	.owner   = THIS_MODULE,
	.open    = led_open,
	.read    = led_read,
	.write   = led_write,
	.release = led_release,
};

static int __init led_init(void)
{
	u32 val;
	int ret;

	/* Map registers */
	IMX6U_CCM_CCGR1   = ioremap(CCM_CCGR1_BASE, 4);
	SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
	SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
	GPIO1_DR          = ioremap(GPIO1_DR_BASE, 4);
	GPIO1_GDIR        = ioremap(GPIO1_GDIR_BASE, 4);

	if (!IMX6U_CCM_CCGR1 || !SW_MUX_GPIO1_IO03 ||
	    !SW_PAD_GPIO1_IO03 || !GPIO1_DR || !GPIO1_GDIR) {
		pr_err("newchrled: ioremap failed\n");
		ret = -ENOMEM;
		goto err_iounmap;
	}

	/* Enable GPIO clock */
	val = readl(IMX6U_CCM_CCGR1);
	val &= ~(3 << 26);
	val |=  (3 << 26);
	writel(val, IMX6U_CCM_CCGR1);

	/* Pin mux and pad config */
	writel(5, SW_MUX_GPIO1_IO03);
	writel(0x10B0, SW_PAD_GPIO1_IO03);

	/* GPIO output */
	val = readl(GPIO1_GDIR);
	val |= (1 << 3);
	writel(val, GPIO1_GDIR);

	/* LED off by default */
	val = readl(GPIO1_DR);
	val |= (1 << 3);
	writel(val, GPIO1_DR);

	/* Allocate device number */
	if (newchrled.major) {
		newchrled.devid = MKDEV(newchrled.major, 0);
		ret = register_chrdev_region(newchrled.devid,
					     NEW_CHR_LED_CNT,
					     NEW_CHR_LED_NAME);
	} else {
		ret = alloc_chrdev_region(&newchrled.devid,
					  0,
					  NEW_CHR_LED_CNT,
					  NEW_CHR_LED_NAME);
		newchrled.major = MAJOR(newchrled.devid);
		newchrled.minor = MINOR(newchrled.devid);
	}

	if (ret)
		goto err_iounmap;

	/* Init cdev */
	cdev_init(&newchrled.cdev, &new_chrled_fops);
	newchrled.cdev.owner = THIS_MODULE;
	ret = cdev_add(&newchrled.cdev,
		       newchrled.devid,
		       NEW_CHR_LED_CNT);
	if (ret)
		goto err_unregister;

	/* Create class and device */
	newchrled.class = class_create(THIS_MODULE, NEW_CHR_LED_NAME);
	if (IS_ERR(newchrled.class)) {
		ret = PTR_ERR(newchrled.class);
		goto err_cdev;
	}

	newchrled.device = device_create(newchrled.class,
					 NULL,
					 newchrled.devid,
					 NULL,
					 NEW_CHR_LED_NAME);
	if (IS_ERR(newchrled.device)) {
		ret = PTR_ERR(newchrled.device);
		goto err_class;
	}

	pr_info("newchrled: driver initialized\n");
	return 0;

err_class:
	class_destroy(newchrled.class);
err_cdev:
	cdev_del(&newchrled.cdev);
err_unregister:
	unregister_chrdev_region(newchrled.devid, NEW_CHR_LED_CNT);
err_iounmap:
	if (IMX6U_CCM_CCGR1)
		iounmap(IMX6U_CCM_CCGR1);
	if (SW_MUX_GPIO1_IO03)
		iounmap(SW_MUX_GPIO1_IO03);
	if (SW_PAD_GPIO1_IO03)
		iounmap(SW_PAD_GPIO1_IO03);
	if (GPIO1_DR)
		iounmap(GPIO1_DR);
	if (GPIO1_GDIR)
		iounmap(GPIO1_GDIR);

	return ret;
}

static void __exit led_exit(void)
{
	device_destroy(newchrled.class, newchrled.devid);
	class_destroy(newchrled.class);
	cdev_del(&newchrled.cdev);
	unregister_chrdev_region(newchrled.devid, NEW_CHR_LED_CNT);

	iounmap(IMX6U_CCM_CCGR1);
	iounmap(SW_MUX_GPIO1_IO03);
	iounmap(SW_PAD_GPIO1_IO03);
	iounmap(GPIO1_DR);
	iounmap(GPIO1_GDIR);

	pr_info("newchrled: driver removed\n");
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("snowclad");
MODULE_DESCRIPTION("Standard cdev-based LED driver for i.MX6UL");
