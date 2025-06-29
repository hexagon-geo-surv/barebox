// SPDX-License-Identifier: GPL-2.0+
/*
 *  Copyright (C) 2018 Texas Instruments Incorporated - https://www.ti.com
 *  Author: Peter Ujfalusi <peter.ujfalusi@ti.com>
 */
#define pr_fmt(fmt) "udma: " fmt

#include <io.h>
#include <malloc.h>
#include <stdio.h>
#include <linux/bitops.h>
#include <linux/sizes.h>
#include <linux/printk.h>
#include <dma.h>
#include <soc/ti/ti-udma.h>
#include <soc/ti/ti_sci_protocol.h>
#include <dma-devices.h>
#include <soc/ti/cppi5.h>
#include <soc/ti/k3-navss-ringacc.h>
#include <clock.h>
#include <linux/bitmap.h>
#include <driver.h>
#include <linux/device.h>

#include "k3-udma-hwdef.h"
#include "k3-psil-priv.h"
#include "k3-udma.h"

struct udma_chan;

static const char * const mmr_names[] = {
	[MMR_GCFG] = "gcfg",
	[MMR_BCHANRT] = "bchanrt",
	[MMR_RCHANRT] = "rchanrt",
	[MMR_TCHANRT] = "tchanrt",
	[MMR_RCHAN] = "rchan",
	[MMR_TCHAN] = "tchan",
	[MMR_RFLOW] = "rflow",
};

static inline int udma_navss_psil_pair(struct udma_dev *ud, u32 src_thread,
				       u32 dst_thread)
{
	struct udma_tisci_rm *tisci_rm = &ud->tisci_rm;

	dst_thread |= UDMA_PSIL_DST_THREAD_ID_OFFSET;

	return tisci_rm->tisci_psil_ops->pair(tisci_rm->tisci,
					      tisci_rm->tisci_navss_dev_id,
					      src_thread, dst_thread);
}

static inline int udma_navss_psil_unpair(struct udma_dev *ud, u32 src_thread,
					 u32 dst_thread)
{
	struct udma_tisci_rm *tisci_rm = &ud->tisci_rm;

	dst_thread |= UDMA_PSIL_DST_THREAD_ID_OFFSET;

	return tisci_rm->tisci_psil_ops->unpair(tisci_rm->tisci,
						tisci_rm->tisci_navss_dev_id,
						src_thread, dst_thread);
}

#define UDMA_RCHAN_RFLOW_RNG_FLOWID_CNT_SHIFT	(16)

/* How SRC/DST tag should be updated by UDMA in the descriptor's Word 3 */
#define UDMA_RFLOW_SRCTAG_NONE		0
#define UDMA_RFLOW_SRCTAG_CFG_TAG	1
#define UDMA_RFLOW_SRCTAG_FLOW_ID	2
#define UDMA_RFLOW_SRCTAG_SRC_TAG	4

#define UDMA_RFLOW_DSTTAG_NONE		0
#define UDMA_RFLOW_DSTTAG_CFG_TAG	1
#define UDMA_RFLOW_DSTTAG_FLOW_ID	2
#define UDMA_RFLOW_DSTTAG_DST_TAG_LO	4
#define UDMA_RFLOW_DSTTAG_DST_TAG_HI	5

#define UDMA_RFLOW_RFC_DEFAULT	\
	((UDMA_RFLOW_SRCTAG_NONE <<  UDMA_RFLOW_RFC_SRC_TAG_HI_SEL_SHIFT) | \
	 (UDMA_RFLOW_SRCTAG_SRC_TAG << UDMA_RFLOW_RFC_SRC_TAG_LO_SEL_SHIFT) | \
	 (UDMA_RFLOW_DSTTAG_DST_TAG_HI << UDMA_RFLOW_RFC_DST_TAG_HI_SEL_SHIFT) | \
	 (UDMA_RFLOW_DSTTAG_DST_TAG_LO << UDMA_RFLOW_RFC_DST_TAG_LO_SE_SHIFT))

#define UDMA_RFLOW_RFx_REG_FDQ_SIZE_SHIFT	(16)

/* TCHAN */
static inline u32 udma_tchan_read(struct udma_tchan *tchan, int reg)
{
	return udma_read(tchan->reg_chan, reg);
}

static inline void udma_tchan_write(struct udma_tchan *tchan, int reg, u32 val)
{
	udma_write(tchan->reg_chan, reg, val);
}

static inline void udma_tchan_update_bits(struct udma_tchan *tchan, int reg,
					  u32 mask, u32 val)
{
	udma_update_bits(tchan->reg_chan, reg, mask, val);
}

/* RCHAN */
static inline u32 udma_rchan_read(struct udma_rchan *rchan, int reg)
{
	return udma_read(rchan->reg_chan, reg);
}

static inline void udma_rchan_write(struct udma_rchan *rchan, int reg, u32 val)
{
	udma_write(rchan->reg_chan, reg, val);
}

static inline void udma_rchan_update_bits(struct udma_rchan *rchan, int reg,
					  u32 mask, u32 val)
{
	udma_update_bits(rchan->reg_chan, reg, mask, val);
}

/* RFLOW */
static inline u32 udma_rflow_read(struct udma_rflow *rflow, int reg)
{
	return udma_read(rflow->reg_rflow, reg);
}

static inline void udma_rflow_write(struct udma_rflow *rflow, int reg, u32 val)
{
	udma_write(rflow->reg_rflow, reg, val);
}

static inline void udma_rflow_update_bits(struct udma_rflow *rflow, int reg,
					  u32 mask, u32 val)
{
	udma_update_bits(rflow->reg_rflow, reg, mask, val);
}

static int udma_get_chan_pair(struct udma_chan *uc)
{
	struct udma_dev *ud = uc->ud;
	int chan_id, end;

	if ((uc->tchan && uc->rchan) && uc->tchan->id == uc->rchan->id) {
		dev_info(ud->dev, "chan%d: already have %d pair allocated\n",
			 uc->id, uc->tchan->id);
		return 0;
	}

	if (uc->tchan) {
		dev_err(ud->dev, "chan%d: already have tchan%d allocated\n",
			uc->id, uc->tchan->id);
		return -EBUSY;
	} else if (uc->rchan) {
		dev_err(ud->dev, "chan%d: already have rchan%d allocated\n",
			uc->id, uc->rchan->id);
		return -EBUSY;
	}

	/* Can be optimized, but let's have it like this for now */
	end = min(ud->tchan_cnt, ud->rchan_cnt);
	for (chan_id = 0; chan_id < end; chan_id++) {
		if (!test_bit(chan_id, ud->tchan_map) &&
		    !test_bit(chan_id, ud->rchan_map))
			break;
	}

	if (chan_id == end)
		return -ENOENT;
	__set_bit(chan_id, ud->tchan_map);
	__set_bit(chan_id, ud->rchan_map);
	uc->tchan = &ud->tchans[chan_id];
	uc->rchan = &ud->rchans[chan_id];
	
	pr_debug("chan%d: got t/rchan%d pair\n", uc->id, chan_id);

	return 0;
}

static int udma_alloc_tchan_sci_req(struct udma_chan *uc)
{
	struct udma_dev *ud = uc->ud;
	int tc_ring = k3_ringacc_get_ring_id(uc->tchan->tc_ring);
	struct ti_sci_msg_rm_udmap_tx_ch_cfg req;
	struct udma_tisci_rm *tisci_rm = &ud->tisci_rm;
	u32 mode;
	int ret;

	if (uc->config.pkt_mode)
		mode = TI_SCI_RM_UDMAP_CHAN_TYPE_PKT_PBRR;
	else
		mode = TI_SCI_RM_UDMAP_CHAN_TYPE_3RDP_BCOPY_PBRR;

	req.valid_params = TI_SCI_MSG_VALUE_RM_UDMAP_CH_CHAN_TYPE_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_CH_FETCH_SIZE_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_CH_CQ_QNUM_VALID;
	req.nav_id = tisci_rm->tisci_dev_id;
	req.index = uc->tchan->id;
	req.tx_chan_type = mode;
	if (uc->config.dir == DMA_MEM_TO_MEM)
		req.tx_fetch_size = sizeof(struct cppi5_desc_hdr_t) >> 2;
	else
		req.tx_fetch_size = cppi5_hdesc_calc_size(uc->config.needs_epib,
							  uc->config.psd_size,
							  0) >> 2;
	req.txcq_qnum = tc_ring;

	ret = tisci_rm->tisci_udmap_ops->tx_ch_cfg(tisci_rm->tisci, &req);
	if (ret) {
		dev_err(ud->dev, "tisci tx alloc failed %d\n", ret);
		return ret;
	}

	return 0;
}

static int udma_alloc_rchan_sci_req(struct udma_chan *uc)
{
	struct udma_dev *ud = uc->ud;
	int fd_ring = k3_ringacc_get_ring_id(uc->rflow->fd_ring);
	int rx_ring = k3_ringacc_get_ring_id(uc->rflow->r_ring);
	int tc_ring = k3_ringacc_get_ring_id(uc->tchan->tc_ring);
	struct ti_sci_msg_rm_udmap_rx_ch_cfg req = { 0 };
	struct ti_sci_msg_rm_udmap_flow_cfg flow_req = { 0 };
	struct udma_tisci_rm *tisci_rm = &ud->tisci_rm;
	u32 mode;
	int ret;

	if (uc->config.pkt_mode)
		mode = TI_SCI_RM_UDMAP_CHAN_TYPE_PKT_PBRR;
	else
		mode = TI_SCI_RM_UDMAP_CHAN_TYPE_3RDP_BCOPY_PBRR;

	req.valid_params = TI_SCI_MSG_VALUE_RM_UDMAP_CH_FETCH_SIZE_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_CH_CQ_QNUM_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_CH_CHAN_TYPE_VALID;
	req.nav_id = tisci_rm->tisci_dev_id;
	req.index = uc->rchan->id;
	req.rx_chan_type = mode;
	if (uc->config.dir == DMA_MEM_TO_MEM) {
		req.rx_fetch_size = sizeof(struct cppi5_desc_hdr_t) >> 2;
		req.rxcq_qnum = tc_ring;
	} else {
		req.rx_fetch_size = cppi5_hdesc_calc_size(uc->config.needs_epib,
							  uc->config.psd_size,
							  0) >> 2;
		req.rxcq_qnum = rx_ring;
	}
	if (ud->match_data->type == DMA_TYPE_UDMA &&
	    uc->rflow->id != uc->rchan->id &&
	    uc->config.dir != DMA_MEM_TO_MEM) {
		req.flowid_start = uc->rflow->id;
		req.flowid_cnt = 1;
		req.valid_params |= TI_SCI_MSG_VALUE_RM_UDMAP_CH_RX_FLOWID_START_VALID |
				    TI_SCI_MSG_VALUE_RM_UDMAP_CH_RX_FLOWID_CNT_VALID;
	}

	ret = tisci_rm->tisci_udmap_ops->rx_ch_cfg(tisci_rm->tisci, &req);
	if (ret) {
		dev_err(ud->dev, "tisci rx %u cfg failed %d\n",
			uc->rchan->id, ret);
		return ret;
	}
	if (uc->config.dir == DMA_MEM_TO_MEM)
		return ret;

	flow_req.valid_params =
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_EINFO_PRESENT_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_PSINFO_PRESENT_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_ERROR_HANDLING_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_DESC_TYPE_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_DEST_QNUM_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_SRC_TAG_HI_SEL_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_SRC_TAG_LO_SEL_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_DEST_TAG_HI_SEL_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_DEST_TAG_LO_SEL_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_FDQ0_SZ0_QNUM_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_FDQ1_QNUM_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_FDQ2_QNUM_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_FDQ3_QNUM_VALID |
			TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_PS_LOCATION_VALID;

	flow_req.nav_id = tisci_rm->tisci_dev_id;
	flow_req.flow_index = uc->rflow->id;

	if (uc->config.needs_epib)
		flow_req.rx_einfo_present = 1;
	else
		flow_req.rx_einfo_present = 0;

	if (uc->config.psd_size)
		flow_req.rx_psinfo_present = 1;
	else
		flow_req.rx_psinfo_present = 0;

	flow_req.rx_error_handling = 0;
	flow_req.rx_desc_type = 0;
	flow_req.rx_dest_qnum = rx_ring;
	flow_req.rx_src_tag_hi_sel = 2;
	flow_req.rx_src_tag_lo_sel = 4;
	flow_req.rx_dest_tag_hi_sel = 5;
	flow_req.rx_dest_tag_lo_sel = 4;
	flow_req.rx_fdq0_sz0_qnum = fd_ring;
	flow_req.rx_fdq1_qnum = fd_ring;
	flow_req.rx_fdq2_qnum = fd_ring;
	flow_req.rx_fdq3_qnum = fd_ring;
	flow_req.rx_ps_location = 0;

	ret = tisci_rm->tisci_udmap_ops->rx_flow_cfg(tisci_rm->tisci,
						     &flow_req);
	if (ret) {
		dev_err(ud->dev, "tisci rx %u flow %u cfg failed %d\n",
			uc->rchan->id, uc->rflow->id, ret);
		return ret;
	}

	return 0;
}

static int udma_alloc_chan_resources(struct udma_chan *uc)
{
	struct udma_dev *ud = uc->ud;
	int ret;

	dev_dbg(ud->dev, "%s: chan:%d as %s\n",
		 __func__, uc->id, udma_get_dir_text(uc->config.dir));

	switch (uc->config.dir) {
	case DMA_MEM_TO_MEM:
		/* Non synchronized - mem to mem type of transfer */
		uc->config.pkt_mode = false;
		ret = udma_get_chan_pair(uc);
		if (ret)
			return ret;

		ret = udma_alloc_tx_resources(uc);
		if (ret)
			goto err_free_res;

		ret = udma_alloc_rx_resources(uc);
		if (ret)
			goto err_free_res;

		uc->config.src_thread = ud->psil_base + uc->tchan->id;
		uc->config.dst_thread = (ud->psil_base + uc->rchan->id) | 0x8000;

		ret = udma_alloc_tchan_sci_req(uc);
		if (ret)
			goto err_free_res;

		ret = udma_alloc_rchan_sci_req(uc);
		if (ret)
			goto err_free_res;
		break;
	case DMA_MEM_TO_DEV:
		/* Slave transfer synchronized - mem to dev (TX) trasnfer */
		ret = udma_alloc_tx_resources(uc);
		if (ret)
			goto err_free_res;

		uc->config.src_thread = ud->psil_base + uc->tchan->id;
		uc->config.dst_thread = uc->config.remote_thread_id;
		uc->config.dst_thread |= 0x8000;

		ret = udma_alloc_tchan_sci_req(uc);
		if (ret)
			goto err_free_res;

		break;
	case DMA_DEV_TO_MEM:
		/* Slave transfer synchronized - dev to mem (RX) trasnfer */
		ret = udma_alloc_rx_resources(uc);
		if (ret)
			goto err_free_res;

		uc->config.src_thread = uc->config.remote_thread_id;
		uc->config.dst_thread = (ud->psil_base + uc->rchan->id) | 0x8000;

		ret = udma_alloc_rchan_sci_req(uc);
		if (ret)
			goto err_free_res;

		break;
	default:
		return -EINVAL;
	}

	if (udma_is_chan_running(uc)) {
		dev_warn(ud->dev, "chan%d: is running!\n", uc->id);
		udma_stop(uc);
		if (udma_is_chan_running(uc)) {
			dev_err(ud->dev, "chan%d: won't stop!\n", uc->id);
			goto err_free_res;
		}
	}

	/* PSI-L pairing */
	ret = udma_navss_psil_pair(ud, uc->config.src_thread, uc->config.dst_thread);
	if (ret) {
		dev_err(ud->dev, "k3_nav_psil_request_link fail\n");
		goto err_free_res;
	}

	return 0;

err_free_res:
	udma_free_tx_resources(uc);
	udma_free_rx_resources(uc);
	uc->config.remote_thread_id = -1;
	return ret;
}

static void udma_free_chan_resources(struct udma_chan *uc)
{
	/* Hard reset UDMA channel */
	udma_stop_hard(uc);
	udma_reset_counters(uc);

	/* Release PSI-L pairing */
	udma_navss_psil_unpair(uc->ud, uc->config.src_thread, uc->config.dst_thread);

	/* Reset the rings for a new start */
	udma_reset_rings(uc);
	udma_free_tx_resources(uc);
	udma_free_rx_resources(uc);

	uc->config.remote_thread_id = -1;
	uc->config.dir = DMA_MEM_TO_MEM;
}

static const char * const range_names[] = {
	[RM_RANGE_BCHAN] = "ti,sci-rm-range-bchan",
	[RM_RANGE_TCHAN] = "ti,sci-rm-range-tchan",
	[RM_RANGE_RCHAN] = "ti,sci-rm-range-rchan",
	[RM_RANGE_RFLOW] = "ti,sci-rm-range-rflow",
	[RM_RANGE_TFLOW] = "ti,sci-rm-range-tflow",
};

static int udma_get_mmrs(struct udma_dev *ud)
{
	u32 cap2, cap3, cap4;
	int i;

	ud->mmrs[MMR_GCFG] = dev_request_mem_region_by_name(ud->dev, mmr_names[MMR_GCFG]);
	if (!ud->mmrs[MMR_GCFG])
		return -EINVAL;

	cap2 = udma_read(ud->mmrs[MMR_GCFG], 0x28);
	cap3 = udma_read(ud->mmrs[MMR_GCFG], 0x2c);

	switch (ud->match_data->type) {
	case DMA_TYPE_UDMA:
		ud->rflow_cnt = cap3 & 0x3fff;
		ud->tchan_cnt = cap2 & 0x1ff;
		ud->echan_cnt = (cap2 >> 9) & 0x1ff;
		ud->rchan_cnt = (cap2 >> 18) & 0x1ff;
		break;
	case DMA_TYPE_BCDMA:
		ud->bchan_cnt = cap2 & 0x1ff;
		ud->tchan_cnt = (cap2 >> 9) & 0x1ff;
		ud->rchan_cnt = (cap2 >> 18) & 0x1ff;
		break;
	case DMA_TYPE_PKTDMA:
		cap4 = udma_read(ud->mmrs[MMR_GCFG], 0x30);
		ud->tchan_cnt = cap2 & 0x1ff;
		ud->rchan_cnt = (cap2 >> 18) & 0x1ff;
		ud->rflow_cnt = cap3 & 0x3fff;
		ud->tflow_cnt = cap4 & 0x3fff;
		break;
	default:
		return -EINVAL;
	}

	for (i = 1; i < MMR_LAST; i++) {
		if (i == MMR_BCHANRT && ud->bchan_cnt == 0)
			continue;
		if (i == MMR_TCHANRT && ud->tchan_cnt == 0)
			continue;
		if (i == MMR_RCHANRT && ud->rchan_cnt == 0)
			continue;

		ud->mmrs[i] = dev_request_mem_region_by_name(ud->dev, mmr_names[i]);
		if (!ud->mmrs[i])
			return -EINVAL;
	}

	return 0;
}

static int udma_setup_resources(struct udma_dev *ud)
{
	struct device *dev = ud->dev;
	int i;
	struct ti_sci_resource_desc *rm_desc;
	struct ti_sci_resource *rm_res;
	struct udma_tisci_rm *tisci_rm = &ud->tisci_rm;

	ud->tchan_map = devm_kmalloc_array(dev, BITS_TO_LONGS(ud->tchan_cnt),
					   sizeof(unsigned long), GFP_KERNEL);
	ud->tchans = devm_kcalloc(dev, ud->tchan_cnt, sizeof(*ud->tchans),
				  GFP_KERNEL);
	ud->rchan_map = devm_kmalloc_array(dev, BITS_TO_LONGS(ud->rchan_cnt),
					   sizeof(unsigned long), GFP_KERNEL);
	ud->rchans = devm_kcalloc(dev, ud->rchan_cnt, sizeof(*ud->rchans),
				  GFP_KERNEL);
	ud->rflow_map = devm_kmalloc_array(dev, BITS_TO_LONGS(ud->rflow_cnt),
					   sizeof(unsigned long), GFP_KERNEL);
	ud->rflow_map_reserved = devm_kcalloc(dev, BITS_TO_LONGS(ud->rflow_cnt),
					      sizeof(unsigned long),
					      GFP_KERNEL);
	ud->rflows = devm_kcalloc(dev, ud->rflow_cnt, sizeof(*ud->rflows),
				  GFP_KERNEL);

	if (!ud->tchan_map || !ud->rchan_map || !ud->rflow_map ||
	    !ud->rflow_map_reserved || !ud->tchans || !ud->rchans ||
	    !ud->rflows)
		return -ENOMEM;

	/*
	 * RX flows with the same Ids as RX channels are reserved to be used
	 * as default flows if remote HW can't generate flow_ids. Those
	 * RX flows can be requested only explicitly by id.
	 */
	bitmap_set(ud->rflow_map_reserved, 0, ud->rchan_cnt);

	/* Get resource ranges from tisci */
	for (i = 0; i < RM_RANGE_LAST; i++) {
		if (i == RM_RANGE_BCHAN || i == RM_RANGE_TFLOW)
			continue;

		tisci_rm->rm_ranges[i] =
			devm_ti_sci_get_of_resource(tisci_rm->tisci, dev,
						    tisci_rm->tisci_dev_id,
						    (char *)range_names[i]);
	}

	/* tchan ranges */
	rm_res = tisci_rm->rm_ranges[RM_RANGE_TCHAN];
	if (IS_ERR(rm_res)) {
		bitmap_zero(ud->tchan_map, ud->tchan_cnt);
	} else {
		bitmap_fill(ud->tchan_map, ud->tchan_cnt);
		for (i = 0; i < rm_res->sets; i++) {
			rm_desc = &rm_res->desc[i];
			bitmap_clear(ud->tchan_map, rm_desc->start,
				     rm_desc->num);
		}
	}

	/* rchan and matching default flow ranges */
	rm_res = tisci_rm->rm_ranges[RM_RANGE_RCHAN];
	if (IS_ERR(rm_res)) {
		bitmap_zero(ud->rchan_map, ud->rchan_cnt);
		bitmap_zero(ud->rflow_map, ud->rchan_cnt);
	} else {
		bitmap_fill(ud->rchan_map, ud->rchan_cnt);
		bitmap_fill(ud->rflow_map, ud->rchan_cnt);
		for (i = 0; i < rm_res->sets; i++) {
			rm_desc = &rm_res->desc[i];
			bitmap_clear(ud->rchan_map, rm_desc->start,
				     rm_desc->num);
			bitmap_clear(ud->rflow_map, rm_desc->start,
				     rm_desc->num);
		}
	}

	/* GP rflow ranges */
	rm_res = tisci_rm->rm_ranges[RM_RANGE_RFLOW];
	if (IS_ERR(rm_res)) {
		bitmap_clear(ud->rflow_map, ud->rchan_cnt,
			     ud->rflow_cnt - ud->rchan_cnt);
	} else {
		bitmap_set(ud->rflow_map, ud->rchan_cnt,
			   ud->rflow_cnt - ud->rchan_cnt);
		for (i = 0; i < rm_res->sets; i++) {
			rm_desc = &rm_res->desc[i];
			bitmap_clear(ud->rflow_map, rm_desc->start,
				     rm_desc->num);
		}
	}

	return 0;
}

static int bcdma_setup_resources(struct udma_dev *ud)
{
	int i;
	struct device *dev = ud->dev;
	struct ti_sci_resource_desc *rm_desc;
	struct ti_sci_resource *rm_res;
	struct udma_tisci_rm *tisci_rm = &ud->tisci_rm;

	ud->bchan_map = devm_kmalloc_array(dev, BITS_TO_LONGS(ud->bchan_cnt),
					   sizeof(unsigned long), GFP_KERNEL);
	ud->bchans = devm_kcalloc(dev, ud->bchan_cnt, sizeof(*ud->bchans),
				  GFP_KERNEL);
	ud->tchan_map = devm_kmalloc_array(dev, BITS_TO_LONGS(ud->tchan_cnt),
					   sizeof(unsigned long), GFP_KERNEL);
	ud->tchans = devm_kcalloc(dev, ud->tchan_cnt, sizeof(*ud->tchans),
				  GFP_KERNEL);
	ud->rchan_map = devm_kmalloc_array(dev, BITS_TO_LONGS(ud->rchan_cnt),
					   sizeof(unsigned long), GFP_KERNEL);
	ud->rchans = devm_kcalloc(dev, ud->rchan_cnt, sizeof(*ud->rchans),
				  GFP_KERNEL);
	ud->rflows = devm_kcalloc(dev, ud->rchan_cnt, sizeof(*ud->rflows),
				  GFP_KERNEL);

	if (!ud->bchan_map || !ud->tchan_map || !ud->rchan_map ||
	    !ud->bchans || !ud->tchans || !ud->rchans ||
	    !ud->rflows)
		return -ENOMEM;

	/* Get resource ranges from tisci */
	for (i = 0; i < RM_RANGE_LAST; i++) {
		if (i == RM_RANGE_RFLOW || i == RM_RANGE_TFLOW)
			continue;

		tisci_rm->rm_ranges[i] =
			devm_ti_sci_get_of_resource(tisci_rm->tisci, dev,
						    tisci_rm->tisci_dev_id,
						    (char *)range_names[i]);
	}

	/* bchan ranges */
	rm_res = tisci_rm->rm_ranges[RM_RANGE_BCHAN];
	if (IS_ERR(rm_res)) {
		bitmap_zero(ud->bchan_map, ud->bchan_cnt);
	} else {
		bitmap_fill(ud->bchan_map, ud->bchan_cnt);
		for (i = 0; i < rm_res->sets; i++) {
			rm_desc = &rm_res->desc[i];
			bitmap_clear(ud->bchan_map, rm_desc->start,
				     rm_desc->num);
			dev_dbg(dev, "ti-sci-res: bchan: %d:%d\n",
				rm_desc->start, rm_desc->num);
		}
	}

	/* tchan ranges */
	rm_res = tisci_rm->rm_ranges[RM_RANGE_TCHAN];
	if (IS_ERR(rm_res)) {
		bitmap_zero(ud->tchan_map, ud->tchan_cnt);
	} else {
		bitmap_fill(ud->tchan_map, ud->tchan_cnt);
		for (i = 0; i < rm_res->sets; i++) {
			rm_desc = &rm_res->desc[i];
			bitmap_clear(ud->tchan_map, rm_desc->start,
				     rm_desc->num);
			dev_dbg(dev, "ti-sci-res: tchan: %d:%d\n",
				rm_desc->start, rm_desc->num);
		}
	}

	/* rchan ranges */
	rm_res = tisci_rm->rm_ranges[RM_RANGE_RCHAN];
	if (IS_ERR(rm_res)) {
		bitmap_zero(ud->rchan_map, ud->rchan_cnt);
	} else {
		bitmap_fill(ud->rchan_map, ud->rchan_cnt);
		for (i = 0; i < rm_res->sets; i++) {
			rm_desc = &rm_res->desc[i];
			bitmap_clear(ud->rchan_map, rm_desc->start,
				     rm_desc->num);
			dev_dbg(dev, "ti-sci-res: rchan: %d:%d\n",
				rm_desc->start, rm_desc->num);
		}
	}

	return 0;
}

static int pktdma_setup_resources(struct udma_dev *ud)
{
	int i;
	struct device *dev = ud->dev;
	struct ti_sci_resource *rm_res;
	struct ti_sci_resource_desc *rm_desc;
	struct udma_tisci_rm *tisci_rm = &ud->tisci_rm;

	ud->tchan_map = devm_kmalloc_array(dev, BITS_TO_LONGS(ud->tchan_cnt),
					   sizeof(unsigned long), GFP_KERNEL);
	ud->tchans = devm_kcalloc(dev, ud->tchan_cnt, sizeof(*ud->tchans),
				  GFP_KERNEL);
	ud->rchan_map = devm_kmalloc_array(dev, BITS_TO_LONGS(ud->rchan_cnt),
					   sizeof(unsigned long), GFP_KERNEL);
	ud->rchans = devm_kcalloc(dev, ud->rchan_cnt, sizeof(*ud->rchans),
				  GFP_KERNEL);
	ud->rflow_map = devm_kcalloc(dev, BITS_TO_LONGS(ud->rflow_cnt),
				     sizeof(unsigned long),
				     GFP_KERNEL);
	ud->rflows = devm_kcalloc(dev, ud->rflow_cnt, sizeof(*ud->rflows),
				  GFP_KERNEL);
	ud->tflow_map = devm_kmalloc_array(dev, BITS_TO_LONGS(ud->tflow_cnt),
					   sizeof(unsigned long), GFP_KERNEL);

	if (!ud->tchan_map || !ud->rchan_map || !ud->tflow_map || !ud->tchans ||
	    !ud->rchans || !ud->rflows || !ud->rflow_map)
		return -ENOMEM;

	/* Get resource ranges from tisci */
	for (i = 0; i < RM_RANGE_LAST; i++) {
		if (i == RM_RANGE_BCHAN)
			continue;

		tisci_rm->rm_ranges[i] =
			devm_ti_sci_get_of_resource(tisci_rm->tisci, dev,
						    tisci_rm->tisci_dev_id,
						    (char *)range_names[i]);
	}

	/* tchan ranges */
	rm_res = tisci_rm->rm_ranges[RM_RANGE_TCHAN];
	if (IS_ERR(rm_res)) {
		bitmap_zero(ud->tchan_map, ud->tchan_cnt);
	} else {
		bitmap_fill(ud->tchan_map, ud->tchan_cnt);
		for (i = 0; i < rm_res->sets; i++) {
			rm_desc = &rm_res->desc[i];
			bitmap_clear(ud->tchan_map, rm_desc->start,
				     rm_desc->num);
			dev_dbg(dev, "ti-sci-res: tchan: %d:%d\n",
				rm_desc->start, rm_desc->num);
		}
	}

	/* rchan ranges */
	rm_res = tisci_rm->rm_ranges[RM_RANGE_RCHAN];
	if (IS_ERR(rm_res)) {
		bitmap_zero(ud->rchan_map, ud->rchan_cnt);
	} else {
		bitmap_fill(ud->rchan_map, ud->rchan_cnt);
		for (i = 0; i < rm_res->sets; i++) {
			rm_desc = &rm_res->desc[i];
			bitmap_clear(ud->rchan_map, rm_desc->start,
				     rm_desc->num);
			dev_dbg(dev, "ti-sci-res: rchan: %d:%d\n",
				rm_desc->start, rm_desc->num);
		}
	}

	/* rflow ranges */
	rm_res = tisci_rm->rm_ranges[RM_RANGE_RFLOW];
	if (IS_ERR(rm_res)) {
		/* all rflows are assigned exclusively to Linux */
		bitmap_zero(ud->rflow_map, ud->rflow_cnt);
	} else {
		bitmap_fill(ud->rflow_map, ud->rflow_cnt);
		for (i = 0; i < rm_res->sets; i++) {
			rm_desc = &rm_res->desc[i];
			bitmap_clear(ud->rflow_map, rm_desc->start,
				     rm_desc->num);
			dev_dbg(dev, "ti-sci-res: rflow: %d:%d\n",
				rm_desc->start, rm_desc->num);
		}
	}

	/* tflow ranges */
	rm_res = tisci_rm->rm_ranges[RM_RANGE_TFLOW];
	if (IS_ERR(rm_res)) {
		/* all tflows are assigned exclusively to Linux */
		bitmap_zero(ud->tflow_map, ud->tflow_cnt);
	} else {
		bitmap_fill(ud->tflow_map, ud->tflow_cnt);
		for (i = 0; i < rm_res->sets; i++) {
			rm_desc = &rm_res->desc[i];
			bitmap_clear(ud->tflow_map, rm_desc->start,
				     rm_desc->num);
			dev_dbg(dev, "ti-sci-res: tflow: %d:%d\n",
				rm_desc->start, rm_desc->num);
		}
	}

	return 0;
}

static int setup_resources(struct udma_dev *ud)
{
	struct device *dev = ud->dev;
	int ch_count, ret;

	switch (ud->match_data->type) {
	case DMA_TYPE_UDMA:
		ret = udma_setup_resources(ud);
		break;
	case DMA_TYPE_BCDMA:
		ret = bcdma_setup_resources(ud);
		break;
	case DMA_TYPE_PKTDMA:
		ret = pktdma_setup_resources(ud);
		break;
	default:
		return -EINVAL;
	}

	if (ret)
		return ret;

	ch_count  = ud->bchan_cnt + ud->tchan_cnt + ud->rchan_cnt;
	if (ud->bchan_cnt)
		ch_count -= bitmap_weight(ud->bchan_map, ud->bchan_cnt);
	ch_count -= bitmap_weight(ud->tchan_map, ud->tchan_cnt);
	ch_count -= bitmap_weight(ud->rchan_map, ud->rchan_cnt);
	if (!ch_count)
		return -ENODEV;

	ud->channels = devm_kcalloc(dev, ch_count, sizeof(*ud->channels),
				    GFP_KERNEL);
	if (!ud->channels)
		return -ENOMEM;

	switch (ud->match_data->type) {
	case DMA_TYPE_UDMA:
		dev_dbg(dev,
			"Channels: %d (tchan: %u, rchan: %u, gp-rflow: %u)\n",
			ch_count,
			ud->tchan_cnt - bitmap_weight(ud->tchan_map,
						      ud->tchan_cnt),
			ud->rchan_cnt - bitmap_weight(ud->rchan_map,
						      ud->rchan_cnt),
			ud->rflow_cnt - bitmap_weight(ud->rflow_map,
						      ud->rflow_cnt));
		break;
	case DMA_TYPE_BCDMA:
		dev_dbg(dev,
			"Channels: %d (bchan: %u, tchan: %u, rchan: %u)\n",
			ch_count,
			ud->bchan_cnt - bitmap_weight(ud->bchan_map,
						      ud->bchan_cnt),
			ud->tchan_cnt - bitmap_weight(ud->tchan_map,
						      ud->tchan_cnt),
			ud->rchan_cnt - bitmap_weight(ud->rchan_map,
						      ud->rchan_cnt));
		break;
	case DMA_TYPE_PKTDMA:
		dev_dbg(dev,
			"Channels: %d (tchan: %u, rchan: %u)\n",
			ch_count,
			ud->tchan_cnt - bitmap_weight(ud->tchan_map,
						      ud->tchan_cnt),
			ud->rchan_cnt - bitmap_weight(ud->rchan_map,
						      ud->rchan_cnt));
		break;
	default:
		break;
	}

	return ch_count;
}

#define TISCI_BCDMA_BCHAN_VALID_PARAMS (			\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_PAUSE_ON_ERR_VALID |	\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_EXTENDED_CH_TYPE_VALID)

#define TISCI_BCDMA_TCHAN_VALID_PARAMS (			\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_PAUSE_ON_ERR_VALID |	\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_TX_SUPR_TDPKT_VALID)

#define TISCI_BCDMA_RCHAN_VALID_PARAMS (			\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_PAUSE_ON_ERR_VALID)

#define TISCI_UDMA_TCHAN_VALID_PARAMS (				\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_PAUSE_ON_ERR_VALID |	\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_TX_FILT_EINFO_VALID |	\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_TX_FILT_PSWORDS_VALID |	\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_CHAN_TYPE_VALID |		\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_TX_SUPR_TDPKT_VALID |	\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_FETCH_SIZE_VALID |		\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_CQ_QNUM_VALID |		\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_ATYPE_VALID)

#define TISCI_UDMA_RCHAN_VALID_PARAMS (				\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_PAUSE_ON_ERR_VALID |	\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_FETCH_SIZE_VALID |		\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_CQ_QNUM_VALID |		\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_CHAN_TYPE_VALID |		\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_RX_IGNORE_SHORT_VALID |	\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_RX_IGNORE_LONG_VALID |	\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_RX_FLOWID_START_VALID |	\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_RX_FLOWID_CNT_VALID |	\
	TI_SCI_MSG_VALUE_RM_UDMAP_CH_ATYPE_VALID)

static int bcdma_tisci_m2m_channel_config(struct udma_chan *uc)
{
	struct udma_dev *ud = uc->ud;
	struct udma_tisci_rm *tisci_rm = &ud->tisci_rm;
	const struct ti_sci_rm_udmap_ops *tisci_ops = tisci_rm->tisci_udmap_ops;
	struct ti_sci_msg_rm_udmap_tx_ch_cfg req_tx = { 0 };
	struct udma_bchan *bchan = uc->bchan;
	int ret = 0;

	req_tx.valid_params = TISCI_BCDMA_BCHAN_VALID_PARAMS;
	req_tx.nav_id = tisci_rm->tisci_dev_id;
	req_tx.extended_ch_type = TI_SCI_RM_BCDMA_EXTENDED_CH_TYPE_BCHAN;
	req_tx.index = bchan->id;

	ret = tisci_ops->tx_ch_cfg(tisci_rm->tisci, &req_tx);
	if (ret)
		dev_err(ud->dev, "bchan%d cfg failed %d\n", bchan->id, ret);

	return ret;
}

static int bcdma_tisci_tx_channel_config(struct udma_chan *uc)
{
	struct udma_dev *ud = uc->ud;
	struct udma_tisci_rm *tisci_rm = &ud->tisci_rm;
	const struct ti_sci_rm_udmap_ops *tisci_ops = tisci_rm->tisci_udmap_ops;
	struct udma_tchan *tchan = uc->tchan;
	struct ti_sci_msg_rm_udmap_tx_ch_cfg req_tx = { 0 };
	int ret = 0;

	req_tx.valid_params = TISCI_BCDMA_TCHAN_VALID_PARAMS;
	req_tx.nav_id = tisci_rm->tisci_dev_id;
	req_tx.index = tchan->id;
	req_tx.tx_supr_tdpkt = uc->config.notdpkt;
	if (uc->config.ep_type == PSIL_EP_PDMA_XY &&
	    ud->match_data->flags & UDMA_FLAG_TDTYPE) {
		/* wait for peer to complete the teardown for PDMAs */
		req_tx.valid_params |=
				TI_SCI_MSG_VALUE_RM_UDMAP_CH_TX_TDTYPE_VALID;
		req_tx.tx_tdtype = 1;
	}

	ret = tisci_ops->tx_ch_cfg(tisci_rm->tisci, &req_tx);
	if (ret)
		dev_err(ud->dev, "tchan%d cfg failed %d\n", tchan->id, ret);

	return ret;
}

#define pktdma_tisci_tx_channel_config bcdma_tisci_tx_channel_config

static int pktdma_tisci_rx_channel_config(struct udma_chan *uc)
{
	struct udma_dev *ud = uc->ud;
	struct udma_tisci_rm *tisci_rm = &ud->tisci_rm;
	const struct ti_sci_rm_udmap_ops *tisci_ops = tisci_rm->tisci_udmap_ops;
	struct ti_sci_msg_rm_udmap_rx_ch_cfg req_rx = { 0 };
	struct ti_sci_msg_rm_udmap_flow_cfg flow_req = { 0 };
	int ret = 0;

	req_rx.valid_params = TISCI_BCDMA_RCHAN_VALID_PARAMS;
	req_rx.nav_id = tisci_rm->tisci_dev_id;
	req_rx.index = uc->rchan->id;

	ret = tisci_ops->rx_ch_cfg(tisci_rm->tisci, &req_rx);
	if (ret) {
		dev_err(ud->dev, "rchan%d cfg failed %d\n", uc->rchan->id, ret);
		return ret;
	}

	flow_req.valid_params =
		TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_EINFO_PRESENT_VALID |
		TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_PSINFO_PRESENT_VALID |
		TI_SCI_MSG_VALUE_RM_UDMAP_FLOW_ERROR_HANDLING_VALID;

	flow_req.nav_id = tisci_rm->tisci_dev_id;
	flow_req.flow_index = uc->rflow->id;

	if (uc->config.needs_epib)
		flow_req.rx_einfo_present = 1;
	else
		flow_req.rx_einfo_present = 0;
	if (uc->config.psd_size)
		flow_req.rx_psinfo_present = 1;
	else
		flow_req.rx_psinfo_present = 0;
	flow_req.rx_error_handling = 0;

	ret = tisci_ops->rx_flow_cfg(tisci_rm->tisci, &flow_req);

	if (ret)
		dev_err(ud->dev, "flow%d config failed: %d\n", uc->rflow->id,
			ret);

	return ret;
}

static int bcdma_alloc_chan_resources(struct udma_chan *uc)
{
	int ret;

	uc->config.pkt_mode = false;

	switch (uc->config.dir) {
	case DMA_MEM_TO_MEM:
		/* Non synchronized - mem to mem type of transfer */
		dev_dbg(uc->ud->dev, "%s: chan%d as MEM-to-MEM\n", __func__,
			uc->id);

		ret = bcdma_alloc_bchan_resources(uc);
		if (ret)
			return ret;

		ret = bcdma_tisci_m2m_channel_config(uc);
		break;
	default:
		return -EINVAL;
	}

	/* check if the channel configuration was successful */
	if (ret)
		goto err_res_free;

	if (udma_is_chan_running(uc)) {
		dev_warn(uc->ud->dev, "chan%d: is running!\n", uc->id);
		udma_stop(uc);
		if (udma_is_chan_running(uc)) {
			dev_err(uc->ud->dev, "chan%d: won't stop!\n", uc->id);
			goto err_res_free;
		}
	}

	udma_reset_rings(uc);

	return 0;

err_res_free:
	bcdma_free_bchan_resources(uc);
	udma_free_tx_resources(uc);
	udma_free_rx_resources(uc);

	udma_reset_uchan(uc);

	return ret;
}

static int pktdma_alloc_chan_resources(struct udma_chan *uc)
{
	struct udma_dev *ud = uc->ud;
	int ret;

	switch (uc->config.dir) {
	case DMA_MEM_TO_DEV:
		/* Slave transfer synchronized - mem to dev (TX) trasnfer */
		dev_dbg(ud->dev, "%s: chan%d as MEM-to-DEV\n", __func__,
			uc->id);

		ret = udma_alloc_tx_resources(uc);
		if (ret) {
			uc->config.remote_thread_id = -1;
			return ret;
		}

		uc->config.src_thread = ud->psil_base + uc->tchan->id;
		uc->config.dst_thread = uc->config.remote_thread_id;
		uc->config.dst_thread |= K3_PSIL_DST_THREAD_ID_OFFSET;

		ret = pktdma_tisci_tx_channel_config(uc);
		break;
	case DMA_DEV_TO_MEM:
		/* Slave transfer synchronized - dev to mem (RX) trasnfer */
		dev_dbg(ud->dev, "%s: chan%d as DEV-to-MEM\n", __func__,
			uc->id);

		ret = udma_alloc_rx_resources(uc);
		if (ret) {
			uc->config.remote_thread_id = -1;
			return ret;
		}

		uc->config.src_thread = uc->config.remote_thread_id;
		uc->config.dst_thread = (ud->psil_base + uc->rchan->id) |
					K3_PSIL_DST_THREAD_ID_OFFSET;

		ret = pktdma_tisci_rx_channel_config(uc);
		break;
	default:
		return -EINVAL;
	}

	/* check if the channel configuration was successful */
	if (ret)
		goto err_res_free;

	/* PSI-L pairing */
	ret = udma_navss_psil_pair(ud, uc->config.src_thread, uc->config.dst_thread);
	if (ret) {
		dev_err(ud->dev, "PSI-L pairing failed: 0x%04x -> 0x%04x\n",
			uc->config.src_thread, uc->config.dst_thread);
		goto err_res_free;
	}

	if (udma_is_chan_running(uc)) {
		dev_warn(ud->dev, "chan%d: is running!\n", uc->id);
		udma_stop(uc);
		if (udma_is_chan_running(uc)) {
			dev_err(ud->dev, "chan%d: won't stop!\n", uc->id);
			goto err_res_free;
		}
	}

	udma_reset_rings(uc);

	if (uc->tchan)
		dev_dbg(ud->dev,
			"chan%d: tchan%d, tflow%d, Remote thread: 0x%04x\n",
			uc->id, uc->tchan->id, uc->tchan->tflow_id,
			uc->config.remote_thread_id);
	else if (uc->rchan)
		dev_dbg(ud->dev,
			"chan%d: rchan%d, rflow%d, Remote thread: 0x%04x\n",
			uc->id, uc->rchan->id, uc->rflow->id,
			uc->config.remote_thread_id);
	return 0;

err_res_free:
	udma_free_tx_resources(uc);
	udma_free_rx_resources(uc);

	udma_reset_uchan(uc);

	return ret;
}

static int udma_request(struct dma *dma)
{
	struct udma_dev *ud = dev_get_priv(dma->dev);
	struct udma_chan_config *ucc;
	struct udma_chan *uc;
	int ret;

	if (dma->id >= (ud->rchan_cnt + ud->tchan_cnt)) {
		dev_err(dma->dev, "invalid dma ch_id %lu\n", dma->id);
		return -EINVAL;
	}

	uc = &ud->channels[dma->id];
	ucc = &uc->config;
	switch (ud->match_data->type) {
	case DMA_TYPE_UDMA:
		ret = udma_alloc_chan_resources(uc);
		break;
	case DMA_TYPE_BCDMA:
		ret = bcdma_alloc_chan_resources(uc);
		break;
	case DMA_TYPE_PKTDMA:
		ret = pktdma_alloc_chan_resources(uc);
		break;
	default:
		return -EINVAL;
	}
	if (ret) {
		dev_err(dma->dev, "alloc dma res failed %d\n", ret);
		return -EINVAL;
	}

	if (uc->config.dir == DMA_MEM_TO_DEV) {
		uc->desc_tx = dma_alloc_coherent(DMA_DEVICE_BROKEN, ucc->hdesc_size, DMA_ADDRESS_BROKEN);
	} else {
		uc->desc_rx = dma_alloc_coherent(DMA_DEVICE_BROKEN,
				ucc->hdesc_size * UDMA_RX_DESC_NUM, DMA_ADDRESS_BROKEN);
	}

	uc->in_use = true;
	uc->desc_rx_cur = 0;
	uc->num_rx_bufs = 0;

	if (uc->config.dir == DMA_DEV_TO_MEM) {
		uc->cfg_data.flow_id_base = uc->rflow->id;
		uc->cfg_data.flow_id_cnt = 1;
	}

	return 0;
}

static int udma_rfree(struct dma *dma)
{
	struct udma_dev *ud = dev_get_priv(dma->dev);
	struct udma_chan *uc;

	if (dma->id >= (ud->rchan_cnt + ud->tchan_cnt)) {
		dev_err(dma->dev, "invalid dma ch_id %lu\n", dma->id);
		return -EINVAL;
	}
	uc = &ud->channels[dma->id];

	if (udma_is_chan_running(uc))
		udma_stop(uc);

	udma_navss_psil_unpair(ud, uc->config.src_thread,
			       uc->config.dst_thread);

	bcdma_free_bchan_resources(uc);
	udma_free_tx_resources(uc);
	udma_free_rx_resources(uc);
	udma_reset_uchan(uc);

	uc->in_use = false;

	return 0;
}

static const struct dma_ops udma_ops = {
	.transfer	= udma_transfer,
	.of_xlate	= udma_of_xlate,
	.request	= udma_request,
	.rfree		= udma_rfree,
	.enable		= udma_enable,
	.disable	= udma_disable,
	.send		= udma_send,
	.receive	= udma_receive,
	.prepare_rcv_buf = udma_prepare_rcv_buf,
	.get_cfg	= udma_get_cfg,
};

static int k3_udma_probe(struct device *dev)
{
	struct udma_dev *ud;
	int i, ret;
	struct udma_tisci_rm *tisci_rm;
	struct udma_chan *uc;
	const struct udma_match_data *match_data;
	struct device_node *np = dev->of_node;
	struct dma_device *dmad;

	match_data = device_get_match_data(dev);

	ud = xzalloc(sizeof(*ud));
	ud->match_data = match_data;
	ud->dev = dev;
	tisci_rm = &ud->tisci_rm;

	dev->priv = ud;

	ret = udma_get_mmrs(ud);
	if (ret)
		return ret;

	ud->psil_base = match_data->psil_base;

	tisci_rm->tisci = ti_sci_get_by_phandle(dev, "ti,sci");
	if (IS_ERR(tisci_rm->tisci))
		return dev_err_probe(dev, PTR_ERR(tisci_rm->tisci), "Can't get tisci\n");

	tisci_rm->tisci_dev_id = -1;
	ret = of_property_read_u32(np, "ti,sci-dev-id", &tisci_rm->tisci_dev_id);
	if (ret) {
		dev_err(dev, "ti,sci-dev-id read failure %d\n", ret);
		return ret;
	}

	tisci_rm->tisci_navss_dev_id = -1;
	ret = of_property_read_u32(np->parent, "ti,sci-dev-id",
			      &tisci_rm->tisci_navss_dev_id);
	if (ret) {
		dev_err(dev, "navss sci-dev-id read failure %d\n", ret);
		return ret;
	}

	tisci_rm->tisci_udmap_ops = &tisci_rm->tisci->ops.rm_udmap_ops;
	tisci_rm->tisci_psil_ops = &tisci_rm->tisci->ops.rm_psil_ops;

	if (ud->match_data->type == DMA_TYPE_UDMA) {
		ud->ringacc = of_k3_ringacc_get_by_phandle(np, "ti,ringacc");
	} else {
		struct k3_ringacc_init_data ring_init_data;

		ring_init_data.tisci = ud->tisci_rm.tisci;
		ring_init_data.tisci_dev_id = ud->tisci_rm.tisci_dev_id;
		if (ud->match_data->type == DMA_TYPE_BCDMA) {
			ring_init_data.num_rings = ud->bchan_cnt +
						   ud->tchan_cnt +
						   ud->rchan_cnt;
		} else {
			ring_init_data.num_rings = ud->rflow_cnt +
						   ud->tflow_cnt;
		}

		ud->ringacc = k3_ringacc_dmarings_init(dev, &ring_init_data);
	}
	if (IS_ERR(ud->ringacc))
		return PTR_ERR(ud->ringacc);

	ret = setup_resources(ud);
	if (ret < 0)
		return ret;

	ud->ch_count = ret;

	for (i = 0; i < ud->bchan_cnt; i++) {
		struct udma_bchan *bchan = &ud->bchans[i];

		bchan->id = i;
		bchan->reg_rt = ud->mmrs[MMR_BCHANRT] + i * 0x1000;
	}

	for (i = 0; i < ud->tchan_cnt; i++) {
		struct udma_tchan *tchan = &ud->tchans[i];

		tchan->id = i;
		tchan->reg_chan = ud->mmrs[MMR_TCHAN] + UDMA_CH_100(i);
		tchan->reg_rt = ud->mmrs[MMR_TCHANRT] + UDMA_CH_1000(i);
	}

	for (i = 0; i < ud->rchan_cnt; i++) {
		struct udma_rchan *rchan = &ud->rchans[i];

		rchan->id = i;
		rchan->reg_chan = ud->mmrs[MMR_RCHAN] + UDMA_CH_100(i);
		rchan->reg_rt = ud->mmrs[MMR_RCHANRT] + UDMA_CH_1000(i);
	}

	for (i = 0; i < ud->rflow_cnt; i++) {
		struct udma_rflow *rflow = &ud->rflows[i];

		rflow->id = i;
		rflow->reg_rflow = ud->mmrs[MMR_RFLOW] + UDMA_CH_40(i);
	}

	for (i = 0; i < ud->ch_count; i++) {
		struct udma_chan *uc = &ud->channels[i];

		uc->ud = ud;
		uc->id = i;
		uc->config.remote_thread_id = -1;
		uc->bchan = NULL;
		uc->tchan = NULL;
		uc->rchan = NULL;
		uc->config.mapped_channel_id = -1;
		uc->config.default_flow_id = -1;
		uc->config.dir = DMA_MEM_TO_MEM;
		snprintf(uc->name, sizeof(uc->name), "UDMA chan%d\n", i);
		if (!i)
			uc->in_use = true;
	}

	dev_dbg(dev, "(rev: 0x%08x) CAP0-3: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
		 udma_read(ud->mmrs[MMR_GCFG], 0),
		 udma_read(ud->mmrs[MMR_GCFG], 0x20),
		 udma_read(ud->mmrs[MMR_GCFG], 0x24),
		 udma_read(ud->mmrs[MMR_GCFG], 0x28),
		 udma_read(ud->mmrs[MMR_GCFG], 0x2c));

	uc = &ud->channels[0];
	ret = 0;
	switch (ud->match_data->type) {
	case DMA_TYPE_UDMA:
		ret = udma_alloc_chan_resources(uc);
		break;
	case DMA_TYPE_BCDMA:
		ret = bcdma_alloc_chan_resources(uc);
		break;
	default:
		break; /* Do nothing in any other case */
	};

	if (ret)
		dev_err(dev, " Channel 0 allocation failure %d\n", ret);

	dmad = &ud->dmad;

	dmad->dev = dev;
	dmad->ops = &udma_ops;

	ret = dma_device_register(dmad);

	return ret;
}

static void udma_remove(struct device *dev)
{
	struct udma_dev *ud = dev_get_priv(dev);
	struct udma_chan *uc = &ud->channels[0];

	switch (ud->match_data->type) {
	case DMA_TYPE_UDMA:
		udma_free_chan_resources(uc);
		break;
	case DMA_TYPE_BCDMA:
		bcdma_free_bchan_resources(uc);
		break;
	default:
		break;
	};
}

static struct udma_match_data am654_main_data = {
	.type = DMA_TYPE_UDMA,
	.psil_base = 0x1000,
	.enable_memcpy_support = true,
	.statictr_z_mask = GENMASK(11, 0),
	.oes = {
		.udma_rchan = 0x200,
	},
	.tpl_levels = 2,
	.level_start_idx = {
		[0] = 8, /* Normal channels */
		[1] = 0, /* High Throughput channels */
	},
};

static struct udma_match_data am654_mcu_data = {
	.type = DMA_TYPE_UDMA,
	.psil_base = 0x6000,
	.enable_memcpy_support = true,
	.statictr_z_mask = GENMASK(11, 0),
	.oes = {
		.udma_rchan = 0x200,
	},
	.tpl_levels = 2,
	.level_start_idx = {
		[0] = 2, /* Normal channels */
		[1] = 0, /* High Throughput channels */
	},
};

static struct udma_match_data j721e_main_data = {
	.type = DMA_TYPE_UDMA,
	.psil_base = 0x1000,
	.enable_memcpy_support = true,
	.flags = UDMA_FLAG_PDMA_ACC32 | UDMA_FLAG_PDMA_BURST | UDMA_FLAG_TDTYPE,
	.statictr_z_mask = GENMASK(23, 0),
	.oes = {
		.udma_rchan = 0x400,
	},
	.tpl_levels = 3,
	.level_start_idx = {
		[0] = 16, /* Normal channels */
		[1] = 4, /* High Throughput channels */
		[2] = 0, /* Ultra High Throughput channels */
	},
};

static struct udma_match_data j721e_mcu_data = {
	.type = DMA_TYPE_UDMA,
	.psil_base = 0x6000,
	.enable_memcpy_support = true,
	.flags = UDMA_FLAG_PDMA_ACC32 | UDMA_FLAG_PDMA_BURST | UDMA_FLAG_TDTYPE,
	.statictr_z_mask = GENMASK(23, 0),
	.oes = {
		.udma_rchan = 0x400,
	},
	.tpl_levels = 2,
	.level_start_idx = {
		[0] = 2, /* Normal channels */
		[1] = 0, /* High Throughput channels */
	},
};

static struct udma_match_data am64_bcdma_data = {
	.type = DMA_TYPE_BCDMA,
	.psil_base = 0x2000, /* for tchan and rchan, not applicable to bchan */
	.enable_memcpy_support = true, /* Supported via bchan */
	.flags = UDMA_FLAG_PDMA_ACC32 | UDMA_FLAG_PDMA_BURST | UDMA_FLAG_TDTYPE,
	.statictr_z_mask = GENMASK(23, 0),
	.oes = {
		.bcdma_bchan_data = 0x2200,
		.bcdma_bchan_ring = 0x2400,
		.bcdma_tchan_data = 0x2800,
		.bcdma_tchan_ring = 0x2a00,
		.bcdma_rchan_data = 0x2e00,
		.bcdma_rchan_ring = 0x3000,
	},
	/* No throughput levels */
};

static struct udma_match_data am64_pktdma_data = {
	.type = DMA_TYPE_PKTDMA,
	.psil_base = 0x1000,
	.enable_memcpy_support = false,
	.flags = UDMA_FLAG_PDMA_ACC32 | UDMA_FLAG_PDMA_BURST | UDMA_FLAG_TDTYPE,
	.statictr_z_mask = GENMASK(23, 0),
	.oes = {
		.pktdma_tchan_flow = 0x1200,
		.pktdma_rchan_flow = 0x1600,
	},
	/* No throughput levels */
};

static struct of_device_id k3_udma_dt_ids[] = {
	{
		.compatible = "ti,am654-navss-main-udmap",
		.data = &am654_main_data,
	}, {
		.compatible = "ti,am654-navss-mcu-udmap",
		.data = &am654_mcu_data,
	}, {
		.compatible = "ti,j721e-navss-main-udmap",
		.data = &j721e_main_data,
	}, {
		.compatible = "ti,j721e-navss-mcu-udmap",
		.data = &j721e_mcu_data,
	}, {
		.compatible = "ti,am64-dmss-bcdma",
		.data = &am64_bcdma_data,
	}, {
		.compatible = "ti,am64-dmss-pktdma",
		.data = &am64_pktdma_data,
	}, {
		/* Sentinel */
	},
};

static struct driver k3_udma_driver = {
	.probe  = k3_udma_probe,
	.remove = udma_remove,
	.name   = "k3-udma",
	.of_compatible = k3_udma_dt_ids,
};

core_platform_driver(k3_udma_driver);
