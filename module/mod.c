#include <accctl.h>

static int mod_init(void)
{
    printk("<6>Hello KPM from module!\n");
    return 0;
}

static void mod_exit(void)
{
    printk("<6>Goodbye KPM from module!\n");
}

KPM_ENTRY(mod_init, mod_exit);