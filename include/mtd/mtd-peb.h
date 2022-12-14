/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __LINUX_MTD_MTDPEB_H
#define __LINUX_MTD_MTDPEB_H

#include <linux/mtd/mtd.h>

int mtd_peb_read(struct mtd_info *mtd, void *buf, int pnum, int offset,
		int len);
int mtd_peb_write(struct mtd_info *mtd, const void *buf, int pnum, int offset,
		 int len);

int mtd_peb_torture(struct mtd_info *mtd, int pnum);
int mtd_peb_erase(struct mtd_info *mtd, int pnum);
int mtd_peb_mark_bad(struct mtd_info *mtd, int pnum);
int mtd_peb_is_bad(struct mtd_info *mtd, int pnum);
int mtd_skip_bad(struct mtd_info *mtd, int *pnum);
int mtd_peb_check_all_ff(struct mtd_info *mtd, int pnum, int offset, int len,
			 int warn);
int mtd_peb_verify(struct mtd_info *mtd, const void *buf, int pnum,
				int offset, int len);
int mtd_num_pebs(struct mtd_info *mtd);
int mtd_peb_create_bitflips(struct mtd_info *mtd, int pnum, int offset,
				   int len, int num_bitflips, int random,
				   int info);
int mtd_peb_read_file(struct mtd_info *mtd, unsigned int peb_start,
		      unsigned int peb_last, void *buf, size_t len);
int mtd_peb_write_file(struct mtd_info *mtd, int peb_start, int max_pebs,
		       const void *buf, size_t len);

#endif /* __LINUX_MTD_MTDPEB_H */
