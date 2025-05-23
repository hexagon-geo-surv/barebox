// SPDX-License-Identifier: GPL-2.0-only
/*
 * MUSB OTG driver host support
 *
 * Copyright 2005 Mentor Graphics Corporation
 * Copyright (C) 2005-2006 by Texas Instruments
 * Copyright (C) 2006-2007 Nokia Corporation
 * Copyright (C) 2008-2009 MontaVista Software, Inc. <source@mvista.com>
 */

#include <common.h>
#include <malloc.h>
#include "musb_core.h"
#include "musb_host.h"

/* MUSB HOST status 22-mar-2006
 *
 * - There's still lots of partial code duplication for fault paths, so
 *   they aren't handled as consistently as they need to be.
 *
 * - PIO mostly behaved when last tested.
 *     + including ep0, with all usbtest cases 9, 10
 *     + usbtest 14 (ep0out) doesn't seem to run at all
 *     + double buffered OUT/TX endpoints saw stalls(!) with certain usbtest
 *       configurations, but otherwise double buffering passes basic tests.
 *     + for 2.6.N, for N > ~10, needs API changes for hcd framework.
 *
 * - DMA (CPPI) ... partially behaves, not currently recommended
 *     + about 1/15 the speed of typical EHCI implementations (PCI)
 *     + RX, all too often reqpkt seems to misbehave after tx
 *     + TX, no known issues (other than evident silicon issue)
 *
 * - DMA (Mentor/OMAP) ...has at least toggle update problems
 *
 * - [23-feb-2009] minimal traffic scheduling to avoid bulk RX packet
 *   starvation ... nothing yet for TX, interrupt, or bulk.
 *
 * - Not tested with HNP, but some SRP paths seem to behave.
 *
 * NOTE 24-August-2006:
 *
 * - Bulk traffic finally uses both sides of hardware ep1, freeing up an
 *   extra endpoint for periodic use enabling hub + keybd + mouse.  That
 *   mostly works, except that with "usbnet" it's easy to trigger cases
 *   with "ping" where RX loses.  (a) ping to davinci, even "ping -f",
 *   fine; but (b) ping _from_ davinci, even "ping -c 1", ICMP RX loses
 *   although ARP RX wins.  (That test was done with a full speed link.)
 */


/*
 * NOTE on endpoint usage:
 *
 * CONTROL transfers all go through ep0.  BULK ones go through dedicated IN
 * and OUT endpoints ... hardware is dedicated for those "async" queue(s).
 * (Yes, bulk _could_ use more of the endpoints than that, and would even
 * benefit from it.)
 *
 * INTERUPPT and ISOCHRONOUS transfers are scheduled to the other endpoints.
 * So far that scheduling is both dumb and optimistic:  the endpoint will be
 * "claimed" until its software queue is no longer refilled.  No multiplexing
 * of transfers between endpoints, or anything clever.
 */

struct musb *hcd_to_musb(struct usb_hcd *hcd)
{
	return (struct musb *) hcd->hcd_priv;
}


static void musb_ep_program(struct musb *musb, u8 epnum,
			struct urb *urb, int is_out,
			u8 *buf, u32 offset, u32 len);

/*
 * Clear TX fifo. Needed to avoid BABBLE errors.
 */
static void musb_h_tx_flush_fifo(struct musb_hw_ep *ep)
{
	struct musb	*musb = ep->musb;
	void __iomem	*epio = ep->regs;
	u16		csr;
	u16		lastcsr = 0;
	int		retries = 1000;

	csr = musb_readw(epio, MUSB_TXCSR);
	while (csr & MUSB_TXCSR_FIFONOTEMPTY) {
		if (csr != lastcsr)
			dev_dbg(musb->controller, "Host TX FIFONOTEMPTY csr: %02x\n", csr);
		lastcsr = csr;
		csr |= MUSB_TXCSR_FLUSHFIFO;
		musb_writew(epio, MUSB_TXCSR, csr);
		csr = musb_readw(epio, MUSB_TXCSR);
		if (!retries--) {
			dev_warn(musb->controller, "Could not flush host TX%d fifo: csr: %04x\n",
				ep->epnum, csr);
			return;
		}
		mdelay(1);
	}
}

static void musb_h_ep0_flush_fifo(struct musb_hw_ep *ep)
{
	struct musb	*musb = ep->musb;
	void __iomem	*epio = ep->regs;
	u16		csr;
	int		retries = 5;

	/* scrub any data left in the fifo */
	do {
		csr = musb_readw(epio, MUSB_TXCSR);
		if (!(csr & (MUSB_CSR0_TXPKTRDY | MUSB_CSR0_RXPKTRDY)))
			break;
		musb_writew(epio, MUSB_TXCSR, MUSB_CSR0_FLUSHFIFO);
		csr = musb_readw(epio, MUSB_TXCSR);
		udelay(10);
	} while (--retries);

	if (!retries)
		dev_warn(musb->controller, "Could not flush host TX%d fifo: csr: %04x\n",
			ep->epnum, csr);

	/* and reset for the next transfer */
	musb_writew(epio, MUSB_TXCSR, 0);
}

/*
 * Start transmit. Caller is responsible for locking shared resources.
 * musb must be locked.
 */
static inline void musb_h_tx_start(struct musb_hw_ep *ep)
{
	u16	txcsr;

	/* NOTE: no locks here; caller should lock and select EP */
	if (ep->epnum) {
		txcsr = musb_readw(ep->regs, MUSB_TXCSR);
		txcsr |= MUSB_TXCSR_TXPKTRDY | MUSB_TXCSR_H_WZC_BITS;
		musb_writew(ep->regs, MUSB_TXCSR, txcsr);
	} else {
		txcsr = MUSB_CSR0_H_SETUPPKT | MUSB_CSR0_TXPKTRDY;
		musb_writew(ep->regs, MUSB_CSR0, txcsr);
	}

}

static inline void musb_h_tx_dma_start(struct musb_hw_ep *ep)
{
	u16	txcsr;

	/* NOTE: no locks here; caller should lock and select EP */
	txcsr = musb_readw(ep->regs, MUSB_TXCSR);
	txcsr |= MUSB_TXCSR_DMAENAB | MUSB_TXCSR_H_WZC_BITS;
	if (is_cppi_enabled())
		txcsr |= MUSB_TXCSR_DMAMODE;
	musb_writew(ep->regs, MUSB_TXCSR, txcsr);
}

static void musb_ep_set_qh(struct musb_hw_ep *ep, int is_in, struct musb_qh *qh)
{
	if (is_in != 0 || ep->is_shared_fifo)
		ep->in_qh  = qh;
	if (is_in == 0 || ep->is_shared_fifo)
		ep->out_qh = qh;
}

static struct musb_qh *musb_ep_get_qh(struct musb_hw_ep *ep, int is_in)
{
	return is_in ? ep->in_qh : ep->out_qh;
}

/*
 * Start the URB at the front of an endpoint's queue
 * end must be claimed from the caller.
 *
 * Context: controller locked, irqs blocked
 */
static void
musb_start_urb(struct musb *musb, int is_in, struct musb_qh *qh)
{
	u16			frame;
	u32			len;
	void __iomem		*mbase =  musb->mregs;
	struct urb		*urb = next_urb(qh);
	void			*buf = urb->transfer_buffer;
	u32			offset = 0;
	struct musb_hw_ep	*hw_ep = qh->hw_ep;
	unsigned		pipe = urb->pipe;
	u8			address = usb_pipedevice(pipe);
	int			epnum = hw_ep->epnum;

	/* initialize software qh state */
	qh->offset = 0;
	qh->segsize = 0;

	/* gather right source of data */
	switch (qh->type) {
	case USB_ENDPOINT_XFER_CONTROL:
		/* control transfers always start with SETUP */
		is_in = 0;
		musb->ep0_stage = MUSB_EP0_START;
		buf = urb->setup_packet;
		len = 8;
		break;
	default:		/* bulk, interrupt */
		/* actual_length may be nonzero on retry paths */
		buf = urb->transfer_buffer + urb->actual_length;
		len = urb->transfer_buffer_length - urb->actual_length;
	}

	dev_dbg(musb->controller, "qh %p urb %p dev%d ep%d%s%s, hw_ep %d, %p/%d\n",
			qh, urb, address, qh->epnum,
			is_in ? "in" : "out",
			({char *s; switch (qh->type) {
			case USB_ENDPOINT_XFER_CONTROL:	s = ""; break;
			case USB_ENDPOINT_XFER_BULK:	s = "-bulk"; break;
			default:			s = "-intr"; break;
			} s; }),
			epnum, buf + offset, len);

	/* Configure endpoint */
	musb_ep_set_qh(hw_ep, is_in, qh);
	musb_ep_program(musb, epnum, urb, !is_in, buf, offset, len);

	/* transmit may have more work: start it when it is time */
	if (is_in)
		return;

	/* determine if the time is right for a periodic transfer */
	switch (qh->type) {
	case USB_ENDPOINT_XFER_INT:
		dev_dbg(musb->controller, "check whether there's still time for periodic Tx\n");
		frame = musb_readw(mbase, MUSB_FRAME);
		/* FIXME this doesn't implement that scheduling policy ...
		 * or handle framecounter wrapping
		 */
		if (1) {	/* Always assume URB_ISO_ASAP */
			/* REVISIT the SOF irq handler shouldn't duplicate
			 * this code; and we don't init urb->start_frame...
			 */
			qh->frame = 0;
			goto start;
		} else {
			qh->frame = urb->start_frame;
			/* enable SOF interrupt so we can count down */
			dev_dbg(musb->controller, "SOF for %d\n", epnum);
#if 1 /* ifndef	CONFIG_ARCH_DAVINCI */
			musb_writeb(mbase, MUSB_INTRUSBE, 0xff);
#endif
		}
		break;
	default:
start:
		dev_dbg(musb->controller, "Start TX%d %s\n", epnum,
			hw_ep->tx_channel ? "dma" : "pio");

		if (!hw_ep->tx_channel)
			musb_h_tx_start(hw_ep);
	}
}

/* Context: caller owns controller lock, IRQs are blocked */
static void musb_giveback(struct musb *musb, struct urb *urb, int status)
__releases(musb->lock)
__acquires(musb->lock)
{
	dev_dbg(musb->controller,
			"complete %p %pF (%d), dev%d ep%d%s, %d/%d\n",
			urb, urb->complete, status,
			usb_pipedevice(urb->pipe),
			usb_pipeendpoint(urb->pipe),
			usb_pipein(urb->pipe) ? "in" : "out",
			urb->actual_length, urb->transfer_buffer_length
			);

	usb_hcd_unlink_urb_from_ep(musb->hcd, urb);
	spin_unlock(&musb->lock);
	usb_hcd_giveback_urb(musb->hcd, urb, status);
	spin_lock(&musb->lock);
}

/* For bulk/interrupt endpoints only */
static inline void musb_save_toggle(struct musb_qh *qh, int is_in,
				    struct urb *urb)
{
	void __iomem		*epio = qh->hw_ep->regs;
	u16			csr;

	/*
	 * FIXME: the current Mentor DMA code seems to have
	 * problems getting toggle correct.
	 */

	if (is_in)
		csr = musb_readw(epio, MUSB_RXCSR) & MUSB_RXCSR_H_DATATOGGLE;
	else
		csr = musb_readw(epio, MUSB_TXCSR) & MUSB_TXCSR_H_DATATOGGLE;

	usb_settoggle(urb->dev, qh->epnum, !is_in, csr ? 1 : 0);
}

/*
 * Advance this hardware endpoint's queue, completing the specified URB and
 * advancing to either the next URB queued to that qh, or else invalidating
 * that qh and advancing to the next qh scheduled after the current one.
 *
 * Context: caller owns controller lock, IRQs are blocked
 */
static void musb_advance_schedule(struct musb *musb, struct urb *urb,
				  struct musb_hw_ep *hw_ep, int is_in)
{
	struct musb_qh		*qh = musb_ep_get_qh(hw_ep, is_in);
	struct musb_hw_ep	*ep = qh->hw_ep;
	int			ready = qh->is_ready;
	int			status;

	status = (urb->status == -EINPROGRESS) ? 0 : urb->status;

	/* save toggle eagerly, for paranoia */
	switch (qh->type) {
	case USB_ENDPOINT_XFER_BULK:
	case USB_ENDPOINT_XFER_INT:
		musb_save_toggle(qh, is_in, urb);
		break;
	}

	qh->is_ready = 0;
	musb_giveback(musb, urb, status);
	qh->is_ready = ready;

	/* reclaim resources (and bandwidth) ASAP; deschedule it, and
	 * invalidate qh as soon as list_empty(&hep->urb_list)
	 */
	if (list_empty(&qh->hep->urb_list)) {
		struct list_head	*head;
		struct dma_controller	*dma = musb->dma_controller;

		if (is_in) {
			ep->rx_reinit = 1;
			if (ep->rx_channel) {
				dma->channel_release(ep->rx_channel);
				ep->rx_channel = NULL;
			}
		} else {
			ep->tx_reinit = 1;
			if (ep->tx_channel) {
				dma->channel_release(ep->tx_channel);
				ep->tx_channel = NULL;
			}
		}

		/* Clobber old pointers to this qh */
		musb_ep_set_qh(ep, is_in, NULL);
		qh->hep->hcpriv = NULL;

		switch (qh->type) {

		case USB_ENDPOINT_XFER_CONTROL:
		case USB_ENDPOINT_XFER_BULK:
			/* fifo policy for these lists, except that NAKing
			 * should rotate a qh to the end (for fairness).
			 */
			if (qh->mux == 1) {
				head = qh->ring.prev;
				list_del(&qh->ring);
				kfree(qh);
				qh = first_qh(head);
				break;
			}

		case USB_ENDPOINT_XFER_INT:
			/* this is where periodic bandwidth should be
			 * de-allocated if it's tracked and allocated;
			 * and where we'd update the schedule tree...
			 */
			kfree(qh);
			qh = NULL;
			break;
		}
	}

	if (qh != NULL && qh->is_ready) {
		dev_dbg(musb->controller, "... next ep%d %cX urb %p\n",
		    hw_ep->epnum, is_in ? 'R' : 'T', next_urb(qh));
		musb_start_urb(musb, is_in, qh);
	}
}

static u16 musb_h_flush_rxfifo(struct musb_hw_ep *hw_ep, u16 csr)
{
	/* we don't want fifo to fill itself again;
	 * ignore dma (various models),
	 * leave toggle alone (may not have been saved yet)
	 */
	csr |= MUSB_RXCSR_FLUSHFIFO | MUSB_RXCSR_RXPKTRDY;
	csr &= ~(MUSB_RXCSR_H_REQPKT
		| MUSB_RXCSR_H_AUTOREQ
		| MUSB_RXCSR_AUTOCLEAR);

	/* write 2x to allow double buffering */
	musb_writew(hw_ep->regs, MUSB_RXCSR, csr);
	musb_writew(hw_ep->regs, MUSB_RXCSR, csr);

	/* flush writebuffer */
	return musb_readw(hw_ep->regs, MUSB_RXCSR);
}

/*
 * PIO RX for a packet (or part of it).
 */
static bool
musb_host_packet_rx(struct musb *musb, struct urb *urb, u8 epnum, u8 iso_err)
{
	u16			rx_count;
	u8			*buf;
	u16			csr;
	bool			done = false;
	u32			length;
	int			do_flush = 0;
	struct musb_hw_ep	*hw_ep = musb->endpoints + epnum;
	void __iomem		*epio = hw_ep->regs;
	struct musb_qh		*qh = hw_ep->in_qh;
	void			*buffer = urb->transfer_buffer;

	/* musb_ep_select(mbase, epnum); */
	rx_count = musb_readw(epio, MUSB_RXCOUNT);
	dev_dbg(musb->controller, "RX%d count %d, buffer %p len %d/%d\n", epnum, rx_count,
			urb->transfer_buffer, qh->offset,
			urb->transfer_buffer_length);

	/* unload FIFO */
	if (0) {
	} else {
		/* non-isoch */
		buf = buffer + qh->offset;
		length = urb->transfer_buffer_length - qh->offset;
		if (rx_count > length) {
			if (urb->status == -EINPROGRESS)
				urb->status = -EOVERFLOW;
			dev_dbg(musb->controller, "** OVERFLOW %d into %d\n", rx_count, length);
			do_flush = 1;
		} else
			length = rx_count;
		urb->actual_length += length;
		qh->offset += length;

		/* see if we are done */
		done = (urb->actual_length == urb->transfer_buffer_length)
			|| (rx_count < qh->maxpacket)
			|| (urb->status != -EINPROGRESS);
		if (done
				&& (urb->status == -EINPROGRESS)
				&& (urb->transfer_flags & URB_SHORT_NOT_OK)
				&& (urb->actual_length
					< urb->transfer_buffer_length))
			urb->status = -EREMOTEIO;
	}

	musb_read_fifo(hw_ep, length, buf);

	csr = musb_readw(epio, MUSB_RXCSR);
	csr |= MUSB_RXCSR_H_WZC_BITS;
	if (unlikely(do_flush))
		musb_h_flush_rxfifo(hw_ep, csr);
	else {
		/* REVISIT this assumes AUTOCLEAR is never set */
		csr &= ~(MUSB_RXCSR_RXPKTRDY | MUSB_RXCSR_H_REQPKT);
		if (!done)
			csr |= MUSB_RXCSR_H_REQPKT;
		musb_writew(epio, MUSB_RXCSR, csr);
	}

	return done;
}

/* we don't always need to reinit a given side of an endpoint...
 * when we do, use tx/rx reinit routine and then construct a new CSR
 * to address data toggle, NYET, and DMA or PIO.
 *
 * it's possible that driver bugs (especially for DMA) or aborting a
 * transfer might have left the endpoint busier than it should be.
 * the busy/not-empty tests are basically paranoia.
 */
static void
musb_rx_reinit(struct musb *musb, struct musb_qh *qh, struct musb_hw_ep *ep)
{
	u16	csr;

	/* NOTE:  we know the "rx" fifo reinit never triggers for ep0.
	 * That always uses tx_reinit since ep0 repurposes TX register
	 * offsets; the initial SETUP packet is also a kind of OUT.
	 */

	/* if programmed for Tx, put it in RX mode */
	if (ep->is_shared_fifo) {
		csr = musb_readw(ep->regs, MUSB_TXCSR);
		if (csr & MUSB_TXCSR_MODE) {
			musb_h_tx_flush_fifo(ep);
			csr = musb_readw(ep->regs, MUSB_TXCSR);
			musb_writew(ep->regs, MUSB_TXCSR,
				    csr | MUSB_TXCSR_FRCDATATOG);
		}

		/*
		 * Clear the MODE bit (and everything else) to enable Rx.
		 * NOTE: we mustn't clear the DMAMODE bit before DMAENAB.
		 */
		if (csr & MUSB_TXCSR_DMAMODE)
			musb_writew(ep->regs, MUSB_TXCSR, MUSB_TXCSR_DMAMODE);
		musb_writew(ep->regs, MUSB_TXCSR, 0);

	/* scrub all previous state, clearing toggle */
	} else {
		csr = musb_readw(ep->regs, MUSB_RXCSR);
		if (csr & MUSB_RXCSR_RXPKTRDY)
			dev_warn(musb->controller, "rx%d, packet/%d ready?\n", ep->epnum,
				musb_readw(ep->regs, MUSB_RXCOUNT));

		musb_h_flush_rxfifo(ep, MUSB_RXCSR_CLRDATATOG);
	}

	/* target addr and (for multipoint) hub addr/port */
	if (musb->is_multipoint) {
		musb_write_rxfunaddr(ep->target_regs, qh->addr_reg);
		musb_write_rxhubaddr(ep->target_regs, qh->h_addr_reg);
		musb_write_rxhubport(ep->target_regs, qh->h_port_reg);

	} else
		musb_writeb(musb->mregs, MUSB_FADDR, qh->addr_reg);

	/* protocol/endpoint, interval/NAKlimit, i/o size */
	musb_writeb(ep->regs, MUSB_RXTYPE, qh->type_reg);
	musb_writeb(ep->regs, MUSB_RXINTERVAL, qh->intv_reg);
	/* NOTE: bulk combining rewrites high bits of maxpacket */
	/* Set RXMAXP with the FIFO size of the endpoint
	 * to disable double buffer mode.
	 */
	if (musb->double_buffer_not_ok)
		musb_writew(ep->regs, MUSB_RXMAXP, ep->max_packet_sz_rx);
	else
		musb_writew(ep->regs, MUSB_RXMAXP,
				qh->maxpacket | ((qh->hb_mult - 1) << 11));

	ep->rx_reinit = 0;
}

/*
 * Program an HDRC endpoint as per the given URB
 * Context: irqs blocked, controller lock held
 */
static void musb_ep_program(struct musb *musb, u8 epnum,
			struct urb *urb, int is_out,
			u8 *buf, u32 offset, u32 len)
{
	void __iomem		*mbase = musb->mregs;
	struct musb_hw_ep	*hw_ep = musb->endpoints + epnum;
	void __iomem		*epio = hw_ep->regs;
	struct musb_qh		*qh = musb_ep_get_qh(hw_ep, !is_out);
	u16			packet_sz = qh->maxpacket;
	u16			csr;

	dev_dbg(musb->controller, "%s hw%d urb %p spd%d dev%d ep%d%s "
				"h_addr%02x h_port%02x bytes %d\n",
			is_out ? "-->" : "<--",
			epnum, urb, urb->dev->speed,
			qh->addr_reg, qh->epnum, is_out ? "out" : "in",
			qh->h_addr_reg, qh->h_port_reg,
			len);

	musb_ep_select(mbase, epnum);

	if (is_out && !len) {
		csr = musb_readw(epio, MUSB_TXCSR);
		csr &= ~MUSB_TXCSR_DMAENAB;
		musb_writew(epio, MUSB_TXCSR, csr);
		hw_ep->tx_channel = NULL;
	}

	/* OUT/transmit/EP0 or IN/receive? */
	if (is_out) {
		u16	csr;
		u16	int_txe;
		u16	load_count;

		csr = musb_readw(epio, MUSB_TXCSR);

		/* disable interrupt in case we flush */
		int_txe = musb->intrtxe;
		musb_writew(mbase, MUSB_INTRTXE, int_txe & ~(1 << epnum));

		/* general endpoint setup */
		if (epnum) {
			/* flush all old state, set default */
			/*
			 * We could be flushing valid
			 * packets in double buffering
			 * case
			 */
			if (!hw_ep->tx_double_buffered)
				musb_h_tx_flush_fifo(hw_ep);

			/*
			 * We must not clear the DMAMODE bit before or in
			 * the same cycle with the DMAENAB bit, so we clear
			 * the latter first...
			 */
			csr &= ~(MUSB_TXCSR_H_NAKTIMEOUT
					| MUSB_TXCSR_AUTOSET
					| MUSB_TXCSR_DMAENAB
					| MUSB_TXCSR_FRCDATATOG
					| MUSB_TXCSR_H_RXSTALL
					| MUSB_TXCSR_H_ERROR
					| MUSB_TXCSR_TXPKTRDY
					);
			csr |= MUSB_TXCSR_MODE;

			if (!hw_ep->tx_double_buffered) {
				if (usb_gettoggle(urb->dev, qh->epnum, 1))
					csr |= MUSB_TXCSR_H_WR_DATATOGGLE
						| MUSB_TXCSR_H_DATATOGGLE;
				else
					csr |= MUSB_TXCSR_CLRDATATOG;
			}

			musb_writew(epio, MUSB_TXCSR, csr);
			/* REVISIT may need to clear FLUSHFIFO ... */
			csr &= ~MUSB_TXCSR_DMAMODE;
			musb_writew(epio, MUSB_TXCSR, csr);
			csr = musb_readw(epio, MUSB_TXCSR);
		} else {
			/* endpoint 0: just flush */
			musb_h_ep0_flush_fifo(hw_ep);
		}

		/* target addr and (for multipoint) hub addr/port */
		if (musb->is_multipoint) {
			musb_write_txfunaddr(mbase, epnum, qh->addr_reg);
			musb_write_txhubaddr(mbase, epnum, qh->h_addr_reg);
			musb_write_txhubport(mbase, epnum, qh->h_port_reg);
/* FIXME if !epnum, do the same for RX ... */
		} else
			musb_writeb(mbase, MUSB_FADDR, qh->addr_reg);

		/* protocol/endpoint/interval/NAKlimit */
		if (epnum) {
			musb_writeb(epio, MUSB_TXTYPE, qh->type_reg);
			if (musb->double_buffer_not_ok) {
				musb_writew(epio, MUSB_TXMAXP,
						hw_ep->max_packet_sz_tx);
			} else if (can_bulk_split(musb, qh->type)) {
				qh->hb_mult = hw_ep->max_packet_sz_tx
						/ packet_sz;
				musb_writew(epio, MUSB_TXMAXP, packet_sz
					| ((qh->hb_mult) - 1) << 11);
			} else {
				musb_writew(epio, MUSB_TXMAXP,
						qh->maxpacket |
						((qh->hb_mult - 1) << 11));
			}
			musb_writeb(epio, MUSB_TXINTERVAL, qh->intv_reg);
		} else {
			musb_writeb(epio, MUSB_NAKLIMIT0, qh->intv_reg);
			if (musb->is_multipoint)
				musb_writeb(epio, MUSB_TYPE0,
						qh->type_reg);
		}

		if (can_bulk_split(musb, qh->type))
			load_count = min((u32) hw_ep->max_packet_sz_tx,
						len);
		else
			load_count = min((u32) packet_sz, len);

		if (load_count) {
			/* PIO to load FIFO */
			qh->segsize = load_count;
			musb_write_fifo(hw_ep, load_count, buf);
		}

		/* re-enable interrupt */
		musb_writew(mbase, MUSB_INTRTXE, int_txe);

	/* IN/receive */
	} else {
		u16	csr;

		if (hw_ep->rx_reinit) {
			musb_rx_reinit(musb, qh, hw_ep);

			/* init new state: toggle and NYET, maybe DMA later */
			if (usb_gettoggle(urb->dev, qh->epnum, 0))
				csr = MUSB_RXCSR_H_WR_DATATOGGLE
					| MUSB_RXCSR_H_DATATOGGLE;
			else
				csr = 0;
			if (qh->type == USB_ENDPOINT_XFER_INT)
				csr |= MUSB_RXCSR_DISNYET;

		} else {
			csr = musb_readw(hw_ep->regs, MUSB_RXCSR);

			if (csr & (MUSB_RXCSR_RXPKTRDY
					| MUSB_RXCSR_DMAENAB
					| MUSB_RXCSR_H_REQPKT))
				dev_err(musb->controller, "broken !rx_reinit, ep%d csr %04x\n",
						hw_ep->epnum, csr);

			/* scrub any stale state, leaving toggle alone */
			csr &= MUSB_RXCSR_DISNYET;
		}

		/* kick things off */

		csr |= MUSB_RXCSR_H_REQPKT;
		dev_dbg(musb->controller, "RXCSR%d := %04x\n", epnum, csr);
		musb_writew(hw_ep->regs, MUSB_RXCSR, csr);
		csr = musb_readw(hw_ep->regs, MUSB_RXCSR);
	}
}

/* Schedule next QH from musb->in_bulk/out_bulk and move the current qh to
 * the end; avoids starvation for other endpoints.
 */
static void musb_bulk_nak_timeout(struct musb *musb, struct musb_hw_ep *ep,
	int is_in)
{
	struct urb		*urb;
	void __iomem		*mbase = musb->mregs;
	void __iomem		*epio = ep->regs;
	struct musb_qh		*cur_qh, *next_qh;
	u16			rx_csr, tx_csr;

	musb_ep_select(mbase, ep->epnum);
	if (is_in) {
		/* clear nak timeout bit */
		rx_csr = musb_readw(epio, MUSB_RXCSR);
		rx_csr |= MUSB_RXCSR_H_WZC_BITS;
		rx_csr &= ~MUSB_RXCSR_DATAERROR;
		musb_writew(epio, MUSB_RXCSR, rx_csr);

		cur_qh = first_qh(&musb->in_bulk);
	} else {
		/* clear nak timeout bit */
		tx_csr = musb_readw(epio, MUSB_TXCSR);
		tx_csr |= MUSB_TXCSR_H_WZC_BITS;
		tx_csr &= ~MUSB_TXCSR_H_NAKTIMEOUT;
		musb_writew(epio, MUSB_TXCSR, tx_csr);

		cur_qh = first_qh(&musb->out_bulk);
	}
	if (cur_qh) {
		urb = next_urb(cur_qh);
		musb_save_toggle(cur_qh, is_in, urb);

		if (is_in) {
			/* move cur_qh to end of queue */
			list_move_tail(&cur_qh->ring, &musb->in_bulk);

			/* get the next qh from musb->in_bulk */
			next_qh = first_qh(&musb->in_bulk);

			/* set rx_reinit and schedule the next qh */
			ep->rx_reinit = 1;
		} else {
			/* move cur_qh to end of queue */
			list_move_tail(&cur_qh->ring, &musb->out_bulk);

			/* get the next qh from musb->out_bulk */
			next_qh = first_qh(&musb->out_bulk);

			/* set tx_reinit and schedule the next qh */
			ep->tx_reinit = 1;
		}
		musb_start_urb(musb, is_in, next_qh);
	}
}

/*
 * Service the default endpoint (ep0) as host.
 * Return true until it's time to start the status stage.
 */
static bool musb_h_ep0_continue(struct musb *musb, u16 len, struct urb *urb)
{
	bool			 more = false;
	u8			*fifo_dest = NULL;
	u16			fifo_count = 0;
	struct musb_hw_ep	*hw_ep = musb->control_ep;
	struct musb_qh		*qh = hw_ep->in_qh;
	struct usb_ctrlrequest	*request;

	switch (musb->ep0_stage) {
	case MUSB_EP0_IN:
		fifo_dest = urb->transfer_buffer + urb->actual_length;
		fifo_count = min_t(size_t, len, urb->transfer_buffer_length -
				   urb->actual_length);
		if (fifo_count < len)
			urb->status = -EOVERFLOW;

		musb_read_fifo(hw_ep, fifo_count, fifo_dest);

		urb->actual_length += fifo_count;
		if (len < qh->maxpacket) {
			/* always terminate on short read; it's
			 * rarely reported as an error.
			 */
		} else if (urb->actual_length <
				urb->transfer_buffer_length)
			more = true;
		break;
	case MUSB_EP0_START:
		request = (struct usb_ctrlrequest *) urb->setup_packet;

		if (!request->wLength) {
			dev_dbg(musb->controller, "start no-DATA\n");
			break;
		} else if (request->bRequestType & USB_DIR_IN) {
			dev_dbg(musb->controller, "start IN-DATA\n");
			musb->ep0_stage = MUSB_EP0_IN;
			more = true;
			break;
		} else {
			dev_dbg(musb->controller, "start OUT-DATA\n");
			musb->ep0_stage = MUSB_EP0_OUT;
			more = true;
		}
		fallthrough;
	case MUSB_EP0_OUT:
		fifo_count = min_t(size_t, qh->maxpacket,
				   urb->transfer_buffer_length -
				   urb->actual_length);
		if (fifo_count) {
			fifo_dest = (u8 *) (urb->transfer_buffer
					+ urb->actual_length);
			dev_dbg(musb->controller, "Sending %d byte%s to ep0 fifo %p\n",
					fifo_count,
					(fifo_count == 1) ? "" : "s",
					fifo_dest);
			musb_write_fifo(hw_ep, fifo_count, fifo_dest);

			urb->actual_length += fifo_count;
			more = true;
		}
		break;
	default:
		dev_err(musb->controller, "bogus ep0 stage %d\n", musb->ep0_stage);
		break;
	}

	return more;
}

/*
 * Handle default endpoint interrupt as host. Only called in IRQ time
 * from musb_interrupt().
 *
 * called with controller irqlocked
 */
irqreturn_t musb_h_ep0_irq(struct musb *musb)
{
	struct urb		*urb;
	u16			csr, len;
	int			status = 0;
	void __iomem		*mbase = musb->mregs;
	struct musb_hw_ep	*hw_ep = musb->control_ep;
	void __iomem		*epio = hw_ep->regs;
	struct musb_qh		*qh = hw_ep->in_qh;
	bool			complete = false;
	irqreturn_t		retval = IRQ_NONE;

	/* ep0 only has one queue, "in" */
	urb = next_urb(qh);

	musb_ep_select(mbase, 0);
	csr = musb_readw(epio, MUSB_CSR0);
	len = (csr & MUSB_CSR0_RXPKTRDY)
			? musb_readb(epio, MUSB_COUNT0)
			: 0;

	dev_dbg(musb->controller, "<== csr0 %04x, qh %p, count %d, urb %p, stage %d\n",
		csr, qh, len, urb, musb->ep0_stage);

	/* if we just did status stage, we are done */
	if (MUSB_EP0_STATUS == musb->ep0_stage) {
		retval = IRQ_HANDLED;
		complete = true;
	}

	/* prepare status */
	if (csr & MUSB_CSR0_H_RXSTALL) {
		dev_dbg(musb->controller, "STALLING ENDPOINT\n");
		status = -EPIPE;

	} else if (csr & MUSB_CSR0_H_ERROR) {
		dev_dbg(musb->controller, "no response, csr0 %04x\n", csr);
		status = -EPROTO;

	} else if (csr & MUSB_CSR0_H_NAKTIMEOUT) {
		dev_dbg(musb->controller, "control NAK timeout\n");

		/* NOTE:  this code path would be a good place to PAUSE a
		 * control transfer, if another one is queued, so that
		 * ep0 is more likely to stay busy.  That's already done
		 * for bulk RX transfers.
		 *
		 * if (qh->ring.next != &musb->control), then
		 * we have a candidate... NAKing is *NOT* an error
		 */
		musb_writew(epio, MUSB_CSR0, 0);
		retval = IRQ_HANDLED;
	}

	if (status) {
		dev_dbg(musb->controller, "aborting\n");
		retval = IRQ_HANDLED;
		if (urb)
			urb->status = status;
		complete = true;

		/* use the proper sequence to abort the transfer */
		if (csr & MUSB_CSR0_H_REQPKT) {
			csr &= ~MUSB_CSR0_H_REQPKT;
			musb_writew(epio, MUSB_CSR0, csr);
			csr &= ~MUSB_CSR0_H_NAKTIMEOUT;
			musb_writew(epio, MUSB_CSR0, csr);
		} else {
			musb_h_ep0_flush_fifo(hw_ep);
		}

		musb_writeb(epio, MUSB_NAKLIMIT0, 0);

		/* clear it */
		musb_writew(epio, MUSB_CSR0, 0);
	}

	if (unlikely(!urb)) {
		/* stop endpoint since we have no place for its data, this
		 * SHOULD NEVER HAPPEN! */
		dev_err(musb->controller, "no URB for end 0\n");

		musb_h_ep0_flush_fifo(hw_ep);
		goto done;
	}

	if (!complete) {
		/* call common logic and prepare response */
		if (musb_h_ep0_continue(musb, len, urb)) {
			/* more packets required */
			csr = (MUSB_EP0_IN == musb->ep0_stage)
				?  MUSB_CSR0_H_REQPKT : MUSB_CSR0_TXPKTRDY;
		} else {
			/* data transfer complete; perform status phase */
			if (usb_pipeout(urb->pipe)
					|| !urb->transfer_buffer_length)
				csr = MUSB_CSR0_H_STATUSPKT
					| MUSB_CSR0_H_REQPKT;
			else
				csr = MUSB_CSR0_H_STATUSPKT
					| MUSB_CSR0_TXPKTRDY;

			/* flag status stage */
			musb->ep0_stage = MUSB_EP0_STATUS;

			dev_dbg(musb->controller, "ep0 STATUS, csr %04x\n", csr);

		}
		musb_writew(epio, MUSB_CSR0, csr);
		retval = IRQ_HANDLED;
	} else
		musb->ep0_stage = MUSB_EP0_IDLE;

	/* call completion handler if done */
	if (complete)
		musb_advance_schedule(musb, urb, hw_ep, 1);
done:
	return retval;
}

/* Service a Tx-Available or dma completion irq for the endpoint */
void musb_host_tx(struct musb *musb, u8 epnum)
{
	int			pipe;
	bool			done = false;
	u16			tx_csr;
	size_t			length = 0;
	size_t			offset = 0;
	struct musb_hw_ep	*hw_ep = musb->endpoints + epnum;
	void __iomem		*epio = hw_ep->regs;
	struct musb_qh		*qh = hw_ep->out_qh;
	struct urb		*urb = next_urb(qh);
	u32			status = 0;
	void __iomem		*mbase = musb->mregs;
	bool			transfer_pending = false;

	musb_ep_select(mbase, epnum);
	tx_csr = musb_readw(epio, MUSB_TXCSR);

	/* with CPPI, DMA sometimes triggers "extra" irqs */
	if (!urb) {
		dev_dbg(musb->controller, "extra TX%d ready, csr %04x\n", epnum, tx_csr);
		return;
	}

	pipe = urb->pipe;
	dev_dbg(musb->controller, "OUT/TX%d end, csr %04x\n", epnum, tx_csr);

	/* check for errors */
	if (tx_csr & MUSB_TXCSR_H_RXSTALL) {
		/* dma was disabled, fifo flushed */
		dev_dbg(musb->controller, "TX end %d stall\n", epnum);

		/* stall; record URB status */
		status = -EPIPE;

	} else if (tx_csr & MUSB_TXCSR_H_ERROR) {
		/* (NON-ISO) dma was disabled, fifo flushed */
		dev_dbg(musb->controller, "TX 3strikes on ep=%d\n", epnum);

		status = -ETIMEDOUT;

	} else if (tx_csr & MUSB_TXCSR_H_NAKTIMEOUT) {
		if (USB_ENDPOINT_XFER_BULK == qh->type && qh->mux == 1
				&& !list_is_singular(&musb->out_bulk)) {
			dev_dbg(musb->controller,
				"NAK timeout on TX%d ep\n", epnum);
			musb_bulk_nak_timeout(musb, hw_ep, 0);
		} else {
			dev_dbg(musb->controller,
				"TX end=%d device not responding\n", epnum);
			/* NOTE:  this code path would be a good place to PAUSE a
			 * transfer, if there's some other (nonperiodic) tx urb
			 * that could use this fifo.  (dma complicates it...)
			 * That's already done for bulk RX transfers.
			 *
			 * if (bulk && qh->ring.next != &musb->out_bulk), then
			 * we have a candidate... NAKing is *NOT* an error
			 */
			musb_ep_select(mbase, epnum);
			musb_writew(epio, MUSB_TXCSR,
					MUSB_TXCSR_H_WZC_BITS
					| MUSB_TXCSR_TXPKTRDY);
		}
			return;
	}

	if (status) {
		/* do the proper sequence to abort the transfer in the
		 * usb core; the dma engine should already be stopped.
		 */
		musb_h_tx_flush_fifo(hw_ep);
		tx_csr &= ~(MUSB_TXCSR_AUTOSET
				| MUSB_TXCSR_DMAENAB
				| MUSB_TXCSR_H_ERROR
				| MUSB_TXCSR_H_RXSTALL
				| MUSB_TXCSR_H_NAKTIMEOUT
				);

		musb_ep_select(mbase, epnum);
		musb_writew(epio, MUSB_TXCSR, tx_csr);
		/* REVISIT may need to clear FLUSHFIFO ... */
		musb_writew(epio, MUSB_TXCSR, tx_csr);
		musb_writeb(epio, MUSB_TXINTERVAL, 0);

		done = true;
	}

	if (!status) {
		length = qh->segsize;
		qh->offset += length;

		if (0) {
		} else {
			/* see if we need to send more data, or ZLP */
			if (qh->segsize < qh->maxpacket)
				done = true;
			else if (qh->offset == urb->transfer_buffer_length
					&& !(urb->transfer_flags
						& URB_ZERO_PACKET))
				done = true;
			if (!done) {
				offset = qh->offset;
				length = urb->transfer_buffer_length - offset;
				transfer_pending = true;
			}
		}
	}

	/* urb->status != -EINPROGRESS means request has been faulted,
	 * so we must abort this transfer after cleanup
	 */
	if (urb->status != -EINPROGRESS) {
		done = true;
		if (status == 0)
			status = urb->status;
	}

	if (done) {
		/* set status */
		urb->status = status;
		urb->actual_length = qh->offset;
		musb_advance_schedule(musb, urb, hw_ep, USB_DIR_OUT);
		return;
	} else	if (tx_csr & MUSB_TXCSR_DMAENAB) {
		dev_dbg(musb->controller, "not complete, but DMA enabled?\n");
		return;
	}

	/*
	 * PIO: start next packet in this URB.
	 *
	 * REVISIT: some docs say that when hw_ep->tx_double_buffered,
	 * (and presumably, FIFO is not half-full) we should write *two*
	 * packets before updating TXCSR; other docs disagree...
	 */
	if (length > qh->maxpacket)
		length = qh->maxpacket;

	musb_write_fifo(hw_ep, length, urb->transfer_buffer + offset);

	qh->segsize = length;

	musb_ep_select(mbase, epnum);
	musb_writew(epio, MUSB_TXCSR,
			MUSB_TXCSR_H_WZC_BITS | MUSB_TXCSR_TXPKTRDY);
}

/*
 * Service an RX interrupt for the given IN endpoint; docs cover bulk, iso,
 * and high-bandwidth IN transfer cases.
 */
void musb_host_rx(struct musb *musb, u8 epnum)
{
	struct urb		*urb;
	struct musb_hw_ep	*hw_ep = musb->endpoints + epnum;
	void __iomem		*epio = hw_ep->regs;
	struct musb_qh		*qh = hw_ep->in_qh;
	size_t			xfer_len;
	void __iomem		*mbase = musb->mregs;
	int			pipe;
	u16			rx_csr, val;
	bool			iso_err = false;
	bool			done = false;
	u32			status;

	musb_ep_select(mbase, epnum);

	urb = next_urb(qh);
	status = 0;
	xfer_len = 0;

	rx_csr = musb_readw(epio, MUSB_RXCSR);
	val = rx_csr;

	if (unlikely(!urb)) {
		/* REVISIT -- THIS SHOULD NEVER HAPPEN ... but, at least
		 * usbtest #11 (unlinks) triggers it regularly, sometimes
		 * with fifo full.  (Only with DMA??)
		 */
		dev_dbg(musb->controller, "BOGUS RX%d ready, csr %04x, count %d\n", epnum, val,
			musb_readw(epio, MUSB_RXCOUNT));
		musb_h_flush_rxfifo(hw_ep, MUSB_RXCSR_CLRDATATOG);
		return;
	}

	pipe = urb->pipe;

	dev_dbg(musb->controller, "<== hw %d rxcsr %04x, urb actual %d (+dma 0)\n",
		epnum, rx_csr, urb->actual_length);

	/* check for errors, concurrent stall & unlink is not really
	 * handled yet! */
	if (rx_csr & MUSB_RXCSR_H_RXSTALL) {
		dev_dbg(musb->controller, "RX end %d STALL\n", epnum);

		/* stall; record URB status */
		status = -EPIPE;

	} else if (rx_csr & MUSB_RXCSR_H_ERROR) {
		dev_dbg(musb->controller, "end %d RX proto error\n", epnum);

		status = -EPROTO;
		musb_writeb(epio, MUSB_RXINTERVAL, 0);

	} else if (rx_csr & MUSB_RXCSR_DATAERROR) {

		if (USB_ENDPOINT_XFER_ISOC != qh->type) {
			dev_dbg(musb->controller, "RX end %d NAK timeout\n", epnum);

			/* NOTE: NAKing is *NOT* an error, so we want to
			 * continue.  Except ... if there's a request for
			 * another QH, use that instead of starving it.
			 *
			 * Devices like Ethernet and serial adapters keep
			 * reads posted at all times, which will starve
			 * other devices without this logic.
			 */
			if (usb_pipebulk(urb->pipe)
					&& qh->mux == 1
					&& !list_is_singular(&musb->in_bulk)) {
				musb_bulk_nak_timeout(musb, hw_ep, 1);
				return;
			}
			musb_ep_select(mbase, epnum);
			rx_csr |= MUSB_RXCSR_H_WZC_BITS;
			rx_csr &= ~MUSB_RXCSR_DATAERROR;
			musb_writew(epio, MUSB_RXCSR, rx_csr);

			goto finish;
		} else {
			dev_dbg(musb->controller, "RX end %d ISO data error\n", epnum);
			/* packet error reported later */
			iso_err = true;
		}
	} else if (rx_csr & MUSB_RXCSR_INCOMPRX) {
		dev_dbg(musb->controller, "end %d high bandwidth incomplete ISO packet RX\n",
				epnum);
		status = -EPROTO;
	}

	/* faults abort the transfer */
	if (status) {
		musb_h_flush_rxfifo(hw_ep, MUSB_RXCSR_CLRDATATOG);
		musb_writeb(epio, MUSB_RXINTERVAL, 0);
		done = true;
		goto finish;
	}

	/* thorough shutdown for now ... given more precise fault handling
	 * and better queueing support, we might keep a DMA pipeline going
	 * while processing this irq for earlier completions.
	 */

	if (rx_csr & MUSB_RXCSR_H_REQPKT)  {
		dev_dbg(musb->controller, "RXCSR%d %04x, reqpkt, len %zu%s\n", epnum, rx_csr,
				xfer_len, "");
		rx_csr &= ~MUSB_RXCSR_H_REQPKT;

		musb_ep_select(mbase, epnum);
		musb_writew(epio, MUSB_RXCSR,
				MUSB_RXCSR_H_WZC_BITS | rx_csr);
	}

	if (urb->status == -EINPROGRESS) {
		/* if no errors, be sure a packet is ready for unloading */
		if (unlikely(!(rx_csr & MUSB_RXCSR_RXPKTRDY))) {
			status = -EPROTO;
			dev_err(musb->controller, "Rx interrupt with no errors or packet!\n");

			/* FIXME this is another "SHOULD NEVER HAPPEN" */

/* SCRUB (RX) */
			/* do the proper sequence to abort the transfer */
			musb_ep_select(mbase, epnum);
			val &= ~MUSB_RXCSR_H_REQPKT;
			musb_writew(epio, MUSB_RXCSR, val);
			goto finish;
		}

		/* we are expecting IN packets */
		if (1) {
			done = musb_host_packet_rx(musb, urb,
					epnum, iso_err);

			dev_dbg(musb->controller, "read %spacket\n", done ? "last " : "");
		}
	}

finish:
	urb->actual_length += xfer_len;
	qh->offset += xfer_len;
	if (done) {

		if (urb->status == -EINPROGRESS)
			urb->status = status;
		musb_advance_schedule(musb, urb, hw_ep, USB_DIR_IN);
	}
}

/* schedule nodes correspond to peripheral endpoints, like an OHCI QH.
 * the software schedule associates multiple such nodes with a given
 * host side hardware endpoint + direction; scheduling may activate
 * that hardware endpoint.
 */
static int musb_schedule(
	struct musb		*musb,
	struct musb_qh		*qh,
	int			is_in)
{
	int			idle;
	int			best_diff;
	int			best_end, epnum;
	struct musb_hw_ep	*hw_ep = NULL;
	struct list_head	*head = NULL;
	u8			toggle;
	u8			txtype;
	struct urb		*urb = next_urb(qh);

	/* use fixed hardware for control and bulk */
	if (qh->type == USB_ENDPOINT_XFER_CONTROL) {
		head = &musb->control;
		hw_ep = musb->control_ep;
		goto success;
	}

	/* else, periodic transfers get muxed to other endpoints */

	/*
	 * We know this qh hasn't been scheduled, so all we need to do
	 * is choose which hardware endpoint to put it on ...
	 *
	 * REVISIT what we really want here is a regular schedule tree
	 * like e.g. OHCI uses.
	 */
	best_diff = 4096;
	best_end = -1;

	for (epnum = 1, hw_ep = musb->endpoints + 1;
			epnum < musb->nr_endpoints;
			epnum++, hw_ep++) {
		int	diff;

		if (musb_ep_get_qh(hw_ep, is_in) != NULL)
			continue;

		if (hw_ep == musb->bulk_ep)
			continue;

		if (is_in)
			diff = hw_ep->max_packet_sz_rx;
		else
			diff = hw_ep->max_packet_sz_tx;
		diff -= (qh->maxpacket * qh->hb_mult);

		if (diff >= 0 && best_diff > diff) {

			/*
			 * Mentor controller has a bug in that if we schedule
			 * a BULK Tx transfer on an endpoint that had earlier
			 * handled ISOC then the BULK transfer has to start on
			 * a zero toggle.  If the BULK transfer starts on a 1
			 * toggle then this transfer will fail as the mentor
			 * controller starts the Bulk transfer on a 0 toggle
			 * irrespective of the programming of the toggle bits
			 * in the TXCSR register.  Check for this condition
			 * while allocating the EP for a Tx Bulk transfer.  If
			 * so skip this EP.
			 */
			hw_ep = musb->endpoints + epnum;
			toggle = usb_gettoggle(urb->dev, qh->epnum, !is_in);
			txtype = (musb_readb(hw_ep->regs, MUSB_TXTYPE)
					>> 4) & 0x3;
			if (!is_in && (qh->type == USB_ENDPOINT_XFER_BULK) &&
				toggle && (txtype == USB_ENDPOINT_XFER_ISOC))
				continue;

			best_diff = diff;
			best_end = epnum;
		}
	}
	/* use bulk reserved ep1 if no other ep is free */
	if (best_end < 0 && qh->type == USB_ENDPOINT_XFER_BULK) {
		hw_ep = musb->bulk_ep;
		if (is_in)
			head = &musb->in_bulk;
		else
			head = &musb->out_bulk;

		/* Enable bulk RX/TX NAK timeout scheme when bulk requests are
		 * multiplexed.  This scheme doen't work in high speed to full
		 * speed scenario as NAK interrupts are not coming from a
		 * full speed device connected to a high speed device.
		 * NAK timeout interval is 8 (128 uframe or 16ms) for HS and
		 * 4 (8 frame or 8ms) for FS device.
		 */
		if (qh->dev)
			qh->intv_reg =
				(USB_SPEED_HIGH == qh->dev->speed) ? 8 : 4;
		goto success;
	} else if (best_end < 0) {
		return -ENOSPC;
	}

	idle = 1;
	qh->mux = 0;
	hw_ep = musb->endpoints + best_end;
	dev_dbg(musb->controller, "qh %p periodic slot %d\n", qh, best_end);
success:
	if (head) {
		idle = list_empty(head);
		list_add_tail(&qh->ring, head);
		qh->mux = 1;
	}
	qh->hw_ep = hw_ep;
	qh->hep->hcpriv = qh;
	if (idle)
		musb_start_urb(musb, is_in, qh);
	return 0;
}


/* check if transaction translator is needed for device */
static int tt_needed(struct musb *musb, struct usb_device *dev)
{
	if ((musb_readb(musb->mregs, MUSB_POWER) & MUSB_POWER_HSMODE) &&
			(dev->speed < USB_SPEED_HIGH))
		return 1;
	return 0;
}

int musb_urb_enqueue(
	struct usb_hcd			*hcd,
	struct urb			*urb,
	gfp_t				mem_flags)
{
	unsigned long			flags;
	struct musb			*musb = hcd_to_musb(hcd);
	struct usb_host_endpoint	*hep = urb->ep;
	struct musb_qh			*qh;
	struct usb_endpoint_descriptor	*epd = &hep->desc;
	int				ret;
	unsigned			type_reg;
	unsigned			interval;
	/* host role must be active */
	if (!is_host_active(musb) || !musb->is_active)
		return -ENODEV;

	spin_lock_irqsave(&musb->lock, flags);
	ret = usb_hcd_link_urb_to_ep(hcd, urb);
	qh = ret ? NULL : hep->hcpriv;
	if (qh)
		urb->hcpriv = qh;
	spin_unlock_irqrestore(&musb->lock, flags);

	/* DMA mapping was already done, if needed, and this urb is on
	 * hep->urb_list now ... so we're done, unless hep wasn't yet
	 * scheduled onto a live qh.
	 *
	 * REVISIT best to keep hep->hcpriv valid until the endpoint gets
	 * disabled, testing for empty qh->ring and avoiding qh setup costs
	 * except for the first urb queued after a config change.
	 */
	if (qh || ret)
		return ret;

	/* Allocate and initialize qh, minimizing the work done each time
	 * hw_ep gets reprogrammed, or with irqs blocked.  Then schedule it.
	 *
	 * REVISIT consider a dedicated qh kmem_cache, so it's harder
	 * for bugs in other kernel code to break this driver...
	 */
	qh = kzalloc(sizeof *qh, mem_flags);
	if (!qh) {
		spin_lock_irqsave(&musb->lock, flags);
		usb_hcd_unlink_urb_from_ep(hcd, urb);
		spin_unlock_irqrestore(&musb->lock, flags);
		return -ENOMEM;
	}

	qh->hep = hep;
	qh->dev = urb->dev;
	INIT_LIST_HEAD(&qh->ring);
	qh->is_ready = 1;

	qh->maxpacket = usb_endpoint_maxp(epd);
	qh->type = usb_endpoint_type(epd);

	/* Bits 11 & 12 of wMaxPacketSize encode high bandwidth multiplier.
	 * Some musb cores don't support high bandwidth ISO transfers; and
	 * we don't (yet!) support high bandwidth interrupt transfers.
	 */
	qh->hb_mult = 1 + ((qh->maxpacket >> 11) & 0x03);
	if (qh->hb_mult > 1) {
		int ok = (qh->type == USB_ENDPOINT_XFER_ISOC);

		if (ok)
			ok = (usb_pipein(urb->pipe) && musb->hb_iso_rx)
				|| (usb_pipeout(urb->pipe) && musb->hb_iso_tx);
		if (!ok) {
			ret = -EMSGSIZE;
			goto done;
		}
		qh->maxpacket &= 0x7ff;
	}

	qh->epnum = usb_endpoint_num(epd);

	/* NOTE: urb->dev->devnum is wrong during SET_ADDRESS */
	qh->addr_reg = (u8) usb_pipedevice(urb->pipe);

	/* precompute rxtype/txtype/type0 register */
	type_reg = (qh->type << 4) | qh->epnum;
	switch (urb->dev->speed) {
	case USB_SPEED_LOW:
		type_reg |= 0xc0;
		break;
	case USB_SPEED_FULL:
		type_reg |= 0x80;
		break;
	default:
		type_reg |= 0x40;
		break;
	}
	qh->type_reg = type_reg;

	/* Precompute RXINTERVAL/TXINTERVAL register */
	switch (qh->type) {
	case USB_ENDPOINT_XFER_INT:
		/*
		 * Full/low speeds use the  linear encoding,
		 * high speed uses the logarithmic encoding.
		 */
		if (urb->dev->speed <= USB_SPEED_FULL) {
			interval = max_t(u8, epd->bInterval, 1);
			break;
		}
		fallthrough;
	case USB_ENDPOINT_XFER_ISOC:
		/* ISO always uses logarithmic encoding */
		interval = min_t(u8, epd->bInterval, 16);
		break;
	default:
		/* REVISIT we actually want to use NAK limits, hinting to the
		 * transfer scheduling logic to try some other qh, e.g. try
		 * for 2 msec first:
		 *
		 * interval = (USB_SPEED_HIGH == urb->dev->speed) ? 16 : 2;
		 *
		 * The downside of disabling this is that transfer scheduling
		 * gets VERY unfair for nonperiodic transfers; a misbehaving
		 * peripheral could make that hurt.  That's perfectly normal
		 * for reads from network or serial adapters ... so we have
		 * partial NAKlimit support for bulk RX.
		 *
		 * The upside of disabling it is simpler transfer scheduling.
		 */
		interval = 0;
		break;
	}
	qh->intv_reg = interval;

	/* precompute addressing for external hub/tt ports */
	if (musb->is_multipoint) {
		struct usb_device	*parent = urb->dev->parent;

		if (parent) {
			qh->h_addr_reg = (u8) parent->devnum;

			if (tt_needed(musb, urb->dev)) {
				u16 hub_port = find_tt(urb->dev);
				qh->h_addr_reg = (u8) (hub_port >> 8);
				qh->h_port_reg = (u8) (hub_port & 0xff);
			}
		}
	}

	/* invariant: hep->hcpriv is null OR the qh that's already scheduled.
	 * until we get real dma queues (with an entry for each urb/buffer),
	 * we only have work to do in the former case.
	 */
	spin_lock_irqsave(&musb->lock, flags);
	if (hep->hcpriv || !next_urb(qh)) {
		/* some concurrent activity submitted another urb to hep...
		 * odd, rare, error prone, but legal.
		 */
		kfree(qh);
		qh = NULL;
		ret = 0;
	} else
		ret = musb_schedule(musb, qh,
				epd->bEndpointAddress & USB_ENDPOINT_DIR_MASK);

	if (ret == 0) {
		urb->hcpriv = qh;
		/* FIXME set urb->start_frame for iso/intr, it's tested in
		 * musb_start_urb(), but otherwise only konicawc cares ...
		 */
	}
	spin_unlock_irqrestore(&musb->lock, flags);

done:
	if (ret != 0) {
		spin_lock_irqsave(&musb->lock, flags);
		usb_hcd_unlink_urb_from_ep(hcd, urb);
		spin_unlock_irqrestore(&musb->lock, flags);
		kfree(qh);
	}
	return ret;
}

/*
 * abort a transfer that's at the head of a hardware queue.
 * called with controller locked, irqs blocked
 * that hardware queue advances to the next transfer, unless prevented
 */
static noinline int musb_cleanup_urb(struct urb *urb, struct musb_qh *qh)
{
	struct musb_hw_ep	*ep = qh->hw_ep;
	void __iomem		*epio = ep->regs;
	unsigned		hw_end = ep->epnum;
	void __iomem		*regs = ep->musb->mregs;
	int			is_in = usb_pipein(urb->pipe);
	int			status = 0;
	u16			csr;

	musb_ep_select(regs, hw_end);

	/* turn off DMA requests, discard state, stop polling ... */
	if (ep->epnum && is_in) {
		/* giveback saves bulk toggle */
		csr = musb_h_flush_rxfifo(ep, 0);

		/* REVISIT we still get an irq; should likely clear the
		 * endpoint's irq status here to avoid bogus irqs.
		 * clearing that status is platform-specific...
		 */
	} else if (ep->epnum) {
		musb_h_tx_flush_fifo(ep);
		csr = musb_readw(epio, MUSB_TXCSR);
		csr &= ~(MUSB_TXCSR_AUTOSET
			| MUSB_TXCSR_DMAENAB
			| MUSB_TXCSR_H_RXSTALL
			| MUSB_TXCSR_H_NAKTIMEOUT
			| MUSB_TXCSR_H_ERROR
			| MUSB_TXCSR_TXPKTRDY);
		musb_writew(epio, MUSB_TXCSR, csr);
		/* REVISIT may need to clear FLUSHFIFO ... */
		musb_writew(epio, MUSB_TXCSR, csr);
		/* flush cpu writebuffer */
		csr = musb_readw(epio, MUSB_TXCSR);
	} else  {
		musb_h_ep0_flush_fifo(ep);
	}
	if (status == 0)
		musb_advance_schedule(ep->musb, urb, ep, is_in);
	return status;
}

int musb_urb_dequeue(struct usb_hcd *hcd, struct urb *urb, int status)
{
	struct musb		*musb = hcd_to_musb(hcd);
	struct musb_qh		*qh;
	unsigned long		flags;
	int			is_in  = usb_pipein(urb->pipe);
	int			ret = 0;

	dev_dbg(musb->controller, "urb=%p, dev%d ep%d%s\n", urb,
			usb_pipedevice(urb->pipe),
			usb_pipeendpoint(urb->pipe),
			is_in ? "in" : "out");

	spin_lock_irqsave(&musb->lock, flags);

	qh = urb->hcpriv;
	if (!qh)
		goto done;

	/*
	 * Any URB not actively programmed into endpoint hardware can be
	 * immediately given back; that's any URB not at the head of an
	 * endpoint queue, unless someday we get real DMA queues.  And even
	 * if it's at the head, it might not be known to the hardware...
	 *
	 * Otherwise abort current transfer, pending DMA, etc.; urb->status
	 * has already been updated.  This is a synchronous abort; it'd be
	 * OK to hold off until after some IRQ, though.
	 *
	 * NOTE: qh is invalid unless !list_empty(&hep->urb_list)
	 */
	if (!qh->is_ready
			|| urb->urb_list.prev != &qh->hep->urb_list
			|| musb_ep_get_qh(qh->hw_ep, is_in) != qh) {
		int	ready = qh->is_ready;

		qh->is_ready = 0;
		musb_giveback(musb, urb, 0);
		qh->is_ready = ready;

		/* If nothing else (usually musb_giveback) is using it
		 * and its URB list has emptied, recycle this qh.
		 */
		if (ready && list_empty(&qh->hep->urb_list)) {
			qh->hep->hcpriv = NULL;
			list_del(&qh->ring);
			kfree(qh);
		}
	} else
		ret = musb_cleanup_urb(urb, qh);
done:
	spin_unlock_irqrestore(&musb->lock, flags);
	return ret;
}

int musb_host_alloc(struct musb *musb)
{
	/* usbcore sets dev->driver_data to hcd, and sometimes uses that... */
	musb->hcd = xzalloc(sizeof(struct usb_hcd));

	musb->hcd->hcd_priv = musb;

	return 0;
}

void musb_host_free(struct musb *musb)
{
}

int musb_host_setup(struct musb *musb, int power_budget)
{
	struct usb_hcd *hcd = musb->hcd;

	MUSB_HST_MODE(musb);

	hcd->self.otg_port = 1;

	return 0;
}

void musb_host_resume_root_hub(struct musb *musb)
{
}

void musb_host_poke_root_hub(struct musb *musb)
{
}
