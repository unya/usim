/*
 * main.c
 * $Id$
 */
#include <stdio.h>
#include <sys/time.h>

int show_video_flag;
extern unsigned long cycles;

struct timeval tv1;

void
timing_start()
{
	gettimeofday(&tv1, NULL);
}

void
timing_stop()
{
	struct timeval tv2, td;
	double t, cps;

	gettimeofday(&tv2, NULL);

	if (tv2.tv_usec < tv1.tv_usec) {
		tv2.tv_sec--;
		td.tv_usec = (tv2.tv_usec + 1000 * 1000) - tv1.tv_usec;
	}
	else
		td.tv_usec = tv2.tv_usec - tv1.tv_usec;

	td.tv_sec = tv2.tv_sec - tv1.tv_sec;

	t = (double)td.tv_sec + ((double)td.tv_usec / (1000.0 * 1000.0));

	cps = cycles / t;

	printf("\ncycle timing:\n");

//	printf("%lu %lu ; %lu %lu \n",
//	       tv2.tv_sec, tv1.tv_sec, tv2.tv_usec, tv1.tv_usec);

	printf("%lu cycles in %g seconds, %10.8g cycles/second\n",
	       cycles, t, cps);
	printf("%.0f ns/cycle\n", (t / cycles) * 1000.0 * 1000.0 * 1000.0);
}


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
