#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

int fd;

void
lashup_init(char *port)
{
	int err;
	struct termios oldtio;
	struct termios newtio;

	fd = open(port, O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		perror(port);
		exit (1);
	}

	// Get current port settings.
	err = tcgetattr(fd, &oldtio);
	if (err) {
		perror("tcgetattr");
		exit(1);
	}

	newtio = oldtio;
	cfmakeraw(&newtio);

	// Set new port settings for canonical input processing.
	cfsetspeed(&newtio, B115200);
	newtio.c_cflag |= CS8 | CLOCAL | CREAD;
	newtio.c_iflag |= IGNPAR;
	newtio.c_cc[VMIN] = 1;
	newtio.c_cc[VTIME] = 0;
	tcflush(fd, TCIFLUSH);

	err = tcsetattr(fd, TCSANOW, &newtio);
	if (err) {
		perror("tcsetattr");
		exit(1);
	}
}

void
lashup_unibus_read(int offset, unsigned int *pv)
{
	switch (offset) {
	case 0100:
		printf("lashup: reading from debugee\n");
		break;
	case 0114:
		printf("lashup: reading from write only address (debugee address)\n");
		break;
	case 0110:
		printf("lashup: reading from write only address (modifier bits)\n");
		break;
	case 0104:
		printf("lashup: bus cycle)\n");
		break;
	default:
		printf("lashup: read: unknown offset\n");
		break;
	}
}

void
lashup_unibus_write(int offset, unsigned int v)
{
	switch (offset) {
	case 0100:
		printf("lashup: writing to debugee\n");
		break;
	case 0114:
		printf("lashup: unibus debugee 1-16 bits\n");
		break;
	case 0110: {
		int bits = v;
		printf("lashup: writing modifier bits\n");
		switch (bits) {
		case 01:
			printf("\tbit 17 of debugee unibus address\n");
			break;
		case 02:
			printf("\treset debugee bus interface\n");
			break;
		case 04:
			printf("\ttimeout inhibit\n");
			break;
		default:
			printf("\tunknown modifier bit: %o; v %o\n", bits, v);
			break;
		}
		break;
	}

	case 0104:
		printf("lashup: writing to read-only address (bus cycle)\n");
		break;
	default:
		printf("lashup: write: unknown offset\n");
		break;
	}
}
