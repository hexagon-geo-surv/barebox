# SPDX-License-Identifier: GPL-2.0-only

hostprogs-always-$(CONFIG_ARCH_TEGRA) += cbootimage

HOSTLDLIBS_cbootimage  = '-lm'

cbootimage-objs := \
	cbootimage.o \
	data_layout.o \
	set.o \
	crypto.o \
	aes_ref.o \
	context.o \
	parse.o \
	t124/parse_t124.o \
	t114/parse_t114.o \
	t30/parse_t30.o \
	t20/parse_t20.o \
	t124/nvbctlib_t124.o \
	t114/nvbctlib_t114.o \
	t30/nvbctlib_t30.o \
	t20/nvbctlib_t20.o
