# SPDX-License-Identifier: GPL-2.0-only

CFLAGS_tegra_avp_init.o := -mcpu=arm7tdmi -march=armv4t
CFLAGS_tegra_avp_init.pbl.o := -mcpu=arm7tdmi -march=armv4t
lwl-y += tegra_avp_init.o
lwl-y += tegra_maincomplex_init.o
obj-y += tegra20.o
obj-y += tegra20-pmc.o
obj-y += tegra20-timer.o
obj-$(CONFIG_BAREBOX_UPDATE) += tegra-bbu.o
