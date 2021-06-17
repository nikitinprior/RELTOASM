/*
  Program REL to ASM disassemble a .ERL or .REL file.

  The author of the algorithm and source code of the program "DISA-REL.PAS"
  in Pascal MT+:

	Professor Ronald E. Bruck
	Department of Mathematics
	University of Southern California
	Los Angeles, CA	 90089

  The program is analogous to the UNREL disassembler Copyright 1986
  Riclin Computer Products.

  The initial conversion from Pascal to C is done using p2c 2.00.Oct.15,
  the Pascal-to-C translator from input file "DISA-REL.PAS".

  Part of the code is used from the utility TB Version 2.2
  Copyright (c) 1988, 1993  MicroGenSf (Russia).

  Adapted by Andrey Nikitin in june 2021:

    - Changed the name of the programs to RELtoASM.
    - The program steps are output to the console, similar
      to the UNREL disassembler Copyright 1986 Riclin Computer Products.
    - Opcodes from the OPCODES.TXT file have been moved to the program code.
    - The algorithm for selecting program options has been changed.
      Without specifying options, the program tries to open the TT.REL file.
      A different name can be specified in the first option. If you do not
      specify a file extension by default, it will be assigned REL.
      The second option specifies the name of the output file with the
      default MAC extension. If the second option is not specified, the
      output will go to the console.
    - The pseudo-operation .z80 has been added to the program output.

  The original program and its version 1.0 in the C language only understands
  8080 opcodes, but displays them in z80 mnemonics.

  The size of the compiled program is 19 KB and practically corresponds to the
  compiled original Pascal version. 
*/

//#define DDEBUG	/* For debugging, remove the comment on this line */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
/*
 *	Used macros 
 */
#define bittst(var,bitno) ((var) & 1	<< (bitno))
#define bitset(var,bitno) ((var) |= 1	<< (bitno))
#define bitclr(var,bitno) ((var) &= ~(1 << (bitno)))

#define MAXPC		16384	// Maximum code size
#define MAXREL		4096	// Maximum REL info size is 1/4 this

#define EXIT_SUCCESS	0
#define EXIT_FAILURE	1

#define FALSE		0
#define TRUE		1
/*
 *	Defining types 
 */
typedef unsigned char  byte;	/* This type is exactly 1 byte  */
typedef short	       word;  	/* This type is exactly 2 bytes */
typedef unsigned short uint;

typedef byte tipe_type;

#define ABSLUTE		0
#define CODE_REL	1
#define DATA_REL	2
#define COMMON_REL	3

#define argMark		'%'

typedef struct op_code_type {
    char follow;		/* # bytes which follow opcode	*/
    char *name;			/* mnemonic for opcode		*/
} op_code_type;

typedef struct a_field {
    tipe_type	tipe;
    word	value;
} a_field;

typedef char name_type[9];

typedef struct ms_item {
    byte	rel;
    word	value;
    tipe_type	tipe;
    word	control;
    a_field	a;
    name_type	b;
} ms_item;

typedef struct ref_type {
    byte	    tipe;
    /* Bit assignments:
	  Bit 0 : 0 = code relative, 1 = data relative;
	  Bit 1 : 0 = public name,   1 = private name.
       Bit 1 is irrelevant if the item is placed in the chain of external references.
    */
    word	     value;
    name_type	     name;
    struct ref_type *ptr;
} ref_type;

typedef struct offset_type {
    char		sign;
    word		loc,
			offset;
    struct offset_type *next;
} offset_type;

/*
 *	Defining Variables 
 */

/*
static word	  sysmem;		   Not used
*/

uint cur_bit;				/* Current bit number  */

static char	  pgm_name[128],/* Disassembled program module name 		*/
		  iname[128],	/* Name of the input object file in rel format	*/
		  oname[128];	/* Output file name in assembly language	*/

static ms_item	  item;

static char	  ch;

static ref_type * next_label,
		* first_code_ref,
		* last_code_ref,
		* first_data_ref,
		* last_data_ref,
		* first_ext_ref,
		* last_ext_ref;

static offset_type * first_offset,
		   * last_offset,
		   * next_offset;

static word	     n,
		     pc;		/* program counter   */

static word	     final_pc;		/* last byte of code */
/*
static word	     old_mark;		   mark top of heap Not used
*/
static word	     pgm_size,		/* Program size */
		     data_size;		/* Data size	*/
/*
static word	     result		   Not used
static FILE	   * fbyte;		   File of char does interpretation Not used
*/
static FILE	   * fin;		/* Input  file descriptor */
static FILE	   * fout;		/* Output file descriptor */

static byte	     code_buffer[MAXPC + 1];
static byte	     rel_info[MAXREL + 1];
/*
 *	Z80 processor opcodes:
 */
static op_code_type op_codes[] = {
    { 0, "NOP"	     }, { 2, "LD BC,%"	 }, { 0, "LD (BC),A"  }, { 0, "INC BC"	   },
    { 0, "INC B"     }, { 0, "DEC B"	 }, { 1, "LD B,%"     }, { 0, "RLCA"	   },
    { 0, "EX AF,AF'" }, { 0, "ADD HL,BC" }, { 0, "LD A,(BC)"  }, { 0, "DEC BC"	   },
    { 0, "INC C"     }, { 0, "DEC C"	 }, { 1, "LD C,%"     }, { 0, "RRC"	   },
    { 1, "DJNZ,%"    }, { 2, "LD DE,%"	 }, { 0, "LD (DE),A"  }, { 0, "INC DE"	   },
    { 0, "INC D"     }, { 0, "DEC D"	 }, { 1, "LD D,%"     }, { 0, "RLA"	   },
    { 1, "JR,%"	     }, { 0, "ADD HL,DE" }, { 0, "LD A,(DE)"  }, { 0, "DEC DE"	   },
    { 0, "INC E"     }, { 0, "DEC E"	 }, { 1, "LD E,%"     }, { 0, "RRA"	   },
    { 1, "JR NZ,%"   }, { 2, "LD HL,%"	 }, { 2, "LD (%),HL " }, { 0, "INC HL"	   },
    { 0, "INC H"     }, { 0, "DEC H"	 }, { 1, "LD H,%"     }, { 0, "DAA"	   },
    { 1, "JR Z,%"    }, { 0, "ADD HL,HL" }, { 2, "LD HL,(%)"  }, { 0, "DEC HL"	   },
    { 0, "INC L"     }, { 0, "DEC L"	 }, { 1, "LD L,%"     }, { 0, "CPL"	   },
    { 1, "JR NC,%"   }, { 2, "LD SP,%"	 }, { 2, "LD (%),A "  }, { 0, "INC SP"	   },
    { 0, "INC (HL)"  }, { 0, "DEC (HL)"	 }, { 1, "LD (HL),%"  }, { 0, "SCF"	   },
    { 1, "JR C,%"    }, { 0, "ADD HL,SP" }, { 2, "LD A,(%)"   }, { 0, "DEC SP"	   },
    { 0, "INC A"     }, { 0, "DEC A"	 }, { 1, "LD A,%"     }, { 0, "CCF"	   },
    { 0, "LD B,B"    }, { 0, "LD B,C"	 }, { 0, "LD B,D"     }, { 0, "LD B,E"	   },
    { 0, "LD B,H"    }, { 0, "LD B,L"	 }, { 0, "LD B,(HL)"  }, { 0, "LD B,A"	   },
    { 0, "LD C,B"    }, { 0, "LD C,C"	 }, { 0, "LD C,D"     }, { 0, "LD C,E"	   },
    { 0, "LD C,H"    }, { 0, "LD C,L"	 }, { 0, "LD C,(HL)"  }, { 0, "LD C,A"	   },
    { 0, "LD D,B"    }, { 0, "LD D,C"	 }, { 0, "LD D,D"     }, { 0, "LD D,E"	   },
    { 0, "LD D,H"    }, { 0, "LD D,L"	 }, { 0, "LD D,(HL)"  }, { 0, "LD D,A"	   },
    { 0, "LD E,B"    }, { 0, "LD E,C"	 }, { 0, "LD E,D"     }, { 0, "LD E,E"	   },
    { 0, "LD E,H"    }, { 0, "LD E,L"	 }, { 0, "LD E,(HL)"  }, { 0, "LD E,A"	   },
    { 0, "LD H,B"    }, { 0, "LD H,C"	 }, { 0, "LD H,D"     }, { 0, "LD H,E"	   },
    { 0, "LD H,H"    }, { 0, "LD H,L"	 }, { 0, "LD H,(HL)"  }, { 0, "LD H,A"	   },
    { 0, "LD L,B"    }, { 0, "LD L,C"	 }, { 0, "LD L,D"     }, { 0, "LD L,E"	   },
    { 0, "LD L,H"    }, { 0, "LD L,L"	 }, { 0, "LD L,(HL)"  }, { 0, "LD L,A"	   },
    { 0, "LD (HL),B" }, { 0, "LD (HL),C" }, { 0, "LD (HL),D"  }, { 0, "LD (HL),E"  },
    { 0, "LD (HL),H" }, { 0, "LD (HL),L" }, { 0, "HALT"	      }, { 0, "LD (HL),A"  },
    { 0, "LD A,B"    }, { 0, "LD A,C"	 }, { 0, "LD A,D"     }, { 0, "LD A,E"	   },
    { 0, "LD A,H"    }, { 0, "LD A,L"	 }, { 0, "LD A,(HL)"  }, { 0, "LD A,A"	   },
    { 0, "ADD A,B"   }, { 0, "ADD A,C"	 }, { 0, "ADD A,D"    }, { 0, "ADD A,E"	   },
    { 0, "ADD A,H"   }, { 0, "ADD A,L"	 }, { 0, "ADD A,(HL)" }, { 0, "ADD A,A"	   },
    { 0, "ADC A,B"   }, { 0, "ADC A,C"	 }, { 0, "ADC A,D"    }, { 0, "ADC A,E"	   },
    { 0, "ADC A,H"   }, { 0, "ADC A,L"	 }, { 0, "ADC A,(HL)" }, { 0, "ADC A,A"	   },
    { 0, "SUB	B"   }, { 0, "SUB C"	 }, { 0, "SUB D"      }, { 0, "SUB E"	   },
    { 0, "SUB H"     }, { 0, "SUB L"	 }, { 0, "SUB (HL)"   }, { 0, "SUB A"	   },
    { 0, "SBC A,B"   }, { 0, "SBC A,C"	 }, { 0, "SBC A,D"    }, { 0, "SBC A,E"	   },
    { 0, "SBC A,H"   }, { 0, "SBC A,L"	 }, { 0, "SBC A,(HL)" }, { 0, "SBC A,A"	   },
    { 0, "AND B"     }, { 0, "AND C"	 }, { 0, "AND D"      }, { 0, "AND E"	   },
    { 0, "AND H"     }, { 0, "AND L"	 }, { 0, "AND (HL)"   }, { 0, "AND A"	   },
    { 0, "XOR B"     }, { 0, "XOR C"	 }, { 0, "XOR D"      }, { 0, "XOR E"	   },
    { 0, "XOR H"     }, { 0, "XOR L"	 }, { 0, "XOR (HL)"   }, { 0, "XOR A"	   },
    { 0, "OR B"	     }, { 0, "OR C"	 }, { 0, "OR D"	      }, { 0, "OR E"	   },
    { 0, "OR H"	     }, { 0, "OR L"	 }, { 0, "OR (HL)"    }, { 0, "OR A"	   },
    { 0, "CP B"	     }, { 0, "CP C"	 }, { 0, "CP D"	      }, { 0, "CP E"	   },
    { 0, "CP H"	     }, { 0, "CP L"	 }, { 0, "CP (HL)"    }, { 0, "CP A"	   },
    { 0, "RET NZ"    }, { 0, "POP BC"	 }, { 2, "JP NZ,%"    }, { 2, "JP %"	   },
    { 2, "CALL NZ,%" }, { 0, "PUSH BC"	 }, { 1, "ADD A,%"    }, { 0, "RST 0"	   },
    { 0, "RET Z"     }, { 0, "RET"	 }, { 2, "JP Z,%"     }, { 0, "???"	   },
    { 2, "CALL Z,%"  }, { 2, "CALL %"	 }, { 1, "ADC A,%"    }, { 0, "RST 8"	   },
    { 0, "RET NC"    }, { 0, "POP DE"	 }, { 2, "JP NC,%"    }, { 1, "OUT A,(%)"  },
    { 2, "CALL NC,%" }, { 0, "PUSH DE"	 }, { 1, "SUB %"      }, { 0, "RST 10H"	   },
    { 0, "RET C"     }, { 0, "EXX"	 }, { 2, "JP C,%"     }, { 1, "IN A,(%)"   },
    { 2, "CALL C,%"  }, { 0, "???"	 }, { 1, "SBC	A,%"  }, { 0, "RST 18H"	   },
    { 0, "RET PO"    }, { 0, "POP HL"	 }, { 2, "JP PO,%"    }, { 0, "EX (SP),HL" },
    { 2, "CALL PO,%" }, { 0, "PUSH HL"	 }, { 1, "AND %"      }, { 0, "RST 20H"	   },
    { 0, "RET PE"    }, { 0, "JP (HL)"	 }, { 2, "JP PE,%"    }, { 0, "EX DE,HL"   },
    { 2, "CALL PE,%" }, { 0, "???"	 }, { 1, "XOR %"      }, { 0, "RST 28H"	   },
    { 0, "RET P"     }, { 0, "POP AF"	 }, { 2, "JP P,%"     }, { 0, "DI"	   },
    { 2, "CALL P,%"  }, { 0, "PUSH AF"	 }, { 1, "OR %"	      }, { 0, "RST 30H"	   },
    { 0, "RET M"     }, { 0, "LD SP,HL"	 }, { 2, "JP M,%"     }, { 0, "EI"	   },
    { 2, "CALL M,%"  }, { 0, "???"	 }, { 1, "CP %"	      }, { 0, "RST 38H"	   },
};

/*
 * Prototype functions:
 */
void *malloc();

void *allocmem(uint size);
/*
void mark();
void release();
void syntax_error();
*/
void err(char * message);
void err2(char *message, char *name);
word test_type(word n);
void set_type(word n, word tipe);
uint g1b();
uint g8b();
uint g2b();
uint g3b();
uint g4b();
uint g16b();
/* uint bits(word n); */
void get_a_field(a_field *a);
void get_b_field(char *b);
void get_ms_item(ms_item *item);
void insert_ext_ref(ref_type **p, ref_type **root, ref_type **last);
void linear_insert(ref_type **p, ref_type **first, ref_type **last);
void create_ref(ref_type **p, tipe_type tipe, word value, char *name, word public);
void chain_external(ms_item item);
void define_entry_point(ms_item item);
void chain_address(ms_item item);
void program_name(ms_item item);
void define_data_size(ms_item item);
void set_load(ms_item item);
void define_program_size(ms_item item);
void name_for_search(ms_item item);
void offset(ms_item item);
void end_pgm(ms_item item);
void end_file(ms_item item);
void handle_special(ms_item item);
void write_name(ref_type *p);
void write_ref_name(word pc);
void write_next_label();
void w_hex(word n, word nbytes);
void hexnibble(byte nibble);
void hexbyte(byte byte);
void hexint(uint word);
void write_offset(word pc);
void WriteDB();
void WriteOpCode();
void dis_asm();
void print_publics(ref_type **p);
void print_it();
void replace_names();
void initialize();
void one_program(ms_item *item);
//int  main(int argc, char * argv[]);
void sourname(register char *dest, char *src, char *ext);
void makename(char *dest, char *src, char *ext);
word compfext(char *name, char *ext);
word stricmp(register char * s1, register char * s2);
//word  swap(word x);

#ifdef DDEBUG
void newline();
void hexDump(char *desc, void *addr, word len);
ref_type * pr_ref_type(register ref_type * st);
void dump_data();
void strout(char * s);
void outdec(uint n);
#endif

/********************************************************************************
 *
 ********************************************************************************
void mark() {

    old_mark = sysmem;
}

void release() {

    sysmem = old_mark;
}

 ********************************************************************************
 *	Syntax error
 ********************************************************************************

void syntax_error() {

    printf("\a*** SYNTAX ERROR:\n\n");
    printf("Syntax should be either\n\n");
    printf("DISASM d:source.ext\n");
    printf("  (routing output to CON:)\n\n");
    printf("  OR\n\n");
    printf("DISASM d:source.ext d:dest.ext\n");
    printf("  (routing output to d:dest.ext\n");
}
*/

/********************************************************************************
 *	err (halt)
 ********************************************************************************/
void err(char * message) {

    fclose(fout);
    fputs(message, stderr);
    exit(EXIT_FAILURE);
}

/********************************************************************************
 *	err2
 ********************************************************************************/
void err2(char *message, char *name) {

    fputs(message, stderr);
    fputs("\"", stderr);
    fputs(name, stderr);
    fputs("\"", stderr);
    exit(EXIT_FAILURE);
}

/********************************************************************************
 *	Test type
 ********************************************************************************/
word test_type(word n) {
    /*
      Each item in the code buffer has associated with it two bits, meaning:
	  00 = 0 ABSOLUTE item, use this byte AS-IS;
	  01 = 1 CODE-RELATIVE item; relative to program base;
	  10 = 2 DATA-RELATIVE item; relative to data base;
	  11 = 3 pointER; the two bytes point to a REF item in the heap giving more information.
     */
    word dv, md;
    word i;

#ifdef DDEBUG
    char *s = NULL;
#endif

    dv = n / 4;
    md = (n % 4) * 2;

    i = 0;
    if (bittst(rel_info[dv], md))   i++;
    if (bittst(rel_info[dv], md+1)) i += 2;

#ifdef DDEBUG
    printf("TEST test_type() n = %04x ", n);
//  printf(" dv = %d   md = %d ", dv, md);
    switch (i) {
      case 0:	s = "ABSLUTE";	break;	// 00
      case 1:	s = "CODE_REL"; break;	// 01
      case 2:	s = "DATA_REL"; break;	// 10
      case 3:	s = "pointER";	break;	// 11
    }
    printf(" return = %d (%s)\n", i, s);
#endif

    return i;
}

/********************************************************************************
 *	Set type
 ********************************************************************************/
void set_type(word n, word tipe) {
    /*
      SETS the two bits mentioned above to reflect tipe.
      Uses only the bottom two bits of tipe.
    */
    word dv, md;

#ifdef DDEBUG
    printf("TEST set_type() n = %04x tipe = %d ", n, tipe);
#endif

    dv = n / 4;
    md = (n % 4) * 2;

#ifdef DDEBUG
//  printf(" dv = %d   md = %d\n", dv, md);
#endif

    if (bittst(tipe, 0)) bitset(rel_info[dv], md);
    else		 bitclr(rel_info[dv], md);

    if (bittst(tipe, 1)) bitset(rel_info[dv], md+1);
    else		 bitclr(rel_info[dv], md+1);
}

/********************************************************************************
 *	get_a_field
 ********************************************************************************/
void get_a_field(a_field *a) {
    uint n;

    n = g2b(); /* bits(2); */
    switch (n) {
      case 0:	a->tipe = ABSLUTE;    break;
      case 1:	a->tipe = CODE_REL;   break;
      case 2:	a->tipe = DATA_REL;   break;
      case 3:	a->tipe = COMMON_REL; break;
    }
    n = g16b(); /* bits(16); */
/*  a->value = swap(n); */
    a->value = n;

#ifdef DDEBUG
    printf("TEST get_a_field n = %4x a->value = %4x a->tipe = %d\n", n, a->value, a->tipe);
#endif

}

#if 0	/*------------------------------------------------------------*/

word swap (word x) {

  return ((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8);
}

#endif	/*------------------------------------------------------------*/

/********************************************************************************
 *	get_b_field
 ********************************************************************************/
void get_b_field(char *b) {
    word i, n;

    n = g3b(); /* bits(3); */
    memcpy((void *)b, (void *)(&n), 1);
    for (i = 0; i < n; i++) b[i] = (char)g8b(); /* bits(8); */
    b[i] = '\0';				// add

#ifdef DDEBUG
    printf("TEST get_b_field n = %d\t\"%s\"\n", n, b);
//    for (i = 0; i < n; i++) printf("%c", b[i]);
//    printf("\"\n");
#endif

}

/********************************************************************************
 *	Get ms item
 ********************************************************************************/
void get_ms_item(register ms_item *item) {
    word n;

#ifdef DDEBUG
    strout("TEST get_ms_item() ");
#endif

    memset((void *)item, '\0', sizeof(ms_item)); // Clear item

    switch (g1b()) {
      case 0:					// Load to value of location counter
	item->rel   = FALSE;
	item->value = g8b(); // bits(8);

#ifdef DDEBUG
	strout("Load to value of location counter = "); w_hex(item->value, 2); newline();
#endif

	break;
      case 1:
	item->rel = TRUE;
	n = g2b(); // bits(2);
	switch (n) {
	  case 0:				// Special link item

#ifdef DDEBUG
	    strout("Special link item"); newline();
#endif

	    item->tipe = ABSLUTE;
	    item->control = g4b(); // bits(4);
	    switch (item->control) {
	      case 0:
	      case 1:
	      case 2:
	      case 3:
	      case 4:	get_b_field(item->b);	break;

	      case 5:
	      case 6:
	      case 7:	get_a_field(&item->a);
			get_b_field(item->b);	break;
	      case 8:
	      case 9:
	      case 10:
	      case 11:
	      case 12:
	      case 13:
	      case 14:	get_a_field(&item->a);	break;
	    }
	    if (item->control == 14) cur_bit = 0; // force to byte boundary
	    break;
	  case 1:				//  Program relative
	    item->tipe = CODE_REL;
	    n = g16b();	// bits(16);
/*	    item->value = swap(n); */
	    item->value = n;
#ifdef DDEBUG
	    strout("Program relative n = "); w_hex(n, 2); newline();
#endif
	    break;
	  case 2:				// Data relative
	    item->tipe = DATA_REL;
	    n = g16b();	// bits(16);
/*	    item->value = swap(n); */
	    item->value = n;
#ifdef DDEBUG
	    strout("Data relative n = "); w_hex(n, 2); newline();
#endif
	    break;
	  case 3:				// Common relative
	    item->tipe = COMMON_REL;
	    n = g16b();	// bits(16);
/*	    item->value = swap(n); */
	    item->value = n;
#ifdef DDEBUG
	    strout("Common relative n = "); w_hex(n, 2); newline();
#endif
	    break;
	}
	break;
    }
}

/********************************************************************************
 *	Insert external reference
 ********************************************************************************/
void insert_ext_ref(ref_type **p, ref_type **root, ref_type **last) {
    /*
     Appends a new REF item to the end of a chain beginning at root.
     We append at the END of the list, instead of the beginning, so
     we keep the EXTERNAL items in the correct order of appearance
     in the .REL file.
    */

#ifdef DDEBUG
    printf("TEST insert_ext_ref()\n");
#endif

    if (*last == NULL) {   /* nothing in chain */
	*root	     = *p;
	(*root)->ptr = NULL;
	*last	     = *root;
	return;
    }
    (*p)->ptr	 = NULL;
    (*last)->ptr = (ref_type *)(*p);
    *last	 = *p;
}

/********************************************************************************
 *	Linear insert
 ********************************************************************************/
void linear_insert(ref_type **p, ref_type **first, ref_type **last) {
    /*
     Assuming there is a chain of ref_type, beginning with
     sentinel values first and ending with last, linearly ordered
     by value, this procedure breaks the chain and inserts p^.
    */
    ref_type *w1, *w2;

#ifdef DDEBUG
    printf("TEST linear_insert()\n");
#endif

    w2		   = *first;
    w1		   = (ref_type *)w2->ptr;
    (*last)->value = (*p)->value;
    while (w1->value < (*p)->value) {
	w2 = w1;
	w1 = (ref_type *)w2->ptr;
    }
    /*
     Insert if the value is new, or if it is repeated
     but the name is more specific.
    */
    if ((*p)->value != w1->value || w1 == *last) {
	(*p)->ptr = (ref_type *)w1;
	w2->ptr	  = (ref_type *)(*p);
	return;
    }
    if (*(*p)->name != '\0') {
	w2->ptr	  = (ref_type *)(*p);
	(*p)->ptr = w1->ptr;
    } else
	*p = w1;
}

/********************************************************************************
 *	Create reference
 ********************************************************************************/
void create_ref(ref_type **p, tipe_type tipe, word value, char *name, word public) {
    /* 0 for public, 1 for private */

#ifdef DDEBUG
    printf("TEST create_ref() tip = %d value = %xh name = \"%s\" public = %d\n",
    tipe, value, name, public);
#endif

    *p = (ref_type *)allocmem(sizeof(ref_type));

    (*p)->value = value;
    strcpy((*p)->name, name);
    if (tipe == CODE_REL) {
	(*p)->tipe = (char)(public * 2);
	linear_insert(p, &first_code_ref, &last_code_ref);
    } else {
	(*p)->tipe = (char)(public * 2 + 1);
	linear_insert(p, &first_data_ref, &last_data_ref);
    }
}

/********************************************************************************
 *	Chain external
 ********************************************************************************/
void chain_external(ms_item item) {
    ref_type *p;
    word      q, q1;		/* Indices into code buffer */
    byte      stop;

#ifdef DDEBUG
    printf("TEST chain_external()\n");
#endif

    p = (ref_type *)allocmem(sizeof(ref_type));

    if (item.a.tipe == CODE_REL)
	p->tipe = '\0';		/* code, public */
    else
	p->tipe = '\1';		/* data, public */

    p->value = item.a.value;
    strcpy(p->name, item.b);

#ifdef DDEBUG
#if CPM
    printf("TEST chain_external	  p = %04x  tipe = %d ", p, item.a.tipe);
#else
    printf("TEST chain_external	  p = %p  tipe = %d ", p, item.a.tipe);
#endif
    printf(" p->value = %x tp->name = %s\n", p->value, p->name);
#endif

    insert_ext_ref(&p, &first_ext_ref, &last_ext_ref);

    q = item.a.value;

    do {   /* Replace code-file REL quantities with pointers to REF */
	stop = ((test_type(q)	== 0) && (code_buffer[q]   == '\0') &&
		(test_type(q+1) == 0) && (code_buffer[q+1] == '\0'));

	set_type(q,   3);
	set_type(q+1, 3);

#ifdef DDEBUG
	printf("TEST chain_external   stop = %d q= %d\n", stop, q);
#endif

	memcpy((void *)(&q1), (void *)(&code_buffer[q]), 2);
	memcpy((void *)(&code_buffer[q]),  (void *)(&p), 2);
#ifdef DDEBUG
	strout("TEST chain_external code_buffer["); w_hex(q, 1); strout("] = ");
	w_hex(code_buffer[q], 2); newline();
#endif
	q = q1;
    } while (!stop);
}

/********************************************************************************
 *	Define entry point
 ********************************************************************************/
void define_entry_point(ms_item item) {	  /* public */
    ref_type *p;

    create_ref(&p, item.a.tipe, item.a.value, item.b, 0);

#ifdef DDEBUG
    printf("TEST define_entry_point() tipe = %d value = %xh b = %sh",
    item.a.tipe, item.a.value, item.b);
    strout(" p = "); w_hex((word)p, 2); newline();
#endif
}

/********************************************************************************
 *	Chain address
 ********************************************************************************/
void chain_address(ms_item item) {   /* private */
    ref_type *p;
    word      q, q1;
    byte      stop;

#ifdef DDEBUG
    printf("TEST chain_address()\n");
#endif

    create_ref(&p, CODE_REL, pc, "", 1);

    q = item.a.value;
    do {   /* Replace code-file REL quantities with pointers to REF */
	stop = ((test_type(q)	== 0) && (code_buffer[q]   == '\0') &&
		(test_type(q+1) == 0) && (code_buffer[q+1] == '\0'));

	set_type(q,   3);
	set_type(q+1, 3);

	memcpy((void *)(&q1), (void *)(&code_buffer[q]), 2);
	memcpy((void *)(&code_buffer[q]),  (void *)(&p), 2);
#ifdef DDEBUG
	strout("TEST chain_address  code_buffer["); w_hex(q, 1); strout("] = ");
	w_hex(code_buffer[q], 2); newline();
#endif
	q = q1;
    } while (!stop);
}

/********************************************************************************
 *	Program name
 ********************************************************************************/
void program_name(ms_item item) {

    strcpy(pgm_name, item.b);

#ifdef DDEBUG
    printf("TEST program_name(), name = %s\n", pgm_name);
#endif
}

/********************************************************************************
 *	Define data size
 ********************************************************************************/
void define_data_size(ms_item item) {

    data_size = item.a.value;

#ifdef DDEBUG
    printf("TEST define_data_size(), size %xh\n", data_size);
#endif
}

/********************************************************************************
 *	Set load
 ********************************************************************************/
void set_load(ms_item item) {

    if (item.a.tipe == CODE_REL) pc = item.a.value;

#ifdef DDEBUG
    printf("TEST set_load(), size %04xh\n", pc);
#endif
}

/********************************************************************************
 *	Define program size
 ********************************************************************************/
void define_program_size(ms_item item) {

    pgm_size = item.a.value;

#ifdef DDEBUG
    printf("TEST define_program_size(), size %04xh\n", pgm_size);
#endif
}

/********************************************************************************
 *	Name for search
 ********************************************************************************/
void name_for_search(ms_item item) {

#ifdef DDEBUG
     printf("TEST name_for_search()\n");
#endif
}

/********************************************************************************
 *	offset
 ********************************************************************************/
void offset(ms_item item) {
    offset_type *p;

#ifdef DDEBUG
     printf("TEST offset()\n");
#endif

    p = (offset_type *)allocmem(sizeof(offset_type));

    if (item.control == 8) p->sign = -1;   /* - offset */
    else		   p->sign =  1;

    p->loc    = pc;
    p->offset = item.a.value;
    p->next   = NULL;
    /*
     Now insert the item at the END of the offset chain.  Because
     the pc increases, the chain will be ordered on its LOC field.
    */
    if (last_offset == NULL) {
	first_offset = p;
	last_offset  = first_offset;
    } else {
	last_offset->next = (offset_type *)p;
	last_offset	  = p;
    }
}

/********************************************************************************
 *	End module
 ********************************************************************************/
void end_pgm(ms_item item) {

#ifdef DDEBUG
     printf("TEST end_pgm()\n");
#endif
}

/********************************************************************************
 *	End file
 ********************************************************************************/
void end_file(ms_item item) {

#ifdef DDEBUG
     printf("TEST end_file()\n");
#endif
}

/********************************************************************************
 *	Handle special
 ********************************************************************************/
void handle_special(ms_item item) {

#ifdef DDEBUG
    printf("TEST handle_special(), item.control = %d\t", item.control);
#endif

    switch (item.control) {
      case 0:			// Entry symbol
#ifdef DDEBUG
	  printf("(Entry symbol)\n");
#endif
	  name_for_search(item);
	  break;
      case 1:			// Select COMMON block
	  /* ignore SELECT COMMON BLOCK */
#ifdef DDEBUG
	  printf("(Select COMMON block)\n");
#endif
	  break;

      case 2:			// Program name
#ifdef DDEBUG
	  printf("(Program name)\n");
#endif
	  program_name(item);
	  break;
      case 3:			// Request library search
	   /* ignore REQUEST LIBRARY SEARCH */
#ifdef DDEBUG
	  printf("(Request library search)\n");
#endif
	  break;
      case 4:			// Reserved for future expansion
	  /* ignore RESERVED FOR FUTURE EXPANSION */
#ifdef DDEBUG
	  printf("(Reserved for future expansion)\n");
#endif
	  break;
      case 5:			// Define common size
	  /* ignore DEFINE COMMON SIZE */
#ifdef DDEBUG
	  printf("(Define common size)\n");
#endif
	  break;
      case 6:			// Chain external
#ifdef DDEBUG
	  printf("(Chain external)\n");
#endif
	  chain_external(item);
	  break;
      case 7:			// Define entry point
#ifdef DDEBUG
	  printf("(Define entry point)\n");
#endif
	  define_entry_point(item);
	  break;
      case 8:			// External plus
      case 9:			// offset
#ifdef DDEBUG
	  printf("(External plus offset)\n");
#endif
	  offset(item);
	  break;
      case 10:			// Define data size
#ifdef DDEBUG
	  printf("(Define data size)\n");
#endif
	  define_data_size(item);
	  break;
      case 11:			// Set location counter
#ifdef DDEBUG
	  printf("(Set location counter)\n");
#endif
	  set_load(item);
	  break;
      case 12:			// Chain address
#ifdef DDEBUG
	  printf("(Chain address)\n");
#endif
	  chain_address(item);
	  break;
      case 13:			// Define program size
#ifdef DDEBUG
	  printf("(Define program size)\n");
#endif
	  define_program_size(item);
	  break;
      case 14:			// End module
#ifdef DDEBUG
	  printf("(End module)\n");
#endif
	  end_pgm(item);
	  break;
      case 15:			// End file
#ifdef DDEBUG
	  printf("(End file)\n");
#endif
	  end_file(item);
	  break;
    }
}  /* handle_special */

/********************************************************************************
 *	Write name
 ********************************************************************************/
void write_name(ref_type *p) {

#ifdef DDEBUG
     printf("TEST write_name() name = \"%s\"\n", p->name);
#endif

    if (*p->name != '\0') {
	fputs(p->name, fout);
    } else {
	if (bittst(p->tipe, 0)) fprintf(fout, "D$");
	else			fprintf(fout, "C$");
	hexint(p->value);
    }
}

/********************************************************************************
 *	Write ref name
 ********************************************************************************/
void write_ref_name(word pc) {
    ref_type *p;

#ifdef DDEBUG
    printf("TEST write_ref_name(%04x) code_buffer[pc] = %04x\n",
    pc, code_buffer[pc]);
#endif

    memcpy((void *)(&p), (void *)(&code_buffer[pc]), 2);
    write_name(p);
}

/********************************************************************************
 *	Write next label
 ********************************************************************************/
void write_next_label() {

#ifdef DDEBUG
    printf("TEST write_next_label()\n");
#endif

    write_name(next_label);
    fprintf(fout, ":\t");
}

/********************************************************************************
 *	Write hex in M80-readable form
 ********************************************************************************/
void w_hex(word n, word nbytes) {
    /*
     Writes the integer n (one or two bytes) to file f in hex form,
    in M80-readable form; e.g., 0FFFFh.
    */
    if (((nbytes == 1) && ((n & 255) >= 0xA0)) ||
	((nbytes == 2) && (((unsigned word)n) >> 8 >= 0xA0))) putc('0', fout);

    switch (nbytes) {
	case 1: hexbyte(n); break;
	case 2: hexint(n); break;
    }
    putc('H', fout);
}

/********************************************************************************
 *	hex nibble
 ********************************************************************************/
void hexnibble(byte nibble) {

    putc("0123456789ABCDEF"[nibble & 0xF], fout);
}

/********************************************************************************
 *	hex byte
 ********************************************************************************/
void hexbyte(byte byte) {
    register word w;

    hexnibble((w = byte) >> 4);
    hexnibble(w);
}

/********************************************************************************
 *	hex int
 ********************************************************************************/
void hexint(uint uword) {
    register uint w;

    hexnibble((w = uword) >> 12);
    hexnibble(w >> 8);
    hexnibble(w >> 4);
    hexnibble(w);
}

/********************************************************************************
 *	Write offset
 ********************************************************************************/
void write_offset(word pc) {
    offset_type *WITH;

#ifdef DDEBUG
     printf("TEST write_offset(%04x) ", pc);

#if CPM
     printf("next_offset = %04x\n", next_offset);
#else
     printf("next_offset = %p\n", next_offset);
#endif

#endif

    if (next_offset == NULL) return;

    WITH = next_offset;

    if (WITH->loc != pc) return;
    if (WITH->sign == 1) {
	if (WITH->offset >= 0)	  fprintf(fout, "+%d",	WITH->offset);
	else			  fprintf(fout, "%d",	WITH->offset);
    } else if (WITH->offset >= 0) fprintf(fout, "-%d",	WITH->offset);
    else			  fprintf(fout, "+%d", -WITH->offset);
    next_offset = (offset_type *)next_offset->next;
}

/********************************************************************************
 *	Write DB
 ********************************************************************************/
void WriteDB() {

    fprintf(fout, "DB\t");
    w_hex((word)code_buffer[pc], 1);
    fprintf(fout, "\n\t;  *** SYNC ERROR: inconsistent REL type\n");
}


word t_l;		// Temporary variable 


/********************************************************************************
 *	Dis asm
 ********************************************************************************/
void dis_asm() {
    ref_type *p;
    word      expect;

#ifdef DDEBUG
    printf("TEST dis_asm() pc = %04xh t = %d\n", pc, t_l);
#endif

    if (next_label->ptr != NULL) {
	if (pc == next_label->value) {
	    write_next_label();
	    next_label = (ref_type *)next_label->ptr;
	} else {
	    putc('\t', fout);
	}
    } else {
	putc('\t', fout);
    }

#ifdef DDEBUG
    printf("TEST dis_asm  type = %d\n", test_type(pc));
#endif

    switch (test_type(pc)) {
      case 0:	// ABSOLUTE item, use this byte AS-IS;
	expect = op_codes[code_buffer[pc]].follow;
#ifdef DDEBUG
	printf("expect = %d name opcode = \"%s\"\n", expect, op_codes[code_buffer[pc]].name);
#endif
	switch (expect) {
	    case 0:
		if (strcmp(op_codes[code_buffer[pc]].name, "???"))
		    WriteOpCode();
		else
		    WriteDB();
		pc++;
		break;
	    case 1:
		if (test_type(pc + 1) != 0) {
		    WriteDB();
		    pc++;
		} else {
		    WriteOpCode();
		    pc += 2;
		}
		break;
	    case 2:
		t_l = test_type(pc + 1);
		if (t_l != test_type(pc + 2)) {
		    WriteDB();
		    pc++;
		} else {
		    WriteOpCode();
		    pc += 3;
		}
		break;
	}/* case expect of */
	break;
      case 1:	// CODE-RELATIVE item; relative to program base;
      case 2:	// DATA-RELATIVE item; relative to data base;
	fprintf(fout, "\t;  *** WOW!!  HOW DID THAT HAPPEN?!!\n");
	pc++;
	break;
      case 3:	// POINTER; the two bytes point to a REF item in the heap giving more information.
	memcpy((void *)(&p), (void *)(&code_buffer[pc]), 2);
	fprintf(fout, "DW\t");

	write_ref_name(pc);

	putc('\n', fout);
	pc += 2;
	break;
    }/* case test_type of */
}

/********************************************************************************
 *	Write OpCode
 ********************************************************************************/
void WriteOpCode() {
    word	  i = 0; // 1
    word	  len;
    char	  ch;
    op_code_type *WITH;	 // word

#ifdef DDEBUG
    printf("TEST WriteOpCode() pc = %04xh t = %d\n", pc, t_l);
#endif

/**/
//    i = DAsm(S, Buf);
/**/

    WITH = &op_codes[code_buffer[pc]];

    len = ((word)strlen(WITH->name))-1;
    ch	= WITH->name[0];

#ifdef DDEBUG
    printf("TEST WriteOpCode   LEN = %d CH = %xh\n", len, ch);
#endif

    while (i <= len && ch != argMark) {
	if (ch == ' ')	putc('\t', fout);
	else		putc(ch,   fout);
	i++;
	ch = WITH->name[i];
    }

    if (WITH->follow != 0) {
	if (WITH->follow == 1) {
	    w_hex(code_buffer[pc+1], 1);
	} else if (WITH->follow == 2) {
	    if (t_l == 0)      w_hex(code_buffer[pc+1], 2);
	    else if (t_l == 3) write_ref_name(pc+1);
	    write_offset(pc+1);
	}
	i++;   /* move behind % */
	ch = WITH->name[i];
	while (i <= len) {
	    if (ch == ' ')  putc('\t', fout);
	    else	    putc(ch,   fout);
	    i++;
	    ch = WITH->name[i];
	}
    }  /* follow # 0 */
    putc('\n', fout);
}

/********************************************************************************
 *	print publics
 ********************************************************************************/
void print_publics(ref_type **p) {
    word count = 0;

#ifdef DDEBUG
    printf("TEST print_publics()\n");
#endif

    while ((*p)->ptr != NULL) {
	if (*(*p)->name != '\0') {
	    if (count == 0) fprintf(fout, "\tPUBLIC\t%s", (*p)->name);
	    else	    fprintf(fout, ",%s",	  (*p)->name);
	    count++;
	    if (count == 6) {
		putc('\n', fout);
		count = 0;
	    }
	}
	*p = (ref_type *)(*p)->ptr;
    }
    if (count > 0) putc('\n', fout);
}

/********************************************************************************
 *	print it
 ********************************************************************************/
void print_it() {
    ref_type *p;
    word      count = 0;

    fprintf(stderr, "\nPass 3 - Disassembling to output\n\n");

    pc = 0;
    p  = first_ext_ref;

//	Print title

    if (*pgm_name != '\0') fprintf(fout, "\tTITLE\t%s\n", pgm_name);

//	Print extern

    while (p != NULL) {
	if (count == 0) fprintf(fout, "\tEXTRN\t%s", p->name); /* if 1 symvol */
	else		fprintf(fout, ",%s",	     p->name);

	count++;
	if (count == 6) {	// if 7 symvol print it on a new line
	    putc('\n', fout);	// followed by commas
	    count = 0;
	}
	p = (ref_type *)p->ptr;
    }
    if (count > 0) putc('\n', fout);

//  Print publics code

    p = (ref_type *)first_code_ref->ptr;
    print_publics(&p);

//  Print publics data

    p = (ref_type *)first_data_ref->ptr;
    print_publics(&p);

//  Print code
    fprintf(fout, "\t.z80\n");
    fprintf(fout, "\tCSEG\n");

    next_label  = (ref_type *)first_code_ref->ptr;
    next_offset = first_offset;

//  Disassemble

    while (pc < pgm_size) dis_asm();
//  while (pc < final_pc) dis_asm();

    if (pc < pgm_size) fprintf(fout, "\tDS\t%d\n", pgm_size - pc);

    while (next_label != last_code_ref) {
	fprintf(fout, "\tORG\t");
	w_hex(next_label->value, 2);
	putc('\n', fout);

	write_next_label();
	putc('\n', fout);
	next_label = (ref_type *)next_label->ptr;
    }

//  Print data

    if (data_size > 0) {
	fprintf(fout, "\tDSEG\t\n");
	next_label = (ref_type *)first_data_ref->ptr;
	pc = 0;
	while (next_label->ptr != NULL) {
	    if (pc != next_label->value) {
		fprintf(fout, "\tDS\t%d\n", next_label->value - pc);
		pc = next_label->value;
		continue;
	    }
	    write_next_label();

	    fprintf(fout, "DS\t");

	    next_label = (ref_type *)next_label->ptr;
	    if (next_label->ptr != NULL) {
		fprintf(fout, "%d\n", next_label->value - pc);
		pc = next_label->value;
	    } else
		fprintf(fout, "%d\n", data_size - pc);
	}
    }
    fprintf(fout, "\tEND\n");	//  Program is over, we print end
}

/********************************************************************************
 *	Replace names
 ********************************************************************************/
void replace_names() {
    ref_type *p;
    byte      found;
    word      n, value;

    fprintf(stderr, "\nPass 2 - Resolving relocations and local labels\n");

#ifdef DDEBUG
    strout("TEST REPLACE_NAMES() final_pc = "); w_hex(final_pc, 2); newline();
#endif

    pc = 0;
    while (pc < final_pc) {
	n = test_type(pc);
#ifdef DDEBUG
	strout("TEST replace_names1  pc = "); w_hex(pc, 2);
	strout(" n = "); outdec(n); newline();
#endif
	switch (n) {
	  case 0: pc++;	   break;
	  case 3: pc += 2; break;

	  case 1:
	  case 2:
		memcpy((void *)(&value), (void *)(&code_buffer[pc]), 2);
		if (n == 1) p = (ref_type *)first_code_ref->ptr;
		else	    p = (ref_type *)first_data_ref->ptr;

		found = FALSE;

		while ((p != last_code_ref) && (p != last_data_ref) && !found) {
		    found = (p->value == value);
		    if (!found) p = (ref_type *)p->ptr;
		}
		if	(p == last_code_ref) create_ref(&p, CODE_REL, value, "", 1);   /* private */
		else if (p == last_data_ref) create_ref(&p, DATA_REL, value, "", 1);
		/*
		 Now that p points to an appropriate reference -- one already
		 in the chain, or one we just added -- we can push its address
		 into the code buffer and adjust the REL bits to pointER.
		 */
		memcpy((void *)(&code_buffer[pc]), (void *)(&p), 2);
#ifdef DDEBUG
		strout("TEST replace_names2  code_buffer["); w_hex(pc, 2); strout("] = ");
		w_hex(code_buffer[pc], 2); newline();
#endif
		set_type(pc,   3);
		set_type(pc+1, 3);
		pc += 2;
		break;
		/* Case 1,2 */
	}/* Case n of */
    }  /* While pc < final_pc */
}  /* replace_names */

/********************************************************************************
 *	Initialize
 ********************************************************************************/
void initialize() {

#ifdef DDEBUG
    printf("TEST initialize()\n");
#endif

    pc	      = 0;
    *pgm_name = '\0';
    pgm_size  = 0;
    data_size = 0;

/*  release();	*/	/* free all space in the heap */

    first_code_ref	= (ref_type *)allocmem(sizeof(ref_type));
    last_code_ref	= (ref_type *)allocmem(sizeof(ref_type));
    first_data_ref	= (ref_type *)allocmem(sizeof(ref_type));
    last_data_ref	= (ref_type *)allocmem(sizeof(ref_type));
    first_code_ref->ptr = (ref_type *)last_code_ref;
    last_code_ref->ptr	= NULL;
    first_data_ref->ptr = (ref_type *)last_data_ref;
    last_data_ref->ptr	= NULL;
    first_ext_ref	= NULL;
    last_ext_ref	= NULL;
    first_offset	= NULL;
    last_offset		= NULL;
}

/********************************************************************************
 *	One program
 ********************************************************************************/
void one_program(ms_item *item) {

#ifdef DDEBUG
    strout("TEST one_program()\n");
#endif

    initialize();
    fprintf(stderr, "Pass 1 - Building segments and symbol tables\n");

    do {
	get_ms_item(item);
	if (item->rel && (item->tipe == ABSLUTE) && (item->control == 15)) return; /* End-of-file */
	if (!item->rel) {
	    memcpy((void *)(&code_buffer[pc]), (void *)(&item->value), 1);

#ifdef DDEBUG
	    strout("TEST ONE_PROGRAM  code_buffer["); w_hex(pc, 2); strout("] = "); w_hex(code_buffer[pc], 1); newline();
#endif
	    set_type(pc, 0);
	    pc++;
	    if (pc > MAXPC) err("*** ERROR: Code file overflow.");
	} else {
	    switch (item->tipe) {
	      case CODE_REL:
	      case DATA_REL:
	      case COMMON_REL:
		if (pc <= MAXPC - 2) {
		     memcpy((void *)(&code_buffer[pc]), (void *)(&item->value), 2);
#ifdef DDEBUG
		     strout("TEST ONE_PROGRAM  code_buffer["); w_hex(pc, 2); strout("] = "); w_hex(code_buffer[pc], 1); newline();
#endif
		}
		else err("*** ERROR: Code file overflow.");

		switch (item->tipe) {
		  case CODE_REL:   n = 1; break;
		  case DATA_REL:   n = 2; break;
		  case COMMON_REL: n = 0; break;
		}
		set_type(pc, n);
		set_type(pc+1, n);
		pc += 2;
		break;
	      case ABSLUTE:
		handle_special(*item);
		break;
	    }
	}
    } while (!(item->rel && item->tipe == ABSLUTE &&
	     (unsigned word)item->control < 32 &&
	     ((1 << item->control) & 0xC000) != 0));

    final_pc = pc;   /* Save program counter */

#ifdef DDEBUG
    dump_data();
#endif

    replace_names();

#ifdef DDEBUG
    dump_data();
#endif

    print_it();
}

#ifdef DDEBUG
/********************************************************************************
 *	Print ref_type
 ********************************************************************************/
ref_type * pr_ref_type(register ref_type * st) {

#if CPM
    printf(" adr = %04x", st);
#else
    printf(" adr = %p", st);
#endif

    printf(" tipe = %d ", st->tipe);
    printf(" %s ", bittst(st->tipe, 0) ?  "data relative" : "code relative");
    printf(" %s ", bittst(st->tipe, 1) ?  "private name " : "public  name ");
    printf(" value = %04x", st->value);
    printf(" name = "); strout(st->name);

#if CPM
    printf(" ptr = %04x\n", st->ptr);
#else
    printf(" ptr = %p\n", st->ptr);
#endif

    return (ref_type *)st->ptr;
}

/********************************************************************************
 *	Print offset_type
 ********************************************************************************/
offset_type * pr_offset_type(register offset_type * st) {
    printf(" sign = %d", st->sign);
    printf(" loc = %04x", st->loc);
    printf(" offset = %0x", st->offset);

#if CPM
    printf(" next = %04x\n", st->next);
#else
    printf(" next = %p\n", st->next);
#endif

    return (offset_type *)st->next;
}

/********************************************************************************
 *	Print dump_data
 ********************************************************************************/
void dump_data() {
    register ref_type	* ppp;
    offset_type         * qqq;

    newline(); hexDump(0, (char *)code_buffer, pgm_size);

    printf("\next_ref:\n");
	ppp = first_ext_ref;
	while (ppp != 0) ppp = pr_ref_type(ppp);
	newline();

    printf("\ncode_ref:\n");
	ppp = first_code_ref;
	while (ppp != 0) ppp = pr_ref_type(ppp);
	newline();

    printf("\ndata_ref:\n");
	ppp = first_data_ref;
	while (ppp != 0) ppp = pr_ref_type(ppp);
	newline();

    printf("\nnext_label:\n");
	ppp = next_label;
	while (ppp != 0) ppp = pr_ref_type(ppp);
	newline();

    printf("\noffset:\n");
	qqq = first_offset;
	while (qqq != 0) qqq = pr_offset_type(qqq);
	newline();
}

/********************************************************************************
 *	Print hexDump
 ********************************************************************************/

void hexDump(char *desc, void *addr, word len) {
    word i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    if (desc != NULL) printf ("%s:\n", desc);
    for (i = 0; i < len; i++) {
	if ((i % 16) == 0) {
	    if (i != 0) printf("  %s\n", buff);
	    printf("  %04x ", i);
	}
	printf(" %02x", pc[i]);
	if ((pc[i] < 0x20) || (pc[i] > 0x7e))	buff[i % 16] = '.';
	else					buff[i % 16] = pc[i];
	buff[(i % 16) + 1] = '\0';
    }
    while ((i % 16) != 0) {
	printf("   ");
	i++;
    }
    printf("  %s\n", buff);
}

/*
 *	Print new line
 */
void newline() {

    putchar('\n');
}

#endif

/********************************************************************************
 *	Main program
 ********************************************************************************/
int main(int argc, char * argv[]) {
    static char * nm;

    fout = stdout;

    fputs("RELtoASM Disassembler Ronald E. Bruck's algorithm on C v1.0 June 2021\n\n", stderr);
    nm = (argc > 1) ?  argv[1] : (char *)"tt.rel";
    if (argc > 2) {
	sourname(oname, argv[2],".mac");
	if (freopen(oname, "w", stdout) == NULL) err2("Can't enter file ", oname);
    }
    sourname(iname, nm, ".rel");
    if ((fin = fopen(iname, "rb")) == NULL)	 err2("Can't open file ", iname);
/*
    if ((inptrl = compfext(iname, ".TRL")) != 0) puts("Source file in TRL format");

    if (inptrl)
	one_program(&item);
    else
 */
	one_program(&item);

    fprintf(stderr, "\nEnd of file - Normal termination.\n");
    fprintf(stderr, "\nProgram_size\t= %04x\n", pgm_size);
    fprintf(stderr, "Data size\t= %04x\n", data_size);

    fclose(fin);
    fflush(stdout);
    if (ferror(stdout)) err("Disk write error");
    exit(EXIT_SUCCESS); /* stdout close automatic */
}

/********************************************************************************
 *	Automatic generation of the input name (from the utility TB Version 2.2)
 ********************************************************************************/
void sourname(register char *dest, char *src, char *ext) {

    strcpy(dest, src);
    if (strrchr(dest, '.') != NULL) return; /* If the extension is specified */
    strcat(dest, ext);
}

/********************************************************************************
 *	Automatic output name generation (from the utility TB Version 2.2)
 ********************************************************************************/
void makename(char *dest, char *src, char *ext) {
    register char *cp;

    strcpy(dest, src);
    if ((cp = strrchr(dest,'.')) != NULL) strcpy(cp,   ext);
    else				  strcat(dest, ext);
}

/********************************************************************************
 *	File Name Extension Comparison (from the utility TB Version 2.2)
 ********************************************************************************/
word compfext(char *name, char *ext) {
    register char * cp;

    if ((cp = strrchr(name,'.')) == NULL) return 0;
    return stricmp(cp, ext) == 0;
}

/********************************************************************************
 *	Get 1 bit (from the utility TB Version 2.2)
 ********************************************************************************/
uint g1b() {
    static char btb[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };

    if (cur_bit == 0) {
	if ((ch = getc(fin)) == (uint) EOF) err("Read after EOF");
	cur_bit = 8; /* bits per byte */
    }
    return (btb[--cur_bit] & ch) != 0;
}

/********************************************************************************
 *	bits (Ability to replace functions: g2b, g3b, g4b, g8b, g16b )
 ********************************************************************************/
/*
uint bits(word n) {
    uint i;
    uint m = 0;

    for (i = 1; i <= n; i++) m = m * 2 + g1b();
    return m;
}
*/

/********************************************************************************
 *	Get 2 bits (from the utility TB Version 2.2)
 ********************************************************************************/
uint g2b() {
    static uint rez;

    rez = g1b();
    return rez * 2 + g1b();
}

/********************************************************************************
 *	Get 3 bits (from the utility TB Version 2.2)
 ********************************************************************************/
uint g3b() {
    static uint rez;

    rez = g1b();
    rez = rez * 2 + g1b();
    return rez * 2 + g1b();
}

/********************************************************************************
 *	Get 4 bits (from the utility TB Version 2.2)
 ********************************************************************************/
uint g4b() {
    static uint rez;

    rez = g1b();
    rez = rez * 2 + g1b();
    rez = rez * 2 + g1b();
    return rez * 2 + g1b();
}

/********************************************************************************
 *	Get 8 bits (from the utility TB Version 2.2)
 ********************************************************************************/
uint g8b() {
    static uint rez;

    rez = g1b();
    rez = rez * 2 + g1b();
    rez = rez * 2 + g1b();
    rez = rez * 2 + g1b();
    rez = rez * 2 + g1b();
    rez = rez * 2 + g1b();
    rez = rez * 2 + g1b();
    return rez * 2 + g1b();
}

/********************************************************************************
 *	Get 16 bits (from the utility TB Version 2.2)
 ********************************************************************************/
uint g16b() {
    static uint rez;

    rez = g8b();
    return rez + (g8b() << 8);
}

/********************************************************************************
 *	allocmem
 ********************************************************************************/
void * allocmem(uint size) {
    register void * ptr;

    if (!(ptr = (void *)malloc(size))) err("*** HALT -- insufficient MEMORY");
    return ptr;
}

#ifdef DDEBUG
/********************************************************************************
 *	Output with redirection
 ********************************************************************************/
void strout(char * s) {

    fputs(s, stdout);
}

/********************************************************************************
 *	outdec (from the utility TB Version 2.2)
 ********************************************************************************/
void outdec(uint n) {
    uint dig;

    if ((dig = n/10) != 0) outdec(dig);
    putchar(n%10 + '0');
}
#endif

/********************************************************************************
 *	stricmp
 ********************************************************************************/
word stricmp(register char * s1, register char * s2) {
    register char r;

    while(!(r = toupper(*s1) - toupper(*s2++)) && *s1++) continue;
    return r;
}

/* End */
