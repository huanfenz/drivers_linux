#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#define DRIVER_CNT 1
#define DRIVER_NAME "timer"

#define CLOSE_CMD       _IO(0xEF, 1)            // 关闭命令
#define OPEN_CMD        _IO(0xEF, 2)            // 打开命令
#define SETPERIOD_CMD   _IOW(0xEF, 3, int)      // 设置周期

/* 设备结构体 */
struct driver_dev {
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;       /* 字符设备 */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    struct device_node *nd; /* 设备节点 */
    int led_gpio;
    struct timer_list timer;/* 定时器 */
    int timeperiod;         /* 定时周期ms */
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

static long driver_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct driver_dev *dev = filp->private_data;    
    int ret = 0;
    int value = 0;
    
    switch (cmd)
    {
    case CLOSE_CMD:
        del_timer_sync(&dev->timer);
        break;
    case OPEN_CMD:
        mod_timer(&dev->timer, jiffies + msecs_to_jiffies(dev->timeperiod));
        break;
    case SETPERIOD_CMD:
        ret = copy_from_user(&value, (int *)arg, sizeof(int));
        if (ret < 0) {
            return -EFAULT;
        }
        dev->timeperiod = value;
        mod_timer(&dev->timer, jiffies + msecs_to_jiffies(dev->timeperiod));
        break;
    default:
        break;
    }

    return ret;
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
    .unlocked_ioctl = driver_ioctl,
    .release = driver_release, 
};

/* 初始化led gpio */
static int initLed(struct driver_dev *pdev)
{
    int ret = 0;

    /* 获取设备节点 */
    pdev->nd = of_find_node_by_path("/gpioled");
    if (pdev->nd == NULL) {
        ret = -EINVAL;
        goto fail_generic;
    }

    /* 获取LED所对应的GPIO */
    pdev->led_gpio = of_get_named_gpio(pdev->nd, "led-gpios", 0);
    if (pdev->led_gpio < 0) {
        printk("can't find led gpio\n");
        ret = -EINVAL;
        goto fail_generic;
    }
    printk("led gpio num = %d\n", pdev->led_gpio);

    /* 申请IO */
    ret = gpio_request(pdev->led_gpio, "led-gpio");
    if (ret) {
        printk("gpio_request failed.\n");
        ret = -EINVAL;
        goto fail_generic;
    }

    /* 设置为输出 */
    ret = gpio_direction_output(pdev->led_gpio, 1);
    if (ret) {
        printk("gpio_direction_output failed.\n");
        ret = -EINVAL;
        goto fail_set_direction;
    }

    return 0;

fail_set_direction:
    /* 释放IO */
    gpio_free(pdev->led_gpio);
fail_generic:
    return ret;
}

static void timer_func(unsigned long arg)
{
    struct driver_dev *dev = (struct driver_dev*)arg;
    static int sta = 1;

    sta = !sta;
    gpio_set_value(dev->led_gpio, sta);

    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(dev->timeperiod));
}

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

    /* 初始化LED */
    ret = initLed(&dev);
    if (ret) {
        printk("initLed failed.\n");
        goto fail_initio;
    }
 
    /* 初始化定时器 */
    init_timer(&dev.timer);
    dev.timeperiod = 500;
    dev.timer.function = timer_func;
    dev.timer.expires = jiffies + msecs_to_jiffies(dev.timeperiod);
    dev.timer.data = (unsigned long)&dev;
    add_timer(&dev.timer);  // 添加到系统

    return 0;

fail_initio:
    /* 销毁设备 */
    device_destroy(dev.class, dev.devid);
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
    /* 关灯 */
    gpio_set_value(dev.led_gpio, 1);
    /* 删除定时器 */
    del_timer(&dev.timer);
    /* 释放IO */
    gpio_free(dev.led_gpio);
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
