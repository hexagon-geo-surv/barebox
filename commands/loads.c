// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: © 2000-2004 Wolfgang Denk <wd@denx.de>, DENX Software Engineering

/* Serial up- and download support */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <s_record.h>
#include <net.h>

static ulong load_serial(ulong offset);
static int read_record(char *buf, ulong len);
static int do_echo = 1;

#ifdef CONFIG_CMD_SAVES
static int save_serial(ulong offset, ulong size);
static int write_record(char *buf);
#endif /* CONFIG_CMD_SAVES */

static int do_load_serial(int argc, char *argv[])
{
	ulong offset = 0;
	ulong addr;
	int i;
	int rcode = 0;

	getenv_bool("loads_echo", &do_echo);

	if (argc == 2) {
		offset = simple_strtoul(argv[1], NULL, 16);
	}

	printf ("## Ready for S-Record download ...\n");

	addr = load_serial(offset);

	/*
	 * Gather any trailing characters (for instance, the ^D which
	 * is sent by 'cu' after sending a file), and give the
	 * box some time (100 * 1 ms)
	 */
	for (i=0; i<100; ++i) {
		if (tstc()) {
			(void) getchar();
		}
		udelay(1000);
	}

	if (addr == ~0) {
		printf("## S-Record download aborted\n");
		rcode = 1;
	} else {
		printf("## Start Addr      = 0x%08lX\n", addr);
	}

	return rcode;
}

static ulong load_serial(ulong offset)
{
	char	record[SREC_MAXRECLEN + 1];	/* buffer for one S-Record	*/
	char	binbuf[SREC_MAXBINLEN];		/* buffer for binary data	*/
	int	binlen;				/* no. of data bytes in S-Rec.	*/
	int	type;				/* return code for record type	*/
	ulong	addr;				/* load address from S-Record	*/
	ulong	size;				/* number of bytes transferred	*/
	ulong	store_addr;
	ulong	start_addr = ~0;
	ulong	end_addr   =  0;
	int	line_count =  0;

	while (read_record(record, SREC_MAXRECLEN + 1) >= 0) {
		type = srec_decode(record, &binlen, &addr, binbuf);

		if (type < 0) {
			return ~0;	/* Invalid S-Record		*/
		}

		switch (type) {
		case SREC_DATA2:
		case SREC_DATA3:
		case SREC_DATA4:
			store_addr = addr + offset;
			memcpy((char *)(store_addr), binbuf, binlen);
			if ((store_addr) < start_addr)
				start_addr = store_addr;
			if ((store_addr + binlen - 1) > end_addr)
				end_addr = store_addr + binlen - 1;
		break;
		case SREC_END2:
		case SREC_END3:
		case SREC_END4:
			udelay(10000);
			size = end_addr - start_addr + 1;
			printf("\n"
			    "## First Load Addr = 0x%08lX\n"
			    "## Last  Load Addr = 0x%08lX\n"
			    "## Total Size      = 0x%08lX = %ld Bytes\n",
			    start_addr, end_addr, size, size
			    );
			pr_setenv("filesize", "%lX", size);
			return addr;
		case SREC_START:
			break;
		default:
			break;
		}
		if (!do_echo) {	/* print a '.' every 100 lines */
			if ((++line_count % 100) == 0)
				putchar('.');
		}
	}

	return ~0;			/* Download aborted		*/
}

static int read_record(char *buf, ulong len)
{
	char *p;
	char c;

	--len;	/* always leave room for terminating '\0' byte */

	for (p=buf; p < buf+len; ++p) {
		c = getchar();		/* read character		*/
		if (do_echo)
			putchar(c);

		switch (c) {
		case '\r':
		case '\n':
			*p = '\0';
			return p - buf;
		case '\0':
		case 0x03:			/* ^C - Control C		*/
			return -1;
		default:
			*p = c;
		}
	}

	/* line too long - truncate */
	*p = '\0';
	return p - buf;
}

#ifdef CONFIG_CMD_SAVES
static int do_save_serial(int argc, char *argv[])
{
	ulong offset = 0;
	ulong size   = 0;

	if (argc >= 2) {
		offset = simple_strtoul(argv[1], NULL, 16);
	}

	if (argc == 3) {
		size = simple_strtoul(argv[2], NULL, 16);
	}

	printf ("## Ready for S-Record upload, press ENTER to proceed ...\n");
	for (;;) {
		if (getchar() == '\r')
			break;
	}
	if (save_serial(offset, size)) {
		printf("## S-Record upload aborted\n");
	} else {
		printf("## S-Record upload complete\n");
	}

	return 0;
}

#define SREC3_START				"S0030000FC\n"
#define SREC3_FORMAT			"S3%02X%08lX%s%02X\n"
#define SREC3_END				"S70500000000FA\n"
#define SREC_BYTES_PER_RECORD	16

static int save_serial(ulong address, ulong count)
{
	int i, c, reclen, checksum, length;
	char *hex = "0123456789ABCDEF";
	char record[2*SREC_BYTES_PER_RECORD+16]; /* buffer for one S-Record */
	char data[2*SREC_BYTES_PER_RECORD+1];	/* buffer for hex data	*/

	reclen = 0;
	checksum = 0;

	if (write_record(SREC3_START))	/* write the header */
		return -1;

	do {
		if (count) {		/* collect hex data in the buffer  */
			c = *(volatile uchar*)(address + reclen);	/* get one byte    */
			checksum += c;	/* accumulate checksum */
			data[2*reclen]   = hex[(c>>4)&0x0f];
			data[2*reclen+1] = hex[c & 0x0f];
			data[2*reclen+2] = '\0';
			++reclen;
			--count;
		}

		if (reclen == SREC_BYTES_PER_RECORD || count == 0) {
			/* enough data collected for one record: dump it */
			if (reclen) {	/* build & write a data record: */
				/* address + data + checksum */
				length = 4 + reclen + 1;

				/* accumulate length bytes into checksum */
				for (i = 0; i < 2; i++)
					checksum += (length >> (8*i)) & 0xff;

				/* accumulate address bytes into checksum: */
				for (i = 0; i < 4; i++)
					checksum += (address >> (8*i)) & 0xff;

				/* make proper checksum byte: */
				checksum = ~checksum & 0xff;

				/* output one record: */
				sprintf(record, SREC3_FORMAT, length, address, data, checksum);
				if (write_record(record))
					return -1;
			}
			address  += reclen;  /* increment address */
			checksum  = 0;
			reclen    = 0;
		}
	} while (count);

	if (write_record(SREC3_END))	/* write the final record */
		return -1;
	return 0;
}

static int write_record(char *buf)
{
	char c;

	while ((c = *buf++))
		putchar(c);

	/* Check for the console hangup (if any different from serial) */

	if (ctrlc()) {
		return -1;
	}
	return 0;
}
#endif /* CONFIG_CMD_SAVES */

BAREBOX_CMD_HELP_START(loads)
BAREBOX_CMD_HELP_TEXT("Load S-Record file over serial line with offset OFFS.")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(loads)
	.cmd		= do_load_serial,
	BAREBOX_CMD_DESC("load binary file over serial line (S-Records)")
	BAREBOX_CMD_OPTS("OFFS")
	BAREBOX_CMD_GROUP(CMD_GRP_BOOT)
	BAREBOX_CMD_HELP(cmd_loads_help)
BAREBOX_CMD_END

/*
 * SAVES always requires LOADS support, but not vice versa
 */

#ifdef CONFIG_CMD_SAVES
BAREBOX_CMD_HELP_START(saves)
BAREBOX_CMD_HELP_TEXT("Save S-Record file to serial line with offset OFFS and length LEN.")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(saves)
	.cmd		= do_save_serial,
	BAREBOX_CMD_DESC("save file over serial line (S-Records)")
	BAREBOX_CMD_OPTS("OFFS LEN")
	BAREBOX_CMD_GROUP(CMD_GRP_BOOT)
	BAREBOX_CMD_HELP(cmd_saves_help)
BAREBOX_CMD_END
#endif	/* CONFIG_CMD_SAVES */
