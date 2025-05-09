// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: © 2007 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix

/* ls.c - list files and directories */

#include <common.h>
#include <command.h>
#include <fs.h>
#include <linux/stat.h>
#include <errno.h>
#include <malloc.h>
#include <getopt.h>
#include <stringlist.h>

/*
 * SIZELEN = strlen(itoa(MAX_LFS_FILESIZE)) + 1;
 */
#ifdef CONFIG_64BIT
#define SIZELEN		20
#else
#define SIZELEN		14
#endif

static void ls_one(const char *path, const char* fullname)
{
	char modestr[11];
	unsigned int namelen = strlen(path);
	struct stat s;
	int ret;

	ret = lstat(fullname, &s);
	if (ret)
		return;

	mkmodestr(s.st_mode, modestr);
	printf("%s %*llu %*.*s", modestr, SIZELEN, s.st_size, namelen,
	       namelen, path);

	if (S_ISLNK(s.st_mode)) {
		char realname[PATH_MAX];

		memset(realname, 0, PATH_MAX);

		if (readlink(fullname, realname, PATH_MAX - 1) >= 0)
			printf(" -> %s", realname);
	}

	puts("\n");
}

int ls(const char *path, ulong flags)
{
	DIR *dir;
	struct dirent *d;
	char tmp[PATH_MAX];
	struct stat s;
	struct string_list sl;
	struct string_list *entry;
	int ret;

	string_list_init(&sl);

	if (stat(path, &s))
		return -errno;

	if (flags & LS_SHOWARG && S_ISDIR(s.st_mode))
		printf("%s:\n", path);

	if (!S_ISDIR(s.st_mode)) {
		ls_one(path, path);
		return 0;
	}

	dir = opendir(path);
	if (!dir)
		return -errno;

	while ((d = readdir(dir))) {
		if (!strcmp(d->d_name, "."))
			continue;
		if (!strcmp(d->d_name, ".."))
			continue;
		string_list_add_sorted(&sl, d->d_name);
	}

	closedir(dir);

	if (flags & LS_COLUMN) {
		string_list_print_by_column(&sl);
	} else {
		string_list_for_each_entry(entry, &sl) {
			sprintf(tmp, "%s/%s", path, entry->str);
			ret = lstat(tmp, &s);
			if (ret) {
				printf("%s: %pe\n", tmp, ERR_PTR(ret));
				continue;
			}

			ls_one(entry->str, tmp);
		}
	}

	if (!(flags & LS_RECURSIVE))
		goto out;

	string_list_for_each_entry(entry, &sl) {
		sprintf(tmp, "%s/%s", path, entry->str);

		ret = lstat(tmp, &s);
		if (ret) {
			printf("%s: %pe\n", tmp, ERR_PTR(ret));
			continue;
		}

		if (S_ISDIR(s.st_mode))
			ls(tmp, flags);
	}

out:
	string_list_free(&sl);

	return 0;
}

static int do_ls(int argc, char *argv[])
{
	int ret, opt, o, exitcode = 0;
	struct stat s;
	ulong flags = LS_COLUMN;
	struct string_list sl;

	if (!strcmp(argv[0], "ll"))
		flags &= ~LS_COLUMN;

	while((opt = getopt(argc, argv, "RCl")) > 0) {
		switch(opt) {
		case 'R':
			flags |= LS_RECURSIVE | LS_SHOWARG;
			break;
		case 'C':
			flags |= LS_COLUMN;
			break;
		case 'l':
			flags &= ~LS_COLUMN;
			break;
		default:
			return COMMAND_ERROR_USAGE;
		}
	}

	if (argc - optind > 1)
		flags |= LS_SHOWARG;

	if (optind == argc) {
		ret = ls(".", flags);
		return ret ? 1 : 0;
	}

	string_list_init(&sl);

	o = optind;

	/* first pass: all files */
	while (o < argc) {
		ret = stat(argv[o], &s);
		if (ret) {
			printf("%s: %s: %m\n", argv[0], argv[o]);
			o++;
			exitcode = COMMAND_ERROR;
			continue;
		}

		if (!S_ISDIR(s.st_mode)) {
			if (flags & LS_COLUMN)
				string_list_add_sorted(&sl, argv[o]);
			else
				ls_one(argv[o], argv[o]);
		}

		o++;
	}

	if (flags & LS_COLUMN)
		string_list_print_by_column(&sl);

	string_list_free(&sl);

	o = optind;

	/* second pass: directories */
	while (o < argc) {
		ret = stat(argv[o], &s);
		if (ret) {
			o++;
			exitcode = COMMAND_ERROR;
			continue;
		}

		if (S_ISDIR(s.st_mode)) {
			ret = ls(argv[o], flags);
			if (ret) {
				perror("ls");
				exitcode = COMMAND_ERROR;
				o++;
				continue;
			}
		}

		o++;
	}

	return exitcode;
}

BAREBOX_CMD_HELP_START(ls)
BAREBOX_CMD_HELP_TEXT("List information about the specified files or directories.")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("Options:")
BAREBOX_CMD_HELP_OPT ("-l",  "long format")
BAREBOX_CMD_HELP_OPT ("-C",  "column format (opposite of long format)")
BAREBOX_CMD_HELP_OPT ("-R",  "list subdirectories recursively")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(ls)
	.cmd		= do_ls,
	BAREBOX_CMD_DESC("list a file or directory")
	BAREBOX_CMD_OPTS("[-lCR] [FILEDIR...]")
	BAREBOX_CMD_GROUP(CMD_GRP_FILE)
	BAREBOX_CMD_HELP(cmd_ls_help)
BAREBOX_CMD_END

BAREBOX_CMD_START(ll)
	.cmd		= do_ls,
	BAREBOX_CMD_DESC("list a file or directory with details")
	BAREBOX_CMD_OPTS("[-lCR] [FILEDIR...]")
	BAREBOX_CMD_GROUP(CMD_GRP_FILE)
	BAREBOX_CMD_HELP(cmd_ls_help)
BAREBOX_CMD_END
