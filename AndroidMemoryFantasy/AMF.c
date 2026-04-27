/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <accctl.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>   // misc_register
#include <linux/fs.h>           // file_operations
#include <linux/uaccess.h>      // copy_to/from_user

// ---------------------------- 定义 ioctl 命令码（将来与用户态保持一致） ----------------------------
#define AMF_IOC_MAGIC  'A'
#define AMF_IOC_HELLO  _IO(AMF_IOC_MAGIC, 0)   // 测试命令，无数据

// ---------------------------- 设备操作函数 ----------------------------
static long amf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
        case AMF_IOC_HELLO:
            printk(KERN_INFO "AMF: received HELLO ioctl\n");
            return 0;
        default:
            return -ENOTTY;
    }
}

// 设备操作结构体
static const struct file_operations amf_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = amf_ioctl,
};

// miscdevice 结构体，自动创建 /dev/amf
static struct miscdevice amf_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "amf",
    .fops  = &amf_fops,
};

// ---------------------------- KPM 入口/出口 ----------------------------
KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0");
KPM_LICENSE("GPL");
KPM_AUTHOR("YourName");
KPM_DESCRIPTION("Memory R/W with ioctl test");

static long my_init(const char *args, const char *event, void __user *reserved)
{
    int ret = misc_register(&amf_misc);
    if (ret) {
        printk(KERN_ERR "AMF: misc_register failed: %d\n", ret);
        return ret;
    }
    printk(KERN_INFO "AMF: loaded, /dev/amf created\n");
    return 0;
}

static long my_exit(void __user *reserved)
{
    misc_deregister(&amf_misc);
    printk(KERN_INFO "AMF: unloaded\n");
    return 0;
}

KPM_INIT(my_init);
KPM_EXIT(my_exit);