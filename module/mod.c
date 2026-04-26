#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

// KPM 相关宏（需要包含 KernelPatch 头文件）
#include <kpm.h>

static int __init mod_init(void)
{
    printk(KERN_INFO "Hello from my KPM module!\n");
    return 0;
}

static void __exit mod_exit(void)
{
    printk(KERN_INFO "Goodbye from my KPM module!\n");
}

module_init(mod_init);
module_exit(mod_exit);

// KPM 必需的宏
KPM_NAME("my_mod");
KPM_VERSION("1.0");
KPM_LICENSE("GPL");
KPM_AUTHOR("You");

MODULE_LICENSE("GPL");