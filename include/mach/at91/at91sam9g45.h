/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: 2008-2009 Atmel Corporation */

/*
 * Chip-specific header file for the AT91SAM9G45 family
 *
 * Common definitions.
 * Based on AT91SAM9G45 preliminary datasheet.
 */

#ifndef AT91SAM9G45_H
#define AT91SAM9G45_H

/*
 * Peripheral identifiers/interrupts.
 */
#define AT91SAM9G45_ID_PIOA	2	/* Parallel I/O Controller A */
#define AT91SAM9G45_ID_PIOB	3	/* Parallel I/O Controller B */
#define AT91SAM9G45_ID_PIOC	4	/* Parallel I/O Controller C */
#define AT91SAM9G45_ID_PIODE	5	/* Parallel I/O Controller D and E */
#define AT91SAM9G45_ID_TRNG	6	/* True Random Number Generator */
#define AT91SAM9G45_ID_US0	7	/* USART 0 */
#define AT91SAM9G45_ID_US1	8	/* USART 1 */
#define AT91SAM9G45_ID_US2	9	/* USART 2 */
#define AT91SAM9G45_ID_US3	10	/* USART 3 */
#define AT91SAM9G45_ID_MCI0	11	/* High Speed Multimedia Card Interface 0 */
#define AT91SAM9G45_ID_TWI0	12	/* Two-Wire Interface 0 */
#define AT91SAM9G45_ID_TWI1	13	/* Two-Wire Interface 1 */
#define AT91SAM9G45_ID_SPI0	14	/* Serial Peripheral Interface 0 */
#define AT91SAM9G45_ID_SPI1	15	/* Serial Peripheral Interface 1 */
#define AT91SAM9G45_ID_SSC0	16	/* Synchronous Serial Controller 0 */
#define AT91SAM9G45_ID_SSC1	17	/* Synchronous Serial Controller 1 */
#define AT91SAM9G45_ID_TCB	18	/* Timer Counter 0, 1, 2, 3, 4 and 5 */
#define AT91SAM9G45_ID_PWMC	19	/* Pulse Width Modulation Controller */
#define AT91SAM9G45_ID_TSC	20	/* Touch Screen ADC Controller */
#define AT91SAM9G45_ID_DMA	21	/* DMA Controller */
#define AT91SAM9G45_ID_UHPHS	22	/* USB Host High Speed */
#define AT91SAM9G45_ID_LCDC	23	/* LCD Controller */
#define AT91SAM9G45_ID_AC97C	24	/* AC97 Controller */
#define AT91SAM9G45_ID_EMAC	25	/* Ethernet MAC */
#define AT91SAM9G45_ID_ISI	26	/* Image Sensor Interface */
#define AT91SAM9G45_ID_UDPHS	27	/* USB Device High Speed */
#define AT91SAM9G45_ID_AESTDESSHA 28	/* AES + T-DES + SHA */
#define AT91SAM9G45_ID_MCI1	29	/* High Speed Multimedia Card Interface 1 */
#define AT91SAM9G45_ID_VDEC	30	/* Video Decoder */
#define AT91SAM9G45_ID_IRQ0	31	/* Advanced Interrupt Controller */

/*
 * User Peripheral physical base addresses.
 */
#define AT91SAM9G45_BASE_UDPHS		0xfff78000
#define AT91SAM9G45_BASE_TCB0		0xfff7c000
#define AT91SAM9G45_BASE_TC0		0xfff7c000
#define AT91SAM9G45_BASE_TC1		0xfff7c040
#define AT91SAM9G45_BASE_TC2		0xfff7c080
#define AT91SAM9G45_BASE_MCI0		0xfff80000
#define AT91SAM9G45_BASE_TWI0		0xfff84000
#define AT91SAM9G45_BASE_TWI1		0xfff88000
#define AT91SAM9G45_BASE_US0		0xfff8c000
#define AT91SAM9G45_BASE_US1		0xfff90000
#define AT91SAM9G45_BASE_US2		0xfff94000
#define AT91SAM9G45_BASE_US3		0xfff98000
#define AT91SAM9G45_BASE_SSC0		0xfff9c000
#define AT91SAM9G45_BASE_SSC1		0xfffa0000
#define AT91SAM9G45_BASE_SPI0		0xfffa4000
#define AT91SAM9G45_BASE_SPI1		0xfffa8000
#define AT91SAM9G45_BASE_AC97C		0xfffac000
#define AT91SAM9G45_BASE_TSC		0xfffb0000
#define AT91SAM9G45_BASE_ISI		0xfffb4000
#define AT91SAM9G45_BASE_PWMC		0xfffb8000
#define AT91SAM9G45_BASE_EMAC		0xfffbc000
#define AT91SAM9G45_BASE_AES		0xfffc0000
#define AT91SAM9G45_BASE_TDES		0xfffc4000
#define AT91SAM9G45_BASE_SHA		0xfffc8000
#define AT91SAM9G45_BASE_TRNG		0xfffcc000
#define AT91SAM9G45_BASE_MCI1		0xfffd0000
#define AT91SAM9G45_BASE_TCB1		0xfffd4000
#define AT91SAM9G45_BASE_TC3		0xfffd4000
#define AT91SAM9G45_BASE_TC4		0xfffd4040
#define AT91SAM9G45_BASE_TC5		0xfffd4080

/*
 * System Peripherals
 */
#define AT91SAM9G45_BASE_ECC	0xffffe200
#define AT91SAM9G45_BASE_DDRSDRC1 0xffffe400
#define AT91SAM9G45_BASE_DDRSDRC0 0xffffe600
#define AT91SAM9G45_BASE_DMA	0xffffec00
#define AT91SAM9G45_BASE_SMC	0xffffe800
#define AT91SAM9G45_BASE_MATRIX	0xffffea00
#define AT91SAM9G45_BASE_DBGU	AT91_BASE_DBGU1
#define AT91SAM9G45_BASE_PIOA	0xfffff200
#define AT91SAM9G45_BASE_PIOB	0xfffff400
#define AT91SAM9G45_BASE_PIOC	0xfffff600
#define AT91SAM9G45_BASE_PIOD	0xfffff800
#define AT91SAM9G45_BASE_PIOE	0xfffffa00
#define AT91SAM9G45_BASE_RSTC	0xfffffd00
#define AT91SAM9G45_BASE_SHDWC	0xfffffd10
#define AT91SAM9G45_BASE_RTT	0xfffffd20
#define AT91SAM9G45_BASE_PIT	0xfffffd30
#define AT91SAM9G45_BASE_WDT	0xfffffd40
#define AT91SAM9G45_BASE_RTC	0xfffffdb0
#define AT91SAM9G45_BASE_GPBR	0xfffffd60

/*
 * Internal Memory.
 */
#define AT91SAM9G45_SRAM_BASE	0x00300000	/* Internal SRAM base address */
#define AT91SAM9G45_SRAM_SIZE	SZ_64K		/* Internal SRAM size (64Kb) */
#define AT91SAM9G45_SRAM_END	(AT91SAM9G45_SRAM_BASE + AT91SAM9G45_SRAM_SIZE)

#define AT91SAM9G45_ROM_BASE	0x00400000	/* Internal ROM base address */
#define AT91SAM9G45_ROM_SIZE	SZ_64K		/* Internal ROM size (64Kb) */

#define AT91SAM9G45_LCDC_BASE	0x00500000	/* LCD Controller */
#define AT91SAM9G45_UDPHS_FIFO	0x00600000	/* USB Device HS controller */
#define AT91SAM9G45_OHCI_BASE	0x00700000	/* USB Host controller (OHCI) */
#define AT91SAM9G45_EHCI_BASE	0x00800000	/* USB Host controller (EHCI) */
#define AT91SAM9G45_VDEC_BASE	0x00900000	/* Video Decoder Controller */

#endif
