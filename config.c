/*
 * config.c
 * $Id$
 */

#include "config.h"

const char *
config_get_promsym_filename(void)
{
	return "promh.sym.9";
}

const char *
config_get_mcrsym_filename(void)
{
	return "ucadr.sym.841";
//	return "ucadr.sym.979";
}


const char *
config_get_disk_filename(void)
{
	return "disk.img";
}

