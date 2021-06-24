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

A part of the programs for CP/M is presented in the order of their writing.

RELDUMP.ASM  8/22/80  Ron Fowler Westland, Mich. RMAC, Size .COM = 1K

RELDUMP.ASM  1981     MML Systems Limited, RMAC, Size .COM = 2K

RELMAP.ASM   1981     MML Systems Limited, RMAC

RELDEL.ASM   1981     MML Systems Limited, RMAC

READREL.C    08/20/83 v1.1 G. A. Edgar, Aztec CII, Size .COM = 11K

DISREL.ASM   10/8/84  Pierre R. Schwob on asm 8080 Size .COM = 14K

LINKMAP.ASM  1984     v1.0 NightOwl Software, Inc., RMAC, Size .COM = 2K

DREL.C       1985     J. E. Hendrix, Small C, Size .COM = 11K

UNREL.C      1986     Riclin Computer Products, MISOSYS C, Size .COM = 19K

DECODREL.C   1986     Riclin Computer Products, MISOSYS C

SPLITLIB.C   1986     Riclin Computer Products, MISOSYS C, Size .COM = 10K

LIBE.RAT     1990     Unknown, RATFOR, Size .COM = 13K

TB.C         1991     v2.2 MicroGenSf (Russia), MicroGenSf C, Size .COM = 11K

DISA-REL.PAS 1994     Ronald E. Bruck, Pascal MT+, Size .COM = 19K

REL.MAC      1997     Werner Cirsovius, M80

RELUTIL.MAC  2015     Werner Cirsovius M80, Size .COM = 3K

RELTOASM.C   2021     v1.0 A. Nikitin, Hi-Tech C, Size .COM = 19K

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



Added CP/M version of SPLITLIB program. June 24, 2021

SPLITLIB is invoked with the syntax:

SPLITLIB infile[.REL] maxlength [drivespec]

infile     - Is the filename of the REL library. If
             the extension is omitted, 'REL' will
             be assumed.

maxlength  - Is the maximum length of an output
             file (in bytes). The module currently
             being output will be continued to it's
             "module end" which will be followed by
             an "end file" byte (X'9E'). Maxlength
             must be in the range <100-32767).

drivespec  - This designates the drive to which the
             file partitions will be written. If
             omitted, the drive specified with
             "infile" will be used. Each output
             partition will be named, "infile.Rxx";
             "xx" being 01, 02, ... for the first,
             second, etc., partitions.

Andrey Nikitin
