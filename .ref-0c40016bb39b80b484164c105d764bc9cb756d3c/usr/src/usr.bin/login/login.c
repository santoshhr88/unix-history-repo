/*
 * Copyright (c) 1980, 1987 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980 Regents of the University of California.\n\
 All rights reserved.\n";
#endif not lint

#ifndef lint
static char sccsid[] = "@(#)login.c	5.24 (Berkeley) %G%";
#endif not lint

/*
 * login [ name ]
 * login -r hostname	(for rlogind)
 * login -h hostname	(for telnetd, etc.)
 * login -f name	(for pre-authenticated login: datakit, xterm, etc.)
 */

#include <sys/param.h>
#include <sys/quota.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <sys/termios.h>
#include <sys/ioctl.h>

#include <utmp.h>
#include <signal.h>
#include <lastlog.h>
#include <errno.h>
#include <ttyent.h>
#include <syslog.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <strings.h>

#define	TTYGRPNAME	"tty"		/* name of group to own ttys */

#define	MOTDFILE	"/etc/motd"
#define	MAILDIR		"/usr/spool/mail"
#define	NOLOGIN		"/etc/nologin"
#define	HUSHLOGIN	".hushlogin"
#define	LASTLOG		"/usr/adm/lastlog"
#define	BSHELL		"/bin/sh"

/*
 * This bounds the time given to login.  Not a define so it can
 * be patched on machines where it's too small.
 */
int	timeout = 300;

struct passwd nouser = {"", "NOLOGIN", -1, -1, -1, "", "", "", "" };
struct sgttyb ttyb;
struct passwd *pwd;
char term[64], *hostname, *username;

struct tchars tc = {
	CINTR, CQUIT, CSTART, CSTOP, CEOT, CBRK
};
struct ltchars ltc = {
	CSUSP, CDSUSP, CRPRNT, CFLUSH, CWERASE, CLNEXT
};

main(argc, argv)
	int argc;
	char **argv;
{
	extern int errno, optind;
	extern char *optarg, **environ;
	struct group *gr;
	register int ch;
	register char *p;
	int fflag, hflag, pflag, rflag, cnt;
	int quietlog, passwd_req, ioctlval, timedout();
	char *domain, *envinit[1], *ttyn, *tty;
	char tbuf[MAXPATHLEN + 2];
	char *ttyname(), *stypeof(), *crypt(), *getpass();
	time_t time();
	off_t lseek();

	(void)signal(SIGALRM, timedout);
	(void)alarm((u_int)timeout);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)setpriority(PRIO_PROCESS, 0, 0);
	(void)quota(Q_SETUID, 0, 0, 0);

	/*
	 * -p is used by getty to tell login not to destroy the environment
	 * -r is used by rlogind to cause the autologin protocol;
 	 * -f is used to skip a second login authentication 
	 * -h is used by other servers to pass the name of the
	 * remote host to login so that it may be placed in utmp and wtmp
	 */
	(void)gethostname(tbuf, sizeof(tbuf));
	domain = index(tbuf, '.');

	fflag = hflag = pflag = rflag = 0;
	passwd_req = 1;
	while ((ch = getopt(argc, argv, "f:h:pr:")) != EOF)
		switch(ch) {
		case 'f':
			if (rflag) {
				fprintf(stderr,
				    "login: only one of -r and -f allowed.\n");
				exit(1);
			}
			fflag = 1;
			username = optarg;
			break;
		case 'h':
			if (getuid()) {
				fprintf(stderr,
				    "login: -h for super-user only.\n");
				exit(1);
			}
			if (rflag) {
				fprintf(stderr,
				    "login: only one of -r and -h allowed.\n");
				exit(1);
			}
			hflag = 1;
			if (domain && (p = index(optarg, '.')) &&
			    strcmp(p, domain) == 0)
				*p = 0;
			hostname = optarg;
			break;
		case 'p':
			pflag = 1;
			break;
		case 'r':
			if (hflag || fflag) {
				fprintf(stderr,
				    "login: -f and -h not allowed with -r.\n");
				exit(1);
			}
			rflag = 1;
			passwd_req = doremotelogin(optarg);
			if (domain && (p = index(optarg, '.')) &&
			    !strcmp(p, domain))
				*p = '\0';
			hostname = optarg;
			break;
		case '?':
		default:
			fprintf(stderr, "usage: login [-p] [username]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;
	if (*argv)
		username = *argv;

	ioctlval = 0;
	(void)ioctl(0, TIOCLSET, &ioctlval);
	(void)ioctl(0, TIOCNXCL, 0);
	(void)ioctl(0, FIONBIO, &ioctlval);
	(void)ioctl(0, FIOASYNC, &ioctlval);
	(void)ioctl(0, TIOCGETP, &ttyb);

	/*
	 * If talking to an rlogin process, propagate the terminal type and
	 * baud rate across the network.
	 */
	if (rflag)
		doremoteterm(&ttyb);
	ttyb.sg_erase = CERASE;
	ttyb.sg_kill = CKILL;
	(void)ioctl(0, TIOCSLTC, &ltc);
	(void)ioctl(0, TIOCSETC, &tc);
	(void)ioctl(0, TIOCSETP, &ttyb);

	for (cnt = getdtablesize(); cnt > 2; cnt--)
		close(cnt);

	ttyn = ttyname(0);
	if (ttyn == NULL || *ttyn == '\0')
		ttyn = "/dev/tty??";
	if (tty = rindex(ttyn, '/'))
		++tty;
	else
		tty = ttyn;

	openlog("login", LOG_ODELAY, LOG_AUTH);

	for (cnt = 0;; username = NULL) {
		ioctlval = 0;
		(void)ioctl(0, TIOCSETD, &ioctlval);

		if (!username)
			getloginname();
		if ((pwd = getpwnam(username)) == NULL)
			pwd = &nouser;

		/* if user not super-user, check for disabled logins */
		if (pwd->pw_uid)
			checknologin();

		if (!strcmp(pwd->pw_shell, "/bin/csh")) {
			ioctlval = NTTYDISC;
			(void)ioctl(0, TIOCSETD, &ioctlval);
		}

		/*
		 * disallow automatic login to root, or if not root
		 * and the uid's differ
		 */
		if (fflag) {
			int uid = getuid();

			passwd_req = !pwd->pw_uid || uid && uid != pwd->pw_uid;
		}

		/*
		 * If no remote login authentication and a password exists
		 * for this user, prompt for one and verify it.
		 */
		if (!passwd_req || !*pwd->pw_passwd)
			break;

		setpriority(PRIO_PROCESS, 0, -4);
		p = getpass("Password:");
		p = crypt(p, pwd->pw_passwd);
		setpriority(PRIO_PROCESS, 0, 0);
		if (!strcmp(p, pwd->pw_passwd))
			break;

		printf("Login incorrect\n");
		if (++cnt >= 5) {
			if (hostname)
			    syslog(LOG_ERR,
				"REPEATED LOGIN FAILURES ON %s FROM %.*s, %.*s",
				tty, UT_HOSTSIZE, hostname, UT_NAMESIZE,
				username);
			else
			    syslog(LOG_ERR,
				"REPEATED LOGIN FAILURES ON %s, %.*s",
				tty, UT_NAMESIZE, username);
			(void)ioctl(0, TIOCHPCL, (struct sgttyb *)NULL);
			sleepexit(1);
		}
	}

	/* committed to login -- turn off timeout */
	(void)alarm((u_int)0);

	/*
	 * If valid so far and root is logging in, see if root logins on
	 * this terminal are permitted.
	 */
	if (!pwd->pw_uid && !rootterm(tty)) {
		if (hostname)
			syslog(LOG_CRIT, "ROOT LOGIN REFUSED ON %s FROM %.*s",
			    tty, UT_HOSTSIZE, hostname);
		else
			syslog(LOG_CRIT, "ROOT LOGIN REFUSED ON %s", tty);
		printf("Login incorrect\n");
		sleepexit(1);
	}

	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = BSHELL;

	if (chdir(pwd->pw_dir) < 0) {
		printf("No directory %s!\n", pwd->pw_dir);
		if (chdir("/"))
			exit(0);
		pwd->pw_dir = "/";
		printf("Logging in with home = \"/\".\n");
	}

	if (quota(Q_SETUID, pwd->pw_uid, 0, 0) < 0 && errno != EINVAL) {
		switch(errno) {
		case EUSERS:
			fprintf(stderr,
		"Too many users logged on already.\nTry again later.\n");
			break;
		case EPROCLIM:
			fprintf(stderr,
			    "You have too many processes running.\n");
			break;
		default:
			perror("quota (Q_SETUID)");
		}
		sleepexit(0);
	}

	{
		struct utmp utmp;

		(void)time(&utmp.ut_time);
		strncpy(utmp.ut_name, username, sizeof(utmp.ut_name));
		strncpy(utmp.ut_host, hostname, sizeof(utmp.ut_host));
		strncpy(utmp.ut_line, tty, sizeof(utmp.ut_line));
		login(&utmp);
	}

	quietlog = access(HUSHLOGIN, F_OK) == 0;
	dolastlog(quietlog, tty);

	if (!hflag && !rflag) {					/* XXX */
		static struct winsize win = { 0, 0, 0, 0 };

		(void)ioctl(0, TIOCSWINSZ, &win);
	}

	(void)chown(ttyn, pwd->pw_uid,
	    (gr = getgrnam(TTYGRPNAME)) ? gr->gr_gid : pwd->pw_gid);
	(void)chmod(ttyn, 0620);
	(void)setgid(pwd->pw_gid);

	initgroups(username, pwd->pw_gid);

	quota(Q_DOWARN, pwd->pw_uid, (dev_t)-1, 0);
	(void)setuid(pwd->pw_uid);

	/* destroy environment unless user has requested preservation */
	if (!pflag)
		environ = envinit;
	setenv("HOME", pwd->pw_dir, 1);
	setenv("SHELL", pwd->pw_shell, 1);
	if (term[0] == '\0')
		strncpy(term, stypeof(tty), sizeof(term));
	setenv("TERM", term, 0);
	setenv("USER", pwd->pw_name, 1);
	setenv("PATH", ":/usr/ucb:/bin:/usr/bin", 0);

	if (tty[sizeof("tty")-1] == 'd')
		syslog(LOG_INFO, "DIALUP %s, %s", tty, pwd->pw_name);
	if (pwd->pw_uid == 0)
		if (hostname)
			syslog(LOG_NOTICE, "ROOT LOGIN %s FROM %.*s",
			    tty, UT_HOSTSIZE, hostname);
		else
			syslog(LOG_NOTICE, "ROOT LOGIN %s", tty);

	if (!quietlog) {
		struct stat st;

		motd();
		(void)sprintf(tbuf, "%s/%s", MAILDIR, pwd->pw_name);
		if (stat(tbuf, &st) == 0 && st.st_size != 0)
			printf("You have %smail.\n",
			    (st.st_mtime > st.st_atime) ? "new " : "");
	}

	(void)signal(SIGALRM, SIG_DFL);
	(void)signal(SIGQUIT, SIG_DFL);
	(void)signal(SIGINT, SIG_DFL);
	(void)signal(SIGTSTP, SIG_IGN);

	tbuf[0] = '-';
	strcpy(tbuf + 1, (p = rindex(pwd->pw_shell, '/')) ?
	    p + 1 : pwd->pw_shell);
	execlp(pwd->pw_shell, tbuf, 0);
	perror(pwd->pw_shell);
	fprintf(stderr, "login: no shell\n");
	exit(0);
}

getloginname()
{
	register int ch;
	register char *p;
	static char nbuf[UT_NAMESIZE + 1];

	for (;;) {
		p = nbuf;
		printf("login: ");
		while ((ch = getchar()) != '\n') {
			if (ch == ' ')
				ch = '_';
			if (ch == EOF)
				exit(0);
			if (p < nbuf+UT_NAMESIZE)
				*p++ = ch;
		}
		if (p > nbuf)
			if (nbuf[0] == '-')
				fprintf(stderr,
				    "login names may not start with '-'.\n");
			else {
				*p = '\0';
				username = nbuf;
				return;
			}
	}
	/* NOTREACHED */
}

timedout()
{
	fprintf(stderr, "Login timed out after %d seconds\n", timeout);
	exit(0);
}

rootterm(tty)
	char *tty;
{
	struct ttyent *t;

	return((t = getttynam(tty)) && t->ty_status&TTY_SECURE);
}

int motdinterrupt;
motd()
{
	register int ch;
	FILE *fp;
	int (*oldint)(), sigint();

	if ((fp = fopen(MOTDFILE, "r")) == NULL)
		return;
	oldint = signal(SIGINT, sigint);
	while ((ch = getc(fp)) != EOF && !motdinterrupt)
		(void)putchar(ch);
	(void)signal(SIGINT, oldint);
	if (motdinterrupt)
		fpurge(fp);
	(void)fclose(fp);
}

sigint()
{
	motdinterrupt = 1;
}

checknologin()
{
	register int fd, nchars;
	char tbuf[1024];

	if ((fd = open(NOLOGIN, O_RDONLY, 0)) >= 0) {
		while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0)
			(void)write(fileno(stdout), tbuf, nchars);
		sleepexit(0);
	}
}

dolastlog(quiet, tty)
	int quiet;
	char *tty;
{
	struct lastlog ll;
	int fd;

	if ((fd = open(LASTLOG, O_RDWR, 0)) >= 0) {
		(void)lseek(fd, (long)pwd->pw_uid * sizeof(ll), L_SET);
		if (!quiet) {
			if (read(fd, (char *)&ll, sizeof(ll)) == sizeof(ll) &&
			    ll.ll_time != 0) {
				printf("Last login: %.*s ",
				    24-5, (char *)ctime(&ll.ll_time));
				if (*ll.ll_host != '\0')
					printf("from %.*s\n",
					    sizeof(ll.ll_host), ll.ll_host);
				else
					printf("on %.*s\n",
					    sizeof(ll.ll_line), ll.ll_line);
			}
			(void)lseek(fd, (long)pwd->pw_uid * sizeof(ll), L_SET);
		}
		(void)time(&ll.ll_time);
		strncpy(ll.ll_line, tty, sizeof(ll.ll_line));
		strncpy(ll.ll_host, hostname, sizeof(ll.ll_host));
		(void)write(fd, (char *)&ll, sizeof(ll));
		(void)close(fd);
	}
}

#undef	UNKNOWN
#define	UNKNOWN	"su"

char *
stypeof(ttyid)
	char *ttyid;
{
	struct ttyent *t;

	return(ttyid && (t = getttynam(ttyid)) ? t->ty_type : UNKNOWN);
}

doremotelogin(host)
	char *host;
{
	static char lusername[UT_NAMESIZE+1];
	char rusername[UT_NAMESIZE+1];

	getstr(rusername, sizeof(rusername), "remuser");
	getstr(lusername, sizeof(lusername), "locuser");
	getstr(term, sizeof(term), "Terminal type");
	username = lusername;
	if (getuid()) {
		pwd = &nouser;
		return(-1);
	}
	pwd = getpwnam(username);
	if (pwd == NULL) {
		pwd = &nouser;
		return(-1);
	}
	return(ruserok(host, (pwd->pw_uid == 0), rusername, username));
}

getstr(buf, cnt, err)
	char *buf, *err;
	int cnt;
{
	char ch;

	do {
		if (read(0, &ch, sizeof(ch)) != sizeof(ch))
			exit(1);
		if (--cnt < 0) {
			fprintf(stderr, "%s too long\r\n", err);
			sleepexit(1);
		}
		*buf++ = ch;
	} while (ch);
}

char *speeds[] = {
	"0", "50", "75", "110", "134", "150", "200", "300", "600",
	"1200", "1800", "2400", "4800", "9600", "19200", "38400",
};
#define	NSPEEDS	(sizeof(speeds) / sizeof(speeds[0]))

doremoteterm(tp)
	struct sgttyb *tp;
{
	register char *cp = index(term, '/'), **cpp;
	char *speed;

	if (cp) {
		*cp++ = '\0';
		speed = cp;
		cp = index(speed, '/');
		if (cp)
			*cp++ = '\0';
		for (cpp = speeds; cpp < &speeds[NSPEEDS]; cpp++)
			if (strcmp(*cpp, speed) == 0) {
				tp->sg_ispeed = tp->sg_ospeed = cpp-speeds;
				break;
			}
	}
	tp->sg_flags = ECHO|CRMOD|ANYP|XTABS;
}

sleepexit(eval)
	int eval;
{
	sleep((u_int)5);
	exit(eval);
}
