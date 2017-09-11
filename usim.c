#include "usim.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/time.h>

#include "ucode.h"
#include "config.h"
#include "Files.h"

int save_state_flag;

extern int display_init(void);
extern void display_poll(void);
extern int disk_init(const char *filename);
extern int read_prom_files(void);
extern void read_sym_files(void);
extern void iob_init(void);
extern int chaos_init(void);
extern void iob_warm_boot_key(void);
extern void run(void);
extern int dcanon(char *cp, int blankok);

void
usage(void)
{
	fprintf(stderr, "usage: usim [OPTION]...\n");
	fprintf(stderr, "CADR simulator\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  -i FILE        set disk image\n");
	fprintf(stderr, "  -r             map /tree to ../sys\n");
	fprintf(stderr, "  -S             save state\n");
	fprintf(stderr, "  -s             halt after prom runs\n");
	fprintf(stderr, "  -w             warm boot\n");
	exit(1);
}

extern char *optarg;

int
main(int argc, char *argv[])
{
	int c;

	printf("CADR emulator v0.9\n");

	while ((c = getopt(argc, argv, "ab:B:c:dC:i:l:nmp:q:rtT:sSw")) != -1) {
		switch (c) {
		case 'i':
			config_set_disk_filename(optarg);
			break;
		case 'r':
		{
			char *p = "../sys";
			char newpath[PATH_MAX];

			realpath(p, newpath);
			dcanon(newpath, 0);
			settreeroot(newpath);
		}
		break;
		case 'S':
			save_state_flag = 1;
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

	display_init();
	display_poll();

	disk_init(config_get_disk_filename());

	read_prom_files();
	read_sym_files();

	iob_init();
	chaos_init();

	if (warm_boot_flag) {
		iob_warm_boot_key();
	}

	run();

	while (run_ucode_flag) {
		display_poll();
	}

	exit(0);
}
