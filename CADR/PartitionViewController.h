//
//  PartitionViewController.h
//  usim
//
//  Created by Greg Gilley on 10/9/12.
//
//

#import <Cocoa/Cocoa.h>

@interface PartitionViewController : NSWindowController<NSTableViewDelegate, NSTableViewDataSource>
{
@private
    NSMutableArray *_tableContents;
    IBOutlet NSTableView *_tableView;
    NSString *diskFileName;
}

- (IBAction)setDefault:(id)sender;
- (IBAction)extractPartition:(id)sender;
- (IBAction)dismissWindow:(id)sender;

- (void) setFilename:(NSString *)filename;

@end
