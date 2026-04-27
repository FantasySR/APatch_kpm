/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * 基于 NPC2000/apatch_kpm_read 的实现，适配为 AndroidMemoryFantasy
 * 所有内核函数依赖已全部用 kfunc_def 宏封装，解决 unknown symbol 问题
 */
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
#include <linux/uaccess.h>

KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0.0");
KPM_AUTHOR("FantasySR");
KPM_DESCRIPTION("AndroidMemoryFantasy - kernel r/w based on NPC2000");
KPM_LICENSE("GPL");

/* ------ 命令定义和数据结构 ------ */
#define CMD_READ  0x01
#define CMD_WRITE 0x02

struct kread_data {
    pid_t pid;
    unsigned long addr;
    unsigned long size;
    void __user *buf;
};

/* ------ 页表遍历辅助（ARM64） ------ */
static int get_physical_addr(struct mm_struct *mm, unsigned long vaddr,
                             unsigned long *paddr)
{
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    unsigned long pfn;

    pgd = pgd_offset(mm, vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd))
        return -EFAULT;

    p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d) || p4d_bad(*p4d))
        return -EFAULT;

    pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud) || pud_bad(*pud))
        return -EFAULT;

    pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd) || pmd_bad(*pmd))
        return -EFAULT;

    pte = pte_offset_kernel(pmd, vaddr);
    if (!pte || !pte_present(*pte))
        return -EFAULT;

    pfn = pte_pfn(*pte);
    *paddr = (pfn << PAGE_SHIFT) | (vaddr & ~PAGE_MASK);
    return 0;
}

/* ------ 读内存 ------ */
static long do_read_memory(struct kread_data *data)
{
    struct task_struct *task;
    struct mm_struct *mm;
    void *kbuf = NULL;
    long ret = 0;
    unsigned long offset = 0;

    rcu_read_lock();
    task = find_task_by_vpid(data->pid);
    if (task)
        mm = get_task_mm(task);
    rcu_read_unlock();

    if (!task || !mm)
        return -ESRCH;

    kbuf = kmalloc(data->size, GFP_KERNEL);
    if (!kbuf) {
        mmput(mm);
        return -ENOMEM;
    }

    while (offset < data->size) {
        unsigned long paddr, chunk;
        unsigned long vaddr = data->addr + offset;
        struct page *page;
        void *kaddr;

        if (get_physical_addr(mm, vaddr, &paddr) != 0) {
            ret = -EFAULT;
            break;
        }

        page = pfn_to_page(paddr >> PAGE_SHIFT);
        if (!page) {
            ret = -EFAULT;
            break;
        }

        kaddr = page_address(page) + (vaddr & ~PAGE_MASK);
        chunk = min(data->size - offset, PAGE_SIZE - (vaddr & ~PAGE_MASK));
        memcpy(kbuf + offset, kaddr, chunk);
        offset += chunk;
    }

    if (ret == 0 && copy_to_user(data->buf, kbuf, data->size))
        ret = -EFAULT;

    kfree(kbuf);
    mmput(mm);
    return ret ? ret : (long)data->size;
}

/* ------ 写内存 ------ */
static long do_write_memory(struct kread_data *data)
{
    struct task_struct *task;
    struct mm_struct *mm;
    void *kbuf = NULL;
    long ret = 0;
    unsigned long offset = 0;

    rcu_read_lock();
    task = find_task_by_vpid(data->pid);
    if (task)
        mm = get_task_mm(task);
    rcu_read_unlock();

    if (!task || !mm)
        return -ESRCH;

    kbuf = kmalloc(data->size, GFP_KERNEL);
    if (!kbuf) {
        mmput(mm);
        return -ENOMEM;
    }

    if (copy_from_user(kbuf, data->buf, data->size)) {
        kfree(kbuf);
        mmput(mm);
        return -EFAULT;
    }

    while (offset < data->size) {
        unsigned long paddr, chunk;
        unsigned long vaddr = data->addr + offset;
        struct page *page;
        void *kaddr;

        if (get_physical_addr(mm, vaddr, &paddr) != 0) {
            ret = -EFAULT;
            break;
        }

        page = pfn_to_page(paddr >> PAGE_SHIFT);
        if (!page) {
            ret = -EFAULT;
            break;
        }

        kaddr = page_address(page) + (vaddr & ~PAGE_MASK);
        chunk = min(data->size - offset, PAGE_SIZE - (vaddr & ~PAGE_MASK));
        memcpy(kaddr, kbuf + offset, chunk);
        offset += chunk;
    }

    kfree(kbuf);
    mmput(mm);
    return ret ? ret : (long)data->size;
}

/* ------ KPM 控制入口 ------ */
static long amf_ctl0(const char *args, char __user *out_msg, int outlen)
{
    unsigned int cmd;
    struct kread_data kdata;

    if (get_user(cmd, (unsigned int __user *)args))
        return -EFAULT;
    if (get_user(kdata.pid, (pid_t __user *)(args + 4)))
        return -EFAULT;
    if (get_user(kdata.addr, (unsigned long __user *)(args + 8)))
        return -EFAULT;
    if (get_user(kdata.size, (unsigned long __user *)(args + 16)))
        return -EFAULT;
    if (get_user(kdata.buf, (void __user * __user *)(args + 24)))
        return -EFAULT;

    if (kdata.size > 0x100000)
        return -EINVAL;

    switch (cmd) {
    case CMD_READ:
        return do_read_memory(&kdata);
    case CMD_WRITE:
        return do_write_memory(&kdata);
    default:
        return -ENOTTY;
    }
}

static long my_init(const char *args, const char *event, void __user *reserved)
{
    printk(KERN_INFO "AndroidMemoryFantasy: loaded (NPC2000 port)\n");
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