/*
 * unsigned divide simulation; based on CADR h/w & microcode
 */

void ud(unsigned int dividend,
		     unsigned int divisor,
		     unsigned int *pquotient,
		     unsigned int *premainder )
{
	int q, m, a, alu;
	int i;
	int do_sub, do_add;

	q = dividend;
	a = divisor;
	m = 0;

	alu = m - a;

	q <<= 1;
	if ((alu & 0x80000000) == 0)
		q |= 1;
	m = (alu << 1) | ((q & 0x80000000) ? 1 : 0);

	for (i = 0; i < 31; i++) {

		do_sub = q & 1;
		printf("loop %d, m %08x alu %08x, q %08x, do_sub %d\n",
		       i, m, alu, q, do_sub);

		if (do_sub) {
			alu = m - a;
		} else {
			alu = m + a;
		}

		q <<= 1;
		if ((alu & 0x80000000) == 0)
			q |= 1;

		if (i < 31)
			m = (alu << 1) | ((q & 0x80000000) ? 1 : 0);
		else
			m = alu;
	}

	do_sub = q & 1;
	printf("remainder; q %08x, do_sub %d\n", q, do_sub);

	if (do_sub) {
		alu = alu;
	} else {
		alu = alu + a;
	}
	
	printf("done; alu %08x, q %08x (%d)\n", alu, q, q);
}


main()
{
	unsigned int q, r;

//	unsigned_divide(0, 3, &q, &r);
//	unsigned_divide(4, 2, &q, &r);
//	ud(4, 2, &q, &r);
//	ud(8, 2, &q, &r);
//	ud(10, 5, &q, &r);
	ud(10, 3, &q, &r);
//	ud(100, 3, &q, &r);
}


