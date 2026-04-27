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
#include <linux/mm.h>            /* 页表宏已经在里面了，不再需要 asm/pgtable.h */
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>       /* get_user, put_user */

KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0.0");
KPM_AUTHOR("FantasySR");
KPM_DESCRIPTION("AndroidMemoryFantasy - final page walk r/w");
KPM_LICENSE("GPL");

#define AMF_IOCTL_READ_MEM  0x01
#define AMF_IOCTL_WRITE_MEM 0x02

struct amf_ioctl_data {
    pid_t pid;
    unsigned long addr;
    unsigned long size;
    void __user *buffer;
};

/* 手动翻译虚拟地址 → 物理页面 + 内核可访问地址 */
static int get_kernel_page(struct mm_struct *mm, unsigned long addr,
                           struct page **ppage, void **kaddr)
{
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    unsigned long pfn;
    struct page *page;

    pgd = pgd_offset(mm, addr);
    if (pgd_none(*pgd) || pgd_bad(*pgd))
        return -EFAULT;

    p4d = p4d_offset(pgd, addr);
    if (p4d_none(*p4d) || p4d_bad(*p4d))
        return -EFAULT;

    pud = pud_offset(p4d, addr);
    if (pud_none(*pud) || pud_bad(*pud))
        return -EFAULT;

    pmd = pmd_offset(pud, addr);
    if (pmd_none(*pmd) || pmd_bad(*pmd))
        return -EFAULT;

    pte = pte_offset_kernel(pmd, addr);
    if (!pte || !pte_present(*pte))
        return -EFAULT;

    pfn = pte_pfn(*pte);
    page = pfn_to_page(pfn);
    if (!page)
        return -EFAULT;

    *ppage = page;
    *kaddr = page_address(page) + (addr & ~PAGE_MASK);
    return 0;
}

/* KPM 控制入口（只用 get_user / put_user，不依赖 copy_from_user） */
static long amf_ctl0(const char *args, char __user *out_msg, int outlen)
{
    unsigned int cmd;
    struct amf_ioctl_data data;

    /* 使用 get_user 安全地从用户态 args 中提取参数 */
    if (get_user(cmd, (unsigned int __user *)args))
        return -EFAULT;
    if (get_user(data.pid, (pid_t __user *)(args + 4)))
        return -EFAULT;
    if (get_user(data.addr, (unsigned long __user *)(args + 8)))
        return -EFAULT;
    if (get_user(data.size, (unsigned long __user *)(args + 16)))
        return -EFAULT;
    if (get_user(data.buffer, (void __user * __user *)(args + 24)))
        return -EFAULT;

    if (data.size <= 0 || data.size > 0x100000)
        return -EINVAL;

    if (cmd == AMF_IOCTL_READ_MEM) {
        struct task_struct *task;
        struct mm_struct *mm = NULL;
        void *kbuf = NULL;
        long ret = 0;
        unsigned long offset = 0;

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

        /* 逐页读取 */
        for (offset = 0; offset < data.size; ) {
            unsigned long chunk, cur_addr = data.addr + offset;
            struct page *page;
            void *kaddr;

            chunk = PAGE_SIZE - (cur_addr & ~PAGE_MASK);
            if (chunk > data.size - offset)
                chunk = data.size - offset;

            if (get_kernel_page(mm, cur_addr, &page, &kaddr) != 0) {
                ret = -EFAULT;
                break;
            }
            memcpy(kbuf + offset, kaddr, chunk);
            offset += chunk;
        }

        if (ret == 0) {
            /* 用 put_user 逐字节拷贝到用户态 out_msg（安全，但慢） */
            unsigned long i;
            unsigned char __user *p = (unsigned char __user *)out_msg;
            for (i = 0; i < data.size; i++) {
                if (put_user(((unsigned char *)kbuf)[i], p + i)) {
                    ret = -EFAULT;
                    break;
                }
            }
            if (ret == 0)
                ret = data.size;
        }

        kfree(kbuf);
        mmput(mm);
        return ret;
    }

    return -ENOTTY;
}

static long my_init(const char *args, const char *event, void __user *reserved)
{
    printk(KERN_INFO "AndroidMemoryFantasy: loaded (page walk)\n");
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