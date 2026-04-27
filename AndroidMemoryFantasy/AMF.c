/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <accctl.h>
#include <compiler.h>
#include <hook.h>
#include <kpmodule.h>
#include <kputils.h>
#include <taskext.h>

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/slab.h>
#include <linux/string.h>

/* ====== 手动补全缺失的宏 ====== */
#ifndef GFP_KERNEL
#define GFP_KERNEL 0xcc0U
#endif
#ifndef FOLL_FORCE
#define FOLL_FORCE 0x10
#endif
#ifndef FOLL_WRITE
#define FOLL_WRITE 0x01
#endif
#ifndef KERN_INFO
#define KERN_INFO "<6>"
#endif

/* ====== 对于 KernelPatch 未提供 kfunc_def 的函数，手动补充 ====== */

/* access_process_vm */
extern int access_process_vm(struct task_struct *tsk, unsigned long addr,
                             void *buf, int len, unsigned int gup_flags);
#define access_process_vm (*kf_access_process_vm)
extern typeof(access_process_vm) kf_access_process_vm;

/* copy_from_user */
extern unsigned long copy_from_user(void *to, const void __user *from, unsigned long n);
#define copy_from_user (*kf_copy_from_user)
extern typeof(copy_from_user) kf_copy_from_user;

/* copy_to_user */
extern unsigned long copy_to_user(void __user *to, const void *from, unsigned long n);
#define copy_to_user (*kf_copy_to_user)
extern typeof(copy_to_user) kf_copy_to_user;

/* rcu_read_lock / rcu_read_unlock */
extern void rcu_read_lock(void);
extern void rcu_read_unlock(void);
#define rcu_read_lock (*kf_rcu_read_lock)
#define rcu_read_unlock (*kf_rcu_read_unlock)
extern typeof(rcu_read_lock) kf_rcu_read_lock;
extern typeof(rcu_read_unlock) kf_rcu_read_unlock;

/* ====== 模块信息 ====== */
KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0.0");
KPM_AUTHOR("FantasySR");
KPM_DESCRIPTION("AndroidMemoryFantasy - kernel rw");
KPM_LICENSE("GPL");

#define AMF_IOCTL_READ_MEM  0x01
#define AMF_IOCTL_WRITE_MEM 0x02

struct amf_ioctl_data {
    pid_t pid;
    unsigned long addr;
    unsigned long size;
    void __user *buffer;
};

static long amf_read_mem(struct amf_ioctl_data __user *user_data)
{
    struct amf_ioctl_data data;
    struct task_struct *task;
    struct mm_struct *mm = NULL;
    void *kbuf = NULL;
    long ret = 0;
    int bytes_read;

    if (copy_from_user(&data, user_data, sizeof(data)))
        return -EFAULT;

    if (data.size <= 0 || data.size > 0x100000)
        return -EINVAL;

    rcu_read_lock();
    task = find_task_by_vpid(data.pid);
    if (task)
        mm = get_task_mm(task);
    rcu_read_unlock();

    if (!mm)
        return -ESRCH;

    kbuf = kmalloc(data.size, GFP_KERNEL);
    if (!kbuf) {
        mmput(mm);
        return -ENOMEM;
    }

    bytes_read = access_process_vm(task, data.addr, kbuf, data.size, FOLL_FORCE);
    if (bytes_read > 0) {
        if (copy_to_user(data.buffer, kbuf, bytes_read))
            ret = -EFAULT;
        else
            ret = bytes_read;
    } else if (bytes_read == 0) {
        ret = 0;
    } else {
        ret = bytes_read;
    }

    kfree(kbuf);
    mmput(mm);
    return ret;
}

static long amf_write_mem(struct amf_ioctl_data __user *user_data)
{
    struct amf_ioctl_data data;
    struct task_struct *task;
    struct mm_struct *mm = NULL;
    void *kbuf = NULL;
    long ret = 0;
    int bytes_written;

    if (copy_from_user(&data, user_data, sizeof(data)))
        return -EFAULT;

    if (data.size <= 0 || data.size > 0x100000)
        return -EINVAL;

    rcu_read_lock();
    task = find_task_by_vpid(data.pid);
    if (task)
        mm = get_task_mm(task);
    rcu_read_unlock();

    if (!mm)
        return -ESRCH;

    kbuf = kmalloc(data.size, GFP_KERNEL);
    if (!kbuf) {
        mmput(mm);
        return -ENOMEM;
    }

    if (copy_from_user(kbuf, data.buffer, data.size)) {
        kfree(kbuf);
        mmput(mm);
        return -EFAULT;
    }

    bytes_written = access_process_vm(task, data.addr, kbuf, data.size,
                                      FOLL_FORCE | FOLL_WRITE);
    if (bytes_written > 0) {
        ret = bytes_written;
    } else if (bytes_written == 0) {
        ret = 0;
    } else {
        ret = bytes_written;
    }

    kfree(kbuf);
    mmput(mm);
    return ret;
}

static long amf_ctl0(const char *args, char __user *out_msg, int outlen)
{
    unsigned int cmd;
    struct amf_ioctl_data __user *user_data;

    if (!args || outlen < 0)
        return -EINVAL;

    if (copy_from_user(&cmd, args, sizeof(cmd)))
        return -EFAULT;
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

static long my_init(const char *args, const char *event, void __user *reserved)
{
    printk(KERN_INFO "AndroidMemoryFantasy: loaded (final)\n");
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