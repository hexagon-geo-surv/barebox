# SPDX-License-Identifier: GPL-2.0-only

obj-y += imx.o iomux-imx.o power.o
pbl-y += iomux-imx.o
obj-$(CONFIG_ARCH_IMX23) += clocksource-imx23.o usb-imx23.o soc-imx23.o
obj-$(CONFIG_ARCH_IMX28) += clocksource-imx28.o usb-imx28.o soc-imx28.o
obj-$(CONFIG_MXS_OCOTP) += ocotp.o
obj-$(CONFIG_MXS_CMD_BCB) += bcb.o
pbl-y += power-init.o mem-init.o lradc-init.o
