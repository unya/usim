//
//  cadrAppDelegate.m
//  sim
//
//  Created by Greg Gilley on 4/7/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "cadrAppDelegate.h"
#import "DebuggerController.h"
#import <Cocoa/Cocoa.h>
#import <mach/mach_time.h> // for mach_absolute_time

#include <sys/time.h>

#include "ucode.h"
//#include "disk.h"
#include "config.h"
#include "decode.h"
//#include "iob.h"
#include "syms.h"
#include "chaos.h"
//#include "ether.h"
#include "diskutil.h"

@implementation cadrAppDelegate

@synthesize window = _window;


//CGImageRef cgImage;

unsigned int video_width = 768;
unsigned int video_height = 897 /*1024*/;

unsigned int tv_bitmap[(768 * 1024)];
BOOL bitmapupdated = YES;

// NSWindow *mWindow;

extern NSView *myView;

unsigned int Black, White;

NSURL *sourceFolderURL = nil;

int show_video_flag;
int mouse_sync_flag = 1;
int alt_prom_flag;
int dump_state_flag;
int save_state_flag;

/* */
struct timeval tv1;

int read_prom_files_from_resources(void);
int read_sym_files_from_resources(void);
void step_poll(unsigned int p0_pc);
void send_accumulated_updates(void);

/*
 * simple wall clock timing to get a notion of the basic cycle time
 */

void
timing_start(void)
{
	gettimeofday(&tv1, NULL);
}

void
timing_stop(void)
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

static void
sigint_handler(int arg)
{
	run_ucode_flag = 0;
}

static void
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

static void
signal_init(void)
{
	signal(SIGINT, sigint_handler);
#ifndef _WIN32
	signal(SIGHUP, sighup_handler);
#endif
}

static void
signal_shutdown(void)
{
    signal(SIGINT, SIG_DFL);
    fflush(stdout);
}

static mach_timebase_info_data_t timebase;

int display_init()
{
    mach_timebase_info(&timebase);
    return 0;
}

void display_poll(void)
{
    static uint64_t last;
    uint64_t current = mach_absolute_time();

    send_accumulated_updates();

    // compute nanoseconds
    if ((current - last) * timebase.numer / timebase.denom > 1000000)
    {
        @autoreleasepool {
            NSApplication *application = [NSApplication sharedApplication];
            
            NSEvent *event = [application nextEventMatchingMask:NSAnyEventMask untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES];
            
            if (event)
            {
                [application sendEvent:event];
                [application updateWindows];
            }
        }
    }
    last = current;
}

void
step_poll(unsigned int pc)
{
    extern int runpause;
    extern int step;

    if (!runpause)
    {
        extern DebuggerController *debugger;

        do {
            [debugger updateDisplay:pc];

            @autoreleasepool {
                NSApplication *application = [NSApplication sharedApplication];
                
                NSEvent *event = [application nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate distantFuture] inMode:NSDefaultRunLoopMode dequeue:YES];
                
                if (event)
                {
                    [application sendEvent:event];
                    [application updateWindows];
                }
            }

        } while (!runpause && !step);
        
        step = 0;
    }
}

int u_minh = 0x7fffffff, u_maxh, u_minv = 0x7fffffff, u_maxv;

static void
accumulate_update(int h, int v, int hs, int vs)
{
#if 0
    SDL_UpdateRect(screen, h, v, 32, 1);
#else
    if (h < u_minh) u_minh = h;
    if (h+hs > u_maxh) u_maxh = h+hs;
    if (v < u_minv) u_minv = v;
    if (v+vs > u_maxv) u_maxv = v+vs;
#endif
}

void
send_accumulated_updates(void)
{
    int hs, vs;
    
    hs = u_maxh - u_minh;
    vs = u_maxv - u_minv;
    if (u_minh != 0x7fffffff && u_minv != 0x7fffffff && u_maxh && u_maxv)
	{
        [myView setNeedsDisplayInRect:NSMakeRect(u_minh,(int)video_height-u_maxv,hs,vs)];
    }
    
    u_minh = 0x7fffffff;
    u_maxh = 0;
    u_minv = 0x7fffffff;
    u_maxv = 0;
}

void
video_read(int offset, unsigned int *pv)
{
	if (1 /*ximage*/) {
		unsigned int bits;
		int i;
        
		offset *= 32;
        
//		v = offset / video_width;
//		h = offset % video_width;
        
		if ((unsigned int)offset > video_width*video_height) {
			if (1) printf("video: video_read past end; "
                          "offset %o\n", offset);
			*pv = 0;
			return;
		}
        
		bits = 0;
		for (i = 0; i < 32; i++)
		{
			if (tv_bitmap[offset + i] == Black)
				bits |= 1 << i;
		}
        
		*pv = bits;
	}
}

void
video_write(int offset, unsigned int bits)
{
	if (1 /*ximage*/) {
		int i, h, v;

		offset *= 32;
        
		v = offset / (int)video_width;
		h = offset % (int)video_width;
        
		if (0) printf("v,h %d,%d <- %o (offset %d)\n", v, h, bits, offset);
        
		for (i = 0; i < 32; i++)
		{
			tv_bitmap[offset + i] = (bits & 1) ? Black : White;
			bits >>= 1;
		}
        
        bitmapupdated = YES;
        
		accumulate_update(h, v, 32, 1);
    }
}

- (void)dealloc
{
    [super dealloc];
}
int
read_prom_files_from_resources(void)
{
    extern u_char prom[6][512];
    extern ucw_t prom_ucode[512];
	int fd, i;
    ssize_t ret;
    NSBundle *mainBundle = [NSBundle mainBundle];
    
	for (i = 0; i < 6; i++) {        

        NSString* name = [NSString stringWithFormat:@"cadr_%1d", i+1];
        NSString* promname = [mainBundle pathForResource:name ofType:@"bin"];
		
		fd = open([promname cStringUsingEncoding:NSASCIIStringEncoding], O_RDONLY);
		if (fd < 0) {
//			perror(name);
			exit(1);
		}
        
		ret = read(fd, prom[i], 512);
		close(fd);

		if (ret != 512) {
			fprintf(stderr, "read_prom_files: short read\n");
			exit(1);
		}
        NSLog(@"read %@", name);
	}
    
	for (i = 0; i < 512; i++) {
		prom_ucode[511-i] =
        ((uint64)prom[0][i] << (8*5)) |
        ((uint64)prom[1][i] << (8*4)) |
        ((uint64)prom[2][i] << (8*3)) |
        ((uint64)prom[3][i] << (8*2)) |
        ((uint64)prom[4][i] << (8*1)) |
        ((uint64)prom[5][i] << (8*0));
	}
    
	return 0;
}

int read_sym_files_from_resources(void)
{
    extern int _sym_sort(struct symtab_s *tab);
    extern struct symtab_s sym_prom;
    extern struct symtab_s sym_mcr;
    NSBundle *mainBundle = [NSBundle mainBundle];

    NSString* promname = [mainBundle pathForResource:@"promh.sym" ofType:@"9"];

	_sym_read_file(&sym_prom, [promname cStringUsingEncoding:NSASCIIStringEncoding]);
	_sym_sort(&sym_prom);
    
    NSString* mcrname = [mainBundle pathForResource:@"ucadr.sym" ofType:@"841"];

	_sym_read_file(&sym_mcr, [mcrname cStringUsingEncoding:NSASCIIStringEncoding]);
	_sym_sort(&sym_mcr);
    
	return 0;
}

- (NSURL*)applicationDataDirectory {
    NSFileManager* sharedFM = [NSFileManager defaultManager];
    NSArray* possibleURLs = [sharedFM URLsForDirectory:NSApplicationSupportDirectory
                                             inDomains:NSUserDomainMask];
    NSURL* appSupportDir = nil;
    NSURL* appDirectory = nil;
    
    if ([possibleURLs count] >= 1) {
        // Use the first directory (if multiple are returned)
        appSupportDir = [possibleURLs objectAtIndex:0];
    }
    
    // If a valid app support directory exists, add the
    // app's bundle ID to it to specify the final directory.
    if (appSupportDir) {
        NSString* appBundleID = [[NSBundle mainBundle] bundleIdentifier];
        appDirectory = [appSupportDir URLByAppendingPathComponent:appBundleID];
    }
    
    return appDirectory;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    int disk_init(char *filename);
    int iob_init(void);
    int ether_init(void);
    void iob_warm_boot_key(void);
    
    // Insert code here to initialize your application
    Black = 0x00000000;
    White = 0xFFFFFFFF;
    
    display_init();
    display_poll();
    
    NSLog(@"%@\n", [self applicationDataDirectory]);

    NSFileManager *fmngr = [[NSFileManager alloc] init];
    NSString *diskImagePath = [NSString stringWithFormat:@"%@/Documents/CADR.disk", NSHomeDirectory()];
    if (![fmngr fileExistsAtPath:diskImagePath])
    {
        default_template();
        create_disk_image([diskImagePath cStringUsingEncoding:NSASCIIStringEncoding]);
    }
    [fmngr release];

    disk_init((char *)[diskImagePath cStringUsingEncoding:NSASCIIStringEncoding]);
    
    read_labl([diskImagePath cStringUsingEncoding:NSASCIIStringEncoding]);

	read_prom_files_from_resources();
    
	read_sym_files_from_resources();
    
	iob_init();
	chaos_init();
	ether_init();
    
    {
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        extern int trace;
        extern int trace_mcr_labels_flag;
        extern int trace_lod_labels_flag;
        extern int trace_prom_flag;
        extern int trace_mcr_flag;
        extern int trace_io_flag;
        extern int trace_vm_flag;
        extern int trace_disk_flag;
        extern int trace_net_flag;
        extern int trace_int_flag;
        extern int trace_late_set;
        extern int trace_after_flag;

        trace = (int)[defaults integerForKey:@"Trace"];
        trace_mcr_labels_flag = (int)[defaults integerForKey:@"TraceMCRLabels"];
        trace_lod_labels_flag = (int)[defaults integerForKey:@"TraceLODLabels"];
        trace_prom_flag = (int)[defaults integerForKey:@"TracePROM"];
        trace_mcr_flag = (int)[defaults integerForKey:@"TraceMCR"];
        trace_io_flag = (int)[defaults integerForKey:@"TraceIO"];
        trace_vm_flag = (int)[defaults integerForKey:@"TraceVM"];
        trace_disk_flag = (int)[defaults integerForKey:@"TraceDisk"];
        trace_net_flag = (int)[defaults integerForKey:@"TraceNet"];
        trace_int_flag = (int)[defaults integerForKey:@"TraceInt"];
        trace_late_set = (int)[defaults integerForKey:@"TraceLateSet"];
        trace_after_flag = (int)[defaults integerForKey:@"TraceAfter"];

        NSData *bookmarkData = [defaults objectForKey:@"SourceFolder"];
        if (bookmarkData)
        {
            NSError *error = nil;
            BOOL bookmarkDataIsStale;
            NSURL *bookmarkFileURL = nil;
            
            bookmarkFileURL = [NSURL
                               URLByResolvingBookmarkData:bookmarkData
                               options:NSURLBookmarkResolutionWithSecurityScope
                               relativeToURL:nil
                               bookmarkDataIsStale:&bookmarkDataIsStale
                               error:&error];
            if (!bookmarkDataIsStale)
            {
                extern NSURL *sourceFolderURL;
                
                sourceFolderURL = [bookmarkFileURL retain];
                [sourceFolderURL startAccessingSecurityScopedResource];
//            [sourceFolderURL stopAccessingSecurityScopedResource];
            }
        }
    }


#if 0
	show_prom();
	disassemble_prom();
#endif
    
	signal_init();
    
	if (warm_boot_flag) {
		iob_warm_boot_key();
	}
    
    // show the microcode and load band booted from in the window title
    NSString *title = [diskImagePath lastPathComponent];
    title = [NSString stringWithFormat:@"%@ (%@, %@)", title, [NSString stringWithCString:mcr_name encoding:NSASCIIStringEncoding],
                            [NSString stringWithCString:lod_name encoding:NSASCIIStringEncoding]];
    [_window setTitle:title];

    [_window orderFront:self];
    // start running the emulator from the main run loop
    [[_window firstResponder] performSelector:@selector(run:) withObject:diskImagePath afterDelay:0.5];
}

@end
