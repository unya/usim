main()
{
	int xbusl, xbush, unibusl, unibush, xbusiol, xbusioh;

	printf("CADR memory map:\n");
	xbusl = 0;
	xbush = 016777777;
	unibusl = 017400000;
	unibush = 017777777;
	xbusiol = 017000000;
	xbusioh = 017377777;
	printf("22-bit      %011o\n", (1 << 22)-1);
	printf("xbus memory %011o-%011o, pn %05o-%05o\n",
	       xbusl, xbush, xbusl >> 8, xbush>> 8);
	printf("xbus io     %011o-%011o, pn %05o-%05o\n",
	       xbusiol, xbusioh, xbusiol >> 8, xbusioh >> 8);
	printf("unibus io   %011o-%011o, pn %05o-%05o\n",
	       unibusl, unibush, unibusl >> 8, unibush >> 8);
}
