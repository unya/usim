//
//  CreateDiskController.h
//  usim
//
//  Created by Greg Gilley on 10/9/12.
//
//

#import <Cocoa/Cocoa.h>

@interface CreateDiskController : NSWindowController<NSTableViewDelegate, NSTableViewDataSource>
{
@private
    NSMutableArray *_tableContents;
    IBOutlet NSTableView *_tableView;
    NSString *diskFileName;
}

- (IBAction)setDefault:(id)sender;
- (IBAction)dismissWindow:(id)sender;
- (IBAction)addPartition:(id)sender;
- (IBAction)deletePartition:(id)sender;
- (void)updateDictionary;
- (void)updateStarts;
- (void)handleAction:(id)sender;

@end
