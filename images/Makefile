# SPDX-License-Identifier: GPL-2.0-only
#
# barebox image generation Makefile
#
# This Makefile generates multiple images from a common barebox image
# and different pbl (pre bootloader) images. Optionally the result is
# encapsulated in SoC (or SoC boot type) specific image formats.
#
# The basic idea here is that we generate a single barebox main binary. This
# is compressed and included into a board specific PBL image. The PBL images
# are generally named after their entrypoints. So a pcm038 specific pbl will
# generate the following files:
#
# start_imx27_pcm038.pbl - The ELF file, linked with the entrypoint start_imx27_pcm038
# start_imx27_pcm038.pblb - The raw binary of the above.
# start_imx27_pcm038.pbl.map - The linker map file
# start_imx27_pcm038.pbl.s - the disassembled ELF, generated with:
#                            make images/start_imx27_pcm038.pbl.s
#
# Example Makefile snippets for the i.MX51 babbage board (for readability in opposite
# order):
#
## image-$(CONFIG_MACH_FREESCALE_MX51_PDK) += barebox-imx51-babbage.img
#
# For CONFIG_MACH_FREESCALE_MX51_PDK build barebox-imx51-babbage.img
#
## FILE_barebox-imx51-babbage.img = start_imx51_babbage.pblb.imximg
#
# barebox-imx51-babbage.img should be generated (copied) from
# start_imx51_babbage.pblb.imximg. This copy process is only done so that we
# can generate images with a sane name. So what we really need for this
# board is a i.MX specific image, a .imximg
#
## CFG_start_imx51_babbage.pblb.imximg = $(board)/freescale-mx51-pdk/flash-header.imxcfg
#
# The .imximg can be generated from a .pblb using a rule specified in Makefile.imx.
# The configfile needed for this image is specified with CFG_<filename> = <configfile>
#
## pblb-$(CONFIG_MACH_FREESCALE_MX51_PDK) += start_imx51_babbage
#
# For this image we need a pblb (self extracting barebox binary) with
# start_imx51_babbage as entrypoint. start_imx51_babbage will be used
# both as entrypoint and as filename
#

include scripts/Makefile.lib

quiet_cmd_objcopy_bin = OBJCOPYB $@
      cmd_objcopy_bin = \
		$(OBJCOPY) -O binary $(OBJCOPYFLAGS) $(OBJCOPYFLAGS_$(@F)) $< $@ && \
		$(objtree)/scripts/fix_size -i -f $@

pbl-lds := $(obj)/pbl.lds
extra-y += $(pbl-lds)

$(pbl-lds): $(obj)/../arch/$(SRCARCH)/lib/pbl.lds.S FORCE
	$(call if_changed_dep,cpp_lds_S)

ifeq ($(CONFIG_PBL_IMAGE_NO_PIGGY),)
BAREBOX_PIGGY_OBJS = $(obj)/piggy.o $(obj)/sha_sum.o
endif

quiet_cmd_elf__ ?= LD      $@
      cmd_elf__ ?= $(LD) $(LDFLAGS_$(patsubst .%,%,$(suffix $(@F)))) 	\
		-e $(2) -Map $@.map $(LDFLAGS_$(@F)) -o $@		\
		--gc-sections --defsym=__pbl_board_entry=$(2)  		\
		-T $(pbl-lds)						\
		--whole-archive $(BAREBOX_PBL_OBJS) $(BAREBOX_PIGGY_OBJS) \

PBL_CPPFLAGS	+= -fdata-sections -ffunction-sections

$(obj)/%.pbl: $(pbl-lds) $(BAREBOX_PBL_OBJS) $(BAREBOX_PIGGY_OBJS) FORCE
	$(call if_changed,elf__,$(*F))
	$(call if_changed,prelink__)

$(obj)/%.elf: $(pbl-lds) $(BAREBOX_PBL_OBJS) $(BAREBOX_PIGGY_OBJS) FORCE
	$(call if_changed,elf__,$(*F))
	$(call if_changed,prelink__)

$(obj)/%.pblb: $(obj)/%.pbl FORCE
	$(call if_changed,objcopy_bin,$(*F))
	$(call if_changed,check_missing_fw,$@,$<)
	$(call if_changed,check_file_size,$@,$(CONFIG_BAREBOX_MAX_IMAGE_SIZE))

#
# For each start symbol create three variables containing the
# relevant sizes in the image:
# PBL_CODE_SIZE_$(symbol)   - contains the pure code size of the image
# PBL_MEMORY_SIZE_$(symbol) - contains the code size + bss size of the
#                             image
# PBL_IMAGE_SIZE_$(symbol)  - contains the full image size including the
#                             compressed payload
#
	$(eval PBL_CODE_SIZE_$* = \
		$(shell $(srctree)/scripts/extract_symbol_offset pbl_code_size $^))
	$(eval PBL_MEMORY_SIZE_$*= \
		$(shell $(srctree)/scripts/extract_symbol_offset pbl_memory_size $^))
	$(eval PBL_IMAGE_SIZE_$*= \
		$(shell $(srctree)/scripts/extract_symbol_offset pbl_image_size $^))

#
# if MAX_PBL_xxx_SIZE_$(symbol) is defined it contains the maximum size the
# code/memory/image for this PBL may get. Check these values.
#
	$(if $(MAX_PBL_CODE_SIZE_$*), \
		$(call if_changed,check_size,$(PBL_CODE_SIZE_$*),$(MAX_PBL_CODE_SIZE_$*)) \
	)

	$(if $(MAX_PBL_MEMORY_SIZE_$*), \
		$(call if_changed,check_size,$(PBL_MEMORY_SIZE_$*),$(MAX_PBL_MEMORY_SIZE_$*)) \
	)

	$(if $(MAX_PBL_IMAGE_SIZE_$*), \
		$(call if_changed,check_size,$(PBL_IMAGE_SIZE_$*),$(MAX_PBL_IMAGE_SIZE_$*)) \
	)

$(obj)/%.s: $(obj)/% FORCE
	$(call if_changed,disasm)

quiet_cmd_itb = FIT.ITS $@
cmd_itb = $(CPP) $(dtc_cpp_flags) -x assembler-with-cpp -o $(dtc-tmp) \
	-D'$(subst -,_,$(*F))_dts=1' $(foreach f,$< $(2),-include '$(f)') /dev/null ; \
	$(MKIMAGE) -f $(dtc-tmp) $@ -E \
	cat $(depfile).pre $(depfile).dtc > $(depfile)

$(obj)/%.itb: $(obj)/%.its FORCE
	$(call if_changed,itb)

.SECONDEXPANSION:
$(obj)/%.fit: $(obj)/$$(FILE_$$(@F)) $(dtstree)/dtbs-list FORCE
	$(call if_changed,fit)

$(obj)/piggy.o: $(obj)/barebox.z FORCE

$(obj)/sha_sum.o: $(obj)/barebox.sha.bin FORCE

$(obj)/barebox.sha.bin: $(obj)/barebox.sum FORCE
	$(call if_changed,sha256bin)

$(obj)/barebox.sum: $(obj)/barebox.z FORCE
	$(call if_changed,sha256sum,$<)


# barebox.z - compressed barebox binary
# ----------------------------------------------------------------
$(obj)/barebox.z: $(obj)/../$(BAREBOX_PROPER) FORCE
	$(call if_changed,$(suffix_y))

# %.img - create a copy from another file
# ----------------------------------------------------------------

missing_fw = $(strip $(wildcard $(obj)/$(FILE_$(@F)).missing-firmware $(basename $(obj)/$(FILE_$(@F))).missing-firmware))

quiet_cmd_report_missing_fw = # no message as we collect info for later
      cmd_report_missing_fw = $(if $2,cat $2 >$@.missing-firmware;$(cmd_delete),rm -f $@.missing-firmware;$(cmd_shipped))

.SECONDEXPANSION:
$(obj)/%.img: $(obj)/$$(FILE_$$(@F)) FORCE
	$(Q)if [ -z $(FILE_$(@F)) ]; then echo "FILE_$(@F) empty!"; false; fi
	$(call if_changed,report_missing_fw,$(missing_fw))

board = $(srctree)/arch/$(SRCARCH)/boards
objboard = $(objtree)/arch/$(SRCARCH)/boards

include $(srctree)/images/Makefile.am33xx
include $(srctree)/images/Makefile.am35xx
include $(srctree)/images/Makefile.ar231x
include $(srctree)/images/Makefile.ath79
include $(srctree)/images/Makefile.bcm283x
include $(srctree)/images/Makefile.bcm47xx
include $(srctree)/images/Makefile.clps711x
include $(srctree)/images/Makefile.imx
include $(srctree)/images/Makefile.loongson
include $(srctree)/images/Makefile.malta
include $(srctree)/images/Makefile.mvebu
include $(srctree)/images/Makefile.mxs
include $(srctree)/images/Makefile.omap3
include $(srctree)/images/Makefile.rockchip
include $(srctree)/images/Makefile.sandbox
include $(srctree)/images/Makefile.socfpga
include $(srctree)/images/Makefile.stm32mp
include $(srctree)/images/Makefile.tegra
include $(srctree)/images/Makefile.versatile
include $(srctree)/images/Makefile.vexpress
include $(srctree)/images/Makefile.xburst
include $(srctree)/images/Makefile.at91
include $(srctree)/images/Makefile.zynq
include $(srctree)/images/Makefile.zynqmp
include $(srctree)/images/Makefile.layerscape
include $(srctree)/images/Makefile.riscv
include $(srctree)/images/Makefile.k3


pblb-$(CONFIG_BOARD_GENERIC_DT) += start_dt_2nd
FILE_barebox-dt-2nd.img = start_dt_2nd.pblb
image-$(CONFIG_BOARD_GENERIC_DT) += barebox-dt-2nd.img

fit-image = barebox-$(call remove_quotes,$(CONFIG_ARCH_LINUX_NAME)).fit
FILE_$(fit-image) = barebox-dt-2nd.img
image-$(CONFIG_BOARD_GENERIC_FIT) += $(fit-image)
targets += $(fit-name)

ifdef CONFIG_ARM
pblb-$(CONFIG_PBL_SINGLE_IMAGE) += start_pbl
FILE_barebox.img = start_pbl.pblb
image-$(CONFIG_PBL_SINGLE_IMAGE) += barebox.img
endif

ifneq ($(pblx-y)$(pblx-),)
  $(error pblx- has been removed. Please use pblb- instead.)
endif

targets += $(image-y) pbl.lds barebox.x barebox.z piggy.o sha_sum.o barebox.sha.bin barebox.sum
targets += $(patsubst %,%.pblb,$(pblb-y))
targets += $(patsubst %,%.pbl,$(pblb-y))
targets += $(patsubst %,%.s,$(pblb-y))
targets += $(foreach m, $(image-y), $(FILE_$(m)))

# Images with full paths
image-y-path := $(addprefix $(obj)/,$(image-y))
# File will have a list of images generated
flash-list := $(obj)/../barebox-flash-images
# Symlink, which will point to non-existent 'multi-image-build' if there are
# multiple images
flash-link := $(obj)/../barebox-flash-image
link-dest := $(if $(filter 1,$(words $(image-y))),$(image-y-path),multi-image-build)

symlink-y-path := $(addprefix $(obj)/,$(symlink-y))

multi-image-build:

images: $(image-y-path) $(flash-link) $(flash-list) $(symlink-y-path) FORCE
	@echo "images built:"
	@for i in $(image-y); do \
	  if [ -s $(obj)/$$i ]; then echo $$i; \
	  else >&2 echo "** $$i skipped due to missing firmware **"; \
	  $(if $(CONFIG_MISSING_FIRMWARE_ERROR), >&2 sed 's/^/\t/' <$(obj)/$${i}.missing-firmware; missing=1;) \
	  fi; done; if [ -n "$$missing" ]; then \
	    echo >&2 "Firmware missing in CONFIG_MISSING_FIRMWARE_ERROR=y build"; exit 1; fi
	@for i in $(symlink-y); do echo $$i; done

__images_install: images
	@for i in $(image-y-path); do if [ -s $$i ]; then install -t "$(INSTALL_PATH)" $$i; fi; done

PHONY += __images_install

$(flash-link): $(link-dest) FORCE
	$(call if_changed,symlink)

$(flash-list): $(image-y-path)
	@for i in $^; do if [ -s $$i ]; then echo $$i; fi; done > $@

.SECONDEXPANSION:
$(symlink-y-path): $(obj)/$$(SYMLINK_TARGET_$$(@F)) FORCE
	$(call if_changed,symlink_quiet)

clean-files := *.pbl *.pblb *.elf *.map start_*.imximg *.img barebox.z start_*.kwbimg \
	start_*.kwbuartimg *.socfpgaimg *.mlo *.t20img *.t20img.cfg *.t30img \
	*.t30img.cfg *.t124img *.t124img.cfg *.mlospi *.mlo *.mxsbs *.mxssd *.rkimg \
	start_*.simximg start_*.usimximg start_*.pimximg start_*.psimximg *.zynqimg \
	*.image *.swapped *.missing-firmware
clean-files += pbl.lds
