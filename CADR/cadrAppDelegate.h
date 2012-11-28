//
//  cadrAppDelegate.h
//  cadr
//
//  Created by Greg Gilley on 4/10/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface cadrAppDelegate : NSObject <NSApplicationDelegate>
{
    NSWindow *_window;
}

@property (assign) IBOutlet NSWindow *window;

@end
