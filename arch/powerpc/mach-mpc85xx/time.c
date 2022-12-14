/*
 * Copyright 2012 GE Intelligent Platforms, Inc.
 * (C) Copyright 2000, 2001
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <common.h>
#include <clock.h>
#include <init.h>
#include <mach/clock.h>

static uint64_t ppc_clocksource_read(void)
{
	return get_ticks();
}

static struct clocksource cs = {
	.read	= ppc_clocksource_read,
	.mask	= CLOCKSOURCE_MASK(64),
	.priority = 80,
};

static int clocksource_init(void)
{
	/* reset time base */
	asm ("li 3,0 ; mttbu 3 ; mttbl 3 ;");

	clocks_calc_mult_shift(&cs.mult, &cs.shift,
				fsl_get_timebase_clock(), NSEC_PER_SEC, 10);

	return init_clock(&cs);
}

core_initcall(clocksource_init);
