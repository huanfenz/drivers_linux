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

#if 0
backlight {
    compatible = "pwm-backlight";
    pwms = <&pwm1 0 5000000>;
    brightness-levels = <0 4 8 16 32 64 128 255>;
    default-brightness-level = <6>;
    status = "okay";
};
#endif

static int __init dtsof_init(void)
{
    int ret = 0;
    struct device_node *backlight_node = NULL;      // 节点
    struct property *compatible_property = NULL;    // 属性
    const char *str = NULL;
    u32 def_value = 0;
    u32 elem_count = 0;
    u32 *brival = NULL;
    u8 i = 0;

    /* 1. 找到backlight节点 路径是 /backlight*/
    backlight_node = of_find_node_by_path("/backlight");
    if (backlight_node == NULL) {
        return -EINVAL;
    }

    /* 2. 获取属性*/
    compatible_property = of_find_property(backlight_node, "compatible", NULL);
    if (compatible_property == NULL) {
        return -EINVAL;
    }
    printk("compatible=%s\n", (char *)(compatible_property->value));

    ret = of_property_read_string(backlight_node, "status", &str);
    if (ret < 0) {
        return -EINVAL;
    }
    printk("status=%s\n", str);

    /* 3. 数字 */
    ret = of_property_read_u32(backlight_node, "default-brightness-level", &def_value);
    if (ret < 0) {
        return -EINVAL;
    }
    printk("default-brightness-level=%u\n", def_value);

    /* 4. 获取数组类型属性 */
    elem_count = of_property_count_elems_of_size(backlight_node, "brightness-levels", sizeof(u32));
    if (elem_count < 0) {
        return -EINVAL;
    }
    printk("brightness-levels elems count=%d\n", elem_count);

    brival = kmalloc(elem_count * sizeof(u32), GFP_KERNEL);
    if (!brival) {
        return -EINVAL;
    }

    ret = of_property_read_u32_array(backlight_node, "brightness-levels", brival, elem_count);
    if (ret < 0) {
        kfree(brival);
        return -EINVAL;
    }
    for (i = 0; i < elem_count; i++) {
        printk("brightness-levels[%d]=%d\n", i, *(brival + i));
    }
    
    kfree(brival);
    return 0;
}

static void __exit dtsof_exit(void)
{

}

/* 模块入口与出口 */
module_init(dtsof_init);
module_exit(dtsof_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wangpeng");
