/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <accctl.h>
#include <compiler.h>
#include <hook.h>
#include <kpmodule.h>
#include <kputils.h>
#include <taskext.h>

KPM_NAME("AndroidMemoryFantasy");
KPM_VERSION("1.0.0");
KPM_AUTHOR("FantasySR");
KPM_DESCRIPTION("minimal test");
KPM_LICENSE("GPL");

static long my_init(const char *args, const char *event, void __user *reserved)
{
    // 不调用任何外部函数，只返回 0
    return 0;
}

static long my_exit(void __user *reserved)
{
    return 0;
}

KPM_INIT(my_init);
KPM_EXIT(my_exit);