/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: installFinal.c,v 1.26 1996/04/13 13:31:42 jkh Exp $
 *
 * Copyright (c) 1995
 *	Jordan Hubbard & Coranth Gryphon.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE FREEBSD PROJECT ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR THEIR PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"
#include <sys/disklabel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mount.h>

/* This file contains all the final configuration thingies */

static DMenu MenuSamba = {
    DMENU_CHECKLIST_TYPE,
    "Samba Services Menu",
    "This allows you to configure various aspects of your Samba server.",
    NULL,
    NULL,
{ { "Homes",		"Make home directories available to users.",
    dmenuVarCheck, dmenuSetVariable, NULL, "SAMBA_homes=YES" },
  { "Printers",		"Allows for sharing of local printers.",
    dmenuVarCheck, dmenuSetVariable, NULL, "SAMBA_printers=YES" },
  { "Export Paths",	"Specify local directories to make available.",
    dmenuVarCheck, dmenuSetVariable, NULL, "SAMBA_export=YES" },
  { NULL } },
};

/* These probably shouldn't be hard-coded, but making them options might prove to be even more confusing! */
#define SMB_CONF "./smb.conf"


/* Load gated package */
int
configGated(dialogMenuItem *self)
{
    if (package_add("gated-3.5a11") == DITEM_SUCCESS)
	variable_set2("gated", "YES");
    return DITEM_SUCCESS;
}

/* Load pcnfsd package */
int
configPCNFSD(dialogMenuItem *self)
{
    if (package_add("pcnfsd-93.02.16") == DITEM_SUCCESS)
	variable_set2("pcnfsd", "YES");
    return DITEM_SUCCESS;
}

int
configSamba(dialogMenuItem *self)
{
    int i = DITEM_SUCCESS;

    if (!dmenuOpenSimple(&MenuSamba))
	i = DITEM_FAILURE;
    else if (package_add("samba-1.9.15p8") != DITEM_SUCCESS)
	i = DITEM_FAILURE;
    else {
	FILE *fptr;
	char tbuf[256], *tptr;
	int tval;

	fptr = fopen("/tmp/smb.conf","w");
	if (fptr) {
	    strcpy(tbuf,"FreeBSD - Samba %v");
	    if (variable_get("SAMBA_string")) {
		tptr = msgGetInput("FreeBSD - Samba %%v", "What should this server list as its description?\n"
				   "Note that the \"%%v\" refers to the samba version number.");
		if (tptr && *tptr)
		    strcpy(tbuf, tptr);
	    }
		
	    fprintf(fptr, "[global]\n");
	    fprintf(fptr, "comment = %s\n", tbuf);
	    fprintf(fptr, "log file = /var/log/samba.log\n");
	    fprintf(fptr, "dont descend = /dev,/proc,/root,/stand\n\n");
	    
	    fprintf(fptr, "printing = bsd\n");
	    fprintf(fptr, "map archive = no\n");
	    fprintf(fptr, "status = yes\n");
	    fprintf(fptr, "public = yes\n");
	    fprintf(fptr, "read only = no\n");
	    fprintf(fptr, "preserve case = yes\n");
	    fprintf(fptr, "strip dot = yes\n");
	    fprintf(fptr, "security = share\n");
	    fprintf(fptr, "guest ok = yes\n\n");
	    
	    if (variable_get("SAMBA_homes")) {
		fprintf(fptr, "[homes]\n");
		fprintf(fptr, "browseable = no\n");
		fprintf(fptr, "comment = User Home Directory\n");
		fprintf(fptr, "create mode = 0775\n");
		fprintf(fptr, "public = no\n\n");
	    }
	    
	    if (variable_get("SAMBA_printers")) {
		fprintf(fptr, "[printers]\n");
		fprintf(fptr, "path = /var/spool\n");
		fprintf(fptr, "comment = Printers\n");
		fprintf(fptr, "create mode = 0700\n");
		fprintf(fptr, "browseable = no\n");
		fprintf(fptr, "printable = yes\n");
		fprintf(fptr, "read only = yes\n");
		fprintf(fptr, "public = no\n\n");
	    }

	    if (variable_get("SAMBA_export")) {
		for (tval = 0; ! tval; tval = msgYesNo("Another?")) {
		    tptr = msgGetInput(NULL,"What directory to export?");
		    if (tptr && *tptr && (tptr[0] == '/')) {
			int len = strlen(tbuf);
			
			strcpy(tbuf, tptr);
			if (tbuf[len - 1] == '/')
			    tbuf[len - 1] = '\0';
			if (directory_exists(tbuf)) {
			    tptr = msgGetInput(pathBaseName(tbuf), "What do you want to call this share?");
			    if (tptr && *tptr) {
				fprintf(fptr, "[%s]\npath = %s\n", tptr, tbuf);
				tptr = msgGetInput(NULL, "Enter a short description of this share?");
				if (tptr && *tptr)
				    fprintf(fptr, "comment = %s\n", tptr);
				if (msgYesNo("Do you want this share to be read only?"))
				    fprintf(fptr, "read only = no\n\n");
				else
				    fprintf(fptr, "read only = yes\n\n");
			    }
			    else
				msgConfirm("Invalid Share Name.");
			}
			else
			    msgConfirm("Directory does not exist.");
		    }	/* end if (tptr)	 */
		}	/* end for loop		 */
	    }	/* end if (SAMBA_export) */
	    fclose(fptr);
	    vsystem("mv -f /tmp/smb.conf %s", SMB_CONF);
	}
	else
	    msgConfirm("Unable to open temporary smb.conf file.\n"
		       "Samba will have to be configured by hand.");
    }
    return i;
}

int
configNFSServer(dialogMenuItem *self)
{
    char cmd[256];

    /* If we're an NFS server, we need an exports file */
    if (!file_readable("/etc/exports")) {
	WINDOW *w = savescr();

	msgConfirm("Operating as an NFS server means that you must first configure\n"
		   "an /etc/exports file to indicate which hosts are allowed certain\n"
		   "kinds of access to your local file systems.\n"
		   "Press [ENTER] now to invoke an editor on /etc/exports\n");
	vsystem("echo '#The following examples export /usr to 3 machines named after ducks,' > /etc/exports");
	vsystem("echo '#/home and all directories under it to machines named after dead rock stars' >> /etc/exports");
	vsystem("echo '#and, finally, /a to 2 privileged machines allowed to write on it as root.' >> /etc/exports");
	vsystem("echo '#/usr                huey louie dewie' >> /etc/exports");
	vsystem("echo '#/home   -alldirs    janice jimmy frank' >> /etc/exports");
	vsystem("echo '#/a      -maproot=0  bill albert' >> /etc/exports");
	vsystem("echo '#' >> /etc/exports");
	vsystem("echo '# You should replace these lines with your actual exported filesystems.' >> /etc/exports");
	vsystem("echo >> /etc/exports");
	sprintf(cmd, "%s /etc/exports", variable_get(VAR_EDITOR));
	dialog_clear();
	systemExecute(cmd);
	restorescr(w);
    }
    variable_set2("nfs_server", "YES");
    return DITEM_SUCCESS;
}
