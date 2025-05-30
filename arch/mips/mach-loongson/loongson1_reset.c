// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2014 Antony Pavlov <antonynpavlov at gmail.com>
 */

#include <common.h>
#include <io.h>
#include <init.h>
#include <restart.h>
#include <mach/loongson1.h>

static void __noreturn longhorn_restart_soc(struct restart_handler *rst,
					    unsigned long flags)
{
	void __iomem *wdt = IOMEM(0);

	OPTIMIZER_HIDE_VAR(wdt);

	__raw_writel(0x1, wdt + WDT_EN);
	__raw_writel(0x1, wdt + WDT_SET);
	__raw_writel(0x1, wdt + WDT_TIMER);

	hang();
}

static int restart_register_feature(void)
{
	restart_handler_register_fn("soc-wdt", longhorn_restart_soc);

	return 0;
}
coredevice_initcall(restart_register_feature);
