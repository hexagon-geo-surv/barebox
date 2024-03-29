// SPDX-License-Identifier: LGPL-2.1-or-later
// SPDX-FileCopyrightText: (C) 1991, 1992, 1993, 1996 Free Software Foundation, Inc.

#include <errno.h>
#include <fnmatch.h>
#include <linux/ctype.h>

# define ISUPPER(c) (isascii(c) && isupper(c))

/* Match STRING against the filename pattern PATTERN, returning zero if
   it matches, nonzero if not.  */
int fnmatch(const char *pattern, const char *string, int flags)
{
	register const char *p = pattern, *n = string;
	register char c;

/* Note that this evaluates C many times.  */
# define FOLD(c) ((flags & FNM_CASEFOLD) && ISUPPER (c) ? tolower (c) : (c))

	while ((c = *p++) != '\0') {
		c = FOLD(c);

		switch (c) {
		case '?':
			if (*n == '\0')
				return FNM_NOMATCH;
			else if ((flags & FNM_FILE_NAME) && *n == '/')
				return FNM_NOMATCH;
			else if ((flags & FNM_PERIOD) && *n == '.' &&
					 (n == string
					  || ((flags & FNM_FILE_NAME)
						  && n[-1] == '/'))) return FNM_NOMATCH;
			break;

		case '\\':
			if (!(flags & FNM_NOESCAPE)) {
				c = *p++;
				if (c == '\0')
					/* Trailing \ loses.  */
					return FNM_NOMATCH;
				c = FOLD(c);
			}
			if (FOLD(*n) != c)
				return FNM_NOMATCH;
			break;

		case '*':
			if ((flags & FNM_PERIOD) && *n == '.' &&
				(n == string || ((flags & FNM_FILE_NAME) && n[-1] == '/')))
				return FNM_NOMATCH;

			for (c = *p++; c == '?' || c == '*'; c = *p++) {
				if ((flags & FNM_FILE_NAME) && *n == '/')
					/* A slash does not match a wildcard under FNM_FILE_NAME.  */
					return FNM_NOMATCH;
				else if (c == '?') {
					/* A ? needs to match one character.  */
					if (*n == '\0')
						/* There isn't another character; no match.  */
						return FNM_NOMATCH;
					else
						/* One character of the string is consumed in matching
						   this ? wildcard, so *??? won't match if there are
						   less than three characters.  */
						++n;
				}
			}

			if (c == '\0')
				return 0;

			{
				char c1 = (!(flags & FNM_NOESCAPE) && c == '\\') ? *p : c;

				c1 = FOLD(c1);
				for (--p; *n != '\0'; ++n)
					if ((c == '[' || FOLD(*n) == c1) &&
						fnmatch(p, n, flags & ~FNM_PERIOD) == 0)
						return 0;
				return FNM_NOMATCH;
			}

		case '[':
		{
			/* Nonzero if the sense of the character class is inverted.  */
			register int not;

			if (*n == '\0')
				return FNM_NOMATCH;

			if ((flags & FNM_PERIOD) && *n == '.' &&
				(n == string || ((flags & FNM_FILE_NAME) && n[-1] == '/')))
				return FNM_NOMATCH;

			not = (*p == '!' || *p == '^');
			if (not)
				++p;

			c = *p++;
			for (;;) {
				register char cstart = c, cend = c;

				if (!(flags & FNM_NOESCAPE) && c == '\\') {
					if (*p == '\0')
						return FNM_NOMATCH;
					cstart = cend = *p++;
				}

				cstart = cend = FOLD(cstart);

				if (c == '\0')
					/* [ (unterminated) loses.  */
					return FNM_NOMATCH;

				c = *p++;
				c = FOLD(c);

				if ((flags & FNM_FILE_NAME) && c == '/')
					/* [/] can never match.  */
					return FNM_NOMATCH;

				if (c == '-' && *p != ']') {
					cend = *p++;
					if (!(flags & FNM_NOESCAPE) && cend == '\\')
						cend = *p++;
					if (cend == '\0')
						return FNM_NOMATCH;
					cend = FOLD(cend);

					c = *p++;
				}

				if (FOLD(*n) >= cstart && FOLD(*n) <= cend)
					goto matched;

				if (c == ']')
					break;
			}
			if (!not)
				return FNM_NOMATCH;
			break;

		  matched:;
			/* Skip the rest of the [...] that already matched.  */
			while (c != ']') {
				if (c == '\0')
					/* [... (unterminated) loses.  */
					return FNM_NOMATCH;

				c = *p++;
				if (!(flags & FNM_NOESCAPE) && c == '\\') {
					if (*p == '\0')
						return FNM_NOMATCH;
					/* XXX 1003.2d11 is unclear if this is right.  */
					++p;
				}
			}
			if (not)
				return FNM_NOMATCH;
		}
			break;

		default:
			if (c != FOLD(*n))
				return FNM_NOMATCH;
		}

		++n;
	}

	if (*n == '\0')
		return 0;

	if ((flags & FNM_LEADING_DIR) && *n == '/')
		/* The FNM_LEADING_DIR flag says that "foo*" matches "foobar/frobozz".  */
		return 0;

	return FNM_NOMATCH;

# undef FOLD
}
