#ifndef lint
static char sccsid[] = "@(#)table.c	1.5 (Berkeley) %G%";
#endif

# include "pass2.h"
/* special shapes (SSOREG,SZERO etc.) shouldn't be or-ed */
# define AWD SNAME|SOREG|SCON|STARNM|STARREG
/* tbl */
# define ANYSIGNED TPOINT|TINT|TSHORT|TCHAR
# define ANYUSIGNED TUNSIGNED|TUSHORT|TUCHAR
# define ANYFIXED ANYSIGNED|ANYUSIGNED
# define TWORD TINT|TUNSIGNED|TPOINT
/* tbl */

struct optab  table[] = {

PCONV,	INAREG|INTAREG,
	SAREG|AWD,	TCHAR|TSHORT,
	SANY,	TPOINT,
		NAREG|NASL,	RESC1,
		"	cvtZLl	AL,A1\n",

PCONV,	INAREG|INTAREG,
	SAREG|AWD,	TUCHAR|TUSHORT,
	SANY,	TPOINT,
		NAREG|NASL,	RESC1,
		"	movzZLl	AL,A1\n",

	/* the following entry is to fix a problem with
	   the manner that the first pass handles the
	   type of a shift expression                 */
PCONV,	INAREG|INTAREG,
	SAREG|AWD,	TINT|TUNSIGNED,
	SANY,	TPOINT,
		NAREG|NASL,	RLEFT,
		"",

SCONV,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TDOUBLE,
	SANY,	TFLOAT,
		NAREG|NASL,	RESC1|RESCC,
		"	ldd	AL\n	cvdf\n	stf	TA1\n",

SCONV,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TFLOAT,
	SANY,	TDOUBLE,
		NAREG|NASL,	RESC1|RESCC,
		"	ldfd	AL\n	std	A1\n",

SCONV,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TINT|TUNSIGNED,
	SANY,	TFLOAT|TDOUBLE,
		NAREG|NASL,	RESC1|RESCC,
		"	cvlZR	AL\n	stZR	TA1\n",

SCONV,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TFLOAT|TDOUBLE,
	SANY,	ANYFIXED,
		NAREG|NASL,	RESC1|RESCC,
		"	ldZL	AL\n	cvZLl	A1\n",

SCONV,	INAREG|INTAREG,
	SAREG|AWD,	ANYFIXED,
	SANY,	ANYFIXED,
		NAREG|NASL,	RESC1|RESCC,
		"ZU",

SCONV,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	ANYFIXED,
	SANY,	ANYFIXED,
		NAREG|NASL,	RESC1|RESCC,
		"ZV",

INIT,	FOREFF,
	SCON,	TANY,
	SANY,	TWORD,
		0,	RNOP,
		"	.long	CL\n",

INIT,	FOREFF,
	SCON,	TANY,
	SANY,	TSHORT|TUSHORT,
		0,	RNOP,
		"	.word	CL\n",

INIT,	FOREFF,
	SCON,	TANY,
	SANY,	TCHAR|TUCHAR,
		0,	RNOP,
		"	.byte	CL\n",

	/* for the use of fortran only */

GOTO,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	jbr	CL\n",

GOTO,	FOREFF,
	AWD,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	jmp	*AL\n",

GOTO,	FOREFF,
	SAREG,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	jmp	(AL)\n",

STARG,	FORARG,
	SCON|SOREG,	TANY,
	SANY,	TANY,
		NTEMP+2*NAREG,	RESC3,
		"ZS",

STASG,	FORARG,
	SNAME|SOREG,	TANY,
	SCON|SAREG,	TANY,
		0,	RNULL,
		"	ZT\nZS",

STASG,	FOREFF,
	SNAME|SOREG,	TANY,
	SCON|SAREG,	TANY,
		0,	RNOP,
		"ZS",

STASG,	INAREG,
	SNAME|SOREG,	TANY,
	SCON,	TANY,
		NAREG,	RESC1,
		"ZS	movl	AR,A1\n",

STASG,	INAREG,
	SNAME|SOREG,	TANY,
	SAREG,	TANY,
		0,	RRIGHT,
		"	pushl	AR\nZS	movl	(sp)+,AR\n",

OPLOG,	FORCC,
	SAREG|AWD,	TWORD,
	SAREG|AWD,	TWORD,
		0,	RESCC,
		"	cmpl	AL,AR\nZP",

OPLOG,	FORCC,
	AWD,		TUSHORT,
	SCON,	TANY,
		NAREG,	RESCC,
		"	movzwl	AL,A1\n	cmpw	A1,AR\nZP",

OPLOG,	FORCC,
	SAREG|AWD,	TSHORT|TUSHORT,
	SCON,	TANY,
		0,	RESCC,
		"	cmpw	AL,ZZ\nZP",

OPLOG,	FORCC,
	SAREG|AWD,	TSHORT|TUSHORT,
	SAREG|AWD,	TSHORT|TUSHORT,
		0,	RESCC,
		"	cmpw	AL,AR\nZP",

OPLOG,	FORCC,
	AWD,		TUCHAR,
	SCON,	 	TANY,
		NAREG,	RESCC,
		"	movzbl	AL,A1\n	cmpb	A1,AR\nZP",

OPLOG,	FORCC,
	SAREG|AWD,	TCHAR|TUCHAR,
	SCON,	TANY,
		0,	RESCC,
		"	cmpb	AL,ZZ\nZP",

OPLOG,	FORCC,
	SAREG|AWD,	TCHAR|TUCHAR,
	SAREG|AWD,	TCHAR|TUCHAR,
		0,	RESCC,
		"	cmpb	AL,AR\nZP",

OPLOG,	FORCC,
	SAREG|AWD,	TFLOAT,
	SAREG|AWD,	TFLOAT,
		0,	RESCC,
		"	cmpZL2	AL,AR\nZP",

OPLOG,	FORCC,
	SZERO,		TDOUBLE,
	SAREG|AWD,	TDOUBLE,
		0,	RESCC,
		"	cmpf2	AL,AR\nZP",

OPLOG,	FORCC,
	SAREG|AWD,	TDOUBLE,
	SAREG|AWD,	TDOUBLE,
		0,	RESCC,
		"	cmpZL2	AL,AR\nZP",

CCODES,	INAREG|INTAREG,
	SANY,	TANY,
	SANY,	TANY,
		NAREG,	RESC1,
		"	movl	$1,A1\nZN",

UNARY CALL,	INAREG|INTAREG,
	SCON,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1, /* should be register 0 */
		"	ZC,CL\n",

UNARY CALL,	INAREG|INTAREG,
	SAREG,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	ZC,(AL)\n",

UNARY CALL,	INAREG|INTAREG,
	SNAME,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1,	/* really reg 0 */
		"	ZC,*AL\n",

UNARY CALL,	INAREG|INTAREG,
	SSOREG,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1,	/* really reg 0 */
		"	ZC,*AL\n",

FORTCALL,	INAREG|INTAREG,
	SCON,	TANY,
	SAREG|AWD,	TFLOAT,
		NAREG|NASL|NASR,	RESC1,
		"	ldf	AR\n	CLf\n	stf	TA1\n",

ASG OPSHFT,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TWORD,
	SAREG|SCON,	ANYFIXED,
		0,	RLEFT|RESCC,
		"	ZH	AR,AL,AL\n",

ASG OPSHFT,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TWORD,
	AWD,	TCHAR|TUCHAR,
		0,	RLEFT|RESCC,
		"	ZH	AR,AL,AL\n",

ASG OPSHFT,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TWORD,
	AWD,	ANYFIXED,
		NAREG,	RLEFT|RESCC,
		"	ZB	AR,A1\n	ZH	A1,AL,AL\n",

OPSHFT,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TWORD,
	SAREG|SCON,	ANYFIXED,
		NAREG|NASL|NASR,	RESC1|RESCC,
		"	ZH	AR,AL,A1\n",

OPSHFT,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TWORD,
	AWD,	TCHAR|TUCHAR,
		NAREG|NASL|NASR,	RESC1|RESCC,
		"	ZH	AR,AL,A1\n",

OPSHFT,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TWORD,
	AWD,	ANYFIXED,
		NAREG|NASR,	RESC1|RESCC,
		"	ZB	AR,A1\n	ZH	A1,AL,A1\n",

INCR,	FOREFF,
	AWD,	TWORD,
	SCON,	TANY,
		0,	RLEFT,
		"	ZE\n",

DECR,	FOREFF,
	AWD,	TWORD,
	SCON,	TANY,
		0,	RLEFT,
		"	ZE\n",

INCR,	FOREFF,
	SAREG,	TWORD,
	SCON,	TANY,
		0,	RLEFT,
		"	ZE\n",

DECR,	FOREFF,
	SAREG,	TWORD,
	SCON,	TANY,
		0,	RLEFT,
		"	ZE\n",

/* jwf INCR and DECR for SAREG TCHAR|TSHORT matched by ASG PLUS etc */

INCR,	INAREG|INTAREG,
	AWD,	TANY,
	SCON,	TANY,
		NAREG,	RESC1,
		"	ZD\n",

DECR,	INAREG|INTAREG,
	AWD,	TANY,
	SCON,	TANY,
		NAREG,	RESC1,
		"	ZD\n",

INCR,	INAREG|INTAREG,
	SAREG,	TWORD,
	SCON,	TANY,
		NAREG,	RESC1,
		"	ZD\n",

DECR,	INAREG|INTAREG,
	SAREG,	TWORD,
	SCON,	TANY,
		NAREG,	RESC1,
		"	ZD\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SIREG,	TDOUBLE,
	SZERO,	TANY,
		0,	RLEFT|RESCC,
		"	cvld	$0\n	std	AL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|SNAME|SOREG,	TDOUBLE,
	SZERO,	TANY,
		0,	RLEFT|RESCC,
		"	clrl	UL\n	clrl	AL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TDOUBLE,
	SIREG,	TDOUBLE,
		0,	RLEFT|RRIGHT|RESCC,
		"	ldd	AR\n	std	AL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SIREG,	TDOUBLE,
	SAREG|AWD,	TDOUBLE,
		0,	RLEFT|RRIGHT|RESCC,
		"	ldd	AR\n	std	AL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|SNAME|SOREG,	TDOUBLE,
	SAREG|SNAME|SOREG,	TDOUBLE,
		0,	RLEFT|RRIGHT|RESCC,
		"	movl	UR,UL\n	movl	AR,AL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TFLOAT,
	SZERO,	TANY,
		0,	RLEFT|RESCC,
		"	clrl	TAL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TFLOAT,
	SAREG|AWD,	TFLOAT,
		0,	RLEFT|RRIGHT|RESCC,
		"	movl	AR,TAL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	ANYFIXED,
	SAREG|AWD,	ANYFIXED,
		0,	RLEFT|RRIGHT|RESCC,
		"	ZA\n",

ASSIGN,	INAREG|FOREFF,
	SFLD,	TANY,
	SZERO,	TANY,
		0,	RRIGHT,
		"	andl2	N,AL\n",

ASSIGN,	INAREG|FOREFF,
	SFLD,	TANY,
	SCON,	TANY,
		0,	RRIGHT,
		"	andl2	N,AL\n	orl2	ZF,AL\n",

ASSIGN,	INAREG|FOREFF,
	SFLD,	TANY,
	SAREG|AWD,	TWORD,
		NAREG|NASL,	RRIGHT,
		"	shll	$H,AR,A1\n	andl2	M,A1\n	andl2	N,AL\n\
	orl2	A1,AL\n",

/* dummy UNARY MUL entry to get U* to possibly match OPLTYPE */
UNARY MUL,	FOREFF,
	SCC,	TANY,
	SCC,	TANY,
		0,	RNULL,
		"	help help help\n",

REG,	INTEMP,
	SANY,	TANY,
	SAREG,	TDOUBLE,
		2*NTEMP,	RESC1,
		"	movl	UR,U1\n	movl	AR,A1\n",

REG,	INTEMP,
	SANY,	TANY,
	SAREG,	TANY,
		NTEMP,	RESC1,
		"	movl	AR,A1\n",

REG,	FORARG,
	SANY,	TANY,
	SAREG,	TFLOAT,
		0,	RNULL,
		"	pushl	$0\n	pushl	AR\n",

REG,	FORARG,
	SANY,	TANY,
	SAREG,	TDOUBLE,
		0,	RNULL,
		"	pushl	UR\n	pushl	AR\n",

OPLEAF,	FOREFF,
	SANY,	TANY,
	SAREG|AWD,	TANY,
		0,	RLEFT,
		"",

OPLTYPE,	INAREG|INTAREG,
	SANY,	TANY,
	SZERO,	TDOUBLE,
		NAREG|NASR,	RESC1,
		"	clrl	U1\n	clrl	A1\n",

OPLTYPE,	INAREG|INTAREG,
	SANY,	TANY,
	SIREG,	TDOUBLE,
		NAREG|NASR,	RESC1,
		"	ldd	AR\n	std	A1\n",

OPLTYPE,	INAREG|INTAREG,
	SANY,	TANY,
	SAREG|SNAME|SOREG|SCON,	TDOUBLE,
		NAREG,		RESC1,
		"	movl	AR,A1\n	movl	UR,U1\n",

OPLTYPE,	INAREG|INTAREG,
	SANY,	TANY,
	SZERO,	TANY,
		NAREG|NASR,	RESC1,
		"	clrl	TA1\n",

OPLTYPE,	INAREG|INTAREG,
	SANY,	TANY,
	SAREG|AWD,	TFLOAT,
		NAREG|NASR,	RESC1,
		"	movl	AR,TA1\n",

OPLTYPE,	INAREG|INTAREG,
	SANY,	TANY,
	SANY,	ANYFIXED,
		NAREG|NASR,	RESC1,
		"	ZA\n",

OPLTYPE,	FORCC,
	SANY,	TANY,
	SIREG,	TDOUBLE,
		0,	RESCC,
		"	ldd	AR\n	tstd\n",

OPLTYPE,	FORCC,
	SANY,	TANY,
	SAREG|AWD,	TFLOAT,
		0,	RESCC,
		"	ldf	AR\n	tstf\n",

OPLTYPE,	FORCC,
	SANY,	TANY,
	SAREG|AWD,	TDOUBLE,
		0,	RESCC,
		"	ldd	AR\n	tstd\n",

OPLTYPE,	FORCC,
	SANY,	TANY,
	SANY,	ANYFIXED,
		0,	RESCC,
		"	tstZR	AR\n",

OPLTYPE,	FORARG,
	SANY,	TANY,
	SANY,	TWORD,
		0,	RNULL,
		"	pushl	AR\n",

OPLTYPE,	FORARG,
	SANY,	TANY,
	SANY,	TCHAR|TSHORT,
		0,	RNULL,
		"	pushZR	AR\n",

OPLTYPE,	FORARG,
	SANY,	TANY,
	SANY,	TUCHAR|TUSHORT,
		0,	RNULL,
		"	movzZRl	AR,-(sp)\n",

OPLTYPE,	FORARG,
	SANY,	TANY,
	SZERO,	TFLOAT|TDOUBLE,
		0,	RNULL,
		"	pushl	$0\n	pushl	$0\n",

OPLTYPE,	FORARG,
	SANY,	TANY,
	SIREG,	TDOUBLE,
		0,	RNULL,
		"	ldd	AR\n	pushd\n",

OPLTYPE,	FORARG,
	SANY,	TANY,
	SAREG|AWD,	TDOUBLE,
		0,	RNULL,
		"	pushl	UR\n	pushl	AR\n",

OPLTYPE,	FORARG,
	SANY,	TANY,
	SAREG|AWD,	TFLOAT,
		0,	RNULL,
		"	pushl	$0\n	pushl	AR\n",

UNARY MINUS,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	ANYSIGNED|TUNSIGNED,
	SANY,	TANY,
		NAREG|NASL,	RESC1|RESCC,
		"	mnegZL	AL,A1\n",

UNARY MINUS,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TFLOAT|TDOUBLE,
	SANY,	TANY,
		NAREG|NASL,	RESC1|RESCC,
		"	lnZL	AL\n	stZL	TA1\n",

COMPL,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	ANYSIGNED|TUNSIGNED,
	SANY,	TANY,
		NAREG|NASL,	RESC1|RESCC,
		"	mcomZL	AL,A1\n",

COMPL,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	ANYFIXED,
	SANY,	TANY,
		NAREG|NASL,	RESC1|RESCC,
		"	cvtZLl	AL,A1\n	mcoml	A1,A1\n",

AND,	FORCC,
	SAREG|AWD,	TWORD,
	SAREG|AWD,	TWORD,
		0,	RESCC,
		"	bitl	AR,AL\n",

AND,	FORCC,
	SAREG|AWD,	TSHORT|TUSHORT,
	SSCON,	TWORD,
		0,	RESCC,
		"	bitw	AR,AL\n",

AND,	FORCC,
	SAREG|AWD,	TSHORT|TUSHORT,
	SAREG|AWD,	TSHORT|TUSHORT,
		0,	RESCC,
		"	bitw	AR,AL\n",

AND,	FORCC,
	SAREG|AWD,	TCHAR|TUCHAR,
	SCCON,	TWORD,
		0,	RESCC,
		"	bitb	AR,AL\n",

AND,	FORCC,
	SAREG|AWD,	TCHAR|TUCHAR,
	SAREG|AWD,	TCHAR|TUCHAR,
		0,	RESCC,
		"	bitb	AR,AL\n",

ASG DIV,	INAREG|FOREFF|FORCC, /* should have been done in mip */
	SAREG|AWD,	ANYFIXED,
	SONE,	TWORD,
		0,	RLEFT|RESCC,
		"	tstZL	AL\n",

ASG DIV,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TUCHAR|TUSHORT,
	SAREG|AWD,	TUCHAR|TUSHORT,
		2*NAREG,	RLEFT|RESC1|RESCC,
		"ZX	OL2	A2,A1\n	cvtlZL	A1,AL\n",

ASG DIV,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TWORD,
	SAREG|AWD,	TUNSIGNED,
		3*NAREG|NEVEN,	RLEFT|RESCC,
		"ZM	OE	AR,A1,A2,A3\n	movl	A2,AL\n",

DIV,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TUCHAR|TUSHORT,
	SAREG|AWD,	TUCHAR|TUSHORT,
		2*NAREG,	RESC1|RESCC,
		"ZX	OL2	A2,A1\n",	

DIV,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TWORD,
	SAREG|AWD,	TUNSIGNED,
		3*NAREG|NEVEN,	RESC2|RESCC,
		"ZM	OE	AR,A1,A2,A3\n",

ASG MOD,	INAREG|FOREFF,
	SAREG|AWD,	ANYFIXED,
	SONE,	TWORD,
		0,	RLEFT,
		"	clrZL	AL\n",

MOD,	INAREG|INTAREG,
	STAREG,	TWORD,
	SONE,	TWORD,
		0,	RLEFT,
		"	clrl	AL\n",

MOD,	INAREG|INTAREG,
	SAREG|AWD,	ANYFIXED,
	SONE,	TWORD,
		0,	RESC1,
		"	clrl	A1\n",

ASG MOD,	INAREG|FOREFF,
	SAREG|AWD,	TWORD,
	SAREG|AWD,	TWORD,
		3*NAREG|NEVEN,	RLEFT,
		"ZM	OE	AR,A1,A2,A3\n	movl	A3,AL\n",

MOD,	INAREG|INTAREG,
	SAREG|AWD,	TWORD,
	SAREG|AWD,	TWORD,
		3*NAREG|NEVEN,	RESC3,
		"ZM	OE	AR,A1,A2,A3\n",

ASG MUL,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TCHAR|TUCHAR,
	SAREG|AWD,	TCHAR|TUCHAR,
		2*NAREG,	RLEFT|RESCC,
		"ZX	OL2	A2,A1\n	cvtlb	A1,AL\n",

ASG MUL,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TUNSIGNED,
	SAREG|AWD,	TUNSIGNED|TINT,
		NAREG|NEVEN,	RLEFT|RESCC,
		"	emul	AR,AL,$0,A1\n	movl	U1,AL\n",

ASG MUL,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TUNSIGNED|TINT,
	SAREG|AWD,	TUNSIGNED,
		NAREG|NEVEN,	RLEFT|RESCC,
		"	emul	AR,AL,$0,A1\n	movl	U1,AL\n",

ASG OPMUL,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TWORD,
	SAREG|AWD,	TWORD,
		0,	RLEFT|RESCC,
		"	OL2	AR,AL\n",

MUL,	INAREG|INTAREG|FORCC,
	STAREG,		TUNSIGNED,
	SAREG|AWD,	TUNSIGNED|TINT, 
		NAREG|NEVEN,	RLEFT|RESCC,
		"	emul	AR,AL,$0,A1\n	movl	U1,AL\n",

MUL,	INAREG|INTAREG|FORCC,
	STAREG,		TUNSIGNED|TINT,
	SAREG|AWD,	TUNSIGNED,
		NAREG|NEVEN,	RLEFT|RESCC,
		"	emul	AR,AL,$0,A1\n	movl	U1,AL\n",

OPMUL,	INAREG|INTAREG|FORCC,
	STAREG,		TWORD,
	SAREG|AWD,	TWORD,
		0,	RLEFT|RESCC,
		"	OL2	AR,AL\n",

MUL,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TCHAR|TUCHAR,
	SAREG|AWD,	TCHAR|TUCHAR,
		2*NAREG,	RESC1|RESCC,
		"ZX	mull2	A2,A1\n",

MUL,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TUNSIGNED,
	SAREG|AWD,	TUNSIGNED|TINT, 
		NAREG|NEVEN,	RESC1|RESCC,
		"	emul	AR,AL,$0,A1\n	movl	U1,A1\n",

MUL,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TUNSIGNED|TINT,
	SAREG|AWD,	TUNSIGNED,
		NAREG|NEVEN,	RESC1|RESCC,
		"	emul	AR,AL,$0,A1\n	movl	U1,A1\n",

OPMUL,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TWORD,
	SAREG|AWD,	TWORD,
		NAREG|NASL|NASR,	RESC1|RESCC,
		"	OL3	AR,AL,A1\n",

ASG PLUS,	INAREG|FOREFF|FORCC,
	SAREG,	TWORD,
	SONE,	TINT,
		0,	RLEFT|RESCC,
		"	incZL	AL\n",

ASG PLUS,	INAREG|FOREFF|FORCC,
	AWD,	ANYFIXED,
	SONE,	TINT,
		0,	RLEFT|RESCC,
		"	incZL	AL\n",

ASG MINUS,	INAREG|FOREFF|FORCC,
	SAREG,	TWORD,
	SONE,	TINT,
		0,	RLEFT|RESCC,
		"	decZL	AL\n",

ASG MINUS,	INAREG|FOREFF|FORCC,
	AWD,	ANYFIXED,
	SONE,	TINT,
		0,	RLEFT|RESCC,
		"	decZL	AL\n",

PLUS,	INAREG|INTAREG|FORCC,
	STAREG,	TWORD,
	SONE,	TWORD,
		0,	RLEFT|RESCC,
		"	incZL	AL\n",

MINUS,	INAREG|INTAREG|FORCC,
	STAREG,	TWORD,
	SONE,	TWORD,
		0,	RLEFT|RESCC,
		"	decZL	AL\n",

ASG OPSIMP,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TWORD,
	SAREG|AWD,	TWORD,
		0,	RLEFT|RESCC,
		"	OL2	AR,AL\n",

ASG OPSIMP,	INAREG|FOREFF|FORCC,
	SAREG,	TWORD,
	SAREG,	TSHORT|TUSHORT|TCHAR|TUCHAR,
		0,	RLEFT|RESCC,
		"	OL2	AR,AL\n",

ASG OPSIMP,	INAREG|FOREFF|FORCC,
	AWD,	TSHORT|TUSHORT,
	SAREG|AWD,	TSHORT|TUSHORT,
		0,	RLEFT|RESCC,
		"	OW2	AR,AL\n",

ASG OPSIMP,	INAREG|FOREFF|FORCC,
	AWD,	TSHORT|TUSHORT,
	SSCON,	TWORD,
		0,	RLEFT|RESCC,
		"	OW2	AR,AL\n",

ASG OPSIMP,	INAREG|FOREFF|FORCC,
	AWD,	TCHAR|TUCHAR,
	SAREG|AWD,	TCHAR|TUCHAR,
		0,	RLEFT|RESCC,
		"	OB2	AR,AL\n",

ASG OPSIMP,	INAREG|FOREFF|FORCC,
	AWD,	TCHAR|TUCHAR,
	SCCON,	TWORD,
		0,	RLEFT|RESCC,
		"	OB2	AR,AL\n",

OPSIMP,	INAREG|INTAREG|FORCC,
	STAREG,	ANYFIXED,
	SAREG|AWD,	TWORD,
		0,	RLEFT|RESCC,
		"	OL2	AR,AL\n",

OPSIMP,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TWORD,
	SAREG|AWD,	TWORD,
		NAREG|NASL|NASR,	RESC1|RESCC,
		"	OL3	AR,AL,A1\n",

ASG OPSIMP,	INAREG|FOREFF,
	SAREG|AWD,	TFLOAT|TDOUBLE,
	SZERO,	TANY,
		0,	RLEFT,
		"",

ASG MUL,	INAREG|FOREFF|FORCC,
	SIREG,	TDOUBLE,
	SZERO,	TANY,
		0,	RLEFT|RESCC,
		"	cvld	$0\n	std	AL\n",

ASG MUL,	INAREG|FOREFF|FORCC,
	SAREG|SNAME|SOREG,	TDOUBLE,
	SZERO,	TANY,
		0,	RLEFT|RESCC,
		"	clrl	UL\n	clrl	AL\n",

ASG MUL,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TFLOAT,
	SZERO,	TANY,
		0,	RLEFT|RESCC,
		"	clrl	TAL\n",

ASG OPFLOAT,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TFLOAT,
	SAREG|AWD,	TFLOAT,
		0,	RLEFT|RESCC,
		"	ldf	AL\n	OF	AR\n	stf	TAL\n",

ASG OPFLOAT,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TDOUBLE,
	SAREG|AWD,	TDOUBLE,
		0,	RLEFT|RESCC,
		"	ldd	AL\n	OD	AR\n	std	AL\n",

OPSIMP,	INAREG|INTAREG,
	SAREG,	TFLOAT|TDOUBLE,
	SZERO,	TANY,
		0,	RLEFT,
#if defined(FORT) || defined(SPRECC)
		"T",
#else
		"",
#endif

OPSIMP,	INAREG|INTAREG|FORCC,
	AWD,	TFLOAT,
	SZERO,	TANY,
		NAREG,	RESC1|RESCC,
		"	movl	AL,A1\n",

OPSIMP,	INAREG|INTAREG|FORCC,
	AWD,	TDOUBLE,
	SZERO,	TANY,
		NAREG,	RESC1|RESCC,
		"	movl	UL,U1\n	movl	AL,A1\n",

MUL,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TDOUBLE,
	SZERO,	TANY,
		NAREG|NASR,	RESC1|RESCC,
		"	clrl	U1\n	clrl	A1\n",

MUL,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TFLOAT,
	SZERO,	TANY,
		NAREG|NASR,	RESC1|RESCC,
		"	clrl	TA1\n",

OPFLOAT,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TFLOAT,
	SAREG|AWD,	TFLOAT,
		NAREG|NASL|NASR,	RESC1|RESCC,
		"	ldf	AL\n	OF	AR\n	stf	TA1\n",

OPFLOAT,	INAREG|INTAREG|FORCC,
	SZERO,		TANY,
	SAREG|AWD,	TDOUBLE,
		NAREG,	RESC1|RESCC,
		"\tclrl\tA1\n\tclrl\tU1\n\tldd\tA1\n\tOD\tAR\n\tstd\tA1\n",

OPFLOAT,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TDOUBLE,
	SAREG|AWD,	TDOUBLE,
		NAREG|NASL|NASR,	RESC1|RESCC,
		"	ldd	AL\n	OD	AR\n	std	A1\n",

	/* Default actions for hard trees ... */

# define DF(x) FORREW,SANY,TANY,SANY,TANY,REWRITE,x,""

UNARY MUL, DF( UNARY MUL ),

INCR, DF(INCR),

DECR, DF(INCR),

ASSIGN, DF(ASSIGN),

STASG, DF(STASG),

FLD, DF(FLD),

OPLEAF, DF(NAME),

OPLOG,	FORCC,
	SANY,	TANY,
	SANY,	TANY,
		REWRITE,	BITYPE,
		"",

OPLOG,	DF(NOT),

COMOP, DF(COMOP),

INIT, DF(INIT),

OPUNARY, DF(UNARY MINUS),


ASG OPANY, DF(ASG PLUS),

OPANY, DF(BITYPE),

FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	"help; I'm in trouble\n" };
