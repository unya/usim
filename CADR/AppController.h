//
//  AppController.h
//  usim
//
//  Created by Greg Gilley on 10/8/12.
//
//

#import <Foundation/Foundation.h>

#include "PreferenceController.h"

@interface AppController : NSObject {

    PreferenceController *preferenceController;
}

-(IBAction)showPreferencePanel:(id)sender;

-(IBAction)setSourceLocationPanel:(id)sender;

@end
