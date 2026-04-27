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

/* ====== 手动补充缺失的内核函数声明 ====== */
extern unsigned long copy_from_user(void *to, const void __user *from, unsigned long n);
extern unsigned long copy_to_user(void __user *to, const void *from, unsigned long n);
extern int access_process_vm(struct task_struct *tsk, unsigned long addr,
                             void *buf, int len, unsigned int gup_flags);
extern void rcu_read_lock(void);
extern void rcu_read_unlock(void);

/* 缺失的宏 */
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
#ifndef KERN_ERR
#define KERN_ERR "<3>"
#endif

KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0.0");
KPM_AUTHOR("FantasySR");
KPM_DESCRIPTION("Hook setuid for memory r/w");
KPM_LICENSE("GPL");

/* 命令结构体，通过 setuid 的第二个参数传递 */
struct amf_cmd {
    pid_t target_pid;
    unsigned long addr;
    unsigned long size;
    void __user *buf;
    unsigned int rw;      // 0=读, 1=写
};

/* 保存原始 setuid 的指针 */
static typeof(sys_setuid) *orig_setuid;

/* 我们的 hook 函数 */
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

    if (copy_from_user(&cmd, args, sizeof(cmd)))
        return -EFAULT;

    if (cmd.size == 0 || cmd.size > 0x100000)
        return -EINVAL;

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

    if (cmd.rw == 0) { /* 读 */
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
    } else if (cmd.rw == 1) { /* 写 */
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

/* 定义一个 hook_t 变量，用于安装 */
static hook_t setuid_hook = {
    .name = "sys_setuid",
    .hook = (void *)hook_setuid,
    .orig = (void **)&orig_setuid,
};

/* 模块加载 */
static long my_init(const char *args, const char *event, void __user *reserved)
{
    hook_install(&setuid_hook);
    printk(KERN_INFO "AndroidMemoryFantasy: hook setuid loaded\n");
    return 0;
}

/* 模块卸载 */
static long my_exit(void __user *reserved)
{
    hook_uninstall(&setuid_hook);
    printk(KERN_INFO "AndroidMemoryFantasy: unloaded\n");
    return 0;
}

KPM_INIT(my_init);
KPM_EXIT(my_exit);