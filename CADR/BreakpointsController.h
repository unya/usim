//
//  BreakpointsController.h
//  usim
//
//  Created by Greg Gilley on 11/18/12.
//
//

#import <Cocoa/Cocoa.h>

@interface BreakpointsController : NSWindowController<NSTableViewDelegate, NSTableViewDataSource>
{
    NSMutableArray *_tableContents;
    IBOutlet NSTableView *_tableView;    
}

- (IBAction)dismissWindow:(id)sender;
- (IBAction)addBreakpoint:(id)sender;
- (IBAction)deleteBreakpoint:(id)sender;

@end
