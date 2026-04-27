#include <accctl.h>
#include <linux/kernel.h>

KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0");
KPM_LICENSE("GPL");
KPM_AUTHOR("YourName");
KPM_DESCRIPTION("Test module with control interface (no extra headers)");

// 控制函数：当用户态调用 kp module control 时触发
static long my_ctl(const char *args, char __user *out_msg, int outlen)
{
    if (args) {
        printk(KERN_INFO "AMF: received control args: %s\n", args);
    } else {
        printk(KERN_INFO "AMF: received control with null args\n");
    }

    // 可选：向用户态返回信息（复制到 out_msg 缓冲区）
    if (out_msg && outlen > 0) {
        const char *reply = "OK";
        // copy_to_user 需要 linux/uaccess.h，但为了简化，先不实现
        // 如果你需要回复，我们后续再加头文件，不影响基本功能
    }
    return 0;
}

static long my_init(const char *args, const char *event, void __user *reserved)
{
    printk(KERN_INFO "AMF: module loaded\n");
    return 0;
}

static long my_exit(void __user *reserved)
{
    printk(KERN_INFO "AMF: module unloaded\n");
    return 0;
}

KPM_INIT(my_init);
KPM_CTL0(my_ctl);   // 注册控制接口
KPM_EXIT(my_exit);