/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <accctl.h>
#include <compiler.h>
#include <hook.h>
#include <kpmodule.h>
#include <kputils.h>
#include <taskext.h>

#include <linux/cred.h>
#include <linux/err.h>
#include <linux/errno.h>       // 补充 errno 宏
#include <linux/fs.h>
#include <linux/gfp.h>         // 补充 GFP_KERNEL
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/rcupdate.h>    // 补充 RCU 函数
#include <linux/sched.h>
#include <linux/sched/mm.h>    // 补充 get_task_mm / mmput
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/uaccess.h>     // 补充 copy_from/to_user

#include <uapi/linux/limits.h>

KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0.0");
KPM_AUTHOR("FantasySR");
KPM_DESCRIPTION("AndroidMemoryFantasy - 内核级内存读写模块");
KPM_LICENSE("GPL");

/* ----- IOCTL 命令定义 ----- */
#define AMF_IOCTL_READ_MEM  0x01
#define AMF_IOCTL_WRITE_MEM 0x02

struct amf_ioctl_data {
    pid_t pid;
    unsigned long addr;
    unsigned long size;
    void __user *buffer;
};

/* ----- 辅助：若内核未定义 FOLL_FORCE 则补上 ----- */
#ifndef FOLL_FORCE
#define FOLL_FORCE 0x1
#endif

/* ----- 手动声明 access_process_vm，防止隐式声明 ----- */
extern int access_process_vm(struct task_struct *tsk, unsigned long addr,
                             void *buf, int len, unsigned int gup_flags);

/* ----- 核心读写实现（使用 RCU + mm 引用，无需 get_task_struct）----- */
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

    // ---------- 获取目标进程的 mm_struct ----------
    rcu_read_lock();
    task = find_task_by_vpid(data.pid);
    if (task)
        mm = get_task_mm(task);   // 会增加 mm 的引用计数
    rcu_read_unlock();

    if (!mm)
        return -ESRCH;

    // ---------- 分配内核缓冲区并读取内存 ----------
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

    // ---------- 获取目标进程的 mm_struct ----------
    rcu_read_lock();
    task = find_task_by_vpid(data.pid);
    if (task)
        mm = get_task_mm(task);
    rcu_read_unlock();

    if (!mm)
        return -ESRCH;

    // ---------- 从用户态拷贝数据到内核缓冲区 ----------
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

    // ---------- 写入目标进程内存 ----------
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

/* ----- KPM 控制入口 ----- */
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

/* ----- 生命周期 ----- */
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