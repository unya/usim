//
//  DebugSettingsController.h
//  usim
//
//  Created by Greg Gilley on 10/17/12.
//
//

#import <Cocoa/Cocoa.h>

@interface DebugSettingsController : NSWindowController
{
@private
    IBOutlet NSButton *traceCheckbox;
    IBOutlet NSButton *traceMCRLabels;
    IBOutlet NSButton *traceLODLabels;
    IBOutlet NSButton *tracePROM;
    IBOutlet NSButton *traceMCR;
    IBOutlet NSButton *traceIO;
    IBOutlet NSButton *traceVM;
    IBOutlet NSButton *traceDisk;
    IBOutlet NSButton *traceNet;
    IBOutlet NSButton *traceInt;
    IBOutlet NSButton *traceLateSet;
    IBOutlet NSButton *traceAfter;
}

- (IBAction)dismissWindow:(id)sender;

@end
