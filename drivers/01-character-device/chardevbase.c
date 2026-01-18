#include <linux/module.h>   // Needed for all modules
#include <linux/kernel.h>   // Needed for KERN_INFO, printk
#include <linux/init.h>     // Needed for __init and __exit macros
#include <linux/fs.h>       // Needed for file_operations
#include <linux/uaccess.h>  // Needed for copy_to_user, copy_from_user

#define CHRDEVBASE_MAJOR 200
#define CHRDEVBASE_NAME  "chardevbase"

static char read_buf[128];
static char write_buf[128];
static char kernel_data[] = {"kernel data"};

/* Open function */
static int chrdevbase_open(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "chrdevbase: device opened\n");
    return 0;
}

/* Read function */
static ssize_t chrdevbase_read(struct file *filp, char __user *buf, size_t cnt, loff_t *ppos)
{
    size_t available;

    available = sizeof(kernel_data) - *ppos;
    if (available == 0) {
        return 0; // EOF
    }

    if (cnt > available) {
        cnt = available;
    }
    memcpy(read_buf, kernel_data + *ppos, cnt);

    if (copy_to_user(buf, read_buf, cnt)) {
        printk(KERN_ERR "chrdevbase: failed to copy %zu bytes to user\n", cnt);
        return -EFAULT;
    }

    *ppos += cnt; // Update file offset
    printk(KERN_INFO "chrdevbase: sent %zu bytes to user\n", cnt);

    return cnt;
}

/* Write function */
static ssize_t chrdevbase_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *ppos)
{
    /* prevent buffer overflow */
    if (cnt > sizeof(write_buf) - 1)
        cnt = sizeof(write_buf) - 1;

    if (copy_from_user(write_buf, buf, cnt)) {
        printk(KERN_ERR "chrdevbase: failed to copy %zu bytes from user\n", cnt);
        return -EFAULT;
    }

    write_buf[cnt] = '\0'; // Ensure null-terminated string
    printk(KERN_INFO "chrdevbase: received data from user: %s\n", write_buf);

    return cnt;
}

/* Release function */
static int chrdevbase_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "chrdevbase: device released\n");
    return 0;
}

/* File operations structure */
static struct file_operations chrdevbase_fops = {
    .owner = THIS_MODULE,
    .open = chrdevbase_open,
    .read = chrdevbase_read,
    .write = chrdevbase_write,
    .release = chrdevbase_release,
};

/* Module initialization */
static int __init chrdevbase_init(void)
{
    int ret;

    ret = register_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME, &chrdevbase_fops);
    if (ret < 0) {
        printk(KERN_ERR "chrdevbase: driver register failed\n");
        return ret;
    }

    printk(KERN_INFO "chrdevbase: module loaded\n");
    return 0;
}

/* Module exit */
static void __exit chrdevbase_exit(void)
{
    unregister_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME);
    printk(KERN_INFO "chrdevbase: module unloaded\n");
}

module_init(chrdevbase_init);
module_exit(chrdevbase_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("snowclad");
MODULE_DESCRIPTION("Optimized character device driver (using register_chrdev)");
