//
//  AppController.m
//  usim
//
//  Created by Greg Gilley on 10/8/12.
//
//

#import "AppController.h"

@implementation AppController

-(IBAction)showPreferencePanel:(id)sender
{
    if (!preferenceController)
    {
        preferenceController = [[PreferenceController alloc] init];
    }
    
    [preferenceController showWindow:self];
}

-(IBAction)setSourceLocationPanel:(id)sender
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    
    [openPanel setAllowsMultipleSelection:NO];
    [openPanel setCanChooseDirectories:YES];
    [openPanel setCanChooseFiles:NO];
    [openPanel setResolvesAliases:YES];
    [openPanel setTitle:@"Select the folder with the lisp sources:"];
    [openPanel setPrompt:@"Select"];
    
    if ([openPanel runModal] == NSOKButton)
    {
        NSURL *sourceFolder = (NSURL *)[[openPanel URLs] objectAtIndex:0];
        NSError *error;
        extern NSURL *sourceFolderURL;
        
        NSData *bookmark = [sourceFolder bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope includingResourceValuesForKeys:nil relativeToURL:nil error:&error];

        [[NSUserDefaults standardUserDefaults] setObject:bookmark forKey:@"SourceFolder"];
        [[NSUserDefaults standardUserDefaults] synchronize];

        sourceFolderURL = [sourceFolder retain];
        [sourceFolderURL startAccessingSecurityScopedResource];
    }
}

@end
