/* SPDX-License-Identifier: GPL-2.0-or-later */
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
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/spinlock.h>
#include <uapi/linux/limits.h>

KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0.0");
KPM_AUTHOR("FantasySR");
KPM_DESCRIPTION("AndroidMemoryFantasy - 内核级内存读写模块");
KPM_LICENSE("GPL");

/* ----- 用户态与内核共享的 IOCTL 结构 ----- */
#define AMF_IOCTL_READ_MEM  0x01
#define AMF_IOCTL_WRITE_MEM 0x02

struct amf_ioctl_data {
    pid_t pid;            // 目标进程 PID
    unsigned long addr;   // 要读写的虚拟地址（在目标进程空间内）
    unsigned long size;   // 要读写的字节数
    void __user *buffer;  // 用户态缓冲区指针
};

/* ----- 辅助函数：获取目标进程的 task_struct ----- */
static struct task_struct *get_task_by_pid(pid_t pid)
{
    struct task_struct *task;
    rcu_read_lock();
    task = find_task_by_vpid(pid);
    if (task)
        get_task_struct(task);   // 增加引用计数，防止被释放
    rcu_read_unlock();
    return task;
}

/* ----- 核心读写实现 ----- */
static long amf_read_mem(struct amf_ioctl_data __user *user_data)
{
    struct amf_ioctl_data data;
    struct task_struct *task;
    struct mm_struct *mm;
    void *kbuf = NULL;
    long ret = 0;
    int bytes_read;

    if (copy_from_user(&data, user_data, sizeof(data)))
        return -EFAULT;

    if (data.size <= 0 || data.size > 0x100000)  // 限制单次1MB，防止恶意大小
        return -EINVAL;

    task = get_task_by_pid(data.pid);
    if (!task)
        return -ESRCH;

    mm = get_task_mm(task);
    if (!mm) {
        put_task_struct(task);
        return -EINVAL;
    }

    kbuf = kmalloc(data.size, GFP_KERNEL);
    if (!kbuf) {
        mmput(mm);
        put_task_struct(task);
        return -ENOMEM;
    }

    // 关键操作：在目标进程的 mm 中读取
    bytes_read = access_process_vm(task, data.addr, kbuf, data.size, FOLL_FORCE);
    if (bytes_read > 0) {
        if (copy_to_user(data.buffer, kbuf, bytes_read))
            ret = -EFAULT;
        else
            ret = bytes_read;   // 返回实际读取的字节数
    } else if (bytes_read == 0) {
        ret = 0;   // 没有读出任何数据（可能地址无效）
    } else {
        ret = bytes_read; // 返回错误码
    }

    kfree(kbuf);
    mmput(mm);
    put_task_struct(task);
    return ret;
}

static long amf_write_mem(struct amf_ioctl_data __user *user_data)
{
    struct amf_ioctl_data data;
    struct task_struct *task;
    struct mm_struct *mm;
    void *kbuf = NULL;
    long ret = 0;
    int bytes_written;

    if (copy_from_user(&data, user_data, sizeof(data)))
        return -EFAULT;

    if (data.size <= 0 || data.size > 0x100000)
        return -EINVAL;

    task = get_task_by_pid(data.pid);
    if (!task)
        return -ESRCH;

    mm = get_task_mm(task);
    if (!mm) {
        put_task_struct(task);
        return -EINVAL;
    }

    kbuf = kmalloc(data.size, GFP_KERNEL);
    if (!kbuf) {
        mmput(mm);
        put_task_struct(task);
        return -ENOMEM;
    }

    if (copy_from_user(kbuf, data.buffer, data.size)) {
        kfree(kbuf);
        mmput(mm);
        put_task_struct(task);
        return -EFAULT;
    }

    // 关键操作：向目标进程写入
    bytes_written = access_process_vm(task, data.addr, kbuf, data.size, FOLL_FORCE | FOLL_WRITE);
    if (bytes_written > 0) {
        ret = bytes_written;
    } else if (bytes_written == 0) {
        ret = 0;
    } else {
        ret = bytes_written;
    }

    kfree(kbuf);
    mmput(mm);
    put_task_struct(task);
    return ret;
}

/* ----- KPM 控制入口：接收用户态 ioctl 命令 ----- */
static long amf_ctl0(const char *args, char __user *out_msg, int outlen)
{
    unsigned int cmd;
    struct amf_ioctl_data __user *user_data;

    // args 的前4字节是命令号，后面是用户态数据结构指针
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

/* ----- 模块初始化/卸载 ----- */
static long my_init(const char *args, const char *event, void __user *reserved)
{
    printk(KERN_INFO "AndroidMemoryFantasy: loaded, ioctl READ=%x WRITE=%x\n",
           AMF_IOCTL_READ_MEM, AMF_IOCTL_WRITE_MEM);
    return 0;
}

static long my_exit(void __user *reserved)
{
    printk(KERN_INFO "AndroidMemoryFantasy: unloaded\n");
    return 0;
}

KPM_INIT(my_init);
KPM_EXIT(my_exit);
KPM_CTL0(amf_ctl0);   // ← 注册控制函数，使模块能被用户态调用