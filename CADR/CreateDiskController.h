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
    IBOutlet NSPopUpButtonCell *_partitionMenu;
    NSString *diskFileName;
    NSArray *partitionTypes;
    int partitionSize[10];
}

- (IBAction)setDefault:(id)sender;
- (IBAction)dismissWindow:(id)sender;
- (IBAction)addPartition:(id)sender;
- (IBAction)deletePartition:(id)sender;
- (void)updateDictionary;
- (void)updateStarts;
- (void)handleAction:(id)sender;
- (NSNumber *)findIndex:(NSString *)name;

@end
