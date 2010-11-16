//
//  PluginList.h
//  Pcsx
//
//  Created by Gil Pedersen on Sun Sep 21 2003.
//  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "PcsxPlugin.h"

//extern NSMutableArray *plugins;

@interface PluginList : NSObject {
    
    @private
    NSMutableArray *pluginList;
	 
	 PcsxPlugin *activeGpuPlugin;
	 PcsxPlugin *activeSpuPlugin;
	 PcsxPlugin *activeCdrPlugin;
	 PcsxPlugin *activePadPlugin;
	 
	 BOOL missingPlugins;
}

+ (PluginList *)list;

- (void)refreshPlugins;
- (NSArray *)pluginsForType:(int)typeMask;
- (BOOL)hasPluginAtPath:(NSString *)path;
- (BOOL)configured;
- (PcsxPlugin *)activePluginForType:(int)type;
- (BOOL)setActivePlugin:(PcsxPlugin *)plugin forType:(int)type;

@end
