/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Computer Consoles Inc.
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
 */

#if defined(SYSLIBC_SCCS) && !defined(lint)
	.asciz "@(#)cmpd.s	1.4 (Berkeley) 6/1/90"
#endif /* SYSLIBC_SCCS and not lint */

#include "DEFS.h"

XENTRY(cmpd, R12|R11|R10|R9)
	movl	4(fp),r12
	movl	8(fp),r10
	movl	12(fp),r11
	movl	16(fp),r9
	tstl	r12
	jgeq	L16
	xorl2	$-2147483648,r12
	tstl	r10
	jeql	L17
	mnegl	r10,r10
	mcoml	r12,r12
	jbr	L16
L17:	mnegl	r12,r12
L16:	tstl	r11
	jgeq	L19
	xorl2	$-2147483648,r11
	tstl	r9
	jeql	L20
	mnegl	r9,r9
	mcoml	r11,r11
	jbr	L19
L20:	mnegl	r11,r11
L19:	cmpl	r12,r11
	jeql	L22
	cmpl	r12,r11
	jleq	L9999
	movl	$1,r0
	jbr	L9998
L9999:	mnegl	$1,r0
L9998:	ret
L22:	cmpl	r10,r9
	jeql	L23
	cmpl	r10,r9
	jlequ	L9997
	movl	$1,r0
	jbr	L9996
L9997:	mnegl	$1,r0
L9996:	ret
L23:	clrl	r0
	ret
