#include <stdio.h>
#include <fcntl.h>

/*
0  TRAP
1  NULL
2  FREE
3  SYMBOL
4  SYMBOL HEADER
5  FIX
6  EXTENDED NUMBER
7  HEADER
10 GC-FORWARD
11 EXTERNAL-VALUE-CELL-POINTER
12 ONE-Q-FORWARD
13 HEADER-FORWARD
14 BODY-FORWARD
15 LOCATIVE
16 LIST -- don't skip
17 U CODE ENTRY
20 FEF
21 ARRAY-POINTER
22 ARRAY-HEADER
23 STACK-GROUP
24 CLOSURE
25 SMALL-FLONUM
26 SELECT-METHOD
27 INSTANCE
0  INSTANCE-HEADER
0  ENTITY
0  STACK-CLOSURE
*/

struct {
	char *name;
	unsigned int a;
	unsigned int v;
} cv[] = {
	{ "A-V-RESIDENT-SYMBOL-AREA", 0 },
	{ "A-V-SYSTEM-COMMUNICATION-AREA", 0 },
	{ "A-V-SCRATCH-PAD-INIT-AREA", 0 },
	{ "A-V-MICRO-CODE-SYMBOL-AREA", 0 },
	{ "A-V-PAGE-TABLE-AREA", 0 },
	{ "A-V-PHYSICAL-PAGE-DATA", 0 },
	{ "A-V-REGION-ORIGIN", 0 },
	{ "A-V-REGION-LENGTH", 0 },
	{ "A-V-REGION-BITS", 0 },
	{ "A-V-ADDRESS-SPACE-MAP", 0 },
	{ "A-V-REGION-FREE-POINTER", 0 },
	{ "A-V-REGION-GC-POINTER", 0 },
	{ "A-V-REGION-LIST-THREAD", 0 },
	{ "A-V-AREA-NAME", 0 },
	{ "A-V-AREA-REGION-LIST", 0 },
	{ "A-V-AREA-REGION-SIZE", 0 },
	{ "A-V-AREA-MAXIMUM-SIZE", 0 },
	{ "A-V-AREA-SWAP-RECOMMENDATIONS", 0 },
	{ "A-V-GC-TABLE-AREA", 0 },
	{ "A-V-SUPPORT-ENTRY-VECTOR", 0 },
	{ "A-V-CONSTANTS-AREA", 0 },
	{ "A-V-EXTRA-PDL-AREA", 0 },
	{ "A-V-MICRO-CODE-ENTRY-AREA", 0 },
	{ "A-V-MICRO-CODE-ENTRY-NAME-AREA", 0 },
	{ "A-V-MICRO-CODE-ENTRY-ARGS-INFO-AREA", 0 },
	{ "A-V-MICRO-CODE-ENTRY-MAX-PDL-USAGE", 0 },
	{ "A-V-MICRO-CODE-ENTRY-ARGLIST-AREA", 0 },
	{ "A-V-MICRO-CODE-SYMBOL-NAME-AREA", 0 },
	{ "A-V-LINEAR-PDL-AREA", 0 },
	{ "A-V-LINEAR-BIND-PDL-AREA", 0 },
	{ "A-V-INIT-LIST-AREA", 0 },
	{ "A-V-FIRST-UNFIXED-AREA", 0 },
	{ (char *)0, 0 }
};

struct {
	char *name;
	unsigned int a;
	unsigned int v;
} sv[] = {
	{ "A-INITIAL-FEF", 0 },
	{ "A-QTRSTKG", 0 },
	{ "A-QCSTKG", 0 },
	{ "A-QISTKG", 0 },
	{ (char *)0, 0 }
};

int lodfd, swapfd;
int partoff;
int bnum = -1;
unsigned int buf[256];

unsigned int
read_virt(int fd, int addr)
{
	int b;
	off_t offset, ret;

	addr &= 077777777;

	b = addr / 256;

	offset = (b + partoff) * (256*4);

	if (b != bnum) {
		bnum = b;

		if (0) printf("block %d(10)\n", b);

		ret = lseek(fd, offset, SEEK_SET);
		if (ret != offset) {
			perror("seek");
		}

		ret = read(fd, buf, 256*4);
		if (ret != 256*4) {
		}
	}

	return buf[ addr % 256 ];
}

unsigned int
vr(int addr)
{
	return read_virt(lodfd, addr);
}

unsigned int
swap_vr(int addr)
{
	return read_virt(swapfd, addr);
}

void
set_swap(void)
{
	bnum = -1;

	/* nice hack, eh?  swap starts @ block 0524 - see diskmaker.c */
	partoff = 0524;
}

void
set_lod1(void)
{
	bnum = -1;

	/* nice hack, eh?  swap starts @ block 0524 - see diskmaker.c */
	partoff = 021210;
}

unsigned int
_show(int fd, int a, int cr)
{
	unsigned int v;
	v = read_virt(fd, a);
	printf("%011o %011o", a, v);
	if (cr) printf("\n");
	return v;
}

unsigned int
_get(int fd, int a)
{
	unsigned int v;
	v = read_virt(fd, a);
	return v;
}

unsigned int
get(int a)
{
	return _get(lodfd, a);
}

unsigned int
show(int a, int cr)
{
	return _show(lodfd, a, cr);
}

unsigned int
swap_show(int a, int cr)
{
	return _show(swapfd, a, cr);
}

void
showstr(int a, int cr)
{
	int t, i, j;
	unsigned int n;
	char s[256];

	t = get(a) & 0xff;
	j = 0;
	for (i = 0; i < t; i += 4) {
		n = get(a+1+(i/4));
		s[j++] = n >> 0;
		s[j++] = n >> 8;
		s[j++] = n >> 16;
		s[j++] = n >> 24;
	}

	s[t] = 0;
	printf("'%s' ", s);
	if (cr) printf("\n");
}

char *op_names[16] = {
	"CALL",
	"CALL0",
	"MOVE",
	"CAR",
	"CDR",
	"CADR",
	"CDDR",
	"CDAR",
	"CAAR",
	"ND1",
	"ND2",
	"ND3",
	"BRANCH",
	"MISC",
	"16 UNUSED",
	"17 UNUSED"
};

char *reg_names[] = {
	"FEF",
	"FEF+100",
	"FEF+200", 
	"FEF+300",
	"CONSTANTS PAGE", 
	"LOCAL BLOCK",
	"ARG POINTER",
	"PDL"
};

char *dest_names[] = {
	"IGNORE",
	"TO STACK",
	"TO NEXT",
	"TO LAST",
	"TO RETURN",
	"TO NEXT QUOTE=1",
	"TO LAST QUOTE=1",
	"TO NEXT LIST",
	"D-MICRO, POPJ",
	"illegal",
	"illegal",
	"illegal",
	"illegal",
	"illegal",
	"illegal",
	"illegal"
};

char *branch_names[] = {
	"BRALW",
	"branch-on-nil",
	"branch-on-not-nil",
	"branch-nil,pop-if-not",
	"branch-not-nil,pop-if",
	"branch-on-atom",
	"branch-on-non-atom",
	"illegal"
};

char *nd1_names[] = {
	"ILLOP",
	"QIADD",
	"QISUB",
	"QIMUL",
	"QIDIV",
	"QIAND",
	"QIXOR",
	"QIIOR"
};

char *nd2_names[] = {
	"QIEQL",
	"QIGRP",
	"QILSP",
	"QIEQ",
	"QISCDR",
	"QISCDDR",
	"QISP1",
	"QISM1"
};

char *nd3_names[] = {
	"QIBND",
	"QIBNDN",
	"QIBNDP",
	"QISETN",
	"QISETZ",
	"QIPSHE",
	"QIMVM",
	"QIPOP"
};

#if 0
struct {
	char *name;
	int value;
} misc_inst[] = {
{ "(CAR . M-CAR)", 242 },
{ "(CDR . M-CDR)", 243 },
{ "(CAAR . M-CAAR)", 244 },
{ "(CADR . M-CADR)", 245 },
{ "(CDAR . M-CDAR)", 246 },
{ "(CDDR . M-CDDR)", 247 },
{ "CAAAR", 250 },
{ "CAADR", 251 },
{ "CADAR", 252 },
{ "CADDR", 253 },
{ "CDAAR", 254 },
{ "CDADR", 255 },
{ "CDDAR", 256 },
{ "CDDDR", 257 },
{ "CAAAAR", 260 },
{ "CAAADR", 261 },
{ "CAADAR", 262 },
{ "CAADDR", 263 },
{ "CADAAR", 264 },
{ "CADADR", 265", },
{ "CADDAR", 266 },
{ "CADDDR", 267 },
{ "CDAAAR", 270 },
{ "CDAADR", 271 },
{ "CDADAR", 272 },
{ "CDADDR", 273 },
{ "CDDAAR", 274 },
{ "CDDADR", 275 },
{ "CDDDAR", 276 },
{ "CDDDDR", 277 },
{ "%LOAD-FROM-HIGHER-CONTEXT", 300
{ "%LOCATE-IN-HIGHER-CONTEXT", 301
{ "%STORE-IN-HIGHER-CONTEXT 302
{ "%DATA-TYPE 303 },
{ "%POINTER 304 },
;305-307 FREE
{ "%MAKE-POINTER 310
{ "%SPREAD 311
{ "%P-STORE-CONTENTS 312
{ "%LOGLDB 313
{ "%LOGDPB 314
{ "LDB 315
{ "DPB 316
{ "%P-STORE-TAG-AND-POINTER 317
{ "GET 320
{ "GETL 321
{ "ASSQ 322
{ "LAST 323
{ "LENGTH 324
{ "1+ 325
{ "1- 326
{ "RPLACA 327
{ "RPLACD 330
{ "ZEROP 331
{ "SET 332
{ "FIXP 333 },
{ "FLOATP 334 },
{ "EQUAL 335
{ "STORE 336
{ "XSTORE 337
{ "FALSE 340
{ "TRUE 341
{ "NOT 342 },
{ "(NULL . NOT)", 342 },
{ "ATOM 343
{ "ODDP 344
{ "EVENP 345
{ "%HALT 346
{ "GET-PNAME 347
{ "LSH 350
{ "ROT 351
{ "*BOOLE 352
{ "NUMBERP 353 },
{ "PLUSP 354
{ "MINUSP 355
{ "\ 356
{ "MINUS 357
{ "PRINT-NAME-CELL-LOCATION 360
{ "VALUE-CELL-LOCATION 361
{ "FUNCTION-CELL-LOCATION 362
{ "PROPERTY-CELL-LOCATION 363
{ "NCONS 364 },
{ "NCONS-IN-AREA 365
{ "CONS 366
{ "CONS-IN-AREA 367
{ "XCONS 370
{ "XCONS-IN-AREA 371
{ "%SPREAD-N 372
{ "SYMEVAL 373
{ "POP-M-FROM-UNDER-N 374
{ "%OLD-MAKE-LIST 375
{ "%CALL-MULT-VALUE 376
{ "%CALL0-MULT-VALUE 377
{ "%RETURN-2 400
{ "%RETURN-3 401
{ "%RETURN-N 402
{ "RETURN-NEXT-VALUE 403
{ "RETURN-LIST 404
{ "UNBIND-TO-INDEX-UNDER-N 405
{ "BIND 406
{ "%MAKE-LEXICAL-CLOSURE 407
{ "MEMQ 410
{ "(INTERNAL-< . M-<) 411
{ "(INTERNAL-> . M->) 412
{ "(= . M-=) 413
{ "CHAR-EQUAL 414 (CH1 CH2) T)
{ "%STRING-SEARCH-CHAR 415 (CHAR STRING START END) T)
{ "%STRING-EQUAL 416 (STRING1 INDEX1 STRING2 INDEX2 COUNT) T)
{ "NTH 417 (N LIST) T)
{ "NTHCDR 420 (N LIST) T)
{ "(*PLUS . M-+) 421 (NUM1 NUM2) T)
{ "(*DIF . M--) 422 (NUM1 NUM2) T)
{ "(*TIMES . M-*) 423 (NUM1 NUM2) T)
{ "(*QUO . M-//) 424 (NUM1 NUM2) T)
{ "(*LOGAND . M-LOGAND) 425 (NUM1 NUM2) T)
{ "(*LOGXOR . M-LOGXOR) 426 (NUM1 NUM2) T)
{ "(*LOGIOR . M-LOGIOR) 427 (NUM1 NUM2) T)
{ "ARRAY-LEADER 430 (ARRAY INDEX) T)
{ "STORE-ARRAY-LEADER 431 (X ARRAY INDEX) T)
{ "GET-LIST-POINTER-INTO-ARRAY 432 (ARRAY) T)
{ "ARRAY-PUSH 433 (ARRAY X) T)
{ "APPLY 434 (FN ARGS) T)
{ "%MAKE-LIST 435 (INITIAL-VALUE AREA LENGTH) T)
{ "LIST 436 (&REST ELEMENTS) T T)
{ "LIST* 437 (FIRST &REST ELEMENTS) T T)   ;{ "(&REST ELEMENTS LAST){ "
{ "LIST-IN-AREA 440 (AREA &REST ELEMENTS) T T)
{ "LIST*-IN-AREA 441 (AREA FIRST &REST ELEMENTS) T T)   ;{ "(AREA &REST ELEMENTS LAST){ "
{ "%P-FLAG-BIT 442 (POINTER) T)
{ "%P-CDR-CODE 443 (POINTER) T)
{ "%P-DATA-TYPE 444 (POINTER) T)
{ "%P-POINTER 445 (POINTER) T)
{ "%PAGE-TRACE 446 (TABLE) T)
{ "%P-STORE-FLAG-BIT 447 (POINTER FLAG-BIT) T)
{ "%P-STORE-CDR-CODE 450 (POINTER CDR-CODE) T)
{ "%P-STORE-DATA-TYPE 451 (POINTER DATA-TYPE) T)
{ "%P-STORE-POINTER 452 (POINTER POINTER) T)
;453-455 FREE
{ "%CATCH-OPEN 456 },
{ "%CATCH-OPEN-MV 457 },
;461, 462 FREE
{ "%FEXPR-CALL 462 },
{ "%FEXPR-CALL-MV 463 },
{ "%LEXPR-CALL 464 },
{ "%LEXPR-CALL-MV 465 },
{ "*CATCH 466 (TAG &REST FORMS) T T)
{ "%BLT 467 (FROM-ADDRESS TO-ADDRESS COUNT INCREMENT) T)
{ "*THROW 470 (TAG VALUE) T)
{ "%XBUS-WRITE-SYNC 471 (IO-ADDR WORD DELAY SYNC-LOC SYNC-MASK SYNC-VAL) T)
{ "%P-LDB 472 (PPSS POINTER) T)
{ "%P-DPB 473 (VALUE PPSS POINTER) T)
{ "MASK-FIELD 474 (PPSS FIXNUM) T)
{ "%P-MASK-FIELD 475  (PPSS POINTER) T)
{ "DEPOSIT-FIELD 476 (VALUE PPSS FIXNUM) T)
{ "%P-DEPOSIT-FIELD 477 (VALUE PPSS POINTER) T)
{ "COPY-ARRAY-CONTENTS 500 (FROM TO) T)
{ "COPY-ARRAY-CONTENTS-AND-LEADER 501 (FROM TO) T)
{ "%FUNCTION-INSIDE-SELF 502 () T)
{ "ARRAY-HAS-LEADER-P 503 (ARRAY) T)
{ "COPY-ARRAY-PORTION 504 (FROM-ARRAY FROM-START FROM-END TO-ARRAY TO-START TO-END) T)
{ "FIND-POSITION-IN-LIST 505 (X LIST) T)
;{ "FIND-POSITION-IN-LIST-EQUAL 506 )
{ "G-L-P 507 (ARRAY) T)
{ "FIND-POSITION-IN-VECTOR 510 (X LIST) NIL)
;{ "FIND-POSITION-IN-VECTOR-EQUAL 511 )
{ "AR-1 512 (ARRAY SUB) T)
{ "AR-2 513 (ARRAY SUB1 SUB2) T)
{ "AR-3 514 (ARRAY SUB1 SUB2 SUB3) T)
{ "AS-1 515 (VALUE ARRAY SUB) T)
{ "AS-2 516 (VALUE ARRAY SUB1 SUB2) T)
{ "AS-3 517 (VALUE ARRAY SUB1 SUB2 SUB3) T)
{ "%INSTANCE-REF 520 (INSTANCE INDEX) T)
{ "%INSTANCE-LOC 521 (INSTANCE INDEX) T)
{ "%INSTANCE-SET 522 (VAL INSTANCE INDEX) T)
{ "%BINDING-INSTANCES 523 (LIST-OF-SYMBOLS) T)
{ "%INTERNAL-VALUE-CELL 524 (SYMBOL) T)
{ "%USING-BINDING-INSTANCES 525 (BINDING-INSTANCES) T)
{ "%GC-CONS-WORK 526 (NQS) T)
{ "%P-CONTENTS-OFFSET 527 (POINTER OFFSET) T)
{ "%DISK-RESTORE 530 (PARTITION-HIGH-16-BITS LOW-16-BITS) T)
{ "%DISK-SAVE 531 (MAIN-MEMORY-SIZE PARTITION-HIGH-16-BITS LOW-16-BITS) T)
{ "%ARGS-INFO 532 (FUNCTION) T)
{ "%OPEN-CALL-BLOCK 533 (FUNCTION ADI-PAIRS DESTINATION) NIL)
{ "%PUSH 534 (X) NIL)
{ "%ACTIVATE-OPEN-CALL-BLOCK 535 () NIL)
{ "%ASSURE-PDL-ROOM 536 (ROOM) NIL)
{ "STACK-GROUP-RETURN 537 },
;{ "%STACK-GROUP-RETURN-MULTI 540 )
;Perhaps the next one should be flushed.
{ "%MAKE-STACK-LIST 541 (N) NIL)
{ "STACK-GROUP-RESUME 542 (SG X) T)
{ "%CALL-MULT-VALUE-LIST 543 },
{ "%CALL0-MULT-VALUE-LIST 544 },
{ "%GC-SCAV-RESET 545 (REGION) T)
{ "%P-STORE-CONTENTS-OFFSET 546 (X POINTER OFFSET) T)
{ "%GC-FREE-REGION 547 (REGION) T)
{ "%GC-FLIP 550 (REGION) T)
{ "ARRAY-LENGTH 551 (ARRAY) T)
{ "ARRAY-ACTIVE-LENGTH 552 (ARRAY) T)
{ "%COMPUTE-PAGE-HASH 553 (ADDR) T)
{ "GET-LOCATIVE-POINTER-INTO-ARRAY 554 (ARRAY-REF) T)
{ "%UNIBUS-READ 555 (UNIBUS-ADDR) T)
{ "%UNIBUS-WRITE 556 (UNIBUS-ADDR WORD) T)
{ "%GC-SCAVENGE 557 (WORK-UNITS) T)
{ "%CHAOS-WAKEUP 560 () T)
{ "%AREA-NUMBER 561 },
{ "*MAX 562 (NUM1 NUM2) T)
{ "*MIN 563 (NUM1 NUM2) T)
{ "CLOSURE 565 (SYMBOL-LIST FUNCTION) T)
;{ "DOWNWARD-CLOSURE 566 (SYMBOL-LIST FUNCTION) T)
{ "LISTP 567 },
{ "NLISTP 570 },
{ "SYMBOLP 571 },
{ "NSYMBOLP 572 },
{ "ARRAYP 573 },
{ "FBOUNDP 574 
{ "STRINGP 575 },
{ "BOUNDP 576 (SYMBOL) T)
{ "INTERNAL-\\ 577
{ "FSYMEVAL 600
{ "AP-1 601
{ "AP-2 602
{ "AP-3 603
{ "AP-LEADER 604
{ "%P-LDB-OFFSET 605
{ "%P-DPB-OFFSET 606
{ "%P-MASK-FIELD-OFFSET 607
{ "%P-DEPOSIT-FIELD-OFFSET 610
{ "%MULTIPLY-FRACTIONS 611
{ "%DIVIDE-DOUBLE 612
{ "%REMAINDER-DOUBLE 613
{ "HAULONG 614
{ "%ALLOCATE-AND-INITIALIZE 615
{ "%ALLOCATE-AND-INITIALIZE-ARRAY 616
{ "%MAKE-POINTER-OFFSET 617
{ "^ 620
{ "%CHANGE-PAGE-STATUS 621
{ "%CREATE-PHYSICAL-PAGE 622
{ "%DELETE-PHYSICAL-PAGE 623
{ "%24-BIT-PLUS 624
{ "%24-BIT-DIFFERENCE 625
{ "%24-BIT-TIMES 626
{ "ABS 627
{ "%POINTER-DIFFERENCE 630
{ "%P-CONTENTS-AS-LOCATIVE 631
{ "%P-CONTENTS-AS-LOCATIVE-OFFSET 632
{ "(EQ . M-EQ) 633
{ "%STORE-CONDITIONAL 634
{ "%STACK-FRAME-POINTER 635
{ "*UNWIND-STACK 636
{ "%XBUS-READ 637
{ "%XBUS-WRITE 640
{ "PACKAGE-CELL-LOCATION 641
{ "MOVE-PDL-TOP 642
{ "SHRINK-PDL-SAVE-TOP 643
{ "SPECIAL-PDL-INDEX 644
{ "UNBIND-TO-INDEX 645
{ "UNBIND-TO-INDEX-MOVE 646
{ "FIX 647
{ "FLOAT 650
{ "SMALL-FLOAT 651
{ "%FLOAT-DOUBLE 652
{ "BIGNUM-TO-ARRAY 653
{ "ARRAY-TO-BIGNUM 654
{ "%UNWIND-PROTECT-CONTINUE 655
{ "%WRITE-INTERNAL-PROCESSOR-MEMORIES 656
{ "%PAGE-STATUS 657
{ "%REGION-NUMBER 660
{ "%FIND-STRUCTURE-HEADER 661
{ "%STRUCTURE-BOXED-SIZE 662
{ "%STRUCTURE-TOTAL-SIZE 663
{ "%MAKE-REGION 664
{ "BITBLT 665
{ "%DISK-OP 666
{ "%PHYSICAL-ADDRESS 667
{ "POP-OPEN-CALL 670
{ "%BEEP 671
{ "%FIND-STRUCTURE-LEADER 672
{ "BPT 673
{ "%FINDCORE 674 () T)
{ "%PAGE-IN 675
{ "ASH 676
{ "%MAKE-EXPLICIT-STACK-LIST 677
{ "%DRAW-CHAR 700
{ "%DRAW-RECTANGLE 701
{ "%DRAW-LINE 702
{ "%DRAW-TRIANGLE 703
{ "%COLOR-TRANSFORM 704

{ "%RECORD-EVENT 705
{ "%AOS-TRIANGLE 706
{ "%SET-MOUSE-SCREEN 707 
{ "%OPEN-MOUSE-CURSOR 710
{ "%ether-wakeup 711
{ "%checksum-pup 712
{ "%decode-pup 713
#endif


void
disass(unsigned int loc, int even, unsigned int inst)
{
	int op, dest, reg, delta;
	int to;
	unsigned int nlc;

	op = (inst >> 011) & 017;
	dest = (inst >> 015) & 07;
	reg = (inst >> 6) & 07;
	delta = (inst >> 0) & 077;
	printf("%011o%c %06o %s ", loc, even ? 'e':'o', inst, op_names[op]);

	switch (op) {
	case 0: /* call */
		printf("reg %s, ", reg_names[reg]);
		printf("dest %s, ", dest_names[dest]);
		printf("delta %o ", delta);

//		nlc = (loc*2 + (even?0:1)) + delta;
//		printf("+%o; %o%c ",
//		       delta, nlc/2, (nlc & 1) ? 'o' : 'e');

		break;
	case 2: /* move */
	case 3:
	case 4:
	case 5:
		printf("reg %s, ", reg_names[reg]);
		printf("dest %s, ", dest_names[dest]);
		printf("delta %o ", delta);
		break;
	case 011:
		printf("%s ", nd1_names[dest]);
		break;
	case 012:
		printf("%s ", nd2_names[dest]);
		break;
	case 013:
		printf("%s ", nd3_names[dest]);
		break;
	case 014: /* branch */
		printf("type %s, ", branch_names[dest]);
		to = (inst & 03777) << 1;
		to |= (inst & 0x8000) ? 1 : 0;

		if (inst & 0400) {
			to = inst & 01777;
			to |= 03000;
			to |= ~01777;
		}

		nlc = (loc*2 + (even?0:1)) + to;

		if (to > 0) {
			printf("+%o; %o%c ",
			       to, nlc/2, (nlc & 1) ? 'o' : 'e');
		} else {
			printf("-%o; %o%c ",
			       -to, nlc/2, (nlc & 1) ? 'o' : 'e');
		}
		break;
	}

	printf("\n");
}

main(int argc, char *argv[])
{
	unsigned int com;
	int i, n;

	if (argc < 2) {
		exit(1);
	}

	/* raw load band file */
	lodfd = open(argv[1], O_RDONLY);
	if (lodfd < 0) {
		perror(argv[1]);
		exit(1);
	}

	/* optional full disk image (to check swap) */
	if (argc > 2) {
		swapfd = open(argv[2], O_RDONLY);
		if (swapfd < 0) {
			perror(argv[2]);
			exit(1);
		}
	}

	/* %SYS-COM-AREA-ORIGIN-PNTR */
	com = show(0400, 1);

#if 0
	for (i = 0; cv[i].name; i++) {
		printf("%s ", cv[i].name);
		cv[i].a = com+i;
		cv[i].v = show(cv[i].a, 0);
		printf("; ");
		show(cv[i].v, 1);
	}
	printf("\n");
#endif

#if 0
	printf("scratch-pad\n");
	for (i = 0; sv[i].name; i++) {
		printf("%s ", sv[i].name);
		sv[i].a = 01000+i;
		sv[i].v = show(sv[i].a, 0);
		printf("; ");
		show(sv[i].v, 1);
	}

	{
		unsigned int v, pc, n, o;
		int i, j;
		unsigned short ib[256];

		v = show(sv[0].a, 0);
		pc = show(v, 1);

		n = show(pc, 1);
		o = n & 0377;
		printf("offset %o\n", o);

		j = 0;
		for (i = 0; i < 64; i++) {
			unsigned int loc, inst;
			unsigned int a;
			loc = pc+i;
			inst = get(loc);

			ib[j++] = inst;
			ib[j++] = inst >> 16;

			if (i < o/2)
			{
				show(loc, 1);
			}

			if (i == 2) {
				printf(" "); v = show(inst, 0);
				printf(" "); showstr(v, 1);
			}
		}

		for (i = o; i < o+10; i++) {
			unsigned int loc;
			loc = pc+i/2;
			disass(loc, (i%2) ? 0 : 1, ib[i]);
		}
	}

	{
		unsigned int v, pc, n, o;
		int i, j;
		unsigned short ib[512];

		printf("\n");

		pc = show(01722706, 1);
		pc = show(pc, 1);

		n = show(pc, 1);
		o = n & 0777;
		printf("offset %o\n", o);

		j = 0;
		for (i = 0; i < 256; i++) {
			unsigned int loc, inst;
			loc = pc+i;
			inst = get(loc);

			ib[j++] = inst;
			ib[j++] = inst >> 16;

			if (i < o/2)
			{
				show(loc, 1);
			}

			if (i == 2) {
				printf(" "); v = show(inst, 0);
				printf(" "); showstr(v, 1);
			}
		}

		for (i = o; i < o+20; i++) {
			unsigned int loc;
			loc = pc+i/2;
			disass(loc, (i%2) ? 0 : 1, ib[i]);
		}
	}

#endif

#if 1
	{
		unsigned int v, pc, n, o;
		int i, j;
		unsigned short ib[512];

		printf("\n");

		sscanf(argv[3], "%o", &pc);
		pc >>= 2;
///		pc = 011010066774 >> 2;
//		pc = 011047720640 >> 2;

		for (i = 0; i < 512; i--) {
			int tag;
			n = show(pc, 1);
			tag = (n >> 24) & 077;
			if (tag == 7) break;
			pc--;
		}

		n = show(pc, 1);
		o = n & 0777;
		printf("offset %o\n", o);

		j = 0;
		for (i = 0; i < 256; i++) {
			unsigned int loc, inst;
			loc = pc+i;
			inst = get(loc);

			ib[j++] = inst;
			ib[j++] = inst >> 16;

			if (i < o/2)
			{
				show(loc, 1);
			}

			if (i == 2) {
				int tag;

				printf(" "); v = show(inst, 0);

				tag = (v >> 24) & 077;
				if (0) printf("tag %o\n", tag);

				if (tag == 3) {
					printf("\n");
					printf(" "); v = show(v, 0);
					tag = (v >> 24) & 077;
				}
				if (tag == 4) {
					printf(" "); showstr(v, 1);
				}
			}
		}

		for (i = o; i < o+128; i++) {
			unsigned int loc;
			loc = pc+i/2;
			disass(loc, (i%2) ? 0 : 1, ib[i]);
		}
	}
#endif

#if 0
	{
		int i;
		unsigned int a;

		printf("sg\n");
		a = sv[3].v & 0x00ffffff;
		printf("a %o\n", a);

		for (i = 10; i >= 0; i--) {
			char b[16];
			sprintf(b, "%d", -i);
			show(a-i, 1);
		}
	}
#endif

#if 0
	if (swapfd)
	{
		int i;
		unsigned int a;

		a = sv[3].v & 0x00ffffff;
		printf("a %o\n", a);

		set_swap();
		printf("sg - swap\n");

		for (i = 10; i >= 0; i--) {
			char b[16];
			sprintf(b, "%d", -i);
			swap_show(a-i, 1);
		}

		set_lod1();
		printf("sg - lod1\n");

		for (i = 10; i >= 0; i--) {
			char b[16];
			sprintf(b, "%d", -i);
			swap_show(a-i, 1);
		}
	}
#endif


}
