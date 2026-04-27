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
#include <linux/gfp.h>          // 即使内容不全，也继续包含
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
/* 不依赖 linux/uaccess.h，因为里面可能没有声明，我们手动 extern */

#include <uapi/linux/limits.h>

/* ----- 手动补充缺失的宏和声明 ----- */

/* 若 GFP_KERNEL 未定义，则自行定义（内核常见值） */
#ifndef GFP_KERNEL
#define GFP_KERNEL (0x400U | 0x40U | 0x80U)   /* __GFP_RECLAIM | __GFP_IO | __GFP_FS */
#endif

/* FOLL_* 宏 */
#ifndef FOLL_WRITE
#define FOLL_WRITE 0x01
#endif
#ifndef FOLL_FORCE
#define FOLL_FORCE 0x10
#endif

/* 手动声明用户空间拷贝函数（返回未复制的字节数，成功为0） */
extern unsigned long copy_from_user(void *to, const void __user *from, unsigned long n);
extern unsigned long copy_to_user(void __user *to, const void *from, unsigned long n);

/* 手动声明 access_process_vm */
extern int access_process_vm(struct task_struct *tsk, unsigned long addr,
                             void *buf, int len, unsigned int gup_flags);

/* ----- 模块元信息 ----- */
KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0.0");
KPM_AUTHOR("FantasySR");
KPM_DESCRIPTION("AndroidMemoryFantasy - 内核级内存读写模块");
KPM_LICENSE("GPL");

/* ----- IOCTL 命令与数据结构 ----- */
#define AMF_IOCTL_READ_MEM  0x01
#define AMF_IOCTL_WRITE_MEM 0x02

struct amf_ioctl_data {
    pid_t pid;
    unsigned long addr;
    unsigned long size;
    void __user *buffer;
};

/* ----- 内存读取 ----- */
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

/* ----- 内存写入 ----- */
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
    printk(KERN_ERR "AndroidMemoryFantasy: my_init called, returning -EIO\n");
    return -EIO;  // 故意返回错误
}

static long my_exit(void __user *reserved)
{
    printk(KERN_INFO "AndroidMemoryFantasy: unloaded\n");
    return 0;
}

KPM_INIT(my_init);
KPM_EXIT(my_exit);
KPM_CTL0(amf_ctl0);