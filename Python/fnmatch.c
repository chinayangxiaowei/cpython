/* http://svnweb.freebsd.org/base/head/usr.bin/csup/fnmatch.c?revision=216370&view=co */

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 * From FreeBSD fnmatch.c 1.11
 * $Id: fnmatch.c,v 1.3 2012-12-15 07:42:42 jkrell Exp $
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)fnmatch.c	8.2 (Berkeley) 4/16/94";
#endif /* LIBC_SCCS and not lint */

/*
 * Function fnmatch() as specified in POSIX 1003.2-1992, section B.6.
 * Compares a filename or pathname to a pattern.
 */

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>

#include "fnmatch.h"

#define	EOS	'\0'
#define	WEOS L'\0'

static const char *rangematch(const char *, char, int);
static const wchar_t* rangematch_w(const wchar_t*, wchar_t, int);

int
fnmatch(const char *pattern, const char *string, int flags)
{
	const char *stringstart;
	char c, test;

	for (stringstart = string;;)
		switch (c = *pattern++) {
		case EOS:
			if ((flags & FNM_LEADING_DIR) && *string == '/')
				return (0);
			return (*string == EOS ? 0 : FNM_NOMATCH);
		case '?':
			if (*string == EOS)
				return (FNM_NOMATCH);
			if (*string == '/' && (flags & FNM_PATHNAME))
				return (FNM_NOMATCH);
			if (*string == '.' && (flags & FNM_PERIOD) &&
			    (string == stringstart ||
			    ((flags & FNM_PATHNAME) && *(string - 1) == '/')))
				return (FNM_NOMATCH);
			++string;
			break;
		case '*':
			c = *pattern;
			/* Collapse multiple stars. */
			while (c == '*')
				c = *++pattern;

			if (*string == '.' && (flags & FNM_PERIOD) &&
			    (string == stringstart ||
			    ((flags & FNM_PATHNAME) && *(string - 1) == '/')))
				return (FNM_NOMATCH);

			/* Optimize for pattern with * at end or before /. */
			if (c == EOS)
				if (flags & FNM_PATHNAME)
					return ((flags & FNM_LEADING_DIR) ||
					    strchr(string, '/') == NULL ?
					    0 : FNM_NOMATCH);
				else
					return (0);
			else if (c == '/' && flags & FNM_PATHNAME) {
				if ((string = strchr(string, '/')) == NULL)
					return (FNM_NOMATCH);
				break;
			}

			/* General case, use recursion. */
			while ((test = *string) != EOS) {
				if (!fnmatch(pattern, string, flags & ~FNM_PERIOD))
					return (0);
				if (test == '/' && flags & FNM_PATHNAME)
					break;
				++string;
			}
			return (FNM_NOMATCH);
		case '[':
			if (*string == EOS)
				return (FNM_NOMATCH);
			if (*string == '/' && flags & FNM_PATHNAME)
				return (FNM_NOMATCH);
			if ((pattern =
			    rangematch(pattern, *string, flags)) == NULL)
				return (FNM_NOMATCH);
			++string;
			break;
		case '\\':
			if (!(flags & FNM_NOESCAPE)) {
				if ((c = *pattern++) == EOS) {
					c = '\\';
					--pattern;
				}
			}
			/* FALLTHROUGH */
		default:
			if (c == *string)
				;
			else if ((flags & FNM_CASEFOLD) &&
				 (tolower((unsigned char)c) ==
				  tolower((unsigned char)*string)))
				;
			else if ((flags & FNM_PREFIX_DIRS) && *string == EOS &&
			     ((c == '/' && string != stringstart) ||
			     (string == stringstart+1 && *stringstart == '/')))
				return (0);
			else
				return (FNM_NOMATCH);
			string++;
			break;
		}
	/* NOTREACHED */
}

static const char *
rangematch(const char *pattern, char test, int flags)
{
	int negate, ok;
	char c, c2;

	/*
	 * A bracket expression starting with an unquoted circumflex
	 * character produces unspecified results (IEEE 1003.2-1992,
	 * 3.13.2).  This implementation treats it like '!', for
	 * consistency with the regular expression syntax.
	 * J.T. Conklin (conklin@ngai.kaleida.com)
	 */
	if ( (negate = (*pattern == '!' || *pattern == '^')) )
		++pattern;

	if (flags & FNM_CASEFOLD)
		test = tolower((unsigned char)test);

	for (ok = 0; (c = *pattern++) != ']';) {
		if (c == '\\' && !(flags & FNM_NOESCAPE))
			c = *pattern++;
		if (c == EOS)
			return (NULL);

		if (flags & FNM_CASEFOLD)
			c = tolower((unsigned char)c);

		if (*pattern == '-'
		    && (c2 = *(pattern+1)) != EOS && c2 != ']') {
			pattern += 2;
			if (c2 == '\\' && !(flags & FNM_NOESCAPE))
				c2 = *pattern++;
			if (c2 == EOS)
				return (NULL);

			if (flags & FNM_CASEFOLD)
				c2 = tolower((unsigned char)c2);

			if ((unsigned char)c <= (unsigned char)test &&
			    (unsigned char)test <= (unsigned char)c2)
				ok = 1;
		} else if (c == test)
			ok = 1;
	}
	return (ok == negate ? NULL : pattern);
}


int
fnmatch_w(const wchar_t* pattern, const wchar_t* string, int flags)
{
	const wchar_t* stringstart;
	wchar_t c, test;

	for (stringstart = string;;)
		switch (c = *pattern++) {
		case WEOS:
			if ((flags & FNM_LEADING_DIR) && *string == L'/')
				return (0);
			return (*string == WEOS ? 0 : FNM_NOMATCH);
		case L'?':
			if (*string == WEOS)
				return (FNM_NOMATCH);
			if (*string == L'/' && (flags & FNM_PATHNAME))
				return (FNM_NOMATCH);
			if (*string == L'.' && (flags & FNM_PERIOD) &&
				(string == stringstart ||
				((flags & FNM_PATHNAME) && *(string - 1) == L'/')))
				return (FNM_NOMATCH);
			++string;
			break;
		case L'*':
			c = *pattern;
			/* Collapse multiple stars. */
			while (c == L'*')
				c = *++pattern;

			if (*string == L'.' && (flags & FNM_PERIOD) &&
				(string == stringstart ||
				((flags & FNM_PATHNAME) && *(string - 1) == L'/')))
				return (FNM_NOMATCH);

			/* Optimize for pattern with * at end or before /. */
			if (c == WEOS)
				if (flags & FNM_PATHNAME)
					return ((flags & FNM_LEADING_DIR) ||
						wcschr(string, L'/') == NULL ?
						0 : FNM_NOMATCH);
				else
					return (0);
			else if (c == L'/' && flags & FNM_PATHNAME) {
				if ((string = wcschr(string, L'/')) == NULL)
					return (FNM_NOMATCH);
				break;
			}

			/* General case, use recursion. */
			while ((test = *string) != WEOS) {
				if (!fnmatch_w(pattern, string, flags & ~FNM_PERIOD))
					return (0);
				if (test == L'/' && flags & FNM_PATHNAME)
					break;
				++string;
			}
			return (FNM_NOMATCH);
		case L'[':
			if (*string == WEOS)
				return (FNM_NOMATCH);
			if (*string == L'/' && flags & FNM_PATHNAME)
				return (FNM_NOMATCH);
			if ((pattern =
				rangematch_w(pattern, *string, flags)) == NULL)
				return (FNM_NOMATCH);
			++string;
			break;
		case L'\\':
			if (!(flags & FNM_NOESCAPE)) {
				if ((c = *pattern++) == WEOS) {
					c = L'\\';
					--pattern;
				}
			}
			/* FALLTHROUGH */
		default:
			if (c == *string)
				;
			else if ((flags & FNM_CASEFOLD) &&
				(towlower(c) ==
					towlower(*string)))
				;
			else if ((flags & FNM_PREFIX_DIRS) && *string == WEOS &&
				((c == L'/' && string != stringstart) ||
				(string == stringstart + 1 && *stringstart == L'/')))
				return (0);
			else
				return (FNM_NOMATCH);
			string++;
			break;
		}
	/* NOTREACHED */
}

static const wchar_t *
rangematch_w(const wchar_t *pattern, wchar_t test, int flags)
{
	int negate, ok;
	wchar_t c, c2;

	/*
	 * A bracket expression starting with an unquoted circumflex
	 * character produces unspecified results (IEEE 1003.2-1992,
	 * 3.13.2).  This implementation treats it like '!', for
	 * consistency with the regular expression syntax.
	 * J.T. Conklin (conklin@ngai.kaleida.com)
	 */
	if ( (negate = (*pattern == L'!' || *pattern == L'^')) )
		++pattern;

	if (flags & FNM_CASEFOLD)
		test = towlower(test);

	for (ok = 0; (c = *pattern++) != L']';) {
		if (c == L'\\' && !(flags & FNM_NOESCAPE))
			c = *pattern++;
		if (c == WEOS)
			return (NULL);

		if (flags & FNM_CASEFOLD)
			c = towlower(c);

		if (*pattern == L'-'
		    && (c2 = *(pattern+1)) != EOS && c2 != L']') {
			pattern += 2;
			if (c2 == L'\\' && !(flags & FNM_NOESCAPE))
				c2 = *pattern++;
			if (c2 == WEOS)
				return (NULL);

			if (flags & FNM_CASEFOLD)
				c2 = towlower(c2);

			if (c <= test &&
				test <= c2)
				ok = 1;
		} else if (c == test)
			ok = 1;
	}
	return (ok == negate ? NULL : pattern);
}
