#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define DRIVER_NAME "led"    // 名字
#define DEVID_COUNT 1

/* LED设备结构体 */
struct newchrled_dev {
    struct cdev cdev;       /* 字符设备 */
    dev_t devid;            /* 设备号 */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    int major;              /* 主设备号 */
    int minor;              /* 次设备号 */
};

struct newchrled_dev newchrled; /* led设备 */

static int led_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &newchrled;
    return 0;
}

static ssize_t led_read(struct file *filp, char __user *buf,     
                               size_t cnt, loff_t *offt)
{
    return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf,  
                                size_t cnt, loff_t *offt)
{
    return 0;
}

static int led_release(struct inode *inode, struct file *filp)
{
    filp->private_data = NULL;
    return 0;
}

static struct file_operations led_fops = { 
    .owner = THIS_MODULE,
    .open = led_open, 
    .read = led_read, 
    .write = led_write, 
    .release = led_release, 
};

static int __init led_init(void)
{
    int ret = 0;

    /* 注册设备号 */
    if (newchrled.major) {
        newchrled.devid = MKDEV(newchrled.major, 0);
        ret = register_chrdev_region(newchrled.devid, DEVID_COUNT, DRIVER_NAME);
        if (ret < 0) {
            printk("register_chrdev_region failed.\n");
            goto fail_devid;
        }
    } else {
        ret = alloc_chrdev_region(&newchrled.devid, 0, DEVID_COUNT, DRIVER_NAME);
        if (ret < 0) {
            printk("alloc_chrdev_region failed.\n");
            goto fail_devid;
        }
        newchrled.major = MAJOR(newchrled.devid);
        newchrled.minor = MINOR(newchrled.devid);
    }
    printk("newchrled major=%d, minor=%d\n", newchrled.major, newchrled.minor);

    /* 注册字符设备 */
    newchrled.cdev.owner = THIS_MODULE;
    cdev_init(&newchrled.cdev, &led_fops);
    /* 添加字符设备 */
    ret = cdev_add(&newchrled.cdev, newchrled.devid, DEVID_COUNT);
    if (ret < 0) {
        printk("cdev_add failed.\n");
        goto fail_cdev;
    }

    /* 创建类 */
    newchrled.class = class_create(THIS_MODULE, DRIVER_NAME);
    if (IS_ERR(newchrled.class)) {
        ret = PTR_ERR(newchrled.class);
        printk("class_create failed.\n");
        goto fail_class;
    }

    /* 创建设备 */
    newchrled.device = device_create(newchrled.class, NULL, newchrled.devid, NULL, DRIVER_NAME);
    if (IS_ERR(newchrled.device)) {
        ret = PTR_ERR(newchrled.device);
        printk("device_create failed.\n");
        goto fail_device;
    }
    
    return 0;

fail_device:
    class_destroy(newchrled.class);
fail_class:
    cdev_del(&newchrled.cdev);
fail_cdev:
    unregister_chrdev_region(newchrled.devid, DEVID_COUNT);
fail_devid:
    return ret;
}

static void __exit led_exit(void)
{
    /* 销毁设备 */
    device_destroy(newchrled.class, newchrled.devid);

    /* 销毁类 */
    class_destroy(newchrled.class);

    /* 删除字符设备 */
    cdev_del(&newchrled.cdev);

    /* 注销字符设备驱动 */
    unregister_chrdev_region(newchrled.devid, DEVID_COUNT);
}

/* 模块入口与出口 */
module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wangpeng");
