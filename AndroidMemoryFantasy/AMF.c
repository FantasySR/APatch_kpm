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
#include <linux/uaccess.h>   /* get_user, put_user */

#include <asm/pgtable.h>
#include <asm/page.h>

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

/* 页表遍历辅助 */
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

static long amf_ctl0(const char *args, char __user *out_msg, int outlen)
{
    unsigned int cmd;
    struct amf_ioctl_data data;
    int n;

    /* 使用 get_user 从 args 中安全获取命令和结构体 */
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

        for (offset = 0; offset < data.size; ) {
            unsigned long chunk;
            unsigned long addr = data.addr + offset;
            struct page *page;
            void *kaddr;

            chunk = PAGE_SIZE - (addr & ~PAGE_MASK);
            if (chunk > data.size - offset)
                chunk = data.size - offset;

            if (get_kernel_page(mm, addr, &page, &kaddr) != 0) {
                ret = -EFAULT;
                break;
            }
            memcpy(kbuf + offset, kaddr, chunk);
            offset += chunk;
        }

        if (ret == 0) {
            /* 用 put_user 逐字节？不行，outlen 是大小，我们可以直接用 memcpy
             * 但要确保 out_msg 是用户空间可写的，memcpy 可能不安全。
             * 但内核中的 memcpy 不会检查用户空间权限，我们需要 copy_to_user。
             * 既然 copy_to_user 不可用，我们使用 put_user 循环？
             * 对于大数据很慢。但是我们还是尝试使用 __copy_to_user 内联函数？
             * 在 arm64 中，copy_to_user 通常是一个外联函数，但头文件有内联版本？
             * 看 <linux/uaccess.h> 是否提供了 raw_copy_to_user 宏。
             * 为保险，我们检查 out_msg 可写，直接用 memcpy 到 out_msg 可能会导致
             * 内核崩溃，如果该页不可写。但在 KPM 系统调用中，out_msg 应该是由
             * KernelPatch 保证映射好的用户态缓冲区，直接 memcpy 可能没问题。
             * 为了绝对安全，我们使用 __put_user 内联宏，但只支持 1,2,4,8 字节。
             * 我们不得不逐字节复制。
             */
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

    /* 写操作暂不支持 */
    return -ENOTTY;
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