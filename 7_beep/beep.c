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

#define BEEP_CNT 1
#define BEEP_NAME "beep"

#define BEEP_OFF 0
#define BEEP_ON 1

/* beep设备结构体 */
struct beep_dev {
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;       /* 字符设备 */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    struct device_node *nd; /* 设备节点 */
    int beep_gpio;
};

struct beep_dev beep;

static int beep_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &beep;
    return 0;
}

static ssize_t beep_read(struct file *filp, char __user *buf,     
                               size_t cnt, loff_t *offt)
{
    return 0;
}

static ssize_t beep_write(struct file *filp, const char __user *buf,  
                                size_t cnt, loff_t *offt)
{
    int ret = 0;
    uint8_t data[1];

    ret = copy_from_user(data, buf, cnt);
    if (ret < 0) {
        printk("kernel write failed.\n");
        return -1;
    }

    if (data[0] == BEEP_ON) {
        gpio_set_value(beep.beep_gpio, 0);
    } else if (data[0] == BEEP_OFF) {
        gpio_set_value(beep.beep_gpio, 1);
    } else {
        printk("param out of range.\n");
        return -1;
    }
    
    return 0;
}

static int beep_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/* 字符设备操作集合 */
static struct file_operations beep_fops = { 
    .owner = THIS_MODULE,
    .open = beep_open, 
    .read = beep_read, 
    .write = beep_write, 
    .release = beep_release, 
};

static int __init beep_init(void)
{
    int ret = 0;
    /* 注册设备号 */
    ret = alloc_chrdev_region(&beep.devid, 0, BEEP_CNT, BEEP_NAME);
    if (ret < 0) {
        printk("alloc_chrdev_region failed.\n");
        goto fail_devid;
    }
    beep.major = MAJOR(beep.devid);
    beep.minor = MINOR(beep.devid);
    printk("beep major = %d, minor = %d\n", beep.major, beep.minor);

    /* 添加字符设备 */
    beep.cdev.owner = THIS_MODULE;
    cdev_init(&beep.cdev, &beep_fops);
    ret = cdev_add(&beep.cdev, beep.devid, BEEP_CNT);
    if (ret < 0) {
        printk("cdev_add failed.\n");
        goto fail_cdev;
    }

    /* 创建类 */
    beep.class = class_create(THIS_MODULE, BEEP_NAME);
    if (IS_ERR(beep.class)) {
        ret = PTR_ERR(beep.class);
        printk("class_create failed.\n");
        goto fail_class;
    }

    /* 创建设备 */
    beep.device = device_create(beep.class, NULL, beep.devid, NULL, BEEP_NAME);
    if (IS_ERR(beep.device)) {
        ret = PTR_ERR(beep.device);
        printk("device_create failed.\n");
        goto fail_device;
    }

    /* 获取设备节点 */
    beep.nd = of_find_node_by_path("/beep");
    if (beep.nd == NULL) {
        ret = -EINVAL;
        goto fail_finddts;
    }

    /* 获取对应的GPIO */
    beep.beep_gpio = of_get_named_gpio(beep.nd, "beep-gpios", 0);
    if (beep.beep_gpio < 0) {
        printk("can't find gpio\n");
        ret = -EINVAL;
        goto fail_finddts;
    }
    printk("gpio num = %d\n", beep.beep_gpio);

    /* 申请IO */
    ret = gpio_request(beep.beep_gpio, "beep-gpio");
    if (ret) {
        printk("gpio_request failed.\n");
        ret = -EINVAL;
        goto fail_finddts;
    }

    /* 设置为输出 */
    ret = gpio_direction_output(beep.beep_gpio, 1);
    if (ret) {
        printk("gpio_direction_output failed.\n");
        ret = -EINVAL;
        goto fail_gpio_direction;
    }

    /* 输出低电平，开启蜂鸣器 */
    gpio_set_value(beep.beep_gpio, 0);

    return 0;

fail_gpio_direction:
    /* 释放IO */
    gpio_free(beep.beep_gpio);
fail_finddts:
    /* 销毁设备 */
    device_destroy(beep.class, beep.devid);
fail_device:
    /* 销毁类 */
    class_destroy(beep.class);
fail_class:
    /* 删除字符设备 */
    cdev_del(&beep.cdev);
fail_cdev:
    /* 释放设备号 */
    unregister_chrdev_region(beep.devid, BEEP_CNT);
fail_devid:
    return ret;
}

static void __exit beep_exit(void)
{
    /* 停止蜂鸣器 */
    gpio_set_value(beep.beep_gpio, 1);
    /* 释放IO */
    gpio_free(beep.beep_gpio);
    /* 销毁设备 */
    device_destroy(beep.class, beep.devid);
    /* 销毁类 */
    class_destroy(beep.class);
    /* 删除字符设备 */
    cdev_del(&beep.cdev);
    /* 释放设备号 */
    unregister_chrdev_region(beep.devid, BEEP_CNT);
}

/* 模块入口与出口 */
module_init(beep_init);
module_exit(beep_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("wangpeng");
