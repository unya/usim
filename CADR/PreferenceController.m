//
//  PreferenceController.m
//  usim
//
//  Created by Greg Gilley on 10/8/12.
//
//

#import "PreferenceController.h"

@interface PreferenceController ()

@end

@implementation PreferenceController

- (id)init
{
    self = [super initWithWindowNibName:@"Preferences"];
    return self;
}

- (id)initWithWindow:(NSWindow *)window
{
    self = [super initWithWindow:window];
    if (self) {
        // Initialization code here.
    }
    
    return self;
}

- (void)windowDidLoad
{
    [super windowDidLoad];
    
    // Implement this method to handle any initialization after your window controller's window has been loaded from its nib file.
}

-(IBAction)changeCheckbox:(id)sender
{
    NSInteger state = [checkbox state];

    NSLog(@"checkbox changed %ld", state);
}
@end
