//
//  SimView.m
//  sim
//
//  Created by Greg Gilley on 4/8/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "SimView.h"
#import "CreateDiskController.h"
#import "PartitionViewController.h"
#import "DebugSettingsController.h"
#import "DebuggerController.h"
#import "BreakpointsController.h"

#include "ucode.h"
//#include "iob.h"
#include "diskutil.h"

@implementation SimView

extern CGImageRef cgImage;
extern NSWindow *mWindow;
extern void iob_sdl_mouse_event(int x, int y, int dx, int dy, int buttons);


NSView *myView;

- (id)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        myView = self;
    }
    return self;
}

- (void)awakeFromNib
{
    extern unsigned int tv_bitmap[(768 * 1024)];
    extern unsigned int video_width;
    extern unsigned int video_height;
    unsigned char *data[1] = {(unsigned char *)tv_bitmap};
    
    myView = self;
    bitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:data pixelsWide:(int)video_width pixelsHigh:(int)video_height bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:(video_width * 32 + 7)/8 bitsPerPixel:32];
    
    memset( tv_bitmap, 255, sizeof(tv_bitmap) );    
}

-(void)updateTrackingAreas
{
    if(trackingArea != nil) {
        [self removeTrackingArea:trackingArea];
        [trackingArea release];
    }
    
    NSTrackingAreaOptions opts = (NSTrackingMouseEnteredAndExited | NSTrackingInVisibleRect | NSTrackingMouseMoved |  NSTrackingEnabledDuringMouseDrag | NSTrackingActiveWhenFirstResponder );
    trackingArea = [ [NSTrackingArea alloc] initWithRect:[self bounds]
                                                 options:opts
                                                   owner:self
                                                userInfo:nil];
    [self addTrackingArea:trackingArea];
}

- (void)drawRect:(NSRect)dirtyRect
{
//    CGContextRef port = [[NSGraphicsContext currentContext] graphicsPort];
    
//    CGContextSaveGState(port);
//    CGContextClipToRect(port, NSRectToCGRect(dirtyRect));
//    CGContextDrawImage(port, NSRectToCGRect([self bounds]), cgImage);
//    CGContextRestoreGState(port);
//    NSLog(@"dirtyRect = %@", NSStringFromRect(dirtyRect));
//    NSLog(@"bounds = %@", NSStringFromRect([self bounds]));

    extern unsigned int tv_bitmap[(768 * 1024)];
    extern unsigned int video_width;
    extern unsigned int video_height;
    const unsigned char *data[1] = {(unsigned char *)tv_bitmap};

    NSDrawBitmap(NSMakeRect(0,0,video_width,video_height), (int)video_width, (int)video_height, 8, 4, 32, (video_width * 32 + 7)/8, NO, YES, NSDeviceRGBColorSpace, data);

//    [bitmap draw];
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)keyDown:(NSEvent *)theEvent {
    void iob_sdl_key_event(int code, int extra);
	int extra;
    NSUInteger modifiers = [theEvent modifierFlags];
    
	extra = 0;
    
//	if (modifiers & NSAlternateKeyMask)
//		extra |= 3 << 12;

    if (modifiers & NSCommandKeyMask)
		extra |= 3 << 12;

	if (modifiers & NSShiftKeyMask)
		extra |= 3 << 6;
    
	if (modifiers & NSAlphaShiftKeyMask)
		extra ^= 3 << 6;
    
	if (modifiers & NSControlKeyMask)
		extra |= 3 << 10;
    
//    printf("keycode: %d\n", [theEvent keyCode]);
    
    iob_sdl_key_event([theEvent keyCode], extra);
}

- (void)mouseMoved:(NSEvent *)theEvent {
    NSPoint eyeCenter = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    NSUInteger buttons = [theEvent buttonMask];
    extern unsigned int video_height;
    
//    printf("mouse: %f, %f, %08x\n", eyeCenter.x, eyeCenter.y, buttons);
    
    iob_sdl_mouse_event((int)eyeCenter.x, (int)video_height - (int)eyeCenter.y - 1, 0, 0, (int)buttons);
}

- (void)mouseDown:(NSEvent *)theEvent {
    NSPoint eyeCenter = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    extern unsigned int video_height;
    
    iob_sdl_mouse_event((int)eyeCenter.x, (int)video_height - (int)eyeCenter.y - 1, 0, 0, 1);
}

- (void)rightMouseDown:(NSEvent *)theEvent {
    NSPoint eyeCenter = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    extern unsigned int video_height;
    
    iob_sdl_mouse_event((int)eyeCenter.x, (int)video_height - (int)eyeCenter.y - 1, 0, 0, 4);
}

- (void)mouseDragged:(NSEvent *)theEvent {
    NSPoint eyeCenter = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    extern unsigned int video_height;
    
    iob_sdl_mouse_event((int)eyeCenter.x, (int)video_height - (int)eyeCenter.y - 1, 0, 0, 1);
}

- (void)rightMouseDragged:(NSEvent *)theEvent {
    NSPoint eyeCenter = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    extern unsigned int video_height;
    
    iob_sdl_mouse_event((int)eyeCenter.x, (int)video_height - (int)eyeCenter.y - 1, 0, 0, 4);
}

- (void)mouseUp:(NSEvent *)theEvent {
    NSPoint eyeCenter = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    NSInteger buttons = [theEvent buttonNumber];
    extern unsigned int video_height;
    
    iob_sdl_mouse_event((int)eyeCenter.x, (int)video_height - (int)eyeCenter.y - 1, 0, 0, (int)buttons);
}

- (void)mouseEntered:(NSEvent *)anEvent
{
    [NSCursor hide];
}

- (void)mouseExited:(NSEvent *)anEvent
{
    [NSCursor unhide];
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)anItem
{
    SEL theAction = [anItem action];

    if (theAction == @selector(run:))
    {
        return YES;
    }
    if (theAction == @selector(setCurrentBand:))
    {
        return YES;
    }
    if (theAction == @selector(extractBand:))
    {
        return YES;
    }
    if (theAction == @selector(dumpState:))
    {
        return YES;
    }
    if (theAction == @selector(showPartitionSheet:))
    {
        return YES;
    }
    if (theAction == @selector(createDiskSheet:))
    {
        return YES;
    }
    if (theAction == @selector(debugSettings:))
    {
        return YES;
    }
    if (theAction == @selector(debugger:))
    {
        return YES;
    }
    if (theAction == @selector(showBreakpointsPanel:))
    {
        return YES;
    }
    return NO;
}

- (void)run:(id)path
{
    int run(void);

    diskImagePath = [path retain];
    NSLog(@"running disk.img: %@", diskImagePath);
    run();
}

- (IBAction)setCurrentBand:(id)sender
{
    set_current_band([diskImagePath cStringUsingEncoding:NSASCIIStringEncoding], "LOD1");
}

- (IBAction)extractBand:(id)sender
{
    extract_partition([diskImagePath cStringUsingEncoding:NSASCIIStringEncoding], [[NSString stringWithFormat:@"%@/Documents/extracted partition", NSHomeDirectory()] cStringUsingEncoding:NSASCIIStringEncoding], "LOD3");
}

- (IBAction)dumpState:(id)sender {
    void dump_state(void);

    dump_state();
}

- (IBAction)showPartitionSheet:(id)sender
{
    if (!partitionViewController)
    {
        partitionViewController = [[PartitionViewController alloc] init];
    }

    [partitionViewController setFilename:diskImagePath];
    [partitionViewController showWindow:self];
    
//    [NSApp beginSheet:partitionSheet modalForWindow:[self window] modalDelegate:nil didEndSelector:NULL contextInfo:NULL];
}

- (IBAction)createDiskSheet:(id)sender
{
    if (!createDiskController)
    {
        createDiskController = [[CreateDiskController alloc] init];
    }
    
    [createDiskController showWindow:self];
}

- (IBAction)endPartitionSheet:(id)sender
{
//    [NSApp endSheet:partitionSheet];
//    [partitionSheet orderOut:sender];
}

- (IBAction)debugSettings:(id)sender
{
    if (!debugSettingsController)
    {
        debugSettingsController = [[DebugSettingsController alloc] init];
    }
    
    [debugSettingsController showWindow:self];
}

- (IBAction)debugger:(id)sender
{
    if (!debuggerController)
    {
        debuggerController = [[DebuggerController alloc] init];
    }
    
    [debuggerController showWindow:self];
}

-(IBAction)showBreakpointsPanel:(id)sender
{
    if (!breakpointsController)
    {
        breakpointsController = [[BreakpointsController alloc] init];
    }
    
    [breakpointsController showWindow:self];
}

@end
