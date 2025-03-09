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

#define DTSLED_CNT 1            /* 设备号个数 */
#define DTSLED_NAME "dtsled"    /* 名字 */

#define LEDOFF 0
#define LEDON 1

/* 映射后的寄存器虚拟地址指针 */
// __iomem 是 Linux 内核中一个关键字，用来标记内存映射 I/O 区域的指针
static void __iomem *CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_GDIR;
static void __iomem *GPIO1_DR;

/* dtsled 设备结构体 */
struct dtsled_dev {
    dev_t devid;            /* 设备号 */
    int major;              /* 主设备号 */
    int minor;              /* 次设备号 */
    struct cdev cdev;       /* 字符设备 */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    struct device_node *nd; /* 设备节点 */
};

struct dtsled_dev dtsled;   /* led 设备 */

static void led_switch(u8 sta)
{
    u32 val = 0;
    if (sta == LEDON) {
        val = readl(GPIO1_DR);
        val &= ~(1 << 3);   // bit3置0
        writel(val, GPIO1_DR);
    } else if (sta == LEDOFF) {
        val = readl(GPIO1_DR);
        val|= (1 << 3);     // bit3置1
        writel(val, GPIO1_DR);
    }
}

static void init_led_gpio(void)
{
    u32 val = 0;

    /* 使能GPIO1时钟 */
    val = readl(CCM_CCGR1);
    val |= (0b11 << 26);    // bit 26 27 set 1
    writel(val, CCM_CCGR1);

    /* 设置 GPIO1_IO03 复用功能 */
    writel(0b101, SW_MUX_GPIO1_IO03);

    /* 设置IO属性 */
    writel(0x10B0, SW_PAD_GPIO1_IO03);

    /* 设置GPIO1_IO03为输出 */
    val = readl(GPIO1_GDIR);
    val |= (1 << 3);
    writel(val, GPIO1_GDIR);

    /* 熄灭灯 */
    led_switch(LEDOFF);
}

static int dtsled_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &dtsled;
    return 0;
}

static ssize_t dtsled_read(struct file *filp, char __user *buf,     
                               size_t cnt, loff_t *offt)
{
    // struct dtsled_dev *dev = (struct dtsled_dev*)filp->private_data;
    return 0;
}

static ssize_t dtsled_write(struct file *filp, const char __user *buf,  
                                size_t cnt, loff_t *offt)
{
    // struct dtsled_dev *dev = (struct dtsled_dev*)filp->private_data;
    int ret = 0;
    uint8_t data[1];

    ret = copy_from_user(data, buf, cnt);
    if (ret < 0) {
        printk("kernel write failed.\n");
        return -1;
    }

    if ((data[0] != LEDON) && (data[0] != LEDOFF)) {
        printk("param out of range.\n");
        return -1;
    }
    led_switch(data[0]);

    return 0;
}

static int dtsled_release(struct inode *inode, struct file *filp)
{
    // struct dtsled_dev *dev = (struct dtsled_dev*)filp->private_data;
    return 0;
}

/* 字符设备操作集合 */
static struct file_operations dtsled_fops = { 
    .owner = THIS_MODULE,
    .open = dtsled_open, 
    .read = dtsled_read, 
    .write = dtsled_write, 
    .release = dtsled_release, 
};

static int __init dtsled_init(void)
{
    int ret = 0;
    const char *str = NULL;
    // u8 reg_count = 0;
// #define reg_data_size 10
    // u32 reg_data[reg_data_size] = { 0 };
    // u8 i = 0;

    /* 注册字符设备 */
    /* 1. 申请设备号 */
    dtsled.major = 0;   /* 设备号由内核分配 */
    if (dtsled.major) { /* 定义了设备号 */
        dtsled.devid = MKDEV(dtsled.major, 0);
        ret = register_chrdev_region(dtsled.devid, DTSLED_CNT, DTSLED_NAME);
        if (ret < 0) {
            printk("register_chrdev_region failed.\n");
            goto fail_devid;
        }
    } else {    /* 没有给定设备号 */
        ret = alloc_chrdev_region(&dtsled.devid, 0, DTSLED_CNT, DTSLED_NAME);
        if (ret < 0) {
            printk("alloc_chrdev_region failed.\n");
            goto fail_devid;
        }
        dtsled.major = MAJOR(dtsled.devid);
        dtsled.minor = MINOR(dtsled.devid);
    }

    /* 2. 添加字符设备 */
    dtsled.cdev.owner = THIS_MODULE;
    cdev_init(&dtsled.cdev, &dtsled_fops);
    ret = cdev_add(&dtsled.cdev, dtsled.devid, DTSLED_CNT);
    if (ret < 0) {
        printk("cdev_add failed.\n");
        goto fail_cdev;
    }

    /* 3. 自动创建设备节点 */
    /* 创建类 */
    dtsled.class = class_create(THIS_MODULE, DTSLED_NAME);
    if (IS_ERR(dtsled.class)) {
        ret = PTR_ERR(dtsled.class);
        printk("class_create failed.\n");
        goto fail_class;
    }

    /* 创建设备 */
    dtsled.device = device_create(dtsled.class, NULL, dtsled.devid, NULL, DTSLED_NAME);
    if (IS_ERR(dtsled.device)) {
        ret = PTR_ERR(dtsled.device);
        printk("device_create failed.\n");
        goto fail_device;
    }

    /* 获取设备树属性内容 */
    dtsled.nd = of_find_node_by_path("/alphaled");
    if (dtsled.nd == NULL) {    /* 失败 */
        ret = -EINVAL;
        goto fail_finddts;
    }

    /* 读字符串 status */
    ret = of_property_read_string(dtsled.nd, "status", &str);
    if (ret < 0) {
        ret = -EINVAL;
        goto fail_finddts;
    }
    printk("status = %s\n", str);

    /* 读字符串 compatible */
    ret = of_property_read_string(dtsled.nd, "compatible", &str);
    if (ret < 0) {
        ret = -EINVAL;
        goto fail_finddts;
    }
    printk("compatible = %s\n", str);

#if 0
    /* 读数组 reg */
    reg_count = of_property_count_elems_of_size(dtsled.nd, "reg", sizeof(u32));
    if (reg_count < 0) {
        ret = -EINVAL;
        goto fail_finddts;
    }
    printk("reg count = %d\n", reg_count);
    if (reg_count != reg_data_size) {
        printk("error: reg count must be %d!\n", reg_data_size);
        ret = -EINVAL;
        goto fail_finddts;
    }
 
    ret = of_property_read_u32_array(dtsled.nd, "reg", reg_data, reg_count);
    if (ret < 0) {
        ret = -EINVAL;
        goto fail_finddts;
    }
    printk("reg data = ");
    for (i = 0; i < reg_count; i++) {
        printk("%#x ", reg_data[i]);
    }
    printk("\n");
#endif

    /* LED 初始化 */
    /* 寄存器地址映射 */
#if 0
    CCM_CCGR1 = ioremap(reg_data[0], reg_data[1]);
    SW_MUX_GPIO1_IO03 = ioremap(reg_data[2], reg_data[3]);
    SW_PAD_GPIO1_IO03 = ioremap(reg_data[4], reg_data[5]);
    GPIO1_GDIR = ioremap(reg_data[6], reg_data[7]);
    GPIO1_DR = ioremap(reg_data[8], reg_data[9]);
#endif
    CCM_CCGR1 = of_iomap(dtsled.nd, 0);
    SW_MUX_GPIO1_IO03 = of_iomap(dtsled.nd, 1);
    SW_PAD_GPIO1_IO03 = of_iomap(dtsled.nd, 2);
    GPIO1_GDIR = of_iomap(dtsled.nd, 3);
    GPIO1_DR = of_iomap(dtsled.nd, 4);

    /* 初始化 led gpio */
    init_led_gpio();
    printk("dtsled init success.\n");

    return 0;

fail_finddts:
    /* 销毁设备 */
    device_destroy(dtsled.class, dtsled.devid);
fail_device:
    /* 销毁类 */
    class_destroy(dtsled.class);
fail_class:
    /* 删除字符设备 */
    cdev_del(&dtsled.cdev);
fail_cdev:
    /* 释放设备号 */
    unregister_chrdev_region(dtsled.devid, DTSLED_CNT);
fail_devid:
    return ret;
}

static void __exit dtsled_exit(void)
{
    /* 释放映射 */
    iounmap(CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_GDIR);
    iounmap(GPIO1_DR);

    /* 销毁设备 */
    device_destroy(dtsled.class, dtsled.devid);

    /* 销毁类 */
    class_destroy(dtsled.class);

    /* 删除字符设备 */
    cdev_del(&dtsled.cdev);

    /* 释放设备号 */
    unregister_chrdev_region(dtsled.devid, DTSLED_CNT);
}

/* 模块入口与出口 */
module_init(dtsled_init);
module_exit(dtsled_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wangpeng");
