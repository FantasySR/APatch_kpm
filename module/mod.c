#include <linux/module.h>
#include <linux/kernel.h>
#include <kpm.h>

static int my_init(void)
{
    printk(KERN_INFO "Hello KPM from module!\n");
    return 0;
}

static void my_exit(void)
{
    printk(KERN_INFO "Goodbye KPM from module!\n");
}

KPM_INIT(my_init);
KPM_EXIT(my_exit);

KPM_NAME("my_module");
KPM_VERSION("1.0");
KPM_LICENSE("GPL");

MODULE_LICENSE("GPL");