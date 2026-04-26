#include <linux/module.h>
#include <linux/kernel.h>
#include <kpm.h>

static int my_init(void)
{
    printk(KERN_INFO "Hello from my KPM module!\n");
    return 0;
}

static void my_exit(void)
{
    printk(KERN_INFO "Goodbye from my KPM module!\n");
}

KPM_INIT(my_init);
KPM_EXIT(my_exit);

KPM_NAME("my_mod");
KPM_VERSION("1.0");
KPM_LICENSE("GPL");
MODULE_LICENSE("GPL");