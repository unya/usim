/*
 * main.c
 * $Id$
 */
#include <stdio.h>

int show_video_flag;

extern char *optarg;

main(int argc, char *argv[])
{
	int c;

	printf("CADR emulator v0.0\n");

	show_video_flag = 1;

	while ((c = getopt(argc, argv, "n")) != -1) {
		switch (c) {
		case 'n':
			show_video_flag = 0;
			break;
		}
	}

	if (show_video_flag) {
		display_init();
		display_poll();
	}

	read_prom_files();

	read_sym_files();

	disk_init("disk.img");

	iob_init();

//	show_prom();
//	disassemble_prom();

	run();

	if (show_video_flag) {
		while (1) {
			display_poll();
		}
	}

	exit(0);
}
