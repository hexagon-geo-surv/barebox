// SPDX-License-Identifier: GPL-2.0+
#include <common.h>
#include <io.h>
#include <asm/syscounter.h>
#include <asm/system.h>
#include <mach/layerscape/errata.h>
#include <mach/layerscape/lowlevel.h>
#include <soc/fsl/immap_lsch2.h>
#include <soc/fsl/fsl_immap.h>
#include <soc/fsl/scfg.h>

enum csu_cslx_access {
	CSU_NS_SUP_R = 0x08,
	CSU_NS_SUP_W = 0x80,
	CSU_NS_SUP_RW = 0x88,
	CSU_NS_USER_R = 0x04,
	CSU_NS_USER_W = 0x40,
	CSU_NS_USER_RW = 0x44,
	CSU_S_SUP_R = 0x02,
	CSU_S_SUP_W = 0x20,
	CSU_S_SUP_RW = 0x22,
	CSU_S_USER_R = 0x01,
	CSU_S_USER_W = 0x10,
	CSU_S_USER_RW = 0x11,
	CSU_ALL_RW = 0xff,
};

struct csu_ns_dev {
	unsigned long ind;
	uint32_t val;
};

enum csu_cslx_ind {
	CSU_CSLX_PCIE2_IO = 0,
	CSU_CSLX_PCIE1_IO,
	CSU_CSLX_MG2TPR_IP,
	CSU_CSLX_IFC_MEM,
	CSU_CSLX_OCRAM,
	CSU_CSLX_GIC,
	CSU_CSLX_PCIE1,
	CSU_CSLX_OCRAM2,
	CSU_CSLX_QSPI_MEM,
	CSU_CSLX_PCIE2,
	CSU_CSLX_SATA,
	CSU_CSLX_USB1,
	CSU_CSLX_QM_BM_SWPORTAL,
	CSU_CSLX_PCIE3 = 16,
	CSU_CSLX_PCIE3_IO,
	CSU_CSLX_USB3 = 20,
	CSU_CSLX_USB2,
	CSU_CSLX_PFE = 23,
	CSU_CSLX_SERDES = 32,
	CSU_CSLX_QDMA,
	CSU_CSLX_LPUART2,
	CSU_CSLX_LPUART1,
	CSU_CSLX_LPUART4,
	CSU_CSLX_LPUART3,
	CSU_CSLX_LPUART6,
	CSU_CSLX_LPUART5,
	CSU_CSLX_DSPI1 = 41,
	CSU_CSLX_QSPI,
	CSU_CSLX_ESDHC,
	CSU_CSLX_IFC = 45,
	CSU_CSLX_I2C1,
	CSU_CSLX_USB_2,
	CSU_CSLX_I2C3 = 48,
	CSU_CSLX_I2C2,
	CSU_CSLX_DUART2 = 50,
	CSU_CSLX_DUART1,
	CSU_CSLX_WDT2,
	CSU_CSLX_WDT1,
	CSU_CSLX_EDMA,
	CSU_CSLX_SYS_CNT,
	CSU_CSLX_DMA_MUX2,
	CSU_CSLX_DMA_MUX1,
	CSU_CSLX_DDR,
	CSU_CSLX_QUICC,
	CSU_CSLX_DCFG_CCU_RCPM = 60,
	CSU_CSLX_SECURE_BOOTROM,
	CSU_CSLX_SFP,
	CSU_CSLX_TMU,
	CSU_CSLX_SECURE_MONITOR,
	CSU_CSLX_SCFG,
	CSU_CSLX_FM = 66,
	CSU_CSLX_SEC5_5,
	CSU_CSLX_BM,
	CSU_CSLX_QM,
	CSU_CSLX_GPIO2 = 70,
	CSU_CSLX_GPIO1,
	CSU_CSLX_GPIO4,
	CSU_CSLX_GPIO3,
	CSU_CSLX_PLATFORM_CONT,
	CSU_CSLX_CSU,
	CSU_CSLX_IIC4 = 77,
	CSU_CSLX_WDT4,
	CSU_CSLX_WDT3,
	CSU_CSLX_ESDHC2 = 80,
	CSU_CSLX_WDT5 = 81,
	CSU_CSLX_SAI2,
	CSU_CSLX_SAI1,
	CSU_CSLX_SAI4,
	CSU_CSLX_SAI3,
	CSU_CSLX_FTM2 = 86,
	CSU_CSLX_FTM1,
	CSU_CSLX_FTM4,
	CSU_CSLX_FTM3,
	CSU_CSLX_FTM6 = 90,
	CSU_CSLX_FTM5,
	CSU_CSLX_FTM8,
	CSU_CSLX_FTM7,
	CSU_CSLX_DSCR = 121,
};

static struct csu_ns_dev ns_dev[] = {
	 {CSU_CSLX_PCIE2_IO, CSU_ALL_RW},
	 {CSU_CSLX_PCIE1_IO, CSU_ALL_RW},
	 {CSU_CSLX_MG2TPR_IP, CSU_ALL_RW},
	 {CSU_CSLX_IFC_MEM, CSU_ALL_RW},
	 {CSU_CSLX_OCRAM, CSU_ALL_RW},
	 {CSU_CSLX_GIC, CSU_ALL_RW},
	 {CSU_CSLX_PCIE1, CSU_ALL_RW},
	 {CSU_CSLX_OCRAM2, CSU_ALL_RW},
	 {CSU_CSLX_QSPI_MEM, CSU_ALL_RW},
	 {CSU_CSLX_PCIE2, CSU_ALL_RW},
	 {CSU_CSLX_SATA, CSU_ALL_RW},
	 {CSU_CSLX_USB1, CSU_ALL_RW},
	 {CSU_CSLX_QM_BM_SWPORTAL, CSU_ALL_RW},
	 {CSU_CSLX_PCIE3, CSU_ALL_RW},
	 {CSU_CSLX_PCIE3_IO, CSU_ALL_RW},
	 {CSU_CSLX_USB3, CSU_ALL_RW},
	 {CSU_CSLX_USB2, CSU_ALL_RW},
	 {CSU_CSLX_PFE, CSU_ALL_RW},
	 {CSU_CSLX_SERDES, CSU_ALL_RW},
	 {CSU_CSLX_QDMA, CSU_ALL_RW},
	 {CSU_CSLX_LPUART2, CSU_ALL_RW},
	 {CSU_CSLX_LPUART1, CSU_ALL_RW},
	 {CSU_CSLX_LPUART4, CSU_ALL_RW},
	 {CSU_CSLX_LPUART3, CSU_ALL_RW},
	 {CSU_CSLX_LPUART6, CSU_ALL_RW},
	 {CSU_CSLX_LPUART5, CSU_ALL_RW},
	 {CSU_CSLX_DSPI1, CSU_ALL_RW},
	 {CSU_CSLX_QSPI, CSU_ALL_RW},
	 {CSU_CSLX_ESDHC, CSU_ALL_RW},
	 {CSU_CSLX_IFC, CSU_ALL_RW},
	 {CSU_CSLX_I2C1, CSU_ALL_RW},
	 {CSU_CSLX_I2C3, CSU_ALL_RW},
	 {CSU_CSLX_I2C2, CSU_ALL_RW},
	 {CSU_CSLX_DUART2, CSU_ALL_RW},
	 {CSU_CSLX_DUART1, CSU_ALL_RW},
	 {CSU_CSLX_WDT2, CSU_ALL_RW},
	 {CSU_CSLX_WDT1, CSU_ALL_RW},
	 {CSU_CSLX_EDMA, CSU_ALL_RW},
	 {CSU_CSLX_SYS_CNT, CSU_ALL_RW},
	 {CSU_CSLX_DMA_MUX2, CSU_ALL_RW},
	 {CSU_CSLX_DMA_MUX1, CSU_ALL_RW},
	 {CSU_CSLX_DDR, CSU_ALL_RW},
	 {CSU_CSLX_QUICC, CSU_ALL_RW},
	 {CSU_CSLX_DCFG_CCU_RCPM, CSU_ALL_RW},
	 {CSU_CSLX_SECURE_BOOTROM, CSU_ALL_RW},
	 {CSU_CSLX_SFP, CSU_ALL_RW},
	 {CSU_CSLX_TMU, CSU_ALL_RW},
	 {CSU_CSLX_SECURE_MONITOR, CSU_ALL_RW},
	 {CSU_CSLX_SCFG, CSU_ALL_RW},
	 {CSU_CSLX_FM, CSU_ALL_RW},
	 {CSU_CSLX_SEC5_5, CSU_ALL_RW},
	 {CSU_CSLX_BM, CSU_ALL_RW},
	 {CSU_CSLX_QM, CSU_ALL_RW},
	 {CSU_CSLX_GPIO2, CSU_ALL_RW},
	 {CSU_CSLX_GPIO1, CSU_ALL_RW},
	 {CSU_CSLX_GPIO4, CSU_ALL_RW},
	 {CSU_CSLX_GPIO3, CSU_ALL_RW},
	 {CSU_CSLX_PLATFORM_CONT, CSU_ALL_RW},
	 {CSU_CSLX_CSU, CSU_ALL_RW},
	 {CSU_CSLX_IIC4, CSU_ALL_RW},
	 {CSU_CSLX_WDT4, CSU_ALL_RW},
	 {CSU_CSLX_WDT3, CSU_ALL_RW},
	 {CSU_CSLX_ESDHC2, CSU_ALL_RW},
	 {CSU_CSLX_WDT5, CSU_ALL_RW},
	 {CSU_CSLX_SAI2, CSU_ALL_RW},
	 {CSU_CSLX_SAI1, CSU_ALL_RW},
	 {CSU_CSLX_SAI4, CSU_ALL_RW},
	 {CSU_CSLX_SAI3, CSU_ALL_RW},
	 {CSU_CSLX_FTM2, CSU_ALL_RW},
	 {CSU_CSLX_FTM1, CSU_ALL_RW},
	 {CSU_CSLX_FTM4, CSU_ALL_RW},
	 {CSU_CSLX_FTM3, CSU_ALL_RW},
	 {CSU_CSLX_FTM6, CSU_ALL_RW},
	 {CSU_CSLX_FTM5, CSU_ALL_RW},
	 {CSU_CSLX_FTM8, CSU_ALL_RW},
	 {CSU_CSLX_FTM7, CSU_ALL_RW},
	 {CSU_CSLX_DSCR, CSU_ALL_RW},
};

static void set_devices_ns_access(unsigned long index, u16 val)
{
	u32 *base = IOMEM(LSCH2_CSU_ADDR);
	u32 *reg;
	uint32_t tmp;

	reg = base + index / 2;
	tmp = in_be32(reg);
	if (index % 2 == 0) {
		tmp &= 0x0000ffff;
		tmp |= val << 16;
	} else {
		tmp &= 0xffff0000;
		tmp |= val;
	}

	out_be32(reg, tmp);
}

static void init_csu(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ns_dev); i++)
		set_devices_ns_access(ns_dev[i].ind, ns_dev[i].val);
}

void ls1046a_init_lowlevel(void)
{
	struct ccsr_cci400 __iomem *cci = IOMEM(LSCH2_CCI400_ADDR);
	struct ccsr_scfg *scfg = IOMEM(LSCH2_SCFG_ADDR);

	scfg_init(SCFG_ENDIANESS_BIG);
	init_csu();
	ls1046a_init_l2_latency();
	set_cntfrq(25000000);
	syscnt_enable(IOMEM(LSCH2_SYS_COUNTER_ADDR));

	/* Make DMA master reads and writes snoopable */
	setbits_be32(&scfg->snpcnfgcr, SCFG_SNPCNFGCR_SECRDSNP |
		SCFG_SNPCNFGCR_SECWRSNP | SCFG_SNPCNFGCR_USB1RDSNP |
		SCFG_SNPCNFGCR_USB1WRSNP | SCFG_SNPCNFGCR_USB2RDSNP |
		SCFG_SNPCNFGCR_USB2WRSNP | SCFG_SNPCNFGCR_USB3RDSNP |
		SCFG_SNPCNFGCR_USB3WRSNP | SCFG_SNPCNFGCR_SATARDSNP |
		SCFG_SNPCNFGCR_SATAWRSNP | SCFG_SNPCNFGCR_EDMASNP);

	/*
	 * Enable snoop requests and DVM message requests for
	 * Slave insterface S4 (A53 core cluster)
	 */
	if (current_el() == 3) {
		out_le32(&cci->slave[4].snoop_ctrl,
			CCI400_DVM_MESSAGE_REQ_EN | CCI400_SNOOP_REQ_EN);
	}

	ls1046a_errata();
}
