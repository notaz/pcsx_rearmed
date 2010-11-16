/***************************************************************************
    PluginGLView.h
    PeopsSoftGPU
  
    Created by Gil Pedersen on Sun April 18 2004.
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

#define PluginGLView NetSfPeopsSoftGPUPluginGLView

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#include <sys/time.h>

#define IMAGE_COUNT  2

@interface PluginGLView : NSOpenGLView
{
	GLubyte  *image_base;
	GLubyte  *image[IMAGE_COUNT];
	
	GLint     buffers;
	//GLint     frame_rate;
	
	GLenum    texture_hint;
	GLboolean rect_texture;
	GLboolean client_storage;
	GLboolean texture_range;

	struct timeval cycle_time;
	
	NSLock *glLock;
	BOOL noDisplay;
	BOOL drawBG;

	int image_width;
	int image_height;
	int image_width2;
	int image_height2;
	int image_depth;
	int image_type;
	float image_tx;
	float image_ty;
	int whichImage;
	int isFullscreen;
}

- (void)renderScreen;
- (void)swapBuffer;
- (void)clearBuffer:(BOOL)display;
- (void)loadTextures: (GLboolean)first;

@end
