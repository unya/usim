//
//  BreakpointsController.m
//  usim
//
//  Created by Greg Gilley on 11/18/12.
//
//

#import "BreakpointsController.h"
#include "Breakpoints.h"

@interface BreakpointsController ()

@end

@implementation BreakpointsController

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
    self = [super initWithWindowNibName:@"Breakpoints"];
    
    return self;
}

- (void)windowDidLoad
{
    [super windowDidLoad];
    
    // Implement this method to handle any initialization after your window controller's window has been loaded from its nib file.
}

- (IBAction)dismissWindow:(id)sender
{
    [[self window] orderOut:sender];
    [[self window] performClose:sender];    
}

- (IBAction)addBreakpoint:(id)sender
{
    int breakpoint = addbreakpoint();

    [_tableView reloadData];

    [_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)breakpoint] byExtendingSelection:NO];
    [_tableView scrollRowToVisible:(NSInteger)breakpoint];
}

- (IBAction)deleteBreakpoint:(id)sender
{
    NSInteger row = [_tableView selectedRow];

    removebreakpoint(row);
    [_tableView reloadData];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    return (NSInteger)breakpoint_count;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    Breakpoint *breakpoint = &breakpoints[row];
    
    // In IB the tableColumn has the identifier set to the same string as the keys in our dictionary
    NSString *identifier = [tableColumn identifier];
    
    if ([identifier isEqualToString:@"EnableCell"]) {
        return [NSNumber numberWithInt:breakpoint->enabled];
    } else if ([identifier isEqualToString:@"LabelCell"]) {
        return breakpoint->identifier ? [NSString stringWithCString:breakpoint->identifier encoding:NSASCIIStringEncoding] : @"";
    } else {
        NSAssert1(NO, @"Unhandled table column identifier %@", identifier);
    }
    return nil;
}

- (void)tableView:(NSTableView *)tableView setObjectValue:(id)object forTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    NSString *identifier = [tableColumn identifier];
    Breakpoint *breakpoint = &breakpoints[row];
    
    if ([identifier isEqualToString:@"EnableCell"]) {
        breakpoint->enabled = [object intValue];
    } else if ([identifier isEqualToString:@"LabelCell"]) {
        const char *ident = [object cStringUsingEncoding:NSASCIIStringEncoding];
        
        if (ident && *ident != '\0')
        {
            if (breakpoint->identifier)
                free(breakpoint->identifier);
            breakpoint->identifier = malloc(strlen(ident) + 1);
            strcpy(breakpoint->identifier, ident);
        }
    } else {
        NSAssert1(NO, @"Unhandled table column identifier %@", identifier);
    }
}


@end
