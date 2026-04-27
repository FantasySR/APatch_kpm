/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <accctl.h>
#include <compiler.h>
#include <hook.h>
#include <kpmodule.h>
#include <kputils.h>
#include <taskext.h>

#include <linux/cred.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include <uapi/linux/limits.h>

/* ====== 关键：定义 kf_ 前缀的外部函数 ====== */
/* 这些函数在 KernelPatch 中都是以 kf_ 开头的别名导出的 */
extern unsigned long kf_copy_from_user(void *to, const void __user *from, unsigned long n);
extern unsigned long kf_copy_to_user(void __user *to, const void *from, unsigned long n);

extern int kf_access_process_vm(struct task_struct *tsk, unsigned long addr,
                                void *buf, int len, unsigned int gup_flags);

extern struct task_struct *kf_find_task_by_vpid(pid_t nr);
extern struct mm_struct *kf_get_task_mm(struct task_struct *task);
extern void kf_mmput(struct mm_struct *mm);

extern void kf_rcu_read_lock(void);
extern void kf_rcu_read_unlock(void);

extern void *kf_kmalloc(size_t size, gfp_t flags);
extern void kf_kfree(const void *objp);

/* printk 通常由 KernelPatch 头文件自动映射，无需手动声明 */
/* 如果没有，就也加上：extern int kf_printk(const char *fmt, ...); */

/* 若 GFP_KERNEL 未定义，自备一个（用数字常量也可以） */
#ifndef GFP_KERNEL
#define GFP_KERNEL 0xcc0U   /* 常见值 */
#endif

/* FOLL 标志 */
#ifndef FOLL_WRITE
#define FOLL_WRITE 0x01
#endif
#ifndef FOLL_FORCE
#define FOLL_FORCE 0x10
#endif

/* ====== 模块信息 ====== */
KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0.0");
KPM_AUTHOR("FantasySR");
KPM_DESCRIPTION("AndroidMemoryFantasy - 内核级内存读写模块");
KPM_LICENSE("GPL");

/* ====== IOCTL 结构和命令 ====== */
#define AMF_IOCTL_READ_MEM  0x01
#define AMF_IOCTL_WRITE_MEM 0x02

struct amf_ioctl_data {
    pid_t pid;
    unsigned long addr;
    unsigned long size;
    void __user *buffer;
};

/* ====== 读内存 ====== */
static long amf_read_mem(struct amf_ioctl_data __user *user_data)
{
    struct amf_ioctl_data data;
    struct task_struct *task;
    struct mm_struct *mm = NULL;
    void *kbuf = NULL;
    long ret = 0;
    int bytes_read;

    if (kf_copy_from_user(&data, user_data, sizeof(data)))
        return -EFAULT;

    if (data.size <= 0 || data.size > 0x100000)
        return -EINVAL;

    kf_rcu_read_lock();
    task = kf_find_task_by_vpid(data.pid);
    if (task)
        mm = kf_get_task_mm(task);
    kf_rcu_read_unlock();

    if (!mm)
        return -ESRCH;

    kbuf = kf_kmalloc(data.size, GFP_KERNEL);
    if (!kbuf) {
        kf_mmput(mm);
        return -ENOMEM;
    }

    bytes_read = kf_access_process_vm(task, data.addr, kbuf, data.size, FOLL_FORCE);
    if (bytes_read > 0) {
        if (kf_copy_to_user(data.buffer, kbuf, bytes_read))
            ret = -EFAULT;
        else
            ret = bytes_read;
    } else if (bytes_read == 0) {
        ret = 0;
    } else {
        ret = bytes_read;
    }

    kf_kfree(kbuf);
    kf_mmput(mm);
    return ret;
}

/* ====== 写内存 ====== */
static long amf_write_mem(struct amf_ioctl_data __user *user_data)
{
    struct amf_ioctl_data data;
    struct task_struct *task;
    struct mm_struct *mm = NULL;
    void *kbuf = NULL;
    long ret = 0;
    int bytes_written;

    if (kf_copy_from_user(&data, user_data, sizeof(data)))
        return -EFAULT;

    if (data.size <= 0 || data.size > 0x100000)
        return -EINVAL;

    kf_rcu_read_lock();
    task = kf_find_task_by_vpid(data.pid);
    if (task)
        mm = kf_get_task_mm(task);
    kf_rcu_read_unlock();

    if (!mm)
        return -ESRCH;

    kbuf = kf_kmalloc(data.size, GFP_KERNEL);
    if (!kbuf) {
        kf_mmput(mm);
        return -ENOMEM;
    }

    if (kf_copy_from_user(kbuf, data.buffer, data.size)) {
        kf_kfree(kbuf);
        kf_mmput(mm);
        return -EFAULT;
    }

    bytes_written = kf_access_process_vm(task, data.addr, kbuf, data.size,
                                         FOLL_FORCE | FOLL_WRITE);
    if (bytes_written > 0) {
        ret = bytes_written;
    } else if (bytes_written == 0) {
        ret = 0;
    } else {
        ret = bytes_written;
    }

    kf_kfree(kbuf);
    kf_mmput(mm);
    return ret;
}

/* ====== KPM 控制入口 ====== */
static long amf_ctl0(const char *args, char __user *out_msg, int outlen)
{
    unsigned int cmd;
    struct amf_ioctl_data __user *user_data;

    if (!args || outlen < 0)
        return -EINVAL;

    cmd = *(unsigned int *)args;
    user_data = (struct amf_ioctl_data __user *)(args + sizeof(unsigned int));

    switch (cmd) {
    case AMF_IOCTL_READ_MEM:
        return amf_read_mem(user_data);
    case AMF_IOCTL_WRITE_MEM:
        return amf_write_mem(user_data);
    default:
        return -ENOTTY;
    }
}

/* ====== 生命周期 ====== */
static long my_init(const char *args, const char *event, void __user *reserved)
{
    printk(KERN_INFO "AndroidMemoryFantasy: loaded\n");
    return 0;
}

static long my_exit(void __user *reserved)
{
    printk(KERN_INFO "AndroidMemoryFantasy: unloaded\n");
    return 0;
}

KPM_INIT(my_init);
KPM_EXIT(my_exit);
KPM_CTL0(amf_ctl0);