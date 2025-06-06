// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2012 Free Electrons

#include <common.h>

#include <environment.h>
#include <fcntl.h>
#include <fs.h>
#include <globalvar.h>
#include <libbb.h>
#include <libfile.h>
#include <magicvar.h>

#include <asm/armlinux.h>

#include "hwdetect.h"

enum board_type {
	BOARD_ID_CFA10036 = 0,
	BOARD_ID_CFA10037 = 1,
	BOARD_ID_CFA10049 = 2,
	BOARD_ID_CFA10055 = 3,
	BOARD_ID_CFA10056 = 4,
	BOARD_ID_CFA10057 = 5,
	BOARD_ID_CFA10058 = 6,
};

struct cfa_eeprom_info {
	u8 board_id;
}__attribute__ ((packed));

static int cfa10036_read_eeprom(const char *file, struct cfa_eeprom_info *info)
{
	int fd;
	int ret;

	fd = open(file, O_RDONLY);
	if (fd < 0) {
		ret = fd;
		goto err;
	}

	ret = read_full(fd, info, sizeof(*info));
	if (ret < 0)
		goto err_open;

	ret = 0;

err_open:
	close(fd);
err:
	if (ret)
		pr_err("can not read eeprom %s (%pe)\n", file, ERR_PTR(ret));
	return ret;
}

void cfa10036_detect_hw(void)
{
	enum board_type cfa_type;
	struct cfa_eeprom_info info;
	char *board_name;
	int ret;

	ret = cfa10036_read_eeprom("/dev/eeprom0", &info);
	if (ret)
		cfa_type = BOARD_ID_CFA10036;
	else
		cfa_type = info.board_id;

	switch (cfa_type) {
	case BOARD_ID_CFA10036:
		board_name = "cfa10036";
		break;
	case BOARD_ID_CFA10037:
		board_name = "cfa10037";
		break;
	case BOARD_ID_CFA10049:
		board_name = "cfa10049";
		break;
	case BOARD_ID_CFA10055:
		board_name = "cfa10055";
		break;
	case BOARD_ID_CFA10056:
		board_name = "cfa10056";
		break;
	case BOARD_ID_CFA10057:
		board_name = "cfa10057";
		break;
	case BOARD_ID_CFA10058:
		board_name = "cfa10058";
		break;
	default:
		pr_err("Board ID not supported\n");
		return;
	}

	globalvar_add_simple("board.variant", board_name);

	pr_info("Booting on a CFA10036 with %s\n", board_name);
}

BAREBOX_MAGICVAR(global.board.variant, "The board variant");
