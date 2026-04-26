/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * 简单 KPM 模板，基于 hosts_redirect 的成功经验
 */

#include <accctl.h>      // 包含 KPM 宏和必要定义
#include <linux/kernel.h>
#include <linux/module.h>

// 模块元数据
KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("you");
KPM_DESCRIPTION("Simple memory r/w demo");

// 模块加载初始化
static long my_init(const char* args, const char* event, void* __user reserved) {
    printk(KERN_INFO "AndroidMemoryFantasy: module loaded\n");
    return 0;
}

// 模块卸载
static long my_exit(void* __user reserved) {
    printk(KERN_INFO "AndroidMemoryFantasy: module unloaded\n");
    return 0;
}

// 注册入口和出口
KPM_INIT(my_init);
KPM_EXIT(my_exit);