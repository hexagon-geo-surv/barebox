// SPDX-License-Identifier: GPL-2.0-only

#include <init.h>
#include <linux/sizes.h>
#include <io.h>
#include <linux/string.h>
#include <debug_ll.h>
#include <mach/omap/debug_ll.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <mach/omap/am33xx-silicon.h>
#include <mach/omap/am33xx-clock.h>
#include <mach/omap/generic.h>
#include <mach/omap/sdrc.h>
#include <mach/omap/sys_info.h>
#include <mach/omap/syslib.h>
#include <mach/omap/am33xx-mux.h>
#include <mach/omap/am33xx-generic.h>
#include <mach/omap/xload.h>

#include "beaglebone.h"

#define DDR2_RD_DQS		0x12
#define DDR2_PHY_FIFO_WE	0x80
#define	DDR2_WR_DQS		0x00
#define	DDR2_PHY_WRLVL		0x00
#define	DDR2_PHY_GATELVL	0x00
#define	DDR2_PHY_WR_DATA	0x40

static const struct am33xx_cmd_control ddr2_cmd_ctrl = {
	.slave_ratio0	= 0x80,
	.dll_lock_diff0	= 0x0,
	.invert_clkout0	= 0x0,
	.slave_ratio1	= 0x80,
	.dll_lock_diff1	= 0x0,
	.invert_clkout1	= 0x0,
	.slave_ratio2	= 0x80,
	.dll_lock_diff2	= 0x0,
	.invert_clkout2	= 0x0,
};

static const struct am33xx_emif_regs ddr2_regs = {
	.emif_read_latency	= 0x100005,
	.emif_tim1		= 0x0666B3C9,
	.emif_tim2		= 0x243631CA,
	.emif_tim3		= 0x0000033F,
	.ocp_config		= 0x00141414,
	.sdram_config		= 0x41805332,
	.sdram_config2		= 0x41805332,
	.sdram_ref_ctrl		= 0x0000081A,
};

static const struct am33xx_ddr_data ddr2_data = {
	.rd_slave_ratio0        = DDR2_RD_DQS,
	.wr_dqs_slave_ratio0    = DDR2_WR_DQS,
	.wrlvl_init_ratio0	= DDR2_PHY_WRLVL,
	.gatelvl_init_ratio0	= DDR2_PHY_GATELVL,
	.fifo_we_slave_ratio0	= DDR2_PHY_FIFO_WE,
	.wr_slave_ratio0        = DDR2_PHY_WR_DATA,
	.use_rank0_delay	= 0x01,
	.dll_lock_diff0		= 0x0,
};

static const struct am33xx_ddr_data ddr3_data = {
	.rd_slave_ratio0        = 0x38,
	.wr_dqs_slave_ratio0    = 0x44,
	.fifo_we_slave_ratio0	= 0x94,
	.wr_slave_ratio0        = 0x7D,
	.use_rank0_delay	= 0x01,
	.dll_lock_diff0		= 0x0,
};

static const struct am33xx_cmd_control ddr3_cmd_ctrl = {
	.slave_ratio0	= 0x80,
	.dll_lock_diff0	= 0x1,
	.invert_clkout0	= 0x0,
	.slave_ratio1	= 0x80,
	.dll_lock_diff1	= 0x1,
	.invert_clkout1	= 0x0,
	.slave_ratio2	= 0x80,
	.dll_lock_diff2	= 0x1,
	.invert_clkout2	= 0x0,
};

static const struct am33xx_emif_regs ddr3_regs = {
	.emif_read_latency	= 0x100007,
	.emif_tim1		= 0x0AAAD4DB,
	.emif_tim2		= 0x266B7FDA,
	.emif_tim3		= 0x501F867F,
	.ocp_config		= 0x00141414,
	.zq_config		= 0x50074BE4,
	.sdram_config		= 0x61C05332,
	.sdram_config2		= 0x0,
	.sdram_ref_ctrl		= 0xC30,
};

extern char __dtb_z_am335x_boneblack_start[];
extern char __dtb_z_am335x_bone_common_start[];
extern char __dtb_z_am335x_bone_start[];

static void __udelay(int us)
{
	volatile int i;

	for (i = 0; i < us; i++);
}

/**
 * @brief The basic entry point for board initialization.
 *
 * This is called as part of machine init (after arch init).
 * This is again called with stack in SRAM, so not too many
 * constructs possible here.
 *
 * @return void
 */
static noinline int beaglebone_sram_init(void)
{
	uint32_t sdram_size;

	if (is_beaglebone_black())
		sdram_size = SZ_512M;
	else
		sdram_size = SZ_256M;

	omap_watchdog_disable(IOMEM(AM33XX_WDT_BASE));

	/* Setup the PLLs and the clocks for the peripherals */
	if (is_beaglebone_black()) {
		am33xx_pll_init(MPUPLL_M_500, DDRPLL_M_400);
		am335x_sdram_init(0x18B, &ddr3_cmd_ctrl, &ddr3_regs,
				&ddr3_data);
	} else {
		am33xx_pll_init(MPUPLL_M_500, DDRPLL_M_266);
		am335x_sdram_init(0x18B, &ddr2_cmd_ctrl, &ddr2_regs,
				&ddr2_data);
	}

	am33xx_uart_soft_reset((void *)AM33XX_UART0_BASE);
	am33xx_enable_uart0_pin_mux();
	omap_debug_ll_init();
	putc_ll('>');

	/*
	 * Some (~5%) Beaglebone Black received from SEEED in batches
	 * after autumn 2024 require a delay to be able to warm start
	 * after reset
	 */
	__udelay(3000);

	if (IS_ENABLED(CONFIG_OMAP_BUILD_IFT))
		barebox_arm_entry(OMAP_DRAM_ADDR_SPACE_START, sdram_size,
				  __dtb_z_am335x_bone_common_start);
	else
		am33xx_hsmmc_start_image();
}

ENTRY_FUNCTION(start_am33xx_beaglebone_sram, bootinfo, r1, r2)
{
	am33xx_save_bootinfo((void *)bootinfo);

	/*
	 * Setup C environment, the board init code uses global variables.
	 * Stackpointer has already been initialized by the ROM code.
	 */
	relocate_to_current_adr();
	setup_c();

	beaglebone_sram_init();
}

ENTRY_FUNCTION(start_am33xx_beaglebone_sdram, r0, r1, r2)
{
	uint32_t sdram_size;
	void *fdt;

	if (is_beaglebone_black()) {
		sdram_size = SZ_512M;
		fdt = __dtb_z_am335x_boneblack_start;
	} else {
		sdram_size = SZ_256M;
		fdt = __dtb_z_am335x_bone_start;
	}

	fdt += get_runtime_offset();

	barebox_arm_entry(OMAP_DRAM_ADDR_SPACE_START, sdram_size, fdt);
}
