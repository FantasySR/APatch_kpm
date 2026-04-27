/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <accctl.h>
#include <compiler.h>
#include <hook.h>
#include <kpmodule.h>
#include <kputils.h>
#include <taskext.h>

#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>

KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0.0");
KPM_AUTHOR("FantasySR");
KPM_DESCRIPTION("Hook setuid for memory r/w");
KPM_LICENSE("GPL");

/* 用户态与内核约定的命令结构体，放在 setuid 的第二个参数中 */
struct amf_cmd {
    pid_t target_pid;
    unsigned long addr;
    unsigned long size;
    void __user *buf;
    unsigned int rw;      // 0 = 读, 1 = 写
};

/* 保存原始的 setuid 系统调用 */
static typeof(sys_setuid) *orig_setuid;

/*
 * 我们自己的 setuid 实现：
 * - 如果 uid == 0xdeadbeef，第二个参数 args 指向 struct amf_cmd，执行内存读写
 * - 否则调用原始的 setuid，保证系统正常
 */
static long hook_setuid(uid_t uid, void __user *args)
{
    struct amf_cmd cmd;
    struct task_struct *task;
    struct mm_struct *mm;
    void *kbuf = NULL;
    long ret = 0;
    int bytes;

    if (uid != 0xdeadbeef)
        return orig_setuid(uid);

    /* 从用户态复制命令结构体 */
    if (copy_from_user(&cmd, args, sizeof(cmd)))
        return -EFAULT;

    if (cmd.size == 0 || cmd.size > 0x100000)
        return -EINVAL;

    /* 获取目标进程的内存描述符 */
    rcu_read_lock();
    task = find_task_by_vpid(cmd.target_pid);
    if (task)
        mm = get_task_mm(task);
    rcu_read_unlock();

    if (!mm)
        return -ESRCH;

    kbuf = kmalloc(cmd.size, GFP_KERNEL);
    if (!kbuf) {
        mmput(mm);
        return -ENOMEM;
    }

    if (cmd.rw == 0) { /* 读取内存 */
        bytes = access_process_vm(task, cmd.addr, kbuf, cmd.size, FOLL_FORCE);
        if (bytes > 0) {
            if (copy_to_user(cmd.buf, kbuf, bytes))
                ret = -EFAULT;
            else
                ret = bytes;
        } else if (bytes == 0) {
            ret = 0;
        } else {
            ret = bytes;
        }
    } else if (cmd.rw == 1) { /* 写入内存 */
        if (copy_from_user(kbuf, cmd.buf, cmd.size)) {
            ret = -EFAULT;
            goto out;
        }
        bytes = access_process_vm(task, cmd.addr, kbuf, cmd.size,
                                  FOLL_FORCE | FOLL_WRITE);
        if (bytes >= 0)
            ret = bytes;
        else
            ret = bytes;
    } else {
        ret = -EINVAL;
    }

out:
    kfree(kbuf);
    mmput(mm);
    return ret;
}

/* 模块加载时安装 hook */
static long my_init(const char *args, const char *event, void __user *reserved)
{
    int rc = hook_install("sys_setuid", hook_setuid, &orig_setuid);
    if (rc) {
        printk(KERN_ERR "AMF: hook install failed, rc=%d\n", rc);
        return rc;
    }
    printk(KERN_INFO "AndroidMemoryFantasy: hook setuid loaded\n");
    return 0;
}

/* 模块卸载时移除 hook */
static long my_exit(void __user *reserved)
{
    hook_uninstall("sys_setuid", hook_setuid);
    printk(KERN_INFO "AndroidMemoryFantasy: unloaded\n");
    return 0;
}

KPM_INIT(my_init);
KPM_EXIT(my_exit);