#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#define DRIVER_MAJOR 200            // 主设备号
#define DRIVER_NAME "led"    // 名字

#define LEDOFF 0
#define LEDON 1

/* 寄存器物理地址 */
#define CCM_CCGR1_BASE (0x020C406C)
#define SW_MUX_GPIO1_IO03_BASE (0x020E0068)
#define SW_PAD_GPIO1_IO03_BASE (0x020E02F4)
#define GPIO1_GDIR_BASE (0x0209C004)
#define GPIO1_DR_BASE (0x0209C000)

/* 映射后的寄存器虚拟地址指针 */
// __iomem 是 Linux 内核中一个关键字，用来标记内存映射 I/O 区域的指针
static void __iomem *CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_GDIR;
static void __iomem *GPIO1_DR;

void led_switch(u8 sta)
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

static int led_open(struct inode *inode, struct file *filp)
{
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
    int ret = 0;
    uint8_t data[1];

    ret = __copy_from_user(data, buf, cnt);
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

static int led_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static struct file_operations driver_fops = { 
    .owner = THIS_MODULE, 
    .open = led_open, 
    .read = led_read, 
    .write = led_write, 
    .release = led_release, 
};

static int __init led_init(void)
{
    int ret = 0;
    uint32_t val = 0;

    /* 初始化 LED */
    /* 1. 寄存器地址映射 */
    CCM_CCGR1 = ioremap(CCM_CCGR1_BASE, 4);
    SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
    SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
    GPIO1_GDIR = ioremap(GPIO1_GDIR_BASE, 4);
    GPIO1_DR = ioremap(GPIO1_DR_BASE, 4);
    
    /* 2. 使能GPIO1时钟 */
    val = readl(CCM_CCGR1);
    val |= (0b11 << 26);    // bit 26 27 set 1
    writel(val, CCM_CCGR1);

    /* 3. 设置 GPIO1_IO03 复用功能 */
    writel(0b101, SW_MUX_GPIO1_IO03);

    /* 4. 设置IO属性 */
    writel(0x10B0, SW_PAD_GPIO1_IO03);

    /* 5. 设置GPIO1_IO03为输出 */
    val = readl(GPIO1_GDIR);
    val |= (1 << 3);
    writel(val, GPIO1_GDIR);

    /* 默认熄灭灯 */
    led_switch(LEDOFF);

    /* 注册字符设备驱动 */
    ret = register_chrdev(DRIVER_MAJOR, DRIVER_NAME, &driver_fops);
    if (ret < 0) {
        printk("led_init failed.\n");
        return -1;
    }

    return 0;
}

static void __exit led_exit(void)
{
    /* 取消映射 */
    iounmap(CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_GDIR);
    iounmap(GPIO1_DR);

    /* 注销字符设备驱动 */
    unregister_chrdev(DRIVER_MAJOR, DRIVER_NAME);
}

/* 模块入口与出口 */
module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wangpeng");
