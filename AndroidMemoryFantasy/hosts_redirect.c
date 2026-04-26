#include <kpmodule.h>

KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0");
KPM_LICENSE("GPL");

static long my_init(const char *args, const char *event, void __user *reserved)
{
    printk("<6>AndroidMemoryFantasy: loaded\n");
    return 0;
}

static long my_exit(void __user *reserved)
{
    printk("<6>AndroidMemoryFantasy: unloaded\n");
    return 0;
}

KPM_INIT(my_init);
KPM_EXIT(my_exit);