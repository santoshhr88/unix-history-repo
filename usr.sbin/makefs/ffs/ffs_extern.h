/*	$NetBSD: ffs_extern.h,v 1.6 2003/08/07 11:25:33 agc Exp $	*/
/* From: NetBSD: ffs_extern.h,v 1.19 2001/08/17 02:18:48 lukem Exp */

/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)ffs_extern.h	8.6 (Berkeley) 3/30/95
 * $FreeBSD$
 */

#include "ffs/buf.h"

struct inode;

/*
 * Structure used to pass around logical block paths generated by
 * ufs_getlbns and used by truncate and bmap code.
 */
struct indir {
	daddr_t in_lbn;		/* Logical block number. */
	int	in_off;			/* Offset in buffer. */
};

	/* ffs.c */
_Noreturn void panic(const char *, ...) __printflike(1, 2);

	/* ffs_alloc.c */
int ffs_alloc(struct inode *, daddr_t, daddr_t, int, daddr_t *);
daddr_t ffs_blkpref_ufs1(struct inode *, daddr_t, int, int32_t *);
daddr_t ffs_blkpref_ufs2(struct inode *, daddr_t, int, int64_t *);
void ffs_blkfree(struct inode *, daddr_t, long);
void ffs_clusteracct(struct fs *, struct cg *, int32_t, int);

	/* ffs_balloc.c */
int ffs_balloc(struct inode *, off_t, int, struct buf **);

	/* ffs_bswap.c */
void ffs_sb_swap(struct fs*, struct fs *);
void ffs_dinode1_swap(struct ufs1_dinode *, struct ufs1_dinode *);
void ffs_dinode2_swap(struct ufs2_dinode *, struct ufs2_dinode *);
void ffs_csum_swap(struct csum *, struct csum *, int);
void ffs_cg_swap(struct cg *, struct cg *, struct fs *);

	/* ffs_subr.c */
void ffs_fragacct(struct fs *, int, int32_t[], int, int);
int ffs_isblock(struct fs *, u_char *, int32_t);
int ffs_isfreeblock(struct fs *, u_char *, int32_t);
void ffs_clrblock(struct fs *, u_char *, int32_t);
void ffs_setblock(struct fs *, u_char *, int32_t);

	/* ufs_bmap.c */
int ufs_getlbns(struct inode *, daddr_t, struct indir *, int *);
