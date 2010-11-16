//
//  PcsxPlugin.m
//  Pcsx
//
//  Created by Gil Pedersen on Fri Oct 03 2003.
//  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "PcsxPlugin.h"
#include "psxcommon.h"
#include "plugins.h"

@implementation PcsxPlugin

+ (NSString *)getPrefixForType:(int)aType
{
    switch (aType) {
        case PSE_LT_GPU: return @"GPU";
        case PSE_LT_CDR: return @"CDR";
        case PSE_LT_SPU: return @"SPU";
        case PSE_LT_PAD: return @"PAD";
        case PSE_LT_NET: return @"NET";
    }
    
    return @"";
}

+ (NSString *)getDefaultKeyForType:(int)aType
{
    //return @"Plugin" [PcsxPlugin getPrefixForType:aType];
    switch (aType) {
        case PSE_LT_GPU: return @"PluginGPU";
        case PSE_LT_CDR: return @"PluginCDR";
        case PSE_LT_SPU: return @"PluginSPU";
        case PSE_LT_PAD: return @"PluginPAD";
        case PSE_LT_NET: return @"PluginNET";
    }
    
    return @"";
}

+ (char **)getConfigEntriesForType:(int)aType
{
	static char *gpu[2] = {(char *)&Config.Gpu, NULL};
	static char *cdr[2] = {(char *)&Config.Cdr, NULL};
	static char *spu[2] = {(char *)&Config.Spu, NULL};
	static char *pad[3] = {(char *)&Config.Pad1, (char *)&Config.Pad2, NULL};
	static char *net[2] = {(char *)&Config.Net, NULL};

    switch (aType) {
        case PSE_LT_GPU: return (char **)gpu;
        case PSE_LT_CDR: return (char **)cdr;
        case PSE_LT_SPU: return (char **)spu;
        case PSE_LT_PAD: return (char **)pad;
        case PSE_LT_NET: return (char **)net;
    }

    return nil;
}

- (id)initWithPath:(NSString *)aPath 
{
    if (!(self = [super init])) {
        return nil;
    }
    
    PSEgetLibType    PSE_getLibType = NULL;
    PSEgetLibVersion PSE_getLibVersion = NULL;
    PSEgetLibName    PSE_getLibName = NULL;
    
    pluginRef = nil;
    name = nil;
    path = [aPath retain];
    NSString *fullPath = [[NSString stringWithCString:Config.PluginsDir] stringByAppendingPathComponent:path];
    
    pluginRef = SysLoadLibrary([fullPath fileSystemRepresentation]);
    if (pluginRef == nil) {
        [self release];
        return nil;
    }
    
    // TODO: add support for plugins with multiple functionalities???
    PSE_getLibType = (PSEgetLibType) SysLoadSym(pluginRef, "PSEgetLibType");
    if (SysLibError() != nil) {
        if (([path rangeOfString: @"gpu" options:NSCaseInsensitiveSearch]).length != 0)
            type = PSE_LT_GPU;
        else if (([path rangeOfString: @"cdr" options:NSCaseInsensitiveSearch]).length != 0)
            type = PSE_LT_CDR;
        else if (([path rangeOfString: @"spu" options:NSCaseInsensitiveSearch]).length != 0)
            type = PSE_LT_SPU;
        else if (([path rangeOfString: @"pad" options:NSCaseInsensitiveSearch]).length != 0)
            type = PSE_LT_PAD;
        else {
            [self release];
            return nil;
        }
    } else {
        type = (int)PSE_getLibType();
        if (type != PSE_LT_GPU && type != PSE_LT_CDR && type != PSE_LT_SPU && type != PSE_LT_PAD) {
            [self release];
            return nil;
        }
    }
    
    PSE_getLibName = (PSEgetLibName) SysLoadSym(pluginRef, "PSEgetLibName");
    if (SysLibError() == nil) {
        name = [[NSString alloc] initWithCString:PSE_getLibName()];
    }
    
    PSE_getLibVersion = (PSEgetLibVersion) SysLoadSym(pluginRef, "PSEgetLibVersion");
    if (SysLibError() == nil) {
        version = PSE_getLibVersion();
    }
    else {
        version = -1;
    }
    
    // save the current modification date
    NSDictionary *fattrs = [[NSFileManager defaultManager] fileAttributesAtPath:fullPath traverseLink:YES];
    modDate = [[fattrs fileModificationDate] retain];
    
    active = 0;
    
    return self;
}

- (void)dealloc
{
    int i;
    
    // shutdown if we had previously been inited
    for (i=0; i<32; i++) {
        if (active & (1 << i)) {
            [self shutdownAs:(1 << i)];
        }
    }
    
    if (pluginRef) SysCloseLibrary(pluginRef);
    
    [path release];
    [name release];
    
    [super dealloc];
}

- (void)runCommand:(id)arg
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSString *funcName = [arg objectAtIndex:0];
    long (*func)(void);
    
    func = SysLoadSym(pluginRef, [funcName lossyCString]);
    if (SysLibError() == nil) {
        func();
    } else {
        NSBeep();
    }
    
    [arg release];
    [pool release];
    return;
}

- (long)initAs:(int)aType
{
    char symbol[255];
    long (*init)(void);
    long (*initArg)(long arg);
    int res = PSE_ERR_FATAL;
    
    if ((active & aType) == aType) {
        return 0;
    }

    sprintf(symbol, "%sinit", [[PcsxPlugin getPrefixForType:aType] lossyCString]);
    init = initArg = SysLoadSym(pluginRef, symbol);
    if (SysLibError() == nil) {
        if (aType != PSE_LT_PAD)
            res = init();
        else
            res = initArg(1|2);
    }
    
    if (0 == res) {
        active |= aType;
    } else {
        NSRunCriticalAlertPanel(NSLocalizedString(@"Plugin Initialization Failed!", nil),
            [NSString stringWithFormat:NSLocalizedString(@"Pcsx failed to initialize the selected %s plugin (error=%i).\nThe plugin might not work with your system.", nil), [PcsxPlugin getPrefixForType:aType], res], 
			nil, nil, nil);
    }
    
    return res;
}

- (long)shutdownAs:(int)aType
{
    char symbol[255];
    long (*shutdown)(void);

    sprintf(symbol, "%sshutdown", [[PcsxPlugin getPrefixForType:aType] lossyCString]);
    shutdown = SysLoadSym(pluginRef, symbol);
    if (SysLibError() == nil) {
        active &= ~aType;
        return shutdown();
    }
    
    return PSE_ERR_FATAL;
}

- (BOOL)hasAboutAs:(int)aType
{
    char symbol[255];

    sprintf(symbol, "%sabout", [[PcsxPlugin getPrefixForType:aType] lossyCString]);
    SysLoadSym(pluginRef, symbol);
    
    return (SysLibError() == nil);
}

- (BOOL)hasConfigureAs:(int)aType
{
    char symbol[255];

    sprintf(symbol, "%sconfigure", [[PcsxPlugin getPrefixForType:aType] lossyCString]);
    SysLoadSym(pluginRef, symbol);
    
    return (SysLibError() == nil);
}

- (void)aboutAs:(int)aType
{
    NSArray *arg;
    char symbol[255];

    sprintf(symbol, "%sabout", [[PcsxPlugin getPrefixForType:aType] lossyCString]);
    arg = [[NSArray alloc] initWithObjects:[NSString stringWithCString:symbol], 
                    [NSNumber numberWithInt:0], nil];
    
    // detach a new thread
    [NSThread detachNewThreadSelector:@selector(runCommand:) toTarget:self 
            withObject:arg];
}

- (void)configureAs:(int)aType
{
    NSArray *arg;
    char symbol[255];
    
    sprintf(symbol, "%sconfigure", [[PcsxPlugin getPrefixForType:aType] lossyCString]);
    arg = [[NSArray alloc] initWithObjects:[NSString stringWithCString:symbol], 
                    [NSNumber numberWithInt:1], nil];
    
    // detach a new thread
    [NSThread detachNewThreadSelector:@selector(runCommand:) toTarget:self 
            withObject:arg];
}

- (NSString *)getDisplayVersion
{
    if (version == -1)
        return @"";
    
	 return [NSString stringWithFormat:@"v%ld.%ld.%ld", version>>16,(version>>8)&0xff,version&0xff];
}

- (int)getType
{
    return type;
}

- (NSString *)path
{
	return path;
}

- (unsigned)hash
{
    return [path hash];
}

- (NSString *)description
{
    if (name == nil)
        return [path lastPathComponent];
    
    return [NSString stringWithFormat:@"%@ %@ [%@]", name, [self getDisplayVersion], [path lastPathComponent]];
}

// the plugin will check if it's still valid and return the status
- (BOOL)verifyOK
{
    // check that the file is still there with the same modification date
    NSFileManager *dfm = [NSFileManager defaultManager];
    NSString *fullPath = [[NSString stringWithCString:Config.PluginsDir] stringByAppendingPathComponent:path];
    if (![dfm fileExistsAtPath:fullPath])
        return NO;
    
    NSDictionary *fattrs = [dfm fileAttributesAtPath:fullPath traverseLink:YES];
    return [[fattrs fileModificationDate] isEqualToDate:modDate];
}

@end
