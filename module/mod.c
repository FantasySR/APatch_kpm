#include <linux/module.h>
#include <linux/kernel.h>

// 入口函数
static int my_init(void) {
    printk(KERN_INFO "Hello KPM: Module loaded successfully!\n");
    return 0;
}

// 出口函数
static void my_exit(void) {
    printk(KERN_INFO "Goodbye KPM: Module unloaded.\n");
}

// 使用标准的 KPM 入口宏进行注册
KPM_INIT(my_init);
KPM_EXIT(my_exit);

// 模块元数据
KPM_NAME("my_module");
KPM_VERSION("1.0");
KPM_LICENSE("GPL");