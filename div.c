/*
;;; DIVIDE SUBROUTINE
;   DIVIDEND IN M-1, DIVISOR IN M-2
;   QUOTIENT IN Q-R, REMAINDER IN M-1, CLOBBERS A-TEM1

DIV	(JUMP-GREATER-OR-EQUAL-XCT-NEXT M-1 A-ZERO DIV1)
       ((A-TEM1 Q-R) M-1)	;Q GETS MAGNITUDE OF DIVIDEND, A-TEM1 SAVES ORIGINAL
	((Q-R) SUB M-ZERO A-TEM1)
DIV1	((M-1) DIVIDE-FIRST-STEP M-ZERO A-2)
DIV1A	(CALL-IF-BIT-SET (BYTE-FIELD 1 0) Q-R TRAP)	;DIVIDE OVERFLOW

(REPEAT 31. ((M-1) DIVIDE-STEP M-1 A-2))

	((M-1) DIVIDE-LAST-STEP M-1 A-2)
	(JUMP-LESS-OR-EQUAL-XCT-NEXT M-ZERO A-TEM1 DIV2) ;JUMP IF POSITIVE DIVIDEND
       ((M-1) DIVIDE-REMAINDER-CORRECTION-STEP M-1 A-2) ;M-1 GETS MAGNITUDE OF REMAINDER
	((M-1) SUB M-ZERO A-1)		;NEGATIVE DIVIDEND => NEGATIVE REMAINDER
DIV2	((A-TEM1) XOR M-2 A-TEM1)	;IF SIGNS OF DIVIDEND AND DIVISOR ARE DIFFERENT,
	(POPJ-LESS-OR-EQUAL M-ZERO A-TEM1)
	(POPJ-AFTER-NEXT
	 (A-TEM1) Q-R)
       ((Q-R) SUB M-ZERO A-TEM1)	;THEN QUOTIENT IS NEGATIVE
*/

/*
 * unsigned divide simulation; based on CADR h/w & microcode
 */

int show = 1;

void ud(unsigned int dividend,
		     unsigned int divisor,
		     unsigned int *pquotient,
		     unsigned int *premainder )
{
	unsigned int q, m, a, alu, old_q;
	int i;
	int do_sub, do_add;
	int negq, nega;

	q = dividend;
	a = divisor;
	m = 0;
	negq = 0;
	nega = 0;

	printf("(/ 0x%x (%o) 0x%x (%o) ); ", q, q, a, a);

	if ((int)q < 0) {
		q = -(int)q;
		negq = 1;
	}

	if ((int)a < 0) {
		nega = 1;
	}

	if (show) printf("\n");

	alu = m - a;

old_q = q;
	q <<= 1;
	if ((alu & 0x80000000) == 0)
		q |= 1;
//	m = (alu << 1) | ((q & 0x80000000) ? 1 : 0);
	m = (alu << 1) | ((old_q & 0x80000000) ? 1 : 0);

	for (i = 0; i < 32; i++) {

		do_sub = q & 1;

		if (show)
		printf("loop %d, m %08x (%o) alu %08x (%o), q %08x (%o), do_sub %d\n",
		       i, m, m, alu, alu, q, q, do_sub);

		if (do_sub) {
			alu = m - a;
		} else {
			alu = m + a;
		}

old_q = q;
		q <<= 1;
		if ((alu & 0x80000000) == 0)
			q |= 1;

		if (i < 31)
//			m = (alu << 1) | ((q & 0x80000000) ? 1 : 0);
			m = (alu << 1) | ((old_q & 0x80000000) ? 1 : 0);
		else
			m = alu;
	}

	do_sub = q & 1;

	if (nega)
		do_sub = !do_sub;

	if (show)
	printf("remainder; m %08x, alu %08x, q %08x, do_sub %d\n", m, alu, q, do_sub);

	if (do_sub) {
		/* setm */
		printf("setm; ");
	} else {
		alu = m + a;
	}

	if (negq ^ nega) {
		printf("diff; ");
		q = -(int)q;
	}

	printf("done; alu %08x (%o), q %08x (%o) (%d,%d)\n", alu, alu, q, q, alu, q);
}


main()
{
	unsigned int q, r;

	show = 0;
	ud(4, 2, &q, &r);
#if 1
	ud(10, 3, &q, &r);

	ud(8, 2, &q, &r);

	ud(10, 5, &q, &r);

//	ud(100, 3, &q, &r);

//	ud(0503, 021, &q, &r);
	ud(06434, 0503, &q, &r);
	ud(0136, 021, &q, &r);

	ud(0142, 021, &q, &r);

	ud(0x7fffffff, 02, &q, &r);
	ud(-1, 2, &q, &r);

	ud(-6, 2, &q, &r);
//	show=1;
	ud(-6, -2, &q, &r);
	ud(6, 2, &q, &r);
#endif

#if 0
	int i;
	for (i = 0; i < 0200; i++) {
		printf("%o / 021 ", i);
		ud(i, 021, &q, &r);
	}
#endif
}


