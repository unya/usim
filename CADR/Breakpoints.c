//
//  Breakpoints.c
//  usim
//
//  Created by Greg Gilley on 11/19/12.
//
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Breakpoints.h"

int breakpoint_count = 0;
Breakpoint *breakpoints;
static size_t breakpoint_pool_size = 0;

int addbreakpoint(void)
{
    if (breakpoint_count == (int)breakpoint_pool_size)
    {
        Breakpoint *oldpool = breakpoints;
        size_t oldbreakpoint_pool_size = breakpoint_pool_size;
        
        breakpoint_pool_size += 10;
        breakpoints = (Breakpoint *)malloc(sizeof(Breakpoint) * breakpoint_pool_size);
        memset(breakpoints, 0, sizeof(Breakpoint) * breakpoint_pool_size);
        memcpy(breakpoints, oldpool, oldbreakpoint_pool_size);
    }
    
    return ++breakpoint_count;
}

void removebreakpoint(int breakpoint)
{
    if (breakpoint_count == 0)
        return;
    
    memcpy(&breakpoints[breakpoint], &breakpoints[breakpoint+1], sizeof(Breakpoint) * (size_t)(breakpoint_count - breakpoint - 1));
    breakpoint_count--;
    memset(&breakpoints[breakpoint_count], 0, sizeof(Breakpoint));
}
