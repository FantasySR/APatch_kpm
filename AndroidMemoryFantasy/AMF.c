/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * AndroidMemoryFantasy - minimal KPM module based on hosts_redirect
 */
#include <accctl.h>
#include <compiler.h>
#include <hook.h>
#include <kpmodule.h>
#include <kputils.h>
#include <taskext.h>
#include <linux/cred.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <uapi/linux/limits.h>

// 模块元数据
KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0");
KPM_LICENSE("GPL");
KPM_AUTHOR("YourName");
KPM_DESCRIPTION("Minimal memory r/w module");

// 可选的控制接口（用于接收用户态命令）
static long amf_ctl(const char *args, char __user *out_msg, int outlen)
{
    if (args)
        printk(KERN_INFO "AMF: ctl args: %s\n", args);
    else
        printk(KERN_INFO "AMF: ctl called\n");
    // 如果需要回复，可以复制数据到 out_msg，但为了简单暂时不实现
    return 0;
}

// 模块加载
static long amf_init(const char *args, const char *event, void __user *reserved)
{
    printk(KERN_INFO "AndroidMemoryFantasy: loaded\n");
    return 0;
}

// 模块卸载
static long amf_exit(void __user *reserved)
{
    printk(KERN_INFO "AndroidMemoryFantasy: unloaded\n");
    return 0;
}

KPM_INIT(amf_init);
KPM_CTL0(amf_ctl);   // 注册控制接口（可移除如果不需要）
KPM_EXIT(amf_exit);