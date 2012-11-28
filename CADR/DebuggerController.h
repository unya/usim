//
//  DebuggerController.h
//  usim
//
//  Created by Greg Gilley on 10/19/12.
//
//

#import <Cocoa/Cocoa.h>

@interface DebuggerController : NSWindowController <NSToolbarDelegate>
{
    IBOutlet NSTextField *spcvalue;
    IBOutlet NSTextField *pdlpvalue;
    IBOutlet NSTextField *pdlivalue;
    IBOutlet NSTextField *pdlbvalue;
    IBOutlet NSTextField *opcvalue;
    IBOutlet NSTextField *qvalue;
    IBOutlet NSTextField *vmavalue;
    IBOutlet NSTextField *mapvalue;
    IBOutlet NSTextField *mdvalue;
    IBOutlet NSTextField *lcvalue;
    IBOutlet NSTextField *upcvalue;
    IBOutlet NSTextField *instruction;
    IBOutlet NSTextField *instructionLabel;
    
    IBOutlet NSToolbar *toolbar;
    IBOutlet NSToolbarItem *runpauseItem;
    IBOutlet NSToolbarItem *macroItem;
}

- (IBAction)runpause:(id)sender;
- (IBAction)step:(id)sender;
- (IBAction)macromicro:(id)sender;

- (void)updateDisplay:(unsigned int)pc;

@end
