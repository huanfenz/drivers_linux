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
#include <linux/spinlock.h>

#define GPIOLED_CNT 1
#define GPIOLED_NAME "gpioled"

#define LEDOFF 0
#define LEDON 1

/* gpioled设备结构体 */
struct gpioled_dev {
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;       /* 字符设备 */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    struct device_node *nd; /* 设备节点 */
    int led_gpio;

    int dev_status;         /* 0表示设备可以使用，大于等于1表示不可使用 */
    spinlock_t lock;
};

struct gpioled_dev gpioled;

static int gpioled_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &gpioled;

    spin_lock(&gpioled.lock);
    if (gpioled.dev_status) {   // 驱动不能使用
        spin_unlock(&gpioled.lock);
        return -EBUSY;
    }

    gpioled.dev_status++;   // 标记被使用
    spin_unlock(&gpioled.lock);
    return 0;
}

static ssize_t gpioled_read(struct file *filp, char __user *buf,     
                               size_t cnt, loff_t *offt)
{
    return 0;
}

static ssize_t gpioled_write(struct file *filp, const char __user *buf,  
                                size_t cnt, loff_t *offt)
{
    int ret = 0;
    uint8_t data[1];

    ret = copy_from_user(data, buf, cnt);
    if (ret < 0) {
        printk("kernel write failed.\n");
        return -1;
    }

    if (data[0] == LEDON) {
        gpio_set_value(gpioled.led_gpio, 0);
    } else if (data[0] == LEDOFF) {
        gpio_set_value(gpioled.led_gpio, 1);
    } else {
        printk("param out of range.\n");
        return -1;
    }
    
    return 0;
}

static int gpioled_release(struct inode *inode, struct file *filp)
{
    struct gpioled_dev *dev = filp->private_data;

    spin_lock(&dev->lock);
    if (dev->dev_status) {
        dev->dev_status--;  /* 标记驱动可以使用 */
    }

    spin_unlock(&dev->lock);
    return 0;
}

/* 字符设备操作集合 */
static struct file_operations gpioled_fops = { 
    .owner = THIS_MODULE,
    .open = gpioled_open, 
    .read = gpioled_read, 
    .write = gpioled_write, 
    .release = gpioled_release, 
};

static int __init led_init(void)
{
    int ret = 0;

    /* 初始化自旋锁 */
    spin_lock_init(&gpioled.lock);
    gpioled.dev_status = 0;

    /* 注册设备号 */
    ret = alloc_chrdev_region(&gpioled.devid, 0, GPIOLED_CNT, GPIOLED_NAME);
    if (ret < 0) {
        printk("alloc_chrdev_region failed.\n");
        goto fail_devid;
    }
    gpioled.major = MAJOR(gpioled.devid);
    gpioled.minor = MINOR(gpioled.devid);
    printk("gpioled major = %d, minor = %d\n", gpioled.major, gpioled.minor);

    /* 添加字符设备 */
    gpioled.cdev.owner = THIS_MODULE;
    cdev_init(&gpioled.cdev, &gpioled_fops);
    ret = cdev_add(&gpioled.cdev, gpioled.devid, GPIOLED_CNT);
    if (ret < 0) {
        printk("cdev_add failed.\n");
        goto fail_cdev;
    }

    /* 创建类 */
    gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
    if (IS_ERR(gpioled.class)) {
        ret = PTR_ERR(gpioled.class);
        printk("class_create failed.\n");
        goto fail_class;
    }

    /* 创建设备 */
    gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, GPIOLED_NAME);
    if (IS_ERR(gpioled.device)) {
        ret = PTR_ERR(gpioled.device);
        printk("device_create failed.\n");
        goto fail_device;
    }

    /* 获取设备节点 */
    gpioled.nd = of_find_node_by_path("/gpioled");
    if (gpioled.nd == NULL) {
        ret = -EINVAL;
        goto fail_finddts;
    }

    /* 获取LED所对应的GPIO */
    gpioled.led_gpio = of_get_named_gpio(gpioled.nd, "led-gpios", 0);
    if (gpioled.led_gpio < 0) {
        printk("can't find led gpio\n");
        ret = -EINVAL;
        goto fail_finddts;
    }
    printk("led gpio num = %d\n", gpioled.led_gpio);

    /* 申请IO */
    ret = gpio_request(gpioled.led_gpio, "led-gpio");
    if (ret) {
        printk("gpio_request failed.\n");
        ret = -EINVAL;
        goto fail_finddts;
    }

    /* 设置为输出 */
    ret = gpio_direction_output(gpioled.led_gpio, 1);
    if (ret) {
        printk("gpio_direction_output failed.\n");
        ret = -EINVAL;
        goto fail_gpio_direction;
    }

    /* 输出低电平，点亮LED灯 */
    gpio_set_value(gpioled.led_gpio, 0);

    return 0;

fail_gpio_direction:
    /* 释放IO */
    gpio_free(gpioled.led_gpio);
fail_finddts:
    /* 销毁设备 */
    device_destroy(gpioled.class, gpioled.devid);
fail_device:
    /* 销毁类 */
    class_destroy(gpioled.class);
fail_class:
    /* 删除字符设备 */
    cdev_del(&gpioled.cdev);
fail_cdev:
    /* 释放设备号 */
    unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);
fail_devid:
    return ret;
}

static void __exit led_exit(void)
{
    /* 关灯 */
    gpio_set_value(gpioled.led_gpio, 1);
    /* 释放IO */
    gpio_free(gpioled.led_gpio);
    /* 销毁设备 */
    device_destroy(gpioled.class, gpioled.devid);
    /* 销毁类 */
    class_destroy(gpioled.class);
    /* 删除字符设备 */
    cdev_del(&gpioled.cdev);
    /* 释放设备号 */
    unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);
}

/* 模块入口与出口 */
module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("wangpeng");
