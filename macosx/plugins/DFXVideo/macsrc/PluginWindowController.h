/***************************************************************************
    PluginWindowController.h
    PeopsSoftGPU
  
    Created by Gil Pedersen on Mon April 11 2004.
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

#define PluginWindowController NetSfPeopsSoftGPUPluginWindowController

#import <Cocoa/Cocoa.h>
#import "PluginGLView.h"

@class PluginWindowController;

extern NSWindow *gameWindow;
extern PluginWindowController *gameController;

@interface PluginWindowController : NSWindowController
{
    IBOutlet NSOpenGLView *glView;
	 
	 NSWindow *fullWindow;
}

+ (id)openGameView;
- (PluginGLView *)getOpenGLView;
- (BOOL)fullscreen;
- (void)setFullscreen:(BOOL)flag;

@end
