/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __ASM_ARM_CPUTYPE_H
#define __ASM_ARM_CPUTYPE_H

#include <linux/stringify.h>
#include <linux/kernel.h>

#ifdef CONFIG_CPU_64v8

#define CPUID_ID	midr_el1
#define CPUID_CACHETYPE	ctr_el0
#define CPUID_MPIDR	mpidr_el1

#define read_cpuid(reg)							\
	({								\
		unsigned int __val;					\
		asm("mrs	%0, " __stringify(reg)			\
		    : "=r" (__val)					\
		    :							\
		    : "cc");						\
		__val;							\
	})
#else

#define CPUID_ID	0
#define CPUID_CACHETYPE	1
#define CPUID_TCM	2
#define CPUID_TLBTYPE	3
#define CPUID_MPIDR	5

#define CPUID_EXT_PFR0	"c1, 0"
#define CPUID_EXT_PFR1	"c1, 1"
#define CPUID_EXT_DFR0	"c1, 2"
#define CPUID_EXT_AFR0	"c1, 3"
#define CPUID_EXT_MMFR0	"c1, 4"
#define CPUID_EXT_MMFR1	"c1, 5"
#define CPUID_EXT_MMFR2	"c1, 6"
#define CPUID_EXT_MMFR3	"c1, 7"
#define CPUID_EXT_ISAR0	"c2, 0"
#define CPUID_EXT_ISAR1	"c2, 1"
#define CPUID_EXT_ISAR2	"c2, 2"
#define CPUID_EXT_ISAR3	"c2, 3"
#define CPUID_EXT_ISAR4	"c2, 4"
#define CPUID_EXT_ISAR5	"c2, 5"

#define read_cpuid(reg)							\
	({								\
		unsigned int __val;					\
		asm("mrc	p15, 0, %0, c0, c0, " __stringify(reg)	\
		    : "=r" (__val)					\
		    :							\
		    : "cc");						\
		__val;							\
	})
#define read_cpuid_ext(ext_reg)						\
	({								\
		unsigned int __val;					\
		asm("mrc	p15, 0, %0, c0, " ext_reg		\
		    : "=r" (__val)					\
		    :							\
		    : "cc");						\
		__val;							\
	})
#endif

extern unsigned int processor_id;

/*
 * The CPU ID never changes at run time, so we might as well tell the
 * compiler that it's constant.  Use this function to read the CPU ID
 * rather than directly reading processor_id or read_cpuid() directly.
 */
static inline unsigned int __attribute_const__ read_cpuid_id(void)
{
	return read_cpuid(CPUID_ID);
}

static inline unsigned int __attribute_const__ read_cpuid_cachetype(void)
{
	return read_cpuid(CPUID_CACHETYPE);
}

static inline unsigned int __attribute_const__ read_cpuid_tcmstatus(void)
{
	return read_cpuid(CPUID_TCM);
}

static inline unsigned int __attribute_const__ read_cpuid_mpidr(void)
{
	return read_cpuid(CPUID_MPIDR);
}

/*
 * Intel's XScale3 core supports some v6 features (supersections, L2)
 * but advertises itself as v5 as it does not support the v6 ISA.  For
 * this reason, we need a way to explicitly test for this type of CPU.
 */
#ifndef CONFIG_CPU_XSC3
#define cpu_is_xsc3()	0
#else
static inline int cpu_is_xsc3(void)
{
	unsigned int id;
	id = read_cpuid_id() & 0xffffe000;
	/* It covers both Intel ID and Marvell ID */
	if ((id == 0x69056000) || (id == 0x56056000))
		return 1;

	return 0;
}
#endif

#if !defined(CONFIG_CPU_XSCALE) && !defined(CONFIG_CPU_XSC3)
#define	cpu_is_xscale()	0
#else
#define	cpu_is_xscale()	1
#endif

#define ARM_CPU_PART_CORTEX_A5  0xC050
#define ARM_CPU_PART_CORTEX_A7  0xC070
#define ARM_CPU_PART_CORTEX_A8  0xC080
#define ARM_CPU_PART_CORTEX_A9  0xC090
#define ARM_CPU_PART_CORTEX_A15 0xC0F0
#define ARM_CPU_PART_CORTEX_A53 0xD030
#define ARM_CPU_PART_CORTEX_A55 0xD050
#define ARM_CPU_PART_CORTEX_A57 0xD070
#define ARM_CPU_PART_CORTEX_A72 0xD080
#define ARM_CPU_PART_CORTEX_R5  0xc150

#endif
