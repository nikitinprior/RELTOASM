PROGRAM dis_rel;
{
  Disassemble a .ERL or .REL file.

  AUTHOR:

        Professor Ronald E. Bruck
	Department of Mathematics
	University of Southern California
	Los Angeles, CA  90089
}

CONST
  tty_name = 'CON:';
  tabu	   = 9;
  bs       = 8;
  bell     = 7;
  argMark = '%';
  op_code_filename = 'A:OPCODES.TXT';
{
  Maximum code size.  Maximum REL info size is 1/4 this:
}
  maxpc    = 16384;
  maxrel   =  4096;

TYPE
  op_code_type =
    RECORD
      follow  : 0..2;		{ # bytes which follow opcode }
      name    : STRING [ 15 ];   { mnemonic for opcode }
    END;

  tipe_type = (abslute, code_rel, data_rel, common_rel);

  a_field =
    RECORD
      tipe  : tipe_type;
      value : INTEGER;
    END;

  name_type = STRING [ 8 ];

  ms_item =
    RECORD
      rel     : BOOLEAN;
      value   : INTEGER;
      tipe    : tipe_type;
      control : INTEGER;
      a       : a_field;
      b       : name_type;
    END;

  ref_type =
    RECORD
      tipe : BYTE;
	{ Bit assignments:  
		Bit 0 : 0 = code relative, 1 = data relative;
		Bit 1 : 0 = public name, 1 = private name.
	  Bit 1 is irrelevant if the item is placed in the chain
	  of external references.
	}
      value : INTEGER;
      name  : name_type;
      ptr   : ^ref_type;
    END;

  ptr_to_ref = ^ref_type;

  offset_type =
    RECORD
      sign   : -1..+1;
      loc,
      offset : INTEGER;
      next   : ^offset_type;
    END;

VAR
  sysmem     : EXTERNAL INTEGER;
  pgm_name,
  in_f_name,
  out_f_name : STRING;
  item       : ms_item;
  ch,tab     : CHAR;
  next_label,
  first_code_ref,
  last_code_ref,
  first_data_ref,
  last_data_ref,
  first_ext_ref,
  last_ext_ref   : ptr_to_ref;
  first_offset,
  last_offset,
  next_offset    : ^offset_type;
  n,
  pc, 			  { program counter   }
  final_pc,		  { last byte of code }
  old_mark,		  { mark top of heap  }
  pgm_size,
  data_size,
  result,
  cur_bit    : INTEGER;
  fbyte      : FILE OF BYTE;  { File of char does interpretation }
  fout       : TEXT;
  code_buffer: ARRAY [ 0..maxpc ] OF BYTE;
  rel_info   : ARRAY [ 0..maxrel ] OF BYTE;
  op_codes   : ARRAY [ 0..255 ] OF op_code_type;

EXTERNAL PROCEDURE @HLT;
EXTERNAL FUNCTION keypress : BOOLEAN;
{
  Procedure to extract two filenames from the command line.
  The CCP has already done this, of course, but this way we don't
  have to define an FCB:
}
EXTERNAL PROCEDURE xtrctcmd ( VAR name1, name2 : STRING );

PROCEDURE mark;
BEGIN
  old_mark := sysmem;
END;

PROCEDURE release;
BEGIN
  sysmem := old_mark;
END;

PROCEDURE syntax_error;
VAR
  i : INTEGER;
BEGIN
  WRITELN ( CHR ( bell ), '*** SYNTAX ERROR:' );
  WRITELN;
  WRITELN ( 'Syntax should be either' );
  WRITELN;
  WRITELN ( 'DISASM d:source.ext' );
  WRITELN ( '  (routing output to CON:)' );
  WRITELN;
  WRITELN ( '  OR' );
  WRITELN;
  WRITELN ( 'DISASM d:source.ext d:dest.ext' );
  WRITELN ( '  (routing output to d:dest.ext' );
END { syntax_error };

PROCEDURE halt ( message : STRING );
BEGIN
  CLOSE ( fbyte, result );
  CLOSE ( fout, result );
  WRITELN;
  WRITELN ( message );
  @HLT;
END;

PROCEDURE open_files;
VAR
  i : INTEGER;
BEGIN
  xtrctcmd ( in_f_name, out_f_name );
  IF (in_f_name = '') THEN syntax_error;
  IF (out_f_name = '') THEN out_f_name := tty_name;
  ASSIGN ( fbyte, in_f_name );
  RESET ( fbyte );
  IF IORESULT = 255 THEN 
    halt ( CONCAT ( 'Unable to open file ', in_f_name, '.' ) );
  cur_bit := 7;
  ASSIGN ( fout, op_code_file_name );
  RESET ( fout );
  IF IORESULT = 255 THEN
    halt ( 'Unable to open file of opcode names.' );
  FOR i := 0 TO 255 DO
    BEGIN
      READ ( fout, op_codes [ i ].follow );
      READ ( fout, ch );
      READLN ( fout, op_codes [ i ].name );
    END;
  CLOSE ( fout, result );
  ASSIGN ( fout, out_f_name );
  REWRITE ( fout );
  IF IORESULT = 255 THEN halt ( 'Unable to REWRITE output file.' );
END;

FUNCTION test_type ( n : INTEGER ) : INTEGER;
{
  Each item in the code buffer has associated with it two bits, meaning:
	00 = ABSOLUTE item, use this byte AS-IS;
	01 = CODE-RELATIVE item; relative to program base;
	10 = DATA-RELATIVE item; relative to data base;
	11 = POINTER; the two bytes point to a REF item in the heap
		giving more information.
}
VAR
  dv, md, i : INTEGER;
BEGIN
  dv := n DIV 4;
  md := 2 * (n MOD 4);
  i := 0;
  IF TSTBIT ( rel_info [ dv ], md ) THEN i := i + 1;
  IF TSTBIT ( rel_info [ dv ], md + 1 ) THEN i := i + 2;
  test_type := i;
END;

PROCEDURE set_type ( n, tipe : INTEGER );
{
  SETS the two bits mentioned above to reflect tipe.
  Uses only the bottom two bits of tipe.
}
VAR
  dv, md : INTEGER;
BEGIN
  dv := n DIV 4;
  md := 2 * ( n MOD 4 );
  IF TSTBIT ( tipe, 0 ) 
    THEN SETBIT ( rel_info [ dv ], md )
    ELSE CLRBIT ( rel_info [ dv ], md );
  IF TSTBIT ( tipe, 1 ) 
    THEN SETBIT ( rel_info [ dv ], md + 1 )
    ELSE CLRBIT ( rel_info [ dv ], md + 1 );
END;

FUNCTION bit : INTEGER;
BEGIN
  IF cur_bit < 0 THEN
    BEGIN
      IF EOF ( fbyte ) THEN halt ( '*** ERROR: Last byte read...' );
      get ( fbyte );
      cur_bit := 7;
    END;
  IF TSTBIT ( fbyte^, cur_bit ) THEN bit := 1 ELSE bit := 0;
  cur_bit := cur_bit - 1;
END;

FUNCTION bits ( n : INTEGER ) : INTEGER;
VAR
  i, m : INTEGER;
BEGIN
  m := 0;
  FOR i := 1 TO n DO m := 2 * m + bit;
  bits := m;
END;

PROCEDURE get_a_field ( VAR a : a_field );
VAR
  n : INTEGER;
BEGIN
  n := bits ( 2 );
  CASE n OF
    0 : a.tipe := abslute;
    1 : a.tipe := code_rel;
    2 : a.tipe := data_rel;
    3 : a.tipe := common_rel;
  END;
  n := bits ( 16 );
  a.value := SWAP ( n );
END;

PROCEDURE get_b_field ( VAR b : name_type );
VAR
  i, n : INTEGER;
BEGIN
  n := bits ( 3 );
  MOVELEFT ( n, b, 1 );
  FOR i := 1 TO n DO b [ i ] := CHR ( bits ( 8 ) );
END;

PROCEDURE get_ms_item ( VAR item : ms_item );
VAR
  n : INTEGER;
BEGIN
  FILLCHAR ( item, SIZEOF ( ms_item ), CHR ( 0 ) );
  CASE bit OF
    0 : BEGIN
	  item.rel := FALSE;
	  item.value := bits ( 8 );
	END;
    1 : BEGIN
	  item.rel := TRUE;
	  n := bits ( 2 );
	  CASE n OF
	    0 : BEGIN
		  item.tipe := abslute;
		  item.control := bits ( 4 );
		  CASE item.control OF
		    0,1,2,3,4 : get_b_field ( item.b );
		    5,6,7     : BEGIN
				  get_a_field ( item.a );
				  get_b_field ( item.b );
				END;
		    8,9,10,11,
		    12,13,14  : get_a_field ( item.a );
		  END;
		  IF item.control = 14 THEN cur_bit := -1;
		    { force to byte boundary }
		END;
	    1 : BEGIN
		  item.tipe := code_rel;
		  n := bits ( 16 );
		  item.value := SWAP ( n );
		END;
	    2 : BEGIN
		  item.tipe := data_rel;
		  n := bits ( 16 );
		  item.value := SWAP ( n );
		END;
	    3 : BEGIN
		  item.tipe := common_rel;
		  n := bits ( 16 );
		  item.value := SWAP ( n );
		END;
	  END;
	END;
  END;
END;


PROCEDURE insert_ext_ref ( VAR p, root, last : ptr_to_ref );
{
  Appends a new REF item to the end of a chain beginning at root.
  We append at the END of the list, instead of the beginning, so
  we keep the EXTERNAL items in the correct order of appearance
  in the .REL file.
}
BEGIN
  IF last = NIL
    THEN { nothing in chain }
      BEGIN
	root := p;
	root^.ptr := NIL;
	last := root;
      END
    ELSE
      BEGIN
	p^.ptr := NIL;
	last^.ptr := p;
	last := p;
      END;
END;

PROCEDURE linear_insert ( VAR p, first, last : ptr_to_ref );
{
  Assuming there is a chain of ref_type, beginning with
  sentinel values first and ending with last, linearly ordered 
  by value, this procedure breaks the chain and inserts p^.
}
VAR
  w1, w2 : ptr_to_ref;
BEGIN
  w2 := first;
  w1 := w2^.ptr;
  last^.value := p^.value;
  WHILE w1^.value < p^.value DO
    BEGIN
      w2 := w1;
      w1 := w2^.ptr;
    END;
{
  Insert if the value is new, or if it is repeated
  but the name is more specific.
}
  IF (p^.value <> w1^.value) OR ( w1 = last )
    THEN
      BEGIN
        p^.ptr := w1;
        w2^.ptr := p;
      END
    ELSE IF (p^.name <> '') THEN
      BEGIN
        w2^.ptr := p;
	p^.ptr := w1^.ptr;
      END
    ELSE p := w1;
END;

PROCEDURE write_memavail;
BEGIN
  IF out_f_name <> tty_name THEN
    BEGIN
      WRITE ( CHR (bs), CHR (bs), CHR (bs), CHR (bs) );
      WRITE_HEX ( OUTPUT, MEMAVAIL, 2 );
    END;
END;


PROCEDURE test_for_create_error;
BEGIN
  IF (MEMAVAIL < SIZEOF ( ref_type ) ) THEN 
    halt ( '*** HALT -- insufficient MEMORY' );
  write_memavail;
END;

PROCEDURE create_ref ( VAR p : ptr_to_ref; tipe : tipe_type;
		       value : INTEGER; name : name_type;
		       public : INTEGER { 0 for public, 1 for private } );
BEGIN
  new ( p );
  test_for_create_error;
  p^.value := value;
  p^.name := name;
  IF tipe = code_rel
    THEN
      BEGIN
	p^.tipe := CHR ( 2 * public );
	linear_insert ( p, first_code_ref, last_code_ref );
      END
    ELSE
      BEGIN
	p^.tipe := CHR ( 1 + 2 * public );
	linear_insert ( p, first_data_ref, last_data_ref );
      END;
END { create_ref };

PROCEDURE chain_external ( item : ms_item );
VAR
  p     : ptr_to_ref;
  q, q1 : INTEGER;  { Indices into code buffer }
  stop  : BOOLEAN;
BEGIN
  new ( p );
  test_for_create_error;
  IF item.a.tipe = code_rel
    THEN
      p^.tipe := CHR ( 0 )  { code, public }
    ELSE
      p^.tipe := CHR ( 1 ); { data, public }
  p^.value := item.a.value;
  p^.name := item.b;
  insert_ext_ref ( p, first_ext_ref, last_ext_ref );
  q := item.a.value;
  REPEAT  { Replace code-file REL quantities with pointers to REF }
    stop := ( test_type ( q ) = 0 ) AND (code_buffer [ q ] = CHR ( 0 ))
	        AND (test_type ( q + 1 ) = 0)
	        AND (code_buffer [ q + 1 ] = CHR ( 0 ));
    set_type ( q, 3 );
    set_type ( q + 1, 3 );
    MOVELEFT ( code_buffer [ q ], q1, 2 );
    MOVELEFT ( p, code_buffer [ q ], 2 );
    q := q1;
  UNTIL stop;
END;

PROCEDURE define_entry_point ( item : ms_item );
VAR
  p : ptr_to_ref;
BEGIN
  create_ref ( p, item.a.tipe, item.a.value, item.b, 0 { public } );
END;

PROCEDURE chain_address ( item : ms_item );
VAR
  p     : ptr_to_ref;
  q, q1 : INTEGER;
  stop  : BOOLEAN;
BEGIN
  create_ref ( p, code_rel, pc, '', 1 { private } );
  q := item.a.value;
  REPEAT  { Replace code-file REL quantities with pointers to REF }
    stop := ( test_type ( q ) = 0 ) AND (code_buffer [ q ] = CHR ( 0 ))
	        AND (test_type ( q + 1 ) = 0)
	        AND (code_buffer [ q + 1 ] = CHR ( 0 ));
    set_type ( q, 3 );
    set_type ( q + 1, 3 );
    MOVELEFT ( code_buffer [ q ], q1, 2 );
    MOVELEFT ( p, code_buffer [ q ], 2 );
    q := q1;
  UNTIL stop;
END;

PROCEDURE program_name ( item : ms_item );
BEGIN
  pgm_name := item.b;
END;

PROCEDURE define_data_size ( item : ms_item );
BEGIN
  data_size := item.a.value;
END;

PROCEDURE set_load ( item : ms_item );
BEGIN
  IF item.a.tipe = code_rel THEN pc := item.a.value;
END;

PROCEDURE define_program_size ( item : ms_item );
BEGIN
  pgm_size := item.a.value;
END;

PROCEDURE name_for_search ( item : ms_item );
BEGIN
END;

PROCEDURE offset ( item : ms_item );
VAR
  p : ^offset_type;
BEGIN
  new ( p );
  test_for_create_error;
  IF item.control = 8 { - offset } THEN p^.sign := -1 ELSE p^.sign := +1;
  p^.loc := pc;
  p^.offset := item.a.value;
  p^.next := NIL;
{
  Now insert the item at the END of the offset chain.  Because
  the pc increases, the chain will be ordered on its LOC field.
}
  IF last_offset = NIL
    THEN
      BEGIN
	first_offset := p;
	last_offset := first_offset;
      END
    ELSE
      BEGIN
	last_offset^.next := p;
	last_offset := p;
      END;
END;

PROCEDURE end_pgm ( item : ms_item );
BEGIN
END;

PROCEDURE end_file ( item : ms_item );
BEGIN
END;

PROCEDURE handle_special ( item : ms_item );
BEGIN
  CASE item.control OF
      0 : name_for_search ( item );
    { 1 : ignore SELECT COMMON BLOCK }
      2 : program_name ( item );
    { 3 : ignore REQUEST LIBRARY SEARCH }
    { 4 : ignore RESERVED FOR FUTURE EXPANSION }
    { 5 : ignore DEFINE COMMON SIZE }
      6 : chain_external ( item );
      7 : define_entry_point ( item );
      8,
      9 : offset ( item );
     10 : define_data_size ( item );
     11 : set_load ( item );
     12 : chain_address ( item );
     13 : define_program_size ( item );
     14 : end_pgm ( item );
     15 : end_file ( item );
  END;
END { handle_special };

PROCEDURE write_name ( p : ptr_to_ref );
VAR
  i : INTEGER;
BEGIN
  IF p^.name <> ''
    THEN 
      WRITE ( fout, p^.name)
    ELSE
      BEGIN
	IF TSTBIT ( p^.tipe, 0 ) 
	  THEN WRITE ( fout, 'D$' )
	  ELSE WRITE ( fout, 'C$' );
	WRITE_HEX ( fout, p^.value, 2 );
      END;
END;

PROCEDURE write_ref_name ( pc : INTEGER );
VAR
  p : ptr_to_ref;
BEGIN
  MOVELEFT ( code_buffer [ pc ], p, 2 );
  write_name ( p );
END;

PROCEDURE write_next_label;
BEGIN
  write_name ( next_label );
  WRITE( fout, ':', tab );
END;
	
PROCEDURE w_hex ( n, nbytes : INTEGER );
{
  Writes the integer n (one or two bytes) to file f in hex form,
  in M80-readable form; e.g., 0FFFFh.
}
BEGIN
  IF ( (nbytes = 1) AND (LO ( n ) >= $a0) ) OR ( (nbytes = 2) AND 
    (HI ( n ) >= $a0) ) THEN WRITE ( fout, '0' );
  WRITE_HEX ( fout, n, nbytes );
  WRITE ( fout, 'H' );
END;

PROCEDURE write_offset ( pc : INTEGER );
BEGIN
  IF next_offset <> NIL THEN WITH next_offset^ DO
    BEGIN
      IF loc = pc THEN
	BEGIN
	  IF sign = 1
	    THEN
	      BEGIN
	        IF offset >= 0
		  THEN WRITE ( fout, '+', offset )
		  ELSE WRITE ( fout, offset );
	      END
	    ELSE IF offset >= 0
	      THEN WRITE ( fout, '-', offset )
	      ELSE WRITE ( fout, '+', -offset );
	  next_offset := next_offset^.next;
	END;
    END;
END;


PROCEDURE WriteDB;
BEGIN
  WRITE ( fout, 'DB', TAB );
  w_hex ( ord ( code_buffer [ pc ] ), 1 );
  WRITELN ( fout );
  WRITELN ( fout, TAB, ';  *** SYNC ERROR: inconsistent REL type' );
END; (* WriteDB *)


PROCEDURE dis_asm ( VAR pc : INTEGER );
VAR
  a : a_field;
  p : ptr_to_ref;
  expect, 
  n, 
  t, 
  temp : INTEGER;

  PROCEDURE WriteOpCode;
  VAR
   i,len: INTEGER;
   ch: CHAR;
  BEGIN
    WITH opCodes[ codeBuffer[ pc ] ] DO BEGIN
      i := 1;
      len := ORD( name[0] );	(* length *)
      ch := name[1];
      WHILE (i <= len) AND (ch <> argMark) DO BEGIN
	IF ch = ' ' THEN 
	  WRITE( fout, TAB )
	ELSE
	  WRITE ( fout, ch );
	i := i + 1;
	ch := name[i];
      END;
      IF follow <> 0 THEN BEGIN
        IF follow = 1 THEN
 	  Whex( ORD( codeBuffer[ pc + 1 ] ), 1 )
        ELSE IF follow = 2 THEN BEGIN
          IF t = 0 THEN BEGIN
	    MOVELEFT ( codeBuffer[ pc + 1 ], temp, 2 );
	    w_hex ( temp, 2 )
	  END
          ELSE IF t = 3 THEN BEGIN
            write_ref_name ( pc + 1 );
          END;
	  write_offset ( pc + 1 );
        END;
	i := i + 1;	(* move behind % *)
        WHILE i <= len DO BEGIN
	  IF ch = ' ' THEN
	    WRITE( fout, tab )
	  ELSE
	    WRITE ( fout, name[i] );
	  i := i + 1;
        END;
      END; (* follow # 0 *)
      WRITELN ( fout );
    END; (* WITH *)
  END; (* WriteOpCode *)

BEGIN
  IF next_label^.ptr <> NIL 
    THEN 
      BEGIN
        IF pc = next_label^.value 
          THEN 
            BEGIN
	      write_next_label;
	      next_label := next_label^.ptr;
            END
          ELSE 
	    WRITE ( fout, TAB );
      END
    ELSE 
      WRITE ( fout, TAB);
  CASE test_type ( pc ) OF
    0 :BEGIN
          expect := op_codes [ code_buffer [ pc ] ].follow;
	  CASE expect OF
	    0 : BEGIN
		  IF op_codes [ code_buffer [ pc ] ].name <> '???' THEN BEGIN
		    WriteOpCode;
		  END
		  ELSE BEGIN 
		    WriteDB; 
		  END;
		  pc := pc + 1;
	        END;
	    1 : BEGIN
		  IF test_type ( pc + 1 ) <> 0 THEN BEGIN
		    WriteDB;
		    pc := pc + 1;
		  END
		  ELSE BEGIN
		    WriteOpCode;
		    pc := pc + 2;
		  END;
		END;
	    2 : BEGIN
		  t := test_type ( pc + 1 );
		  IF t <> test_type ( pc + 2 ) THEN BEGIN
		    WriteDB;
		    pc := pc + 1;
		  END
		  ELSE BEGIN
		    WriteOpCode;
		    pc := pc + 3;
		  END;
		END;
	    END; { case expect of }
          END;
    1,2:
	BEGIN
	  WRITELN ( fout, TAB, ';  *** WOW!!  HOW DID THAT HAPPEN?!!' );
	  pc := pc + 1;
	END;
    3 : BEGIN
          MOVELEFT ( code_buffer [ pc ], p, 2 );
	  WRITE ( fout, 'DW', TAB );
	  write_ref_name ( pc );
	  WRITELN ( fout );
	  pc := pc + 2;
	END;
  END { case test_type of }
END;


PROCEDURE print_publics ( VAR p : ptr_to_ref );
VAR
  count : INTEGER;
BEGIN
  count := 0;
  WHILE p^.ptr <> NIL DO
    BEGIN
      IF p^.name <> '' THEN
	BEGIN
	  IF count = 0
	    THEN WRITE ( fout, TAB, 'PUBLIC', TAB, p^.name )
	    ELSE WRITE ( fout, ',' , p^.name );
	  count := count + 1;
	  IF count = 6 THEN
	    BEGIN
	      WRITELN ( fout );
	      count := 0;
	    END;
	END;
      p := p^.ptr;
    END;
  IF count > 0 THEN WRITELN ( fout );
END { print_publics };

PROCEDURE print_it;
VAR
  p     : ptr_to_ref;
  count : INTEGER;
BEGIN
  IF out_f_name <> tty_name THEN
    BEGIN
      WRITELN;
      WRITELN ( 'Disassembling ', pgm_name );
    END;
  pc := 0;
  p := first_ext_ref;
  IF pgm_name <> '' THEN
    WRITELN ( fout, TAB, 'TITLE', TAB, pgm_name );
  count := 0;
  WHILE p <> NIL DO
    BEGIN
      IF count = 0 
	THEN WRITE ( fout, TAB, 'EXTRN', TAB, p^.name )
	ELSE WRITE ( fout, ',' , p^.name );
      count := count + 1;
      IF count = 6 THEN
        BEGIN
	  WRITELN ( fout );
	  count := 0;
        END;
      p := p^.ptr;
    END;
  IF count > 0 THEN WRITELN ( fout );
  p := first_code_ref^.ptr;
  print_publics ( p );
  p := first_data_ref^.ptr;
  print_publics ( p );
  WRITELN ( fout, TAB, 'CSEG' );
  next_label := first_code_ref^.ptr;
  next_offset := first_offset;
  WHILE pc < final_pc DO
    dis_asm ( pc );
  IF pc < pgm_size THEN
    WRITELN ( fout, TAB, 'DS', TAB, pgm_size - pc );
  WHILE next_label <> last_code_ref DO
    BEGIN
      WRITE ( fout, TAB, 'ORG', TAB );
      w_hex ( next_label^.value, 2 );
      WRITELN ( fout );
      write_next_label;
      WRITELN ( fout );
      next_label := nextlabel^.ptr;
    END;
  IF data_size > 0 THEN
    BEGIN
      WRITELN ( fout, TAB, 'DSEG', TAB );
      next_label := first_data_ref^.ptr;
      pc := 0;
      WHILE next_label^.ptr <> NIL DO
        BEGIN
          IF pc = next_label^.value
            THEN
	      BEGIN
	        write_next_label;
	        WRITE ( fout, 'DS', TAB );
	        next_label := next_label^.ptr;
	        IF next_label^.ptr <> NIL
	          THEN
		    BEGIN
		      WRITELN ( fout, next_label^.value - pc );
		      pc := next_label^.value;
		    END
	          ELSE
		    WRITELN ( fout, data_size - pc );
	      END
	    ELSE
	      BEGIN
	        WRITELN ( fout, TAB, 'DS', TAB, 
			  next_label^.value - pc );
	        pc := next_label^.value;
	      END;
        END;
    END;
  WRITELN ( fout, TAB, 'END' );
END;

PROCEDURE replace_names;
VAR
  p     : ptr_to_ref;
  found : BOOLEAN;
  n,
  value : INTEGER;
BEGIN
  pc := 0;
  WHILE pc < final_pc DO
    BEGIN
      n := test_type ( pc );
      CASE n OF
	0   : pc := pc + 1;
	3   : pc := pc + 2;
	1,2 : BEGIN
		MOVELEFT ( code_buffer [ pc ], value, 2 );
		IF n = 1 
		  THEN p := first_code_ref^.ptr
		  ELSE p := first_data_ref^.ptr;
	        found := FALSE;
		WHILE (p <> last_code_ref) AND (p <> last_data_ref) 
		      AND NOT found DO
		  BEGIN
		    found := (p^.value = value);
		    IF NOT found THEN p := p^.ptr;
		  END;
		IF (p = last_code_ref)
		  THEN create_ref ( p, code_rel, value, '', 1 { private } )
		  ELSE IF (p = last_data_ref) THEN
		    create_ref ( p, data_rel, value, '', 1 );
	{
	  Now that p points to an appropriate reference -- one already
	  in the chain, or one we just added -- we can push its address
	  into the code buffer and adjust the REL bits to POINTER.
	}
		MOVELEFT ( p, code_buffer [ pc ], 2 );
		set_type ( pc, 3 );
		set_type ( pc + 1, 3 );
		pc := pc + 2;
	      END  { Case 1,2 };
      END { Case n of };
    END { While pc < final_pc };
END { replace_names };

PROCEDURE initialize;
BEGIN
  pc := 0;
  pgm_name := '';
  pgm_size := 0;
  data_size := 0;
  release;  			{ free all space in the heap }
  new ( first_code_ref );
  new ( last_code_ref  );
  new ( first_data_ref );
  new ( last_data_ref  );
  first_code_ref^.ptr := last_code_ref;
  last_code_ref^.ptr := NIL;
  first_data_ref^.ptr := last_data_ref;
  last_data_ref^.ptr := NIL;
  first_ext_ref := NIL;
  last_ext_ref  := NIL;
  first_offset := NIL;
  last_offset := NIL;
END;


PROCEDURE one_program ( VAR item : ms_item );
BEGIN
  initialize;
  IF out_f_name <> tty_name THEN
    WRITE ( 'Memory remaining:     ' );
  REPEAT
    get_ms_item ( item );
    IF (item.rel) AND (item.tipe = abslute) AND (item.control = 15) THEN EXIT;
      { End-of-file }
    IF NOT item.rel
      THEN
	BEGIN
	  MOVELEFT ( item.value, code_buffer [ pc ], 1 );
	  set_type ( pc, 0 );
	  pc := pc + 1;
	  IF pc > maxpc THEN halt ( '*** ERROR: Code file overflow.' );
	END
      ELSE
	CASE item.tipe OF
	    code_rel,
	    data_rel,
	    common_rel : BEGIN
			   IF pc <= maxpc - 2 
			     THEN
			       MOVELEFT ( item.value, code_buffer [ pc ], 2 )
			     ELSE
			       halt ( '*** ERROR: Code file overflow.' );
			   CASE item.tipe OF
			     code_rel : n := 1;
			     data_rel : n := 2;
			     common_rel: n := 0;
		           END;
			   set_type ( pc, n );
		    	   set_type ( pc+1, n );
			   pc := pc + 2;
			 END;
	    abslute    : handle_special ( item );
	END;
    UNTIL (item.rel) AND (item.tipe=abslute) AND (item.control IN [14,15]);
    final_pc := pc;  { save program counter }
    replace_names;
    print_it;
END;

BEGIN { >>> MAIN PROGRAM <<< }
  tab := CHR( tabu );
  open_files;
  mark;  { mark heap space }
  REPEAT
    one_program ( item )
  UNTIL (item.rel) AND (item.tipe = abslute) AND (item.control = 15 );
  halt ( 'End of file - Normal termination.' );
END.
