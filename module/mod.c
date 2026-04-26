#include <linux/module.h>
#include <linux/kernel.h>
#include <accctl.h>

static int mod_init(void)
{
    printk(KERN_INFO "Hello KPM from module!\n");
    return 0;
}

static void mod_exit(void)
{
    printk(KERN_INFO "Goodbye KPM from module!\n");
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");