# RELTOASM
Conversion from a Pascal program to a C program for disassembly .REL to .ASM

The .rel object file for relocatable code proposed by Microsoft is essentially a
blackbox. A programmer writing a program in assembly or other high-level language
does not need to know how this file works. Object files are an intermediary in the
process of creating an executable file by linking object files and, if necessary,
libraries.

However, information about the .rel format was partially available and several
programs were written to parse the .rel files generated during compilation of the
source code.

In a tabular form, a part of the programs for CP/M is presented in the order of
their writing.
+--------------+---------+----------------------------+-------------+------+----------------------------+
| Program      | Date    | Author                     | Language    | Size | Description                |
| name         | writing |                            | sourse      | .COM |                            |
+--------------+---------+----------------------------+-------------+------+----------------------------+
| RELDUMP      | 8/22/80 | Ron Fowler Westland, Mich. | asm 8080    |  1 K | Display contents .REL file |
+--------------+---------+----------------------------+-------------+------+----------------------------+
| RELDUMP      | 1981    | MML Systems Limited        | asm 8080    |  2 K | Display contents .REL file |
| RELMAP       | 1981    |                            | asm 8080    |      | Display names in .REL file |
| RELDEL       | 1981    |                            | asm 8080    |      | Remove any entry names     |
+--------------+---------+----------------------------+-------------+------+----------------------------+
| READREL  v1.1| 08/20/83| G. A. Edgar                | Aztec CII   | 11 K | Display contents .REL file |
+--------------+---------+----------------------------+-------------+------+----------------------------+
| DISREL       | 10/8/84 | Pierre R. Schwob           | asm 8080    | 14 K | Disasemble rel to ASM      |
+--------------+---------+----------------------------+-------------+------+----------------------------+
| LINKMAP  v1.0| 1984    | NightOwl Software, Inc.    | asm 8080    |  2 K | Display contents .REL file |
+--------------+---------+----------------------------+-------------+------+----------------------------+
| DREL         | 1985    | J. E. Hendrix              | Small C     | 11 K | Dump REL or LIB file       |
+--------------+---------+----------------------------+-------------+------+----------------------------+
| UNREL        | 1986    | Riclin Computer Products   | MISOSYS C   | 19 K | Disassemble rel to ASM     |
| DECODREL     | 1986    |                            |             |      | Display contents .REL file |
| SPLITLIB     | 1986    |                            |             | 10 K | Splitting the library      |
+--------------+---------+----------------------------+-------------+------+----------------------------+
| LIBE         | 1990    | Unknown                    | RATFOR      | 13 K | Edit $.rel files           |
+--------------+---------+----------------------------+-------------+------+----------------------------+
| TB       v2.2| 1991    | MicroGenSf (Russia)        | MicroGenSf C| 11 K | Display contents .REL file |
+--------------+---------+----------------------------+-------------+------+----------------------------+
| DISA-REL     | 1994    | Ronald E. Bruck            | Pascal MT+  | 19 K | Disassemble rel to ASM     |
+--------------+---------+----------------------------+-------------+------+----------------------------+
| REL          | 1997    | Werner Cirsovius           | asm Z80     |      | Checking .REL file format  |
+--------------+---------+----------------------------+-------------+------+----------------------------+
| RELUTIL      | 2015    | Werner Cirsovius           | asm Z80     |  3 K | Сonbines 3  MML utilities  |
+--------------+---------+----------------------------+-------------+------+----------------------------+
| RELtoASM v1.0| 2021    | A. Nikitin                 | Hi-Tech C   | 19 K | Translation DISA-REL into C|
+--------------+---------+----------------------------+-------------+------+----------------------------+
Many of these programs display the information contained in the object file in one
form or another. Two of them are trying to translate it into assembly language code.
These are DISA-REL and UNREL. There is Pascal MT+ source code for DISA-REL.

The program is analogous to the UNREL disassembler Copyright 1986
Riclin Computer Products.

The initial conversion from Pascal to C is done using p2c 2.00.Oct.15, the Pascal-to-C
translator from input file "DISA-REL.PAS".

Part of the code is used from the utility TB Version 2.2 Copyright (c) 1988, 1993 
MicroGenSf (Russia).

I converted this program to Hi-Tech C with the following changes:

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

The following files are provided:

RELTOASM.C      - Hi-Tech C source code program 
DISA-REL.PAS    - Pascal MT+ source code DISA-REL
DISASM.DOC      - DISA-REL description
LIBEDIT.RAT     - the source code of the LIBE program in the RATFOR language 
LIBEDIT.txt     - description of the LIBE program in English
LIBEDIT_rus.txt - description of the LIBE program in Russian 



Andrey Nikitin
