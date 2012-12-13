//
//  CreateDiskController.m
//  usim
//
//  Created by Greg Gilley on 10/9/12.
//
//

#import "CreateDiskController.h"
#include "diskutil.h"

@interface CreateDiskController ()

@end

@implementation CreateDiskController

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
    self = [super initWithWindowNibName:@"CreateDisk"];
    
    partitionTypes = [[NSArray alloc] initWithObjects:@"MCR1", @"MCR2", @"MCR3", @"MCR4", @"PAGE", @"LOD1", @"LOD2", @"LOD3", @"LOD4", @"FILE", nil];
    partitionSize[0] = 148;
    partitionSize[1] = 148;
    partitionSize[2] = 148;
    partitionSize[3] = 148;
    partitionSize[4] = 32768;
    partitionSize[5] = 25344;
    partitionSize[6] = 25344;
    partitionSize[7] = 25344;
    partitionSize[8] = 25344;
    partitionSize[9] = 131072;

    diskFileName = 0;
    return self;
}

- (void)updateDictionary
{
    [_tableContents removeAllObjects];

    for (unsigned short i=0; i < part_count; i++)
    {
        NSString *def = (strcasecmp(lod_name, parts[i].name) == 0 || strcasecmp(mcr_name, parts[i].name) == 0) ? @"*" : @"";
        NSString *name = [NSString stringWithCString:parts[i].name encoding:NSASCIIStringEncoding];
        NSString *start = [[NSNumber numberWithUnsignedInt:parts[i].start] stringValue];
        NSString *size = [[NSNumber numberWithUnsignedInt:parts[i].size] stringValue];
        NSString *description = [NSString stringWithCString:parts[i].label encoding:NSASCIIStringEncoding];
        NSString *filename = parts[i].filename ? [[NSString stringWithCString:parts[i].filename encoding:NSASCIIStringEncoding] lastPathComponent] : @"";
        
        // our model will consist of a dictionary with Name/Image key pairs
        NSMutableDictionary *dictionary = [[[NSMutableDictionary alloc] initWithObjectsAndKeys:def, @"Default", name, @"Name", start, @"Start", size, @"Size", description, @"Description", filename, @"Filename", nil] autorelease];

        [_tableContents addObject:dictionary];
    }    
}

- (void)windowDidLoad
{
    [super windowDidLoad];

    _tableContents = [NSMutableArray new];
    
    default_template();

    [_partitionMenu addItemsWithTitles:partitionTypes];

    [_tableView setDataSource:self];
    [_tableView setAction:@selector(handleAction:)];
    [self updateDictionary];
    [_tableView reloadData];
}

- (NSString *)windowNibName {
    return @"CreateDisk";
}

- (void)dealloc {
    [_tableContents release];
    [super dealloc];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    return (NSInteger)[_tableContents count];
}

- (BOOL)tableView:(NSTableView *)tableView shouldEditTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)rowIndex
{
    NSString *identifier = [tableColumn identifier];
    
    if ([identifier isEqualToString:@"StartCell"])
        return NO;
    return YES;
}

- (NSString *)tableView:(NSTableView *)tableView toolTipForCell:(NSCell *)aCell rect:(NSRectPointer)rect tableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row mouseLocation:(NSPoint)mouseLocation
{
    NSString *identifier = [tableColumn identifier];

    if ([identifier isEqualToString:@"FilenameCell"])
        return parts[row].filename ? [NSString stringWithCString:parts[row].filename encoding:NSASCIIStringEncoding] : nil;
    return nil;
}

- (NSNumber *)findIndex:(NSString *)name
{
    int i = 1;
    
    for (id object in partitionTypes)
    {
        if ([name compare:object] == NSOrderedSame)
            break;
        i++;
    }
    return [NSNumber numberWithInt:i];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    // Group our "model" object, which is a dictionary
    NSDictionary *dictionary = [_tableContents objectAtIndex:(NSUInteger)row];
    
    // In IB the tableColumn has the identifier set to the same string as the keys in our dictionary
    NSString *identifier = [tableColumn identifier];
    
    if ([identifier isEqualToString:@"DefaultCell"]) {
        return [dictionary objectForKey:@"Default"];
    } else if ([identifier isEqualToString:@"PartitionTypeCell"]) {
        return [self findIndex:[dictionary objectForKey:@"Name"]];
    } else if ([identifier isEqualToString:@"StartCell"]) {
        return [dictionary objectForKey:@"Start"];
    } else if ([identifier isEqualToString:@"SizeCell"]) {
        return [dictionary objectForKey:@"Size"];
    } else if ([identifier isEqualToString:@"DescriptionCell"]) {
        return [dictionary objectForKey:@"Description"];
    } else if ([identifier isEqualToString:@"FilenameCell"]) {
        return [dictionary objectForKey:@"Filename"];
    } else {
        NSAssert1(NO, @"Unhandled table column identifier %@", identifier);
    }
    return nil;
}

- (void)tableView:(NSTableView *)tableView setObjectValue:(id)anObject forTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    // Group our "model" object, which is a dictionary
    NSMutableDictionary *dictionary = [_tableContents objectAtIndex:(NSUInteger)row];
    
    // In IB the tableColumn has the identifier set to the same string as the keys in our dictionary
    NSString *identifier = [tableColumn identifier];
    
    if ([identifier isEqualToString:@"PartitionTypeCell"])
    {
        int index = [anObject intValue] - 1;

        [dictionary setObject:[partitionTypes objectAtIndex:index] forKey:@"Name"];
        [dictionary setObject:[[NSNumber numberWithInt:partitionSize[index]] stringValue] forKey:@"Size"];
        
        free((void *)parts[row].name);
        parts[row].name = strdup([[partitionTypes objectAtIndex:index] cStringUsingEncoding:NSASCIIStringEncoding]);
        parts[row].size = partitionSize[index];
        [self updateStarts];
        [self updateDictionary];
        [_tableView reloadData];        
    }
    else if ([identifier isEqualToString:@"DescriptionCell"])
    {
        const char *description = [anObject cStringUsingEncoding:NSASCIIStringEncoding];
        size_t len = description ? strlen(description) : 0;
        
        if (description)
            strcpy(parts[row].label, description);
        
        for (size_t i = len; i < 16; i++)
            parts[row].label[i] = ' ';
        [self updateDictionary];
    }
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
        if (lod_name)
            free((void *)lod_name);
        lod_name = strdup(parts[row].name);
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
        if (mcr_name)
            free((void *)mcr_name);
        mcr_name = strdup(parts[row].name);
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

- (void)updateStarts
{
    unsigned int start = 17;

    for (unsigned short i = 0; i < part_count; i++)
    {
        parts[i].start = start;
        start = start + parts[i].size;
    }
}

- (IBAction)addPartition:(id)sender
{
    NSInteger row = [_tableView selectedRow] + 1;

    if (part_count < MAX_PARTITIONS)
    {
        for (NSInteger i = (NSInteger)part_count - 1; i >= row; i--)
            parts[i + 1] = parts[i];

        parts[row].name = "LOD1";
        parts[row].filename = 0;
        parts[row].size = 200;
        parts[row].start = 0;
        strcpy(parts[row].label, "                ");
        part_count++;
        
        [self updateStarts];
        [self updateDictionary];
        [_tableView reloadData];
    }
}

- (IBAction)deletePartition:(id)sender
{
    NSInteger row = [_tableView selectedRow];

    if (part_count > 0)
    {
        for (unsigned short i = (unsigned short)row; i < part_count - 1; i++)
            parts[i] = parts[i + 1];

        part_count--;
    }

    [self updateStarts];
    [self updateDictionary];
    [_tableView reloadData];
}

- (void)handleAction:(id)sender
{
    NSInteger row = [sender clickedRow];
    NSInteger col = [sender clickedColumn];

    if (col == -1 || row == -1)
        return;

    printf("clicked row=%ld col=%ld\n", row, col);

    if (col == 5)
    {
        
    }
}

- (IBAction)dismissWindow:(id)sender
{
    int i;

    [[self window] orderOut:sender];
    [[self window] performClose:sender];

    NSFileManager *fmngr = [[NSFileManager alloc] init];
    diskFileName = [NSString stringWithFormat:@"%@/Documents/CADR.disk", NSHomeDirectory()];
    for (i = 1; ; i++)
    {
        if (![fmngr fileExistsAtPath:diskFileName])
            break;
        diskFileName = [NSString stringWithFormat:@"%@/Documents/CADR%d.disk", NSHomeDirectory(), i];
    }
    [fmngr release];
    
    NSSavePanel *savePanel = [NSSavePanel savePanel];
    
    if (i == 1)
        [savePanel setNameFieldStringValue:[NSString stringWithFormat:@"CADR.disk"]];
    else
        [savePanel setNameFieldStringValue:[NSString stringWithFormat:@"CADR%d.disk", i-1]];
    [savePanel setDirectoryURL:[NSURL fileURLWithPath:[NSString stringWithFormat:@"%@/Documents", NSHomeDirectory()]]];
    if ([savePanel runModal] ==  NSFileHandlingPanelOKButton)
        create_disk_image([[[savePanel URL] path] fileSystemRepresentation]);
}

@end
