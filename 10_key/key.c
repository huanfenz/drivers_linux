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

#define KEY_CNT 1
#define KEY_NAME "key"

#define KEY0_VALUE  0xF0
#define INVALID_KEY 0x00

/* key设备结构体 */
struct key_dev {
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;       /* 字符设备 */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    struct device_node *nd; /* 设备节点 */
    int key_gpio;
    atomic_t key_value;
};

struct key_dev key;

static int key_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &key;
    return 0;
}

static ssize_t key_read(struct file *filp, char __user *buf,     
                               size_t cnt, loff_t *offt)
{
    struct key_dev *dev = filp->private_data;
    int ret = 0;
    int value = 0;

    if (gpio_get_value(dev->key_gpio) == 0) {   // 按下
        while (!gpio_get_value(dev->key_gpio)); // 等待松开
        atomic_set(&dev->key_value, KEY0_VALUE);
    } else {
        atomic_set(&dev->key_value, INVALID_KEY);
    }

    value = atomic_read(&dev->key_value);
    ret = copy_to_user(buf, &value, sizeof(value));

    return 0;
}

static ssize_t key_write(struct file *filp, const char __user *buf,  
                                size_t cnt, loff_t *offt)
{
    return 0;
}

static int key_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/* 字符设备操作集合 */
static struct file_operations key_fops = { 
    .owner = THIS_MODULE,
    .open = key_open, 
    .read = key_read, 
    .write = key_write, 
    .release = key_release, 
};

/* key io初始化 */
static int keyio_init(struct key_dev *dev)
{
    int ret = 0;

    /* 获取设备节点 */
    dev->nd = of_find_node_by_path("/key");
    if (dev->nd == NULL) {
        ret = -EINVAL;
        goto fail_general;
    }

    /* 获取对应的GPIO */
    dev->key_gpio = of_get_named_gpio(dev->nd, "key-gpios", 0);
    if (dev->key_gpio < 0) {
        printk("can't find gpio\n");
        ret = -EINVAL;
        goto fail_general;
    }
    printk("gpio num = %d\n", dev->key_gpio);

    /* 申请IO */
    ret = gpio_request(dev->key_gpio, "key0");
    if (ret) {
        printk("gpio_request failed.\n");
        ret = -EINVAL;
        goto fail_general;
    }

    /* 设置为输出 */
    ret = gpio_direction_input(dev->key_gpio);
    if (ret) {
        printk("gpio_direction_output failed.\n");
        ret = -EINVAL;
        goto fail_gpio_direction;
    }

    /* 初始化 atomic */
    atomic_set(&key.key_value, INVALID_KEY);

    return 0;

fail_gpio_direction:
    gpio_free(dev->key_gpio);
fail_general:
    return ret;
}

static int __init key_init(void)
{
    int ret = 0;

    /* 注册设备号 */
    ret = alloc_chrdev_region(&key.devid, 0, KEY_CNT, KEY_NAME);
    if (ret < 0) {
        printk("alloc_chrdev_region failed.\n");
        goto fail_devid;
    }
    key.major = MAJOR(key.devid);
    key.minor = MINOR(key.devid);
    printk("key major = %d, minor = %d\n", key.major, key.minor);

    /* 添加字符设备 */
    key.cdev.owner = THIS_MODULE;
    cdev_init(&key.cdev, &key_fops);
    ret = cdev_add(&key.cdev, key.devid, KEY_CNT);
    if (ret < 0) {
        printk("cdev_add failed.\n");
        goto fail_cdev;
    }

    /* 创建类 */
    key.class = class_create(THIS_MODULE, KEY_NAME);
    if (IS_ERR(key.class)) {
        ret = PTR_ERR(key.class);
        printk("class_create failed.\n");
        goto fail_class;
    }

    /* 创建设备 */
    key.device = device_create(key.class, NULL, key.devid, NULL, KEY_NAME);
    if (IS_ERR(key.device)) {
        ret = PTR_ERR(key.device);
        printk("device_create failed.\n");
        goto fail_device;
    }

    ret = keyio_init(&key);
    if (ret) {
        printk("keyio_init failed.\n");
        goto fail_initio;
    }

    return 0;

fail_initio:
    /* 销毁设备 */
    device_destroy(key.class, key.devid);
fail_device:
    /* 销毁类 */
    class_destroy(key.class);
fail_class:
    /* 删除字符设备 */
    cdev_del(&key.cdev);
fail_cdev:
    /* 释放设备号 */
    unregister_chrdev_region(key.devid, KEY_CNT);
fail_devid:
    return ret;
}

static void __exit key_exit(void)
{
    /* 释放IO */
    gpio_free(key.key_gpio);
    /* 销毁设备 */
    device_destroy(key.class, key.devid);
    /* 销毁类 */
    class_destroy(key.class);
    /* 删除字符设备 */
    cdev_del(&key.cdev);
    /* 释放设备号 */
    unregister_chrdev_region(key.devid, KEY_CNT);
}

/* 模块入口与出口 */
module_init(key_init);
module_exit(key_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("wangpeng");
