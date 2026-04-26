#include <kpm.h>
#include <linux/kernel.h>

static int my_init(void) {
    printk(KERN_INFO "AndroidMemoryFantasy module loaded\n");
    return 0;
}

static void my_exit(void) {
    printk(KERN_INFO "AndroidMemoryFantasy module unloaded\n");
}

KPM_INIT(my_init);
KPM_EXIT(my_exit);
KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0");
KPM_LICENSE("GPL");
MODULE_LICENSE("GPL");