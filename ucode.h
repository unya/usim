/*
 * ucode.h
 * $Id$
 */

typedef unsigned long long ucw_t;

extern int trace;
#define tracef	if (trace) printf

extern int trace_io_flag;
#define traceio	if (trace_io_flag) printf

extern int trace_disk_flag;
#define tracedio if (trace_disk_flag) printf

