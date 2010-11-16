/***************************************************************************
    PluginWindow.m
    PeopsSoftGPU
  
    Created by Gil Pedersen on Wed April 21 2004.
    Copyright (c) 2004 Gil Pedersen.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version. See also the license.txt file for *
 *   additional informations.                                              *
 *                                                                         *
 ***************************************************************************/

#import "PluginWindow.h"

@implementation NetSfPeopsSoftGPUPluginWindow
/*
- (BOOL)windowShouldClose:(id)sender
{
	[[NSNotificationCenter defaultCenter] postNotificationName:@"emuWindowDidClose" object:self];
	
	return YES;
}*/

- (void)sendEvent:(NSEvent *)theEvent
{
	int type = [theEvent type];
	if (type == NSKeyDown || type == NSKeyUp) {
		if (type == NSKeyDown && [theEvent keyCode] == 53 /* escape */) {
			// reroute to menu event
			[[NSApp mainMenu] performKeyEquivalent:theEvent];
		}
		
		// ignore all key Events
		return;
	}

	[super sendEvent:theEvent];
}

@end
