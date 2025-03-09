#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
// #include <linux/of.h>
// #include <linux/of_address.h>
// #include <linux/of_irq.h>
// #include <linux/slab.h>

#define DRIVER_CNT 1
#define DRIVER_NAME "myname"

/* 设备结构体 */
struct driver_dev {
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;       /* 字符设备 */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    struct device_node *nd; /* 设备节点 */
};

struct driver_dev dev;

static int driver_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &dev;
    return 0;
}

static ssize_t driver_read(struct file *filp, char __user *buf,     
                               size_t cnt, loff_t *offt)
{
    return 0;
}

static ssize_t driver_write(struct file *filp, const char __user *buf,  
                                size_t cnt, loff_t *offt)
{
    return 0;
}

static int driver_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/* 字符设备操作集合 */
static struct file_operations key_fops = { 
    .owner = THIS_MODULE,
    .open = driver_open, 
    .read = driver_read, 
    .write = driver_write, 
    .release = driver_release, 
};

static int __init my_driver_init(void)
{
    int ret = 0;

    /* 注册设备号 */
    ret = alloc_chrdev_region(&dev.devid, 0, DRIVER_CNT, DRIVER_NAME);
    if (ret < 0) {
        printk("alloc_chrdev_region failed.\n");
        goto fail_devid;
    }
    dev.major = MAJOR(dev.devid);
    dev.minor = MINOR(dev.devid);
    printk("dev major = %d, minor = %d\n", dev.major, dev.minor);

    /* 添加字符设备 */
    dev.cdev.owner = THIS_MODULE;
    cdev_init(&dev.cdev, &key_fops);
    ret = cdev_add(&dev.cdev, dev.devid, DRIVER_CNT);
    if (ret < 0) {
        printk("cdev_add failed.\n");
        goto fail_cdev;
    }

    /* 创建类 */
    dev.class = class_create(THIS_MODULE, DRIVER_NAME);
    if (IS_ERR(dev.class)) {
        ret = PTR_ERR(dev.class);
        printk("class_create failed.\n");
        goto fail_class;
    }

    /* 创建设备 */
    dev.device = device_create(dev.class, NULL, dev.devid, NULL, DRIVER_NAME);
    if (IS_ERR(dev.device)) {
        ret = PTR_ERR(dev.device);
        printk("device_create failed.\n");
        goto fail_device;
    }

    return 0;

fail_device:
    /* 销毁类 */
    class_destroy(dev.class);
fail_class:
    /* 删除字符设备 */
    cdev_del(&dev.cdev);
fail_cdev:
    /* 释放设备号 */
    unregister_chrdev_region(dev.devid, DRIVER_CNT);
fail_devid:
    return ret;
}

static void __exit my_driver_exit(void)
{
    /* 销毁设备 */
    device_destroy(dev.class, dev.devid);
    /* 销毁类 */
    class_destroy(dev.class);
    /* 删除字符设备 */
    cdev_del(&dev.cdev);
    /* 释放设备号 */
    unregister_chrdev_region(dev.devid, DRIVER_CNT);
}

/* 模块入口与出口 */
module_init(my_driver_init);
module_exit(my_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("wangpeng");
