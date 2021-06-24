/*
 *	SPLITLIB/CCC - split a /REL library into manageable parts
 *	Version 1.0 - 09/08/86
 *	Copyright 1986 Riclin Computer Products. All rights reserved.
 *	
 *	Now put into public domain (http://www.tim-mann.org/misosys.html#down):
 *	   "Misosys is Roy Soltoff's old TRS-80 software company. 
 *	   Roy has been out of the business for some years, and he has given me
 *	   permission to put his TRS-80 software and documentation up for free
 *	   dissemination on the Web.
 *	   Many thanks to Roy for his generosity."
 *
 *	Converted for compilation using Hi-Tech C in CP/M operating system.
 *	Two functions have been added to the program (addext() and genspec()),
 *      similar in their actions to the functions from the INLIB library.
 *	Andrey Nikitin  June 2021
 */

#include <stdio.h>

#ifdef CPM

#include <stdlib.h>     /* For HiTech C  */
#include <ctype.h>
#include <string.h>
#define bittst(var,bitno) ((var) & 1 << (bitno))
int   getbit();
int   getnum();
int   bitest();
char *addext();
char *strrchr();
char *genspec();
extern char * sys_err[];

#else			/* Comments must be removed before compiling with MISOSYS C  */
/*
#option MAXFILES	2
#option FIXBUFS		ON
#option INLIB		ON
*/
#endif

#define FSPECLEN	14
#define ME		"SPLITLIB"
#define DATE		"09/08/86"
#define RELEOF		0x9E

char inspec[FSPECLEN+1] = "", outspec[FSPECLEN+1], symbol[9], *drive = "";
int byte, bitno = 7, currlength;
FILE *infp, *outfp = NULL;

main(argc, argv)
    int argc;
    char *argv[];
{
    static int maxlength;
    static char * p;

    printf("%s - Split /REL Library - Version 1.0\nCopyright 1986 Riclin Computer Products. All rights reserved.\n\n", ME);

    if (argc < 3)
	usage();
    for (p = argv[2]; isdigit(*p++); )
    	;

    maxlength = atoi(argv[2]);

#ifdef CPM	
    if (maxlength < 100)
#else
    if (--p - argv[2] > 5 || *p || maxlength < 100)
#endif
	usage();

    if (argc == 4)
	drive = argv[3];

#ifdef CPM	
    if (drive[1] != ':')					/* HiTech C  */
#else
    if (drive[0] != ':' || drive[1] < '0' || drive[1] > '7')	/* MISOSYS C */
#endif
	usage ;

#ifdef CPM	
    if (! (infp = fopen(addext(strncpy(inspec, argv[1], FSPECLEN), "REL") , "rb")))
#else
    if (! (infp = fopen(addext(strncpy(inspec, argv[1], FSPECLEN), "REL") , "r")))
#endif
	ioerror(inspec);
    printf("Reading input file %s\n", inspec);

    openout();    			/* open first output file */
    getbyte();    			/* prime the pump 	*/

    for ( ; ; )   			/* Loop forever		*/
    {
	if (! getbit())			/* Absolute item	*/
	    skipbits(8);
	else				/* Relocatable item 	*/
	    switch (getnum(2))
	    {
		case 1:			/* Program relative 	*/
		case 2:			/* Data relative	*/
		case 3:			/* Common relative 	*/
		    skipbits(16);
		    break;
		case 0:		/* Special link item */
		    switch (getnum(8))	/* Compute ctl field	*/
		    {
			case 0:		/* entry symbol		*/
			case 1:		/* select common block	*/
			case 3:		/* request lib search	*/
			case 4:		/* reserved		*/
			    b_field();
			    break;
			case 2:		/* Program name		*/
			    b_field();
			    printf("Module %s\n", symbol);
			    break;
			case 5:		/* define common size 	*/
			case 7:		/* define entry point 	*/
			case 8:		/* reserved 		*/
			case 6:		/* chain external 	*/
			    a_and_b();
			    break;
			case  9:	/* external + offset 	 */
			case 10:	/* define data area size */
			case 11:	/* set counter 		 */
			case 12:	/* chain address 	 */
			case 13:	/* define program size 	 */
			    a_field();
			    break;
			case 14:	/* End program */
			    a_field();
			    if (currlength >= maxlength)
				openout();	/* close current, open next */
			    if (bitno != 7)	/* force to byte boundary   */
			    {
				bitno = 7;
				getbyte();
			    }
			    break;
			case 15:		/* End file		*/
			    fclose(outfp);	/* 0x9E already written */
			    exit(0);
		    } /* end inner switch */
		} /* end outer switch */
    } /* end for */
}

a_and_b() /* decode A- and B-fields */
{
    a_field();
    b_field();
}

a_field() /* decode A-field */
{
    skipbits(18);
}

b_field() /* decode B-field */
{
    static int len, n;

    for (len = getnum(4), n = 0; n < len; ++n) /* get symbol */
	symbol[n] = getnum(128) & 0x7F;
    symbol[n] ='\0';
}

skipbits(n) /* skip n bits */
    int n;
{
    while (n--)
	getbit();
}

getnum(power) /* get a number <= 2*power_i */
    int power;
{
    static int n;

    for (n = 0; power > 0; power >>= 1)
	n += getbit() * power;
    return n;
}

#if 1
getbit() /* retrieve next bit from file v1.0 */
{
    static int bit;

    bit = bitest(bitno--, byte);
    if (bitno < 0)
    {
	bitno = 7;	/* bitno runs modulo 8 */
	getbyte();      /* next byte */
    }
    return bit;
} 

#else

getbit() /* retrieve next bit from file v1.0a */
{
    static int bit;

    if(bitno < 0) {
	bitno = 7;
	getbyte();
    }
    bit = bitest(bitno--, byte);
    return bit;
}

#endif

int bitest(bitno, value) /* test bit version 2 */
    int bitno, value;
{
#ifdef CPM

    if(bittst(value, bitno) == 0) return 0;
    else			  return 1;

#else

#asm
$BIT0E	  EQU	43H	      ; BIT 0,E instruction
	  $GA	BC,DE         ; bit # in C, value in E	
	  LD	A,C
	  ADD	A,A           ; shift bit # left 3x
	  ADD	A,A
	  ADD	A,A
	  OR	$BIT0E        ; mask bit # into instr
	  LD	($BITINST),A  ; modify bit test code
	  BIT	0,E           ; test bit in E
$BITINST: EQU	$-1
	  LD	HL,0          ; assume not set
	  RET	Z             ; rtn FALSE
	  INC  HL
#endasm

#endif
}

getbyte() /* read next byte from input file, write out */
{
    if ((byte = getc(infp)) == EOF)
	if (ferror(infp))
	    ioerror(inspec);
    putbyte(byte);
    ++currlength;
}

putbyte(c)
    int c;
{
    if (putc(c, outfp) != c)
	ioerror(outspec);
}

usage()
{
    abend("usage: %s infile[.REL] maxlength [:d]\n", ME, NULL);
}

ioerror(f)
    char *f;
{
    extern int errno;

#ifdef CPM	
    abend("%s: %s\n", f, sys_err[errno]);	/* HiTech C  */
#else
    abend("%s: %s\n", f, syserrlist(errno));	/* MISOSYS C */
#endif
}

abend(format, a1, a2)
    char *format, *a1, *a2;
{
    fprintf(stderr, "%s: " , ME);
    fprintf(stderr, format, a1, a2);
    exit(1);
}

openout()
{
    static char outext[] = "Rnn";
    static int currout   = 0;

    if (outfp)
    {
	putbyte(RELEOF);
	fclose(outfp);
    }
    outext[1] = ++currout / 10 + '0';
    outext[2] = currout % 10 + '0';

    strcpy(outspec, drive);

    if (! (outfp = fopen(genspec(inspec, outspec, outext), "w")))
	ioerror(outspec);
    printf("\nWriting output file %s\n", outspec);
    currlength = 0;
}

#ifdef CPM
/*
 *	Generating a file specification (CP/M version)
 */
char *genspec(inspec, partspec, extn) char *inspec, *partspec, *extn; {
    register char* tmp;

    tmp = inspec;
    if (drive[0] != 0) {
	if(strrchr(inspec,':') != NULL) tmp = strrchr(inspec,':')+1;
    }
    tmp[strlen(tmp)-3] = '\0';
    strncat(partspec, tmp, (strrchr(tmp,'.') - inspec + 1));
    partspec = strcat(partspec, extn);
    return partspec;
}

/*
 *	Add default ext to filespec (CP/M version)
 */
char *addext(filespec, defextn) char *filespec, *defextn; {

    if (strrchr(filespec,'.') == NULL) { /* If the extension is no specified */
	strcat(filespec, ".");
	strcat(filespec, defextn);
    }
    return filespec;
}
#endif

