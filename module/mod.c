#include <linux/module.h>
#include <linux/kernel.h>

static int __init mod_init(void) {
    printk(KERN_INFO "Hello from mod.kpm!\n");
    return 0;
}

static void __exit mod_exit(void) {
    printk(KERN_INFO "Goodbye from mod.kpm!\n");
}

module_init(mod_init);
module_exit(mod_exit);

KPM_NAME("my_mod");
KPM_VERSION("1.0");
KPM_LICENSE("GPL");
KPM_AUTHOR("You");

MODULE_LICENSE("GPL");