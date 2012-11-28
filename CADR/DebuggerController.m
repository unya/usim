//
//  DebuggerController.m
//  usim
//
//  Created by Greg Gilley on 10/19/12.
//
//

#import "DebuggerController.h"
#include "syms.h"
#include "ucode.h"
#include "decode.h"
#include "DataTypes.h"

@interface DebuggerController ()

@end

int runpause = 1;
int step = 0;
int macromicro = 1;
DebuggerController *debugger;

@implementation DebuggerController

- (id)init
{
    self = [super initWithWindowNibName:@"Debugger"];
    return self;
}

- (id)initWithWindow:(NSWindow *)window
{
    self = [super initWithWindow:window];
    if (self) {
        // Initialization code here.
        debugger = self;
    }
    
    return self;
}

- (void)windowDidLoad
{
    [super windowDidLoad];
    
    if (runpause)
    {
        NSImage *image = [NSImage imageNamed:@"pause.png"];
        
        [runpauseItem setLabel:@"Pause"];
        [runpauseItem setImage:image];
    }
    else
    {
        NSImage *image = [NSImage imageNamed:@"play.png"];
        
        [runpauseItem setLabel:@"Run"];
        [runpauseItem setImage:image];
    }

    if (macromicro)
    {
        NSImage *image = [NSImage imageNamed:@"micro.png"];
        
        [macroItem setLabel:@"Micro"];
        [macroItem setImage:image];
    }
    else
    {
        NSImage *image = [NSImage imageNamed:@"macro.png"];
        
        [macroItem setLabel:@"Macro"];
        [macroItem setImage:image];
    }

    [self updateDisplay:0];    
}

- (void)updateDisplay:(unsigned int)pc
{
    extern unsigned int md;
    extern unsigned int vma;
    extern unsigned int q;
    extern unsigned int opc;
    extern int lc;
    extern int pdl_ptr;
    extern int pdl_index;
    extern int spc_stack_ptr;
    extern int u_pc;
    
    [mdvalue setStringValue:[NSString stringWithFormat:@"%o", md]];
    [vmavalue setStringValue:[NSString stringWithFormat:@"%o", vma]];
    [qvalue setStringValue:[NSString stringWithFormat:@"%o", q]];
    [opcvalue setStringValue:[NSString stringWithFormat:@"%o", opc]];
    [lcvalue setStringValue:[NSString stringWithFormat:@"%011o", lc]];
    [pdlpvalue setStringValue:[NSString stringWithFormat:@"%o", pdl_ptr]];
    [pdlivalue setStringValue:[NSString stringWithFormat:@"%o", pdl_index]];
    [spcvalue setStringValue:[NSString stringWithFormat:@"%o", spc_stack_ptr]];
    [upcvalue setStringValue:[NSString stringWithFormat:@"%o", u_pc]];

    if (pc)
    {
        if (macromicro)
        {
            char *disass(unsigned int fefptr, unsigned int loc, int even, unsigned int inst);
            char *name = find_function_name(pc);
            unsigned int instr;
            char *decoded;

            if (name)
                [instructionLabel setStringValue:[NSString stringWithCString:name encoding:NSASCIIStringEncoding]];
            else
                [instructionLabel setStringValue:@""];
            
            if (read_mem_debug(pc >> 2, &instr))
            {
                printf("fault reading lc\n");
            }

            if (pc & 2)
                instr = instr >> 16;
            decoded = disass(0, pc & 0377777777, 0, instr & 0xffff);
            [instruction setStringValue:[NSString stringWithCString:decoded encoding:NSASCIIStringEncoding]];
            
        }
        else
        {
    //        printf("%2d %011o ", i, pc);
            extern int prom_enabled_flag;
            extern ucw_t ucode[16*1024];
            int offset;
            char *sym;
            
            if ((sym = sym_find_last(!prom_enabled_flag, (int)pc, &offset)))
            {
                if (offset)
                    [instructionLabel setStringValue:[NSString stringWithFormat:@"%@+%o", [NSString stringWithCString:sym encoding:NSASCIIStringEncoding], offset]];
                else
                     [instructionLabel setStringValue:[NSString stringWithCString:sym encoding:NSASCIIStringEncoding]];
            }
            else
                [instructionLabel setStringValue:@""];

            ucw_t u = ucode[pc];
            [instruction setStringValue:[NSString stringWithCString:disassemble_ucode_loc(pc, u) encoding:NSASCIIStringEncoding]];
        }
    }
    
}

- (IBAction)runpause:(id)sender
{    
    runpause = !runpause;
    if (runpause)
    {
        NSImage *image = [NSImage imageNamed:@"pause.png"];
        
        [runpauseItem setLabel:@"Pause"];
        [runpauseItem setImage:image];
    }
    else
    {
        NSImage *image = [NSImage imageNamed:@"play.png"];

        [runpauseItem setLabel:@"Run"];
        [runpauseItem setImage:image];
    }
    printf("run/pause\n");
}

- (IBAction)step:(id)sender
{
    step++;
}

- (IBAction)macromicro:(id)sender
{
    printf("macro/micro\n");
    
    macromicro = !macromicro;
    if (macromicro)
    {
        NSImage *image = [NSImage imageNamed:@"micro.png"];
        
        [macroItem setLabel:@"Micro"];
        [macroItem setImage:image];
    }
    else
    {
        NSImage *image = [NSImage imageNamed:@"macro.png"];
        
        [macroItem setLabel:@"Macro"];
        [macroItem setImage:image];
    }
}
@end
