// SPDX-License-Identifier: GPL-2.0-or-later
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

// ---------- 设备名与 ioctl 命令定义 ----------
#define AMF_DEV_NAME "amf"
#define AMF_IOC_MAGIC 'A'
#define AMF_IOC_ECHO _IOWR(AMF_IOC_MAGIC, 1, struct amf_ioc_echo)

// ---------- 定义用户态与内核态交换数据的结构体 ----------
struct amf_ioc_echo {
    char in_data[256];   // 从用户态传入的数据
    char out_data[256];  // 由内核模块填写的返回数据
};

// ---------- 处理 ioctl 请求的核心函数 ----------
static long amf_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct amf_ioc_echo data;

    switch (cmd) {
        case AMF_IOC_ECHO:
            if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
                return -EFAULT; // 数据拷贝失败
            }
            /* ================================ */
            /* === 在这里处理C++程序发来的命令 === */
            /* ================================ */
            pr_info("AMF: Received from C++: %s\n", data.in_data);
            snprintf(data.out_data, sizeof(data.out_data), "Kernel says: '%s' is received!", data.in_data);

            if (copy_to_user((void __user *)arg, &data, sizeof(data))) {
                return -EFAULT; // 回传数据失败
            }
            return 0;
        default:
            return -ENOTTY; // 命令不存在
    }
}

// ---------- 注册设备操作函数 ----------
static const struct file_operations amf_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = amf_ioctl,
};

// ---------- 将设备注册为misc设备，自动生成 /dev/amf ----------
static struct miscdevice amf_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = AMF_DEV_NAME,
    .fops = &amf_fops,
};

// ---------- KPM 模块的加载与卸载入口 ----------
static long amf_init(const char *args, const char *event, void __user *reserved)
{
    int ret = misc_register(&amf_misc);
    if (ret) {
        pr_err("AMF: misc_register failed, error %d\n", ret);
        return ret;
    }
    pr_info("AMF: module loaded, /dev/amf created\n");
    return 0;
}

static long amf_exit(void __user *reserved)
{
    misc_deregister(&amf_misc);
    pr_info("AMF: module unloaded, /dev/amf removed\n");
    return 0;
}

// ---------- KPM 框架宏 ----------
KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0");
KPM_LICENSE("GPL");
KPM_AUTHOR("YourName");
KPM_DESCRIPTION("Test module with ioctl communication");
KPM_INIT(amf_init);
KPM_EXIT(amf_exit);