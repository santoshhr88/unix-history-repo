/*
 * Copyright (c) 1983 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)main.c	5.13 (Berkeley) %G%";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>

#include <ufs/ffs/fs.h>
#include <ufs/ufs/dinode.h>
#include <protocols/dumprestore.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pathnames.h"
#include "restore.h"
#include "extern.h"

int	bflag = 0, cvtflag = 0, dflag = 0, vflag = 0, yflag = 0;
int	hflag = 1, mflag = 1, Nflag = 0;
char	command = '\0';
long	dumpnum = 1;
long	volno = 0;
long	ntrec;
char	*dumpmap;
char	*clrimap;
ino_t	maxino;
time_t	dumptime;
time_t	dumpdate;
FILE 	*terminal;

static void obsolete __P((int *, char **[]));
static void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch;
	ino_t ino;
	char *inputdev = _PATH_DEFTAPE;
	char *symtbl = "./restoresymtable";
	char *p, name[MAXPATHLEN];

	if (argc < 2)
		usage();

	obsolete(&argc, &argv);
	while ((ch = getopt(argc, argv, "b:cdf:himNRrs:tvxy")) != EOF)
		switch(ch) {
		case 'b':
			/* Change default tape blocksize. */
			bflag = 1;
			ntrec = strtol(optarg, &p, 10);
			if (*p)
				err("illegal blocksize -- %s", optarg);
			if (ntrec <= 0)
				err("block size must be greater than 0");
			break;
		case 'c':
			cvtflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'f':
			inputdev = optarg;
			break;
		case 'h':
			hflag = 0;
			break;
		case 'i':
		case 'R':
		case 'r':
		case 't':
		case 'x':
			if (command != '\0')
				err("%c and %c options are mutually exclusive",
				    ch, command);
			command = ch;
			break;
		case 'm':
			mflag = 0;
			break;
		case 'N':
			Nflag = 1;
			break;
		case 's':
			/* Dumpnum (skip to) for multifile dump tapes. */
			dumpnum = strtol(optarg, &p, 10);
			if (*p)
				err("illegal dump number -- %s", optarg);
			if (dumpnum <= 0)
				err("dump number must be greater than 0");
			break;
		case 'v':
			vflag = 1;
			break;
		case 'y':
			yflag = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (command == '\0')
		err("none of i, R, r, t or x options specified");

	if (signal(SIGINT, onintr) == SIG_IGN)
		(void) signal(SIGINT, SIG_IGN);
	if (signal(SIGTERM, onintr) == SIG_IGN)
		(void) signal(SIGTERM, SIG_IGN);
	setlinebuf(stderr);

	setinput(inputdev);

	if (argc == 0) {
		argc = 1;
		*--argv = ".";
	}

	switch (command) {
	/*
	 * Interactive mode.
	 */
	case 'i':
		setup();
		extractdirs(1);
		initsymtable(NULL);
		runcmdshell();
		break;
	/*
	 * Incremental restoration of a file system.
	 */
	case 'r':
		setup();
		if (dumptime > 0) {
			/*
			 * This is an incremental dump tape.
			 */
			vprintf(stdout, "Begin incremental restore\n");
			initsymtable(symtbl);
			extractdirs(1);
			removeoldleaves();
			vprintf(stdout, "Calculate node updates.\n");
			treescan(".", ROOTINO, nodeupdates);
			findunreflinks();
			removeoldnodes();
		} else {
			/*
			 * This is a level zero dump tape.
			 */
			vprintf(stdout, "Begin level 0 restore\n");
			initsymtable((char *)0);
			extractdirs(1);
			vprintf(stdout, "Calculate extraction list.\n");
			treescan(".", ROOTINO, nodeupdates);
		}
		createleaves(symtbl);
		createlinks();
		setdirmodes(FORCE);
		checkrestore();
		if (dflag) {
			vprintf(stdout, "Verify the directory structure\n");
			treescan(".", ROOTINO, verifyfile);
		}
		dumpsymtable(symtbl, (long)1);
		break;
	/*
	 * Resume an incremental file system restoration.
	 */
	case 'R':
		initsymtable(symtbl);
		skipmaps();
		skipdirs();
		createleaves(symtbl);
		createlinks();
		setdirmodes(FORCE);
		checkrestore();
		dumpsymtable(symtbl, (long)1);
		break;
	/*
	 * List contents of tape.
	 */
	case 't':
		setup();
		extractdirs(0);
		initsymtable((char *)0);
		while (argc--) {
			canon(*argv++, name);
			ino = dirlookup(name);
			if (ino == 0)
				continue;
			treescan(name, ino, listfile);
		}
		break;
	/*
	 * Batch extraction of tape contents.
	 */
	case 'x':
		setup();
		extractdirs(1);
		initsymtable((char *)0);
		while (argc--) {
			canon(*argv++, name);
			ino = dirlookup(name);
			if (ino == 0)
				continue;
			if (mflag)
				pathcheck(name);
			treescan(name, ino, addfile);
		}
		createfiles();
		createlinks();
		setdirmodes(0);
		if (dflag)
			checkrestore();
		break;
	}
	done(0);
	/* NOTREACHED */
}

static void
usage()
{
	(void)fprintf(stderr, "usage:\t%s%s%s%s%s",
	    "restore tfhsvy [file ...]\n",
	    "\trestore xfhmsvy [file ...]\n",
	    "\trestore ifhmsvy\n",
	    "\trestore rfsvy\n",
	    "\trestore Rfsvy\n");
	done(1);
}

/*
 * obsolete --
 *	Change set of key letters and ordered arguments into something
 *	getopt(3) will like.
 */
static void
obsolete(argcp, argvp)
	int *argcp;
	char **argvp[];
{
	int argc, flags;
	char *ap, **argv, *flagsp, **nargv, *p;

	/* Setup. */
	argv = *argvp;
	argc = *argcp;

	/* Return if no arguments or first argument has leading dash. */
	ap = argv[1];
	if (argc == 1 || *ap == '-')
		return;

	/* Allocate space for new arguments. */
	if ((*argvp = nargv = malloc((argc + 1) * sizeof(char *))) == NULL ||
	    (p = flagsp = malloc(strlen(ap) + 2)) == NULL)
		err("%s", strerror(errno));

	*nargv++ = *argv;
	argv += 2;

	for (flags = 0; *ap; ++ap) {
		switch(*ap) {
		case 'b':
		case 'f':
		case 's':
			if ((nargv[0] = malloc(strlen(*argv) + 2 + 1)) == NULL)
				err("%s", strerror(errno));
			nargv[0][0] = '-';
			nargv[0][1] = *ap;
			(void)strcpy(&nargv[0][2], *argv);
			if (*argv != NULL)
				++argv;
			++nargv;
			break;
		default:
			if (!flags) {
				*p++ = '-';
				flags = 1;
			}
			*p++ = *ap;
			break;
		}
	}

	/* Terminate flags. */
	if (flags) {
		*p = '\0';
		*nargv++ = flagsp;
	}

	/* Copy remaining arguments. */
	while (*nargv++ = *argv++);
}

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

__dead void
#if __STDC__
err(const char *fmt, ...)
#else
err(fmt, va_alist)
	char *fmt;
        va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)fprintf(stderr, "restore: ");
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	exit(1);
	/* NOTREACHED */
}
