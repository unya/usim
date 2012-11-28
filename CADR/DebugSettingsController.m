//
//  DebugSettingsController.m
//  usim
//
//  Created by Greg Gilley on 10/17/12.
//
//

#import "DebugSettingsController.h"

@interface DebugSettingsController ()

@end

@implementation DebugSettingsController

- (id)init
{
    self = [super initWithWindowNibName:@"DebugSettings"];
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
    extern int trace;
    extern int trace_mcr_labels_flag;
    extern int trace_lod_labels_flag;
    extern int trace_prom_flag;
    extern int trace_mcr_flag;
    extern int trace_io_flag;
    extern int trace_vm_flag;
    extern int trace_disk_flag;
    extern int trace_net_flag;
    extern int trace_int_flag;
    extern int trace_late_set;
    extern int trace_after_flag;

    [super windowDidLoad];
    
    // Implement this method to handle any initialization after your window controller's window has been loaded from its nib file.
    
    [traceCheckbox setState:trace];
    [traceMCRLabels setState:trace_mcr_labels_flag];
    [traceLODLabels setState:trace_lod_labels_flag];
    [tracePROM setState:trace_prom_flag];
    [traceMCR setState:trace_mcr_flag];
    [traceIO setState:trace_io_flag];
    [traceVM setState:trace_vm_flag];
    [traceDisk setState:trace_disk_flag];
    [traceNet setState:trace_net_flag];
    [traceInt setState:trace_int_flag];
    [traceLateSet setState:trace_late_set];
    [traceAfter setState:trace_after_flag];
}

- (IBAction)changeTrace:(id)sender {
    extern int trace;
    
    trace = [traceCheckbox state];
    [[NSUserDefaults standardUserDefaults] setInteger:trace forKey:@"Trace"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction)changeTraceMCRLabels:(id)sender {
    extern int trace_mcr_labels_flag;
    
    trace_mcr_labels_flag = [traceMCRLabels state];
    [[NSUserDefaults standardUserDefaults] setInteger:trace_mcr_labels_flag forKey:@"TraceMCR"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction)changeTraceLOD:(id)sender {
    extern int trace_lod_labels_flag;
    
    trace_lod_labels_flag = [traceLODLabels state];
    [[NSUserDefaults standardUserDefaults] setInteger:trace_lod_labels_flag forKey:@"TraceLODLabels"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction)changeTracePROM:(id)sender {
    extern int trace_prom_flag;
    
    trace_prom_flag = [tracePROM state];
    [[NSUserDefaults standardUserDefaults] setInteger:trace_prom_flag forKey:@"TracePROM"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction)changeTraceMCR:(id)sender {
    extern int trace_mcr_flag;
    
    trace_mcr_flag = [traceMCR state];
    [[NSUserDefaults standardUserDefaults] setInteger:trace_mcr_flag forKey:@"TraceMCR"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction)changeTraceIO:(id)sender {
    extern int trace_io_flag;
    
    trace_io_flag = [traceIO state];
    [[NSUserDefaults standardUserDefaults] setInteger:trace_io_flag forKey:@"TraceIO"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction)changeTraceVM:(id)sender {
    extern int trace_vm_flag;
    
    trace_vm_flag = [traceVM state];
    [[NSUserDefaults standardUserDefaults] setInteger:trace_vm_flag forKey:@"TraceVM"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction)changeTraceDisk:(id)sender {
    extern int trace_disk_flag;
    
    trace_disk_flag = [traceDisk state];
    [[NSUserDefaults standardUserDefaults] setInteger:trace_disk_flag forKey:@"TraceDisk"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction)changeTraceNet:(id)sender {
    extern int trace_net_flag;
    
    trace_net_flag = [traceNet state];
    [[NSUserDefaults standardUserDefaults] setInteger:trace_net_flag forKey:@"TraceNet"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction)changeTraceInt:(id)sender {
    extern int trace_int_flag;
    
    trace_int_flag = [traceInt state];
    [[NSUserDefaults standardUserDefaults] setInteger:trace_int_flag forKey:@"TraceInt"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction)changeTraceLateSet:(id)sender {
    extern int trace_late_set;
    
    trace_late_set = [traceLateSet state];
    [[NSUserDefaults standardUserDefaults] setInteger:trace_late_set forKey:@"TraceLateSet"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction)changeTraceAfter:(id)sender {
    extern int trace_after_flag;
    
    trace_after_flag = [traceAfter state];
    [[NSUserDefaults standardUserDefaults] setInteger:trace_after_flag forKey:@"TraceAfter"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction)dismissWindow:(id)sender {
    [[self window] orderOut:sender];
    [[self window] performClose:sender];
}

@end
