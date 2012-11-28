//
//  Breakpoints.h
//  usim
//
//  Created by Greg Gilley on 11/19/12.
//
//

#ifndef usim_Breakpoints_h
#define usim_Breakpoints_h

typedef struct Breakpoint
{
    int enabled;
    char *identifier;
} Breakpoint;

extern int breakpoint_count;

extern Breakpoint *breakpoints;

int addbreakpoint(void);
void removebreakpoint(int breakpoint);

#endif
