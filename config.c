/*
 * config.c
 * $Id$
 */

#include "config.h"

static char mcrsym_filename[1024];

const char *
config_get_promsym_filename(void)
{
	return "promh.sym.9";
}

const char *
config_get_mcrsym_filename(void)
{
	if (mcrsym_filename[0])
		return mcrsym_filename;

	return "ucadr.sym.841";
}

void
config_set_mcrsym_filename(const char *fn)
{
	strcpy(mcrsym_filename, fn);
}


const char *
config_get_disk_filename(void)
{
	return "disk.img";
}

