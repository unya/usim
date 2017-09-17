#ifndef USIM_USIM_H
#define USIM_USIM_H

#include <stdbool.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define tracef(...)
#define tracevm(...)
#define traceint(...)
#define tracedio(...)
#define tracenet(...)
#define traceio(...)

extern char *disk_filename;
extern char *mcrsym_filename;
extern char *promsym_filename;

extern bool save_state_flag;
extern bool warm_boot_flag;
extern bool stop_after_prom_flag;
extern bool run_ucode_flag;
extern bool prom_enabled_flag;

#endif
