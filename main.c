/*
 * main.c
 * $Id$
 */
#include <stdio.h>

main(int argc, char *argv[])
{
	read_prom_files();
	disk_init("disk.img");
//	show_prom();
	disassemble_prom();

	run();

	exit(0);
}
