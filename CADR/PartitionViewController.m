//
//  PartitionViewController.m
//  usim
//
//  Created by Greg Gilley on 10/9/12.
//
//

#import "PartitionViewController.h"
#include "diskutil.h"

@interface PartitionViewController ()

@end

@implementation PartitionViewController

- (id)initWithWindow:(NSWindow *)window
{
    self = [super initWithWindow:window];
    if (self) {
        // Initialization code here.
    }
    
    return self;
}

- (id)init
{
    self = [super initWithWindowNibName:@"Partitions"];
    
    diskFileName = 0;
    return self;
}

- (void)windowDidLoad
{
    [super windowDidLoad];

    _tableContents = [NSMutableArray new];
    
    if (diskFileName)
        read_labl([diskFileName cStringUsingEncoding:NSASCIIStringEncoding]);
    
    for (unsigned short i=0; i < part_count; i++)
    {
        NSString *def = (strcasecmp(lod_name, parts[i].name) == 0 || strcasecmp(mcr_name, parts[i].name) == 0) ? @"*" : @"";
        NSString *name = [NSString stringWithCString:parts[i].name encoding:NSASCIIStringEncoding];
        NSString *start = [[NSNumber numberWithUnsignedInt:parts[i].start] stringValue];
        NSString *size = [[NSNumber numberWithUnsignedInt:parts[i].size] stringValue];
        NSString *description = [NSString stringWithCString:parts[i].label encoding:NSASCIIStringEncoding];

        // our model will consist of a dictionary with Name/Image key pairs
        NSMutableDictionary *dictionary = [[[NSMutableDictionary alloc] initWithObjectsAndKeys:def, @"Default", name, @"Name", start, @"Start",
                                     size, @"Size", description, @"Description", nil] autorelease];
        [_tableContents addObject:dictionary];
    }
    [_tableView reloadData];
}

- (NSString *)windowNibName {
    return @"Partitions";
}

- (void)dealloc {
    [_tableContents release];
    [super dealloc];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    return (NSInteger)[_tableContents count];
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row {
    // Group our "model" object, which is a dictionary
    NSDictionary *dictionary = [_tableContents objectAtIndex:(NSUInteger)row];
    
    // In IB the tableColumn has the identifier set to the same string as the keys in our dictionary
    NSString *identifier = [tableColumn identifier];
    
    if ([identifier isEqualToString:@"DefaultCell"]) {
        NSTableCellView *cellView = [tableView makeViewWithIdentifier:identifier owner:self];
        // Then setup properties on the cellView based on the column
        cellView.textField.stringValue = [dictionary objectForKey:@"Default"];
        return cellView;
    } else if ([identifier isEqualToString:@"NameCell"]) {
        NSTableCellView *cellView = [tableView makeViewWithIdentifier:identifier owner:self];
        // Then setup properties on the cellView based on the column
        cellView.textField.stringValue = [dictionary objectForKey:@"Name"];
        return cellView;
    } else if ([identifier isEqualToString:@"StartCell"]) {
        NSTableCellView *cellView = [tableView makeViewWithIdentifier:identifier owner:self];
        // Then setup properties on the cellView based on the column
        cellView.textField.stringValue = [dictionary objectForKey:@"Start"];
        return cellView;
    } else if ([identifier isEqualToString:@"SizeCell"]) {
        NSTableCellView *cellView = [tableView makeViewWithIdentifier:identifier owner:self];
        // Then setup properties on the cellView based on the column
        cellView.textField.stringValue = [dictionary objectForKey:@"Size"];
        return cellView;
    } else if ([identifier isEqualToString:@"DescriptionCell"]) {
        NSTableCellView *cellView = [tableView makeViewWithIdentifier:identifier owner:self];
        // Then setup properties on the cellView based on the column
        cellView.textField.stringValue = [dictionary objectForKey:@"Description"];
        return cellView;
    } else {
        NSAssert1(NO, @"Unhandled table column identifier %@", identifier);
    }
    return nil;
}

static int isloadband(const char *name)
{
    if (toupper(name[0]) == 'L' && toupper(name[1]) == 'O' && toupper(name[2]) == 'D')
        return 1;
    return 0;
}

static int ismcrband(const char *name)
{
    if (toupper(name[0]) == 'M' && toupper(name[1]) == 'C' && toupper(name[2]) == 'R')
        return 1;
    return 0;
}

- (IBAction)setDefault:(id)sender
{
    NSInteger row = [_tableView selectedRow];
    NSMutableDictionary *dictionary = [_tableContents objectAtIndex:(NSUInteger)row];
    
    if (isloadband(parts[row].name) && strcasecmp(parts[row].name, lod_name) != 0)
    {
        set_current_band([diskFileName cStringUsingEncoding:NSASCIIStringEncoding], parts[row].name);
        for (unsigned short i = 0; i < part_count; i++)
        {
            NSMutableDictionary *dict = [_tableContents objectAtIndex:i];
            if (isloadband(parts[i].name))
                [dict setValue:@"" forKey:@"Default"];
        }
        [dictionary setValue:@"*" forKey:@"Default"];
    }
    else if (ismcrband(parts[row].name) && strcasecmp(parts[row].name, mcr_name) != 0)
    {
        set_current_mcr([diskFileName cStringUsingEncoding:NSASCIIStringEncoding], parts[row].name);
        for (unsigned short i = 0; i < part_count; i++)
        {
            NSMutableDictionary *dict = [_tableContents objectAtIndex:i];
            if (ismcrband(parts[i].name))
                [dict setValue:@"" forKey:@"Default"];
        }
        [dictionary setValue:@"*" forKey:@"Default"];
    }
    
    [_tableView reloadData];
}

- (IBAction)extractPartition:(id)sender
{
    NSInteger row = [_tableView selectedRow];
    NSSavePanel *panel	= [NSSavePanel savePanel];
    
    [panel setDirectoryURL:[NSURL fileURLWithPath:diskFileName]];
    if (ismcrband(parts[row].name))
        [panel setAllowedFileTypes:@[@"mcr"]];
    else
        [panel setAllowedFileTypes:@[@"band"]];
    [panel setAllowsOtherFileTypes:YES];
    
    if ([panel runModal] == NSOKButton)
    {
        extract_partition([diskFileName cStringUsingEncoding:NSASCIIStringEncoding], [[[panel URL] path] fileSystemRepresentation], parts[row].name);
        
       NSError *error = NULL;
        
        if (ismcrband(parts[row].name))
            [[panel URL] setResourceValue:@"com.cadr.mcr" forKey:NSURLTypeIdentifierKey error:&error];
        else
            [[panel URL] setResourceValue:@"com.cadr.band" forKey:NSURLTypeIdentifierKey error:&error];

        if (error)
            NSLog(@"Error: %@\n", [error localizedDescription]);
        
    }
}

- (IBAction)dismissWindow:(id)sender
{
    [[self window] orderOut:sender];
    [[self window] performClose:sender];
}

- (void)setFilename:(NSString *)filename
{
    diskFileName = [filename retain];
}

@end
