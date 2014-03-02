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
    [wideIntegers setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"WideIntegers"]];
}

-(IBAction)changeCheckbox:(id)sender
{
    NSInteger state = [wideIntegers state];
    extern int wide_integer;

    [[NSUserDefaults standardUserDefaults] setBool:state forKey:@"WideIntegers"];
    [[NSUserDefaults standardUserDefaults] synchronize];
    
    wide_integer = (int)state;

    NSLog(@"wide integers changed %ld", state);
}
@end
