#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define CHRDEVBASE_MAJOR 200            // 主设备号
#define CHRDEVBASE_NAME "chrdevbase"    // 名字

static char writeBuf[100] = { 0 };      // 写缓冲
static const char kerneldata[] = "This is kernel data!";

static int chrdevbase_open(struct inode *inode, struct file *filp)
{
    // printk("chrdevbase_open\n");
    return 0;
}

static ssize_t chrdevbase_read(struct file *filp, char __user *buf,     
                               size_t cnt, loff_t *offt)
{
    int ret = 0;

    ret = copy_to_user(buf, kerneldata, sizeof(kerneldata));
    if (ret != 0) {
        printk("copy_to_user failed.\n");
    }
    
    return 0;
}

static ssize_t chrdevbase_write(struct file *filp, const char __user *buf,  
                                size_t cnt, loff_t *offt)
{
    int ret = 0;

    ret = copy_from_user(writeBuf, buf, cnt);
    if (ret != 0) {
        printk("copy_from_user failed.\n");
    }
    printk("kernel revedata:%s\n", writeBuf);

    return 0;
}

static int chrdevbase_release(struct inode *inode, struct file *filp)
{
    // printk("chrdevbase_release\n");
    return 0;
}

static struct file_operations chrdevbase_fops = { 
    .owner = THIS_MODULE, 
    .open = chrdevbase_open, 
    .read = chrdevbase_read, 
    .write = chrdevbase_write, 
    .release = chrdevbase_release, 
};

static int __init chrdevbase_init(void)
{
    int ret = 0;

    printk("chrdevbase_init\n");

    ret = register_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME, &chrdevbase_fops);
    if (ret < 0) {
        printk("chrdevbase init failed.\n");
    }

    return 0;
}

static void __exit chrdevbase_exit(void)
{
    unregister_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME);
    printk("chrdevbase_exit\n");
}

/* 模块入口与出口 */
module_init(chrdevbase_init);
module_exit(chrdevbase_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wangpeng");
