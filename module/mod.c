// KernelPatch KPM 模块的核心头文件
#include <kpm.h>     

// 定义一个模块加载时执行的函数
static int hello_init(void) {
    // 使用内核打印函数，等级为 KERN_INFO
    printk(KERN_INFO "Hello KPM: Module loaded successfully!\n");
    return 0;
}

// 定义一个模块卸载时执行的函数
static void hello_exit(void) {
    printk(KERN_INFO "Goodbye KPM: Module unloaded.\n");
}

// 使用 KPM 框架的宏来“绑定”上面定义的函数
KPM_INIT(hello_init);
KPM_EXIT(hello_exit);

// 提供模块的元数据，这些都是必需的
KPM_NAME("my_module");
KPM_VERSION("1.0");
KPM_LICENSE("GPL");