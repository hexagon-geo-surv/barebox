// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2009 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix

/* cpuinfo.c - Show information about cp15 registers */

#include <common.h>
#include <getopt.h>
#include <command.h>
#include <complete.h>
#include <memory.h>
#include <asm/system.h>
#include <asm/barebox-arm.h>
#include <asm/cputype.h>

#define CPU_ARCH_UNKNOWN	0
#define CPU_ARCH_ARMv3		1
#define CPU_ARCH_ARMv4		2
#define CPU_ARCH_ARMv4T		3
#define CPU_ARCH_ARMv5		4
#define CPU_ARCH_ARMv5T		5
#define CPU_ARCH_ARMv5TE	6
#define CPU_ARCH_ARMv5TEJ	7
#define CPU_ARCH_ARMv6		8
#define CPU_ARCH_ARMv7		9
#define CPU_ARCH_ARMv8		10

static void decode_cache(unsigned long size)
{
	int linelen = 1 << ((size & 0x3) + 3);
	int mult = 2 + ((size >> 2) & 1);
	int cache_size = mult << (((size >> 6) & 0x7) + 8);

	if (((size >> 2) & 0xf) == 1)
		printf("no cache\n");
	else
		printf("%d bytes (linelen = %d)\n", cache_size, linelen);
}

static char *crbits[] = {"M", "A", "C", "W", "P", "D", "L", "B", "S", "R",
	"F", "Z", "I", "V", "RR", "L4", "DT", "", "IT", "ST", "", "FI", "U", "XP",
	"VE", "EE", "L2", "", "TRE", "AFE", "TE"};

static int do_cpuinfo(int argc, char *argv[])
{
	unsigned long mainid, cache, cr;
	char *architecture, *implementer;
	int opt, i;
	int cpu_arch;

	while ((opt = getopt(argc, argv, "sm")) > 0) {
		switch (opt) {
		case 'm':
			print_pbl_mem_layout(arm_mem_membase_get(),
					     arm_mem_endmem_get(),
					     mem_malloc_end() + 1);
			return 0;
		case 's':
			if (!IS_ENABLED(CONFIG_ARCH_HAS_STACK_DUMP))
				return -ENOSYS;

			printf("SP: 0x%08lx\n", get_sp());
			dump_stack();
			return 0;
		default:
			return COMMAND_ERROR_USAGE;
		}
	}

#ifdef CONFIG_CPU_64v8
	__asm__ __volatile__(
		"mrs	%0, midr_el1\n"
		: "=r" (mainid)
		:
		: "memory");

	__asm__ __volatile__(
		"mrs	%0, ctr_el0\n"
		: "=r" (cache)
		:
		: "memory");
#else
	__asm__ __volatile__(
		"mrc    p15, 0, %0, c0, c0, 0   @ read control reg\n"
		: "=r" (mainid)
		:
		: "memory");

	__asm__ __volatile__(
		"mrc    p15, 0, %0, c0, c0, 1   @ read control reg\n"
		: "=r" (cache)
		:
		: "memory");
#endif
	cr = get_cr();

	switch (mainid >> 24) {
	case 0x41:
		implementer = "ARM";
		break;
	case 0x44:
		implementer = "Digital Equipment Corp.";
		break;
	case 0x40:
		implementer = "Motorola - Freescale Semiconductor Inc.";
		break;
	case 0x56:
		implementer = "Marvell Semiconductor Inc.";
		break;
	case 0x69:
		implementer = "Intel Corp.";
		break;
	default:
		implementer = "Unknown";
	}

	if ((mainid & 0x0008f000) == 0) {
		cpu_arch = CPU_ARCH_UNKNOWN;
	} else if ((mainid & 0x0008f000) == 0x00007000) {
		cpu_arch = (mainid & (1 << 23)) ? CPU_ARCH_ARMv4T : CPU_ARCH_ARMv3;
	} else if ((mainid & 0x00080000) == 0x00000000) {
		cpu_arch = (mainid >> 16) & 7;
		if (cpu_arch)
			cpu_arch += CPU_ARCH_ARMv3;
	} else if ((mainid & 0x000f0000) == 0x000f0000) {
#ifdef CONFIG_CPU_64v8
		unsigned int isar2;

		__asm__ __volatile__(
			"mrs	%0, id_isar2_el1\n"
			: "=r" (isar2)
			:
			: "memory");


		/* Check Load/Store acquire to check if ARMv8 or not */

		if (isar2 & 0x2)
			cpu_arch = CPU_ARCH_ARMv8;
		else
			cpu_arch = CPU_ARCH_UNKNOWN;
#else
		unsigned int mmfr0;

		/* Revised CPUID format. Read the Memory Model Feature
		 * Register 0 and check for VMSAv7 or PMSAv7 */
		asm("mrc	p15, 0, %0, c0, c1, 4"
		    : "=r" (mmfr0));
		if ((mmfr0 & 0x0000000f) >= 0x00000003 ||
		    (mmfr0 & 0x000000f0) >= 0x00000030)
			cpu_arch = CPU_ARCH_ARMv7;
		else if ((mmfr0 & 0x0000000f) == 0x00000002 ||
			 (mmfr0 & 0x000000f0) == 0x00000020)
			cpu_arch = CPU_ARCH_ARMv6;
		else
			cpu_arch = CPU_ARCH_UNKNOWN;
#endif
	} else
		cpu_arch = CPU_ARCH_UNKNOWN;

	/*
	 * Special case for ARMv6 (K/Z) (has v7 compatible MMU, but is v6
	 * otherwise). The below check just matches all ARMv6, as done in the
	 * Linux kernel.
	 */
	if ((mainid & 0x7f000) == 0x7b000)
		cpu_arch = CPU_ARCH_ARMv6;

	switch (cpu_arch) {
	case CPU_ARCH_ARMv3:
		architecture = "v3";
		break;
	case CPU_ARCH_ARMv4:
		architecture = "v4";
		break;
	case CPU_ARCH_ARMv4T:
		architecture = "v4T";
		break;
	case CPU_ARCH_ARMv5:
		architecture = "v5";
		break;
	case CPU_ARCH_ARMv5T:
		architecture = "v5T";
		break;
	case CPU_ARCH_ARMv5TE:
		architecture = "v5TE";
		break;
	case CPU_ARCH_ARMv5TEJ:
		architecture = "v5TEJ";
		break;
	case CPU_ARCH_ARMv6:
		architecture = "v6";
		break;
	case CPU_ARCH_ARMv7:
		architecture = "v7";
		break;
	case CPU_ARCH_ARMv8:
		architecture = "v8";
		break;
	case CPU_ARCH_UNKNOWN:
	default:
		architecture = "Unknown";
	}

	printf("implementer: %s\narchitecture: %s\n",
			implementer, architecture);

	if (cpu_arch >= CPU_ARCH_ARMv7) {
		unsigned int major, minor;
		const char *part = NULL;
		major = (mainid >> 20) & 0xf;
		minor = mainid & 0xf;
		switch (mainid & 0xfff0) {
		case ARM_CPU_PART_CORTEX_A5:
			part = "Cortex-A5";
			break;
		case ARM_CPU_PART_CORTEX_A7:
			part = "Cortex-A7";
			break;
		case ARM_CPU_PART_CORTEX_A8:
			part = "Cortex-A8";
			break;
		case ARM_CPU_PART_CORTEX_A9:
			part = "Cortex-A9";
			break;
		case ARM_CPU_PART_CORTEX_A15:
			part = "Cortex-A15";
			break;
		case ARM_CPU_PART_CORTEX_A53:
			part = "Cortex-A53";
			break;
		case ARM_CPU_PART_CORTEX_A55:
			part = "Cortex-A55";
			break;
		case ARM_CPU_PART_CORTEX_A57:
			part = "Cortex-A57";
			break;
		case ARM_CPU_PART_CORTEX_A72:
			part = "Cortex-A72";
			break;
		default:
			printf("core: unknown (0x%08lx) r%up%u\n",
			       mainid, major, minor);
			break;
		}

		if (part)
			printf("core: %s r%up%u\n", part, major, minor);
	}

#ifdef CONFIG_CPU_64v8
	printf("exception level: %u\n", current_el());
#endif

	if (cache & (1 << 24)) {
		/* separate I/D cache */
		printf("I-cache: ");
		decode_cache(cache & 0xfff);
		printf("D-cache: ");
		decode_cache((cache >> 12) & 0xfff);
	} else {
		/* unified I/D cache */
		printf("cache: ");
		decode_cache(cache & 0xfff);
	}

	printf("Control register: ");
	for (i = 0; i < ARRAY_SIZE(crbits); i++)
		if (cr & (1 << i))
			printf("%s ", crbits[i]);
	printf("\n");

	return 0;
}

BAREBOX_CMD_HELP_START(cpuinfo)
BAREBOX_CMD_HELP_TEXT("Shows misc info about CPU")
BAREBOX_CMD_HELP_OPT ("-s", "print call stack info (if supported)")
BAREBOX_CMD_HELP_OPT ("-m", "print PBL memory layout")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(cpuinfo)
	.cmd            = do_cpuinfo,
	BAREBOX_CMD_DESC("show info about CPU")
	BAREBOX_CMD_OPTS("[-sm]")
	BAREBOX_CMD_GROUP(CMD_GRP_INFO)
	BAREBOX_CMD_COMPLETE(empty_complete)
	BAREBOX_CMD_HELP(cmd_cpuinfo_help)
BAREBOX_CMD_END
