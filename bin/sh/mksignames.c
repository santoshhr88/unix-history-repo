/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	$Id: mksignames.c,v 1.2 1994/09/24 02:57:56 davidg Exp $
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mksignames.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

/*
 * This program generates the signames.h and signames.c files.
 */
#include <stdio.h>
#include <signal.h>



struct sig {
	int signo;		/* signal number */
	char *name;		/* signal name (without leading "SIG") */
	char *mesg;		/* description */
};


struct sig sigtab[] = {
	SIGHUP, "HUP", "Hangup",
	SIGINT, "INT", "Interrupt",	/* normally don't print message */
	SIGQUIT, "QUIT", "Quit",
	SIGILL, "ILL", "Illegal instruction",
	SIGTRAP, "TRAP", "Trace/BPT trap",
#ifdef SIGABRT
	SIGABRT, "ABRT", "abort",
#endif
#if defined(SIGIOT) && (! defined(SIGABRT) || SIGABRT != SIGIOT)
	SIGIOT, "IOT", "abort",
#endif
#ifdef SIGEMT
	SIGEMT, "EMT", "EMT trap",
#endif
	SIGFPE, "FPE", "Floating exception",
	SIGKILL, "KILL", "Killed",
	SIGBUS, "BUS", "Bus error",
	SIGSEGV, "SEGV", "Memory fault",
	SIGSYS, "SYS", "Bad system call",
	SIGPIPE, "PIPE", "Broken pipe",	/* normally don't print message */
	SIGALRM, "ALRM", "Alarm call",
	SIGTERM, "TERM", "Terminated",
#ifdef SIGUSR1
	SIGUSR1, "USR1", "User signal 1",
#endif
#ifdef SIGUSR2
	SIGUSR2, "USR2", "User signal 2",
#endif
#ifdef SIGCLD
	SIGCLD, "CLD", NULL,
#endif
#if defined(SIGCHLD) && ! defined(SIGCLD)
	SIGCHLD, "CLD", NULL,
#endif
#ifdef SIGPWR
	SIGPWR, "PWR", "Power fail",
#endif
#ifdef SIGPOLL
	SIGPOLL, "POLL", "Poll",
#endif
	/* Now for the BSD signals */
#ifdef SIGURG
	SIGURG, "URG", NULL,
#endif
#ifdef SIGSTOP
	SIGSTOP, "STOP", "Stopped",
#endif
#ifdef SIGTSTP
	SIGTSTP, "TSTP", "Stopped",
#endif
#ifdef SIGCONT
	SIGCONT, "CONT", NULL,
#endif
#ifdef SIGTTIN
	SIGTTIN, "TTIN", "Stopped (input)",
#endif
#ifdef SIGTTOU
	SIGTTOU, "TTOU", "Stopped (output)",
#endif
#ifdef SIGIO
	SIGIO, "IO", NULL,
#endif
#ifdef SIGXCPU
	SIGXCPU, "XCPU", "Time limit exceeded",
#endif
#ifdef SIGXFSZ
	SIGXFSZ, "XFSZ", NULL,
#endif
#ifdef SIGVTALARM
	SIGVTALARM, "VTALARM", "Virtual alarm",
#endif
#ifdef SIGPROF
	SIGPROF, "PROF", "Profiling alarm",
#endif
#ifdef SIGWINCH
	SIGWINCH, "WINCH", NULL,
#endif
	0, NULL, NULL
};


#define MAXSIG 64


char *sigmesg[MAXSIG + 1];


char writer[] = "\
/*\n\
 * This file was generated by the mksignames program.\n\
 */\n\
\n";



main(argc, argv)  char **argv; {
	FILE *cfile, *hfile;
	struct sig *sigp;
	int maxsig;
	int i;

	if ((cfile = fopen("signames.c", "w")) == NULL) {
		fputs("Can't create signames.c\n", stderr);
		exit(2);
	}
	if ((hfile = fopen("signames.h", "w")) == NULL) {
		fputs("Can't create signames.h\n", stderr);
		exit(2);
	}
	maxsig = 0;
	for (sigp = sigtab ; sigp->signo != 0 ; sigp++) {
		if (sigp->signo < 0 || sigp->signo > MAXSIG)
			continue;
		sigmesg[sigp->signo] = sigp->mesg;
		if (maxsig < sigp->signo)
			maxsig = sigp->signo;
	}

	fputs(writer, hfile);
	fprintf(hfile, "#define MAXSIG %d\n\n", maxsig);
	fprintf(hfile, "extern char *const sigmesg[MAXSIG+1];\n");

	fputs(writer, cfile);
	fprintf(cfile, "#include \"shell.h\"\n\n");
	fprintf(cfile, "char *const sigmesg[%d] = {\n", maxsig + 1);
	for (i = 0 ; i <= maxsig ; i++) {
		if (sigmesg[i] == NULL) {
			fprintf(cfile, "      0,\n");
		} else {
			fprintf(cfile, "      \"%s\",\n", sigmesg[i]);
		}
	}
	fprintf(cfile, "};\n");
	exit(0);
}
