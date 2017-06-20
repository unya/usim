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

struct {
	char *name;
	int value;
} misc_inst[] = {
	{ "", 0 },
	{ "(CAR . M-CAR)", 0242 },
	{ "(CDR . M-CDR)", 0243 },
	{ "(CAAR . M-CAAR)", 0244 },
	{ "(CADR . M-CADR)", 0245 },
	{ "(CDAR . M-CDAR)", 0246 },
	{ "(CDDR . M-CDDR)", 0247 },
	{ "CAAAR", 0250 },
	{ "CAADR", 0251 },
	{ "CADAR", 0252 },
	{ "CADDR", 0253 },
	{ "CDAAR", 0254 },
	{ "CDADR", 0255 },
	{ "CDDAR", 0256 },
	{ "CDDDR", 0257 },
	{ "CAAAAR", 0260 },
	{ "CAAADR", 0261 },
	{ "CAADAR", 0262 },
	{ "CAADDR", 0263 },
	{ "CADAAR", 0264 },
	{ "CADADR", 0265 },
	{ "CADDAR", 0266 },
	{ "CADDDR", 0267 },
	{ "CDAAAR", 0270 },
	{ "CDAADR", 0271 },
	{ "CDADAR", 0272 },
	{ "CDADDR", 0273 },
	{ "CDDAAR", 0274 },
	{ "CDDADR", 0275 },
	{ "CDDDAR", 0276 },
	{ "CDDDDR", 0277 },
	{ "%LOAD-FROM-HIGHER-CONTEXT", 0300 },
	{ "%LOCATE-IN-HIGHER-CONTEXT", 0301 },
	{ "%STORE-IN-HIGHER-CONTEXT", 0302 },
	{ "%DATA-TYPE", 0303 },
	{ "%POINTER", 0304 },
	/* 305-307 FREE */
	{ "%MAKE-POINTER", 0310 },
	{ "%SPREAD", 0311 },
	{ "%P-STORE-CONTENTS", 0312 },
	{ "%LOGLDB", 0313 },
	{ "%LOGDPB", 0314 },
	{ "LDB", 0315 },
	{ "DPB", 0316 },
	{ "%P-STORE-TAG-AND-POINTER", 0317 },
	{ "GET", 0320 },
	{ "GETL", 0321 },
	{ "ASSQ", 0322 },
	{ "LAST", 0323 },
	{ "LENGTH", 0324 },
	{ "1+", 0325 },
	{ "1-", 0326 },
	{ "RPLACA", 0327 },
	{ "RPLACD", 0330 },
	{ "ZEROP", 0331 },
	{ "SET", 0332 },
	{ "FIXP", 0333 },
	{ "FLOATP", 0334 },
	{ "EQUAL", 0335 },
	{ "STORE", 0336 },
	{ "XSTORE", 0337 },
	{ "FALSE", 0340 },
	{ "TRUE", 0341 },
	{ "NOT", 0342 },
	{ "(NULL . NOT)", 0342 },
	{ "ATOM", 0343 },
	{ "ODDP", 0344 },
	{ "EVENP", 0345 },
	{ "%HALT", 0346 },
	{ "GET-PNAME", 0347 },
	{ "LSH", 0350 },
	{ "ROT", 0351 },
	{ "*BOOLE", 0352 },
	{ "NUMBERP", 0353 },
	{ "PLUSP", 0354 },
	{ "MINUSP", 0355 },
	{ "\\", 0356 },
	{ "MINUS", 0357 },
	{ "PRINT-NAME-CELL-LOCATION", 0360 },
	{ "VALUE-CELL-LOCATION", 0361 },
	{ "FUNCTION-CELL-LOCATION", 0362 },
	{ "PROPERTY-CELL-LOCATION", 0363 },
	{ "NCONS", 0364 },
	{ "NCONS-IN-AREA", 0365 },
	{ "CONS", 0366 },
	{ "CONS-IN-AREA", 0367 },
	{ "XCONS", 0370 },
	{ "XCONS-IN-AREA", 0371 },
	{ "%SPREAD-N", 0372 },
	{ "SYMEVAL", 0373 },
	{ "POP-M-FROM-UNDER-N", 0374 },
	{ "%OLD-MAKE-LIST", 0375 },
	{ "%CALL-MULT-VALUE", 0376 },
	{ "%CALL0-MULT-VALUE", 0377 },
	{ "%RETURN-2", 0400 },
	{ "%RETURN-3", 0401 },
	{ "%RETURN-N", 0402 },
	{ "RETURN-NEXT-VALUE", 0403 },
	{ "RETURN-LIST", 0404 },
	{ "UNBIND-TO-INDEX-UNDER-N", 0405 },
	{ "BIND", 0406 },
	{ "%MAKE-LEXICAL-CLOSURE", 0407 },
	{ "MEMQ", 0410 },
	{ "(INTERNAL-< . M-<)", 0411 },
	{ "(INTERNAL-> . M->)", 0412 },
	{ "(= . M-=)", 0413 },
	{ "CHAR-EQUAL", 0414 },
	{ "%STRING-SEARCH-CHAR", 0415 },
	{ "%STRING-EQUAL", 0416 },
	{ "NTH", 0417 },
	{ "NTHCDR", 0420 },
	{ "(*PLUS . M-+)", 0421  },
	{ "(*DIF . M--)", 0422  },
	{ "(*TIMES . M-*)", 0423  },
	{ "(*QUO . M-//)", 0424  },
	{ "(*LOGAND . M-LOGAND)", 0425  },
	{ "(*LOGXOR . M-LOGXOR)", 0426  },
	{ "(*LOGIOR . M-LOGIOR)", 0427  },
	{ "ARRAY-LEADER", 0430 },
	{ "STORE-ARRAY-LEADER", 0431 },
	{ "GET-LIST-POINTER-INTO-ARRAY", 0432 },
	{ "ARRAY-PUSH", 0433 },
	{ "APPLY", 0434 },
	{ "%MAKE-LIST", 0435 },
	{ "LIST", 0436 },
	{ "LIST*", 0437 },
	{ "LIST-IN-AREA", 0440 },
	{ "LIST*-IN-AREA", 0441 },
	{ "%P-FLAG-BIT", 0442 },
	{ "%P-CDR-CODE", 0443 },
	{ "%P-DATA-TYPE", 0444 },
	{ "%P-POINTER", 0445 },
	{ "%PAGE-TRACE", 0446 },
	{ "%P-STORE-FLAG-BIT", 0447 },
	{ "%P-STORE-CDR-CODE", 0450 },
	{ "%P-STORE-DATA-TYPE", 0451 },
	{ "%P-STORE-POINTER", 0452 },
	/* 453-455 FREE */
	{ "%CATCH-OPEN", 0456 },
	{ "%CATCH-OPEN-MV", 0457 },
	/* 461, 0462 FREE */
	{ "%FEXPR-CALL", 0462 },
	{ "%FEXPR-CALL-MV", 0463 },
	{ "%LEXPR-CALL", 0464 },
	{ "%LEXPR-CALL-MV", 0465 },
	{ "*CATCH", 0466 },
	{ "%BLT", 0467 },
	{ "*THROW", 0470 },
	{ "%XBUS-WRITE-SYNC", 0471 },
	{ "%P-LDB", 0472 },
	{ "%P-DPB", 0473 },
	{ "MASK-FIELD", 0474 },
	{ "%P-MASK-FIELD", 0475},
	{ "DEPOSIT-FIELD", 0476 },
	{ "%P-DEPOSIT-FIELD", 0477 },
	{ "COPY-ARRAY-CONTENTS", 0500 },
	{ "COPY-ARRAY-CONTENTS-AND-LEADER", 0501 },
	{ "%FUNCTION-INSIDE-SELF", 0502 },
	{ "ARRAY-HAS-LEADER-P", 0503 },
	{ "COPY-ARRAY-PORTION", 0504 },
	{ "FIND-POSITION-IN-LIST", 0505 },
	{ "FIND-POSITION-IN-LIST-EQUAL", 0506 },
	{ "G-L-P", 0507 },
	{ "FIND-POSITION-IN-VECTOR", 0510 },
	{ "FIND-POSITION-IN-VECTOR-EQUAL", 0511 },
	{ "AR-1", 0512 },
	{ "AR-2", 0513 },
	{ "AR-3", 0514 },
	{ "AS-1", 0515 },
	{ "AS-2", 0516 },
	{ "AS-3", 0517 },
	{ "%INSTANCE-REF", 0520 },
	{ "%INSTANCE-LOC", 0521 },
	{ "%INSTANCE-SET", 0522 },
	{ "%BINDING-INSTANCES", 0523 },
	{ "%INTERNAL-VALUE-CELL", 0524 },
	{ "%USING-BINDING-INSTANCES", 0525 },
	{ "%GC-CONS-WORK", 0526 },
	{ "%P-CONTENTS-OFFSET", 0527 },
	{ "%DISK-RESTORE", 0530 },
	{ "%DISK-SAVE", 0531 },
	{ "%ARGS-INFO", 0532 },
	{ "%OPEN-CALL-BLOCK", 0533 },
	{ "%PUSH", 0534 },
	{ "%ACTIVATE-OPEN-CALL-BLOCK", 0535 },
	{ "%ASSURE-PDL-ROOM", 0536 },
	{ "STACK-GROUP-RETURN", 0537 },
	{ "%STACK-GROUP-RETURN-MULTI", 0540 },
	{ "%MAKE-STACK-LIST", 0541 },
	{ "STACK-GROUP-RESUME", 0542 },
	{ "%CALL-MULT-VALUE-LIST", 0543 },
	{ "%CALL0-MULT-VALUE-LIST", 0544 },
	{ "%GC-SCAV-RESET", 0545 },
	{ "%P-STORE-CONTENTS-OFFSET", 0546 },
	{ "%GC-FREE-REGION", 0547 },
	{ "%GC-FLIP", 0550 },
	{ "ARRAY-LENGTH", 0551 },
	{ "ARRAY-ACTIVE-LENGTH", 0552 },
	{ "%COMPUTE-PAGE-HASH", 0553 },
	{ "GET-LOCATIVE-POINTER-INTO-ARRAY", 0554 },
	{ "%UNIBUS-READ", 0555 },
	{ "%UNIBUS-WRITE", 0556 },
	{ "%GC-SCAVENGE", 0557 },
	{ "%CHAOS-WAKEUP", 0560 },
	{ "%AREA-NUMBER", 0561 },
	{ "*MAX", 0562 },
	{ "*MIN", 0563 },
	{ "CLOSURE", 0565 },
	{ "DOWNWARD-CLOSURE", 0566 },
	{ "LISTP", 0567 },
	{ "NLISTP", 0570 },
	{ "SYMBOLP", 0571 },
	{ "NSYMBOLP", 0572 },
	{ "ARRAYP", 0573 },
	{ "FBOUNDP", 0574 },
	{ "STRINGP", 0575 },
	{ "BOUNDP", 0576 },
	{ "INTERNAL-\\", 0577 },
	{ "FSYMEVAL", 0600 },
	{ "AP-1", 0601 },
	{ "AP-2", 0602 },
	{ "AP-3", 0603 },
	{ "AP-LEADER", 0604 },
	{ "%P-LDB-OFFSET", 0605 },
	{ "%P-DPB-OFFSET", 0606 },
	{ "%P-MASK-FIELD-OFFSET", 0607 },
	{ "%P-DEPOSIT-FIELD-OFFSET", 0610 },
	{ "%MULTIPLY-FRACTIONS", 0611 },
	{ "%DIVIDE-DOUBLE", 0612 },
	{ "%REMAINDER-DOUBLE", 0613 },
	{ "HAULONG", 0614 },
	{ "%ALLOCATE-AND-INITIALIZE", 0615 },
	{ "%ALLOCATE-AND-INITIALIZE-ARRAY", 0616 },
	{ "%MAKE-POINTER-OFFSET", 0617 },
	{ "^", 0620 },
	{ "%CHANGE-PAGE-STATUS", 0621 },
	{ "%CREATE-PHYSICAL-PAGE", 0622 },
	{ "%DELETE-PHYSICAL-PAGE", 0623 },
	{ "%24-BIT-PLUS", 0624 },
	{ "%24-BIT-DIFFERENCE", 0625 },
	{ "%24-BIT-TIMES", 0626 },
	{ "ABS", 0627 },
	{ "%POINTER-DIFFERENCE", 0630 },
	{ "%P-CONTENTS-AS-LOCATIVE", 0631 },
	{ "%P-CONTENTS-AS-LOCATIVE-OFFSET", 0632 },
	{ "(EQ . M-EQ)", 0633 },
	{ "%STORE-CONDITIONAL", 0634 },
	{ "%STACK-FRAME-POINTER", 0635 },
	{ "*UNWIND-STACK", 0636 },
	{ "%XBUS-READ", 0637 },
	{ "%XBUS-WRITE", 0640 },
	{ "PACKAGE-CELL-LOCATION", 0641 },
	{ "MOVE-PDL-TOP", 0642 },
	{ "SHRINK-PDL-SAVE-TOP", 0643 },
	{ "SPECIAL-PDL-INDEX", 0644 },
	{ "UNBIND-TO-INDEX", 0645 },
	{ "UNBIND-TO-INDEX-MOVE", 0646 },
	{ "FIX", 0647 },
	{ "FLOAT", 0650 },
	{ "SMALL-FLOAT", 0651 },
	{ "%FLOAT-DOUBLE", 0652 },
	{ "BIGNUM-TO-ARRAY", 0653 },
	{ "ARRAY-TO-BIGNUM", 0654 },
	{ "%UNWIND-PROTECT-CONTINUE", 0655 },
	{ "%WRITE-INTERNAL-PROCESSOR-MEMORIES", 0656 },
	{ "%PAGE-STATUS", 0657 },
	{ "%REGION-NUMBER", 0660 },
	{ "%FIND-STRUCTURE-HEADER", 0661 },
	{ "%STRUCTURE-BOXED-SIZE", 0662 },
	{ "%STRUCTURE-TOTAL-SIZE", 0663 },
	{ "%MAKE-REGION", 0664 },
	{ "BITBLT", 0665 },
	{ "%DISK-OP", 0666 },
	{ "%PHYSICAL-ADDRESS", 0667 },
	{ "POP-OPEN-CALL", 0670 },
	{ "%BEEP", 0671 },
	{ "%FIND-STRUCTURE-LEADER", 0672 },
	{ "BPT", 0673 },
	{ "%FINDCORE", 0674 },
	{ "%PAGE-IN", 0675 },
	{ "ASH", 0676 },
	{ "%MAKE-EXPLICIT-STACK-LIST", 0677 },
	{ "%DRAW-CHAR", 0700 },
	{ "%DRAW-RECTANGLE", 0701 },
	{ "%DRAW-LINE", 0702 },
	{ "%DRAW-TRIANGLE", 0703 },
	{ "%COLOR-TRANSFORM", 0704 },
	{ "%RECORD-EVENT", 0705 },
	{ "%AOS-TRIANGLE", 0706 },
	{ "%SET-MOUSE-SCREEN", 0707  },
	{ "%OPEN-MOUSE-CURSOR", 0710 },
	{ "%ether-wakeup", 0711 },
	{ "%checksum-pup", 0712 },
	{ "%decode-pup", 0713 },
	{ (char *)0, 0 }
};

static int misc_inst_vector[1024];
static int misc_inst_vector_setup;

void
disass(unsigned int fefptr, unsigned int loc, int even, unsigned int inst,
       unsigned int width)

{
	int op, dest, reg, delta, adr;
	int to;
	unsigned int nlc;

	if (!misc_inst_vector_setup) {
		int i, index;
		for (i = 0; i < 1024; i++) {
			if (misc_inst[i].name == 0)
				break;
			index = misc_inst[i].value;
			misc_inst_vector[index] = i;
		}
		misc_inst_vector_setup = 1;
	}

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

		{
			unsigned int v, tag;
			v = get(fefptr + delta);
			tag = (v >> width) & 037;
			if (0) printf("(tag%o %o) ", tag, v);
			switch (tag) {
			case 3:
				v = get(v);
				showstr(v, 0);
				break;
			case 4:
				showstr(v, 0);
				break;
			case 027:
				break;
			default:
				v = get(v);
				show_fef_func_name( v , width);
			}
		}
		//		nlc = (loc*2 + (even?0:1)) + delta;
		//		printf("+%o; %o%c ",
		//		       delta, nlc/2, (nlc & 1) ? 'o' : 'e');

		break;
	case 2: /* move */
	case 3: /* car */
	case 4: /* cdr */
	case 5: /* cadr */
		printf("reg %s, ", reg_names[reg]);
		printf("dest %s, ", dest_names[dest]);
		printf("delta %o ", delta);
		break;
	case 011: /* nd1 */
		printf("%s ", nd1_names[dest]);
		break;
	case 012: /* nd2 */
		printf("%s ", nd2_names[dest]);
		break;
	case 013: /* nd3 */
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
	case 015: /* misc */
		adr = inst & 0777;
		if (adr < 1024 && misc_inst_vector[adr]) {
			printf("%s ", misc_inst[ misc_inst_vector[adr] ].name);
		} else {
			printf("%o ", adr);
		}
		printf("dest %s, ", dest_names[dest]);
		break;
	}

	printf("\n");
}
