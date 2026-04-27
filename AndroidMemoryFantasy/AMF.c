/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <accctl.h>
#include <compiler.h>
#include <hook.h>
#include <kpmodule.h>
#include <kputils.h>
#include <taskext.h>

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/slab.h>

KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0.0");
KPM_AUTHOR("FantasySR");
KPM_DESCRIPTION("AndroidMemoryFantasy - kernel rw via access_process_vm");
KPM_LICENSE("GPL");

#define AMF_IOCTL_READ_MEM  0x01
#define AMF_IOCTL_WRITE_MEM 0x02

struct amf_ioctl_data {
    pid_t pid;
    unsigned long addr;
    unsigned long size;
    void __user *buffer;
};

/* 
 * 手动补充 kfunc_def 风格的函数指针声明。
 * KernelPatch 加载模块时，会自动把这些指针绑定到真正的内核函数地址。
 * 这样既不依赖头文件是否提供声明，也不会产生 unknown symbol。
 */
extern typeof(access_process_vm) *kf_access_process_vm;
extern typeof(copy_from_user)    *kf_copy_from_user;
extern typeof(copy_to_user)      *kf_copy_to_user;

/* 注意：find_task_by_vpid, get_task_mm, mmput, kmalloc, kfree 等已由
 * KernelPatch 头文件通过 kfunc_def 声明，不用再手动写。
 */

/* 读内存 */
static long amf_read_mem(struct amf_ioctl_data __user *user_data)
{
    struct amf_ioctl_data data;
    struct task_struct *task;
    struct mm_struct *mm = NULL;
    void *kbuf = NULL;
    long ret = 0;
    int bytes_read;

    /* 从用户态拷贝数据结构 */
    if ((*kf_copy_from_user)(&data, user_data, sizeof(data)))
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

    /* 关键：直接读取目标进程内存，无任何文件操作 */
    bytes_read = (*kf_access_process_vm)(task, data.addr, kbuf, data.size,
                                          FOLL_FORCE);
    if (bytes_read > 0) {
        if ((*kf_copy_to_user)(data.buffer, kbuf, bytes_read))
            ret = -EFAULT;
        else
            ret = bytes_read;   // 返回实际读取的字节数
    } else if (bytes_read == 0) {
        ret = 0;
    } else {
        ret = bytes_read;
    }

    kfree(kbuf);
    mmput(mm);
    return ret;
}

/* 写内存 */
static long amf_write_mem(struct amf_ioctl_data __user *user_data)
{
    struct amf_ioctl_data data;
    struct task_struct *task;
    struct mm_struct *mm = NULL;
    void *kbuf = NULL;
    long ret = 0;
    int bytes_written;

    if ((*kf_copy_from_user)(&data, user_data, sizeof(data)))
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

    if ((*kf_copy_from_user)(kbuf, data.buffer, data.size)) {
        kfree(kbuf);
        mmput(mm);
        return -EFAULT;
    }

    bytes_written = (*kf_access_process_vm)(task, data.addr, kbuf,
                                            data.size, FOLL_FORCE | FOLL_WRITE);
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

/* KPM 控制入口 */
static long amf_ctl0(const char *args, char __user *out_msg, int outlen)
{
    unsigned int cmd;
    struct amf_ioctl_data __user *user_data;

    if (!args || outlen < 0)
        return -EINVAL;

    /* 从 args 中提取 cmd 和结构体 */
    if ((*kf_copy_from_user)(&cmd, args, sizeof(cmd)))
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
    printk(KERN_INFO "AndroidMemoryFantasy: loaded (access_process_vm)\n");
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