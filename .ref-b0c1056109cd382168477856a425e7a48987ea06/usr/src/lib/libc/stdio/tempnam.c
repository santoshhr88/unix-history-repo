/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)tempnam.c	4.3 (Berkeley) %G%";
#endif LIBC_SCCS and not lint

char *tmpnam(s)
char *s;
{
	static seed;

	sprintf(s, "temp.%d.%d", getpid(), seed++);
	return(s);
}
