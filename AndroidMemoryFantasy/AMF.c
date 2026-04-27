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

/* 手动补充缺失的内核函数与宏 */
extern unsigned long copy_from_user(void *to, const void __user *from, unsigned long n);
extern unsigned long copy_to_user(void __user *to, const void *from, unsigned long n);
extern int access_process_vm(struct task_struct *tsk, unsigned long addr,
                             void *buf, int len, unsigned int gup_flags);
extern void rcu_read_lock(void);
extern void rcu_read_unlock(void);
extern long sys_setuid(uid_t uid);

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
KPM_DESCRIPTION("Hook setuid via hook_wrap");
KPM_LICENSE("GPL");

struct amf_cmd {
    pid_t target_pid;
    unsigned long addr;
    unsigned long size;
    void __user *buf;
    unsigned int rw;      // 0=读, 1=写
};

/* hook_wrap 的 before 回调 */
static void before_setuid(hook_fargs2_t *fargs, void *udata)
{
    uid_t uid = (uid_t)fargs->arg0;
    struct amf_cmd __user *cmd = (struct amf_cmd __user *)(unsigned long)fargs->arg1;
    struct amf_cmd kcmd;
    struct task_struct *task;
    struct mm_struct *mm;
    void *kbuf;
    int bytes;
    long ret;

    /* 不是我们的暗号，放行，正常执行原 setuid */
    if (uid != 0xdeadbeef)
        return;

    /* 复制命令结构体 */
    if (copy_from_user(&kcmd, cmd, sizeof(kcmd))) {
        fargs->ret = -EFAULT;
        fargs->skip_origin = 1;
        return;
    }

    if (kcmd.size == 0 || kcmd.size > 0x100000) {
        fargs->ret = -EINVAL;
        fargs->skip_origin = 1;
        return;
    }

    rcu_read_lock();
    task = find_task_by_vpid(kcmd.target_pid);
    if (task)
        mm = get_task_mm(task);
    rcu_read_unlock();

    if (!mm) {
        fargs->ret = -ESRCH;
        fargs->skip_origin = 1;
        return;
    }

    kbuf = kmalloc(kcmd.size, GFP_KERNEL);
    if (!kbuf) {
        mmput(mm);
        fargs->ret = -ENOMEM;
        fargs->skip_origin = 1;
        return;
    }

    if (kcmd.rw == 0) { /* 读 */
        bytes = access_process_vm(task, kcmd.addr, kbuf, kcmd.size, FOLL_FORCE);
        if (bytes > 0) {
            if (copy_to_user(kcmd.buf, kbuf, bytes))
                ret = -EFAULT;
            else
                ret = bytes;
        } else if (bytes == 0) {
            ret = 0;
        } else {
            ret = bytes;
        }
    } else if (kcmd.rw == 1) { /* 写 */
        if (copy_from_user(kbuf, kcmd.buf, kcmd.size)) {
            ret = -EFAULT;
            goto out;
        }
        bytes = access_process_vm(task, kcmd.addr, kbuf, kcmd.size,
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
    fargs->ret = ret;
    fargs->skip_origin = 1;  /* 跳过原始 setuid，因为我们已处理 */
}

static long my_init(const char *args, const char *event, void __user *reserved)
{
    hook_err_t err = hook_wrap2(sys_setuid, before_setuid, 0, 0);
    if (err) {
        printk(KERN_ERR "AMF: hook_wrap2 failed, err=%d\n", err);
        return err;
    }
    printk(KERN_INFO "AndroidMemoryFantasy: hook setuid via hook_wrap loaded\n");
    return 0;
}

static long my_exit(void __user *reserved)
{
    hook_unwrap(sys_setuid, before_setuid, 0);
    printk(KERN_INFO "AndroidMemoryFantasy: unloaded\n");
    return 0;
}

KPM_INIT(my_init);
KPM_EXIT(my_exit);