//
//  SimView.h
//  sim
//
//  Created by Greg Gilley on 4/8/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class CreateDiskController;
@class PartitionViewController;
@class DebugSettingsController;
@class DebuggerController;
@class BreakpointsController;

@interface SimView : NSView {


@private
    NSTrackingArea *trackingArea;
    NSBitmapImageRep *bitmap;
    NSString *diskImagePath;
    IBOutlet NSWindow *partitionSheet;
    CreateDiskController *createDiskController;
    PartitionViewController *partitionViewController;
    DebugSettingsController *debugSettingsController;
    DebuggerController *debuggerController;
    BreakpointsController *breakpointsController;
}

- (IBAction)setCurrentBand:(id)sender;
- (IBAction)extractBand:(id)sender;
- (IBAction)showPartitionSheet:(id)sender;
- (IBAction)createDiskSheet:(id)sender;
- (IBAction)endPartitionSheet:(id)sender;
- (IBAction)debugSettings:(id)sender;
- (IBAction)debugger:(id)sender;
-(IBAction)showBreakpointsPanel:(id)sender;

@end
