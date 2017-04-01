/*
 * main.c
 *
 * CADR simulator
 * main;
 * 
 * $Id$
 */

#include "usim.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <limits.h>

#if defined(__linux__) || defined(OSX) || defined(BSD)
#include <unistd.h>
#include <sys/time.h>
#endif

#include "ucode.h"
#include "config.h"

#ifdef DISPLAY_SDL
#include <SDL.h>
#endif /* DISPLAY_SDL */

#include "Files.h"

int show_video_flag;
int mouse_sync_flag;
int alt_prom_flag;
int dump_state_flag;
int save_state_flag;

/* */
extern int display_init(void);
extern void display_poll(void);
extern int disk_init(const char *filename);
extern int read_prom_files(void);
extern int read_sym_files(void);
extern int iob_init(void);
extern int chaos_init(void);
extern int ether_init(void);
extern void iob_warm_boot_key(void);
extern void run(void);
extern int dcanon(char *cp, int blankok);

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
sighup_handler(int arg)
{
//	char *b = "XMMUL";
//	char *b = "FMPY";
	char *b = "MPY";
	extern int trace_late_set;
	breakpoint_set_mcr(b);
	printf("set breakpoint in %s\n", b);
	trace_late_set = 1;
}

void
signal_init(void)
{
	signal(SIGINT, sigint_handler);
#ifndef _WIN32
	signal(SIGHUP, sighup_handler);
#endif
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
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "-a		use alternate prom file\n");
	fprintf(stderr, "-b <sym-name>	set breakpoint in microcode\n");
	fprintf(stderr, "-c <number>	set max # of microcode cycles to run\n");
	fprintf(stderr, "-C <number>	set max # of traced microcode cycles to run\n");
	fprintf(stderr, "-l <sym-name>	start tracing at symbol\n");
	fprintf(stderr, "-n		run with no SDL video window\n");
	fprintf(stderr, "-p <sym-name>	set breakpoint in prom\n");
	fprintf(stderr, "-q <number>	break after hitting breakpoint n times\n");
#if defined(OSX) || defined(__linux__)
	fprintf(stderr, "-r		map /tree to ../../lisp\n");
#endif
	fprintf(stderr, "-t		turn on microcode tracing\n");
	fprintf(stderr, "-T<flags>	turn on tracing\n");
	fprintf(stderr, "   d - disk\n");
	fprintf(stderr, "   i - interrupts\n");
	fprintf(stderr, "   o - i/o\n");
	fprintf(stderr, "   p - prom\n");
	fprintf(stderr, "   c - microcode\n");
	fprintf(stderr, "   m - microcode labels\n");
	fprintf(stderr, "   l - lod labels\n");
	fprintf(stderr, "-s		halt after prom runs\n");
	fprintf(stderr, "-w		warm boot\n");

	exit(1);
}

extern char *optarg;
extern int trace;
extern int trace_mcr_labels_flag;

int
main(int argc, char *argv[])
{
	int c;

	printf("CADR emulator v0.9\n");

	show_video_flag = 1;
	mouse_sync_flag = 1;

	while ((c = getopt(argc, argv, "ab:B:c:dC:i:l:nmp:q:rtT:sSw")) != -1) {
		switch (c) { 
		case 'a':
			alt_prom_flag = 1;
			break;
		case 'b':
			breakpoint_set_mcr(optarg);
			break;
		case 'B':
			begin_trace_cycle = atol(optarg);
			break;
		case 'c':
			max_cycles = atol(optarg);
			break;
		case 'C':
			max_trace_cycles = atol(optarg);
			break;
		case 'd':
			dump_state_flag = 1;
			break;
		case 'i':
			config_set_disk_filename(optarg);
			break;
		case 'l':
			tracelabel_set_mcr(optarg);
			break;
		case 'n':
			show_video_flag = 0;
			break;
		case 'm':
			mouse_sync_flag = 0;
			break;
		case 'p':
			breakpoint_set_prom(optarg);
			break;
		case 'q':
			breakpoint_set_count(atoi(optarg));
			break;
#if defined(MAP_SITE_TREE_DIRECTORY)
		case 'r':
			{			
				char *p = "../../lisp";
				char newpath[PATH_MAX];
				realpath (p, newpath);
				dcanon(newpath, 0);
				settreeroot(newpath);
			}
			break;
#endif
		case 'S':
			save_state_flag = 1;
			break;
		case 't':
			trace = 1;
			break;
		case 'T':
			switch (optarg[0]) {
			case 'a': trace_after_flag = 1; break;
			case 'd': trace_disk_flag = 1; break;
			case 'i': trace_int_flag = 1; break;
			case 'o': trace_io_flag = 1; break;
			case 'p': trace_prom_flag = 1; break;
			case 'c': trace_mcr_flag = 1; break;
			case 'm': trace_mcr_labels_flag = 1; break;
			case 'n': trace_net_flag = 1; break;
			case 'l': trace_lod_labels_flag = 1; break;
			case 'v': trace_vm_flag = 1; break;
			}
			break;
		case 's':
			stop_after_prom_flag = 1;
			break;
		case 'w':
			warm_boot_flag = 1;
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
	chaos_init();
	ether_init();

#if 0
	show_prom();
	disassemble_prom();
#endif

	signal_init();

	if (warm_boot_flag) {
		iob_warm_boot_key();
	}

	run();

	signal_shutdown();

	if (show_video_flag) {
		while (run_ucode_flag) {
			display_poll();
		}
	}

	exit(0);
}
