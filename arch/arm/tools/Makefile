# SPDX-License-Identifier: GPL-2.0-only

#
# linux/arch/arm/tools/Makefile
#
# Copyright (C) 2001 Russell King
#

include/generated/mach-types.h: $(src)/gen-mach-types $(src)/mach-types
	$(Q)$(kecho) '  Generating $@'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(AWK) -f $^ > $@
