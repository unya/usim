/*
 * main.c
 * $Id$
 */
#include <stdio.h>

main(int argc, char *argv[])
{
	extern int trace;

	read_prom_files();

	read_sym_files();


	disk_init("disk.img");
//	show_prom();

//	disassemble_prom();


	run();

	exit(0);
}
