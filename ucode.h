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

extern int trace_int_flag;
#define traceint if (trace_int_flag) printf

extern int trace_mcr_labels_flag;
extern int trace_lod_labels_flag;
extern int trace_prom_flag;
extern int stop_after_prom_flag;

extern unsigned long max_cycles;
extern unsigned long max_trace_cycles;

extern char *sym_find_by_val(int mcr, int v);
extern char *sym_find_last(int mcr, int v, int *poffset);
extern char *sym_find_by_type_val(int mcr, int t, int v);
