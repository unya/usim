/*
 * main.c
 *
 * CADR simulator
 * main;
 * 
 * $Id$
 */

#include <stdio.h>
#include <sys/time.h>
#include <signal.h>

#include "ucode.h"
#include "config.h"

int show_video_flag;
extern unsigned long cycles;
extern int run_ucode_flag;

struct timeval tv1;

/*
 * simple wall clock timing to get a notion of the basic cycle time
 */

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

void
sigint_handler(int arg)
{
	run_ucode_flag = 0;
}

void
signal_init(void)
{
	signal(SIGINT, sigint_handler);
}

void
signal_shutdown(void)
{
    signal(SIGINT, SIG_DFL);
    fflush(stdout);
}


void
usage(void)
{
	printf("usage:\n");
	exit(1);
}

extern char *optarg;
extern int trace;
extern int trace_mcr_labels_flag;

main(int argc, char *argv[])
{
	int c;

	printf("CADR emulator v0.2\n");

	show_video_flag = 1;

	while ((c = getopt(argc, argv, "b:c:C:l:np:q:tT:s")) != -1) {
		switch (c) {
		case 'n':
			show_video_flag = 0;
			break;
		case 'b':
			breakpoint_set_mcr(optarg);
			break;
		case 'c':
			max_cycles = atol(optarg);
			break;
		case 'C':
			max_trace_cycles = atol(optarg);
			break;
		case 'l':
			tracelabel_set_mcr(optarg);
			break;
		case 'p':
			breakpoint_set_prom(optarg);
			break;
		case 'q':
			breakpoint_set_count(atoi(optarg));
			break;
		case 't':
			trace = 1;
			break;
		case 'T':
			switch (optarg[0]) {
			case 'd': trace_disk_flag = 1; break;
			case 'i': trace_int_flag = 1; break;
			case 'o': trace_io_flag = 1; break;
			case 'p': trace_prom_flag = 1; break;
			case 'c': trace_mcr_flag = 1; break;
			case 'm': trace_mcr_labels_flag = 1; break;
			case 'l': trace_lod_labels_flag = 1; break;
			}
			break;
		case 's':
			stop_after_prom_flag = 1;
			break;
		default:
			usage();
		}
	}

	if (show_video_flag) {
		display_init();
		display_poll();
	}

	disk_init( config_get_disk_filename() );

	read_prom_files();

	read_sym_files();

	iob_init();

#if 0
	show_prom();
	disassemble_prom();
#endif

	signal_init();

	run();

	signal_shutdown();

	if (show_video_flag) {
		while (1) {
			display_poll();
		}
	}

	exit(0);
}
