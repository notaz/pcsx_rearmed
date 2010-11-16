/***************************************************************************
    PluginGLView.m
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

#import <OpenGL/gl.h>
#import <OpenGL/glext.h>
#import <OpenGL/glu.h>
#import <GLUT/glut.h>
#import <Carbon/Carbon.h>
#import "PluginGLView.h"
#include "externals.h"
#undef BOOL
#include "gpu.h"
#include "swap.h"

#include <time.h>
extern time_t tStart;

static int mylog2(int val)
{
	int i;
	for (i=1; i<31; i++)
		if (val <= (1 << i))
			return (1 << i);
	
	return -1;
}

#if 0
void BlitScreen16NS(unsigned char * surf,long x,long y)
{
 unsigned long lu;
 unsigned short row,column;
 unsigned short dx=PreviousPSXDisplay.Range.x1>>1;
 unsigned short dy=PreviousPSXDisplay.DisplayMode.y;
 unsigned short LineOffset,SurfOffset;
 long lPitch=image_width<<1;

 if(PreviousPSXDisplay.Range.y0)								// centering needed?
  {
   surf+=PreviousPSXDisplay.Range.y0*lPitch;
   dy-=PreviousPSXDisplay.Range.y0;
  }

  {
   unsigned long * SRCPtr = (unsigned long *)(psxVuw + (y<<10) + x);
   unsigned long * DSTPtr = ((unsigned long *)surf)+(PreviousPSXDisplay.Range.x0>>1);

   LineOffset = 512 - dx;
   SurfOffset = (lPitch>>2) - dx;

   for(column=0;column<dy;column++)
    {
     for(row=0;row<dx;row++)
      {
       lu=GETLE16D(SRCPtr++);

       *DSTPtr++= lu;//((lu<<11)&0xf800f800)|((lu<<1)&0x7c007c0)|((lu>>10)&0x1f001f);
      }
     SRCPtr += LineOffset;
     DSTPtr += SurfOffset;
    }
  }
}
#endif

@implementation PluginGLView

//- (id)initWithFrame:(NSRect)frameRect
- (id) initWithCoder: (NSCoder *) coder
{
	const GLubyte * strExt;
	
	if ((self = [super initWithCoder:coder]) == nil)
		return nil;

	glLock = [[NSLock alloc] init];
	if (nil == glLock) {
		[self release];
		return nil;
	}

	// Init pixel format attribs
	NSOpenGLPixelFormatAttribute attrs[] =
	{
		NSOpenGLPFAAccelerated,
		NSOpenGLPFANoRecovery,
		NSOpenGLPFADoubleBuffer,
		0
	};

	// Get pixel format from OpenGL
	NSOpenGLPixelFormat* pixFmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
	if (!pixFmt)
	{
		NSLog(@"No Accelerated OpenGL pixel format found\n");
		
		NSOpenGLPixelFormatAttribute attrs2[] =
		{
			NSOpenGLPFANoRecovery,
			0
		};

		// Get pixel format from OpenGL
		pixFmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs2];
		if (!pixFmt) {
			NSLog(@"No OpenGL pixel format found!\n");
			
			[self release];
			return nil;
		}
	}
	
	[self setPixelFormat:[pixFmt autorelease]];

	/*
	long swapInterval = 1 ;
	[[self openGLContext]
			setValues:&swapInterval
			forParameter:NSOpenGLCPSwapInterval];
	*/
	[glLock lock];
	[[self openGLContext] makeCurrentContext];

	// Init object members
	strExt = glGetString (GL_EXTENSIONS);
	texture_range  = gluCheckExtension ((const unsigned char *)"GL_APPLE_texture_range", strExt) ? GL_TRUE : GL_FALSE;
	texture_hint   = GL_STORAGE_SHARED_APPLE ;
	client_storage = gluCheckExtension ((const unsigned char *)"GL_APPLE_client_storage", strExt) ? GL_TRUE : GL_FALSE;
	rect_texture   = gluCheckExtension((const unsigned char *)"GL_EXT_texture_rectangle", strExt) ? GL_TRUE : GL_FALSE;

	// Setup some basic OpenGL stuff
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	[NSOpenGLContext clearCurrentContext];
	[glLock unlock];

	image_width = 1024;
	image_height = 512;
	image_depth = 16;

	image_type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	image_base = (GLubyte *) calloc(((IMAGE_COUNT * image_width * image_height) / 3) * 4, image_depth >> 3);
	if (image_base == nil) {
		[self release];
		return nil;
	}

	// Create and load textures for the first time
	[self loadTextures:GL_TRUE];
	
	// Init fps timer
	//gettimeofday(&cycle_time, NULL);
	
	drawBG = YES;
	
	// Call for a redisplay
	noDisplay = YES;
	PSXDisplay.Disabled = 1;
	[self setNeedsDisplay:true];
	
	return self;
}

- (void)dealloc
{
	int i;

	[glLock lock];

	[[self openGLContext] makeCurrentContext];
	for(i = 0; i < IMAGE_COUNT; i++)
	{
		GLuint dt = i+1;
		glDeleteTextures(1, &dt);
	}
	if(texture_range) glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_EXT, IMAGE_COUNT * image_width * image_height * (image_depth >> 3), image_base);

	[NSOpenGLContext clearCurrentContext];
	[glLock unlock];
	[glLock release];

	if (image_base)
		free(image_base);

	[super dealloc];
}

- (BOOL)isOpaque
{
	return YES;
}

- (BOOL)acceptsFirstResponder
{
	return NO;
}

- (void)drawRect:(NSRect)aRect
{
	// Check if an update has occured to the buffer
	if ([self lockFocusIfCanDraw]) {

	// Make this context current
	if (drawBG) {
		[[NSColor blackColor] setFill];
		[NSBezierPath fillRect:[self visibleRect]];
	}

	//glFinish() ;
	// Swap buffer to screen
	//[[self openGLContext] flushBuffer];

	[self unlockFocus];
	}
}

#if 0
- (void)update  // moved or resized
{
	NSRect rect;

	[super update];

	[[self openGLContext] makeCurrentContext];
	[[self openGLContext] update];

	rect = [self bounds];
	
	glViewport(0, 0, (int) rect.size.width, (int) rect.size.height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity(); 

	//[self setNeedsDisplay:true];
}
#endif

- (void)reshape	// scrolled, moved or resized
{
	[glLock lock];

	NSOpenGLContext *oglContext = [self openGLContext];
	NSRect rect;

	[super reshape];

	[oglContext makeCurrentContext];
	[oglContext update];

	rect = [[oglContext view] bounds];

	glViewport(0, 0, (int) rect.size.width, (int) rect.size.height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	drawBG = YES;

	[NSOpenGLContext clearCurrentContext];
	
//	[self setNeedsDisplay:true];
	
	[self renderScreen];
	[glLock unlock];
}

- (void)renderScreen
{
	int bufferIndex = whichImage;

	if (1/*[glLock tryLock]*/) {
		// Make this context current
		[[self openGLContext] makeCurrentContext];
		if (PSXDisplay.Disabled) {
			glClear(GL_COLOR_BUFFER_BIT);
		} else {
			// Bind, update and draw new image
			if(rect_texture)
			{
				glBindTexture(GL_TEXTURE_RECTANGLE_EXT, bufferIndex+1);

				glTexSubImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0, image_width, image_height, GL_BGRA, image_type, image[bufferIndex]);
				glBegin(GL_QUADS);
					glTexCoord2f(0.0f, 0.0f);
					glVertex2f(-1.0f, 1.0f);
					
					glTexCoord2f(0.0f, image_height);
					glVertex2f(-1.0f, -1.0f);
					
					glTexCoord2f(image_width, image_height);
					glVertex2f(1.0f, -1.0f);
					
					glTexCoord2f(image_width, 0.0f);
					glVertex2f(1.0f, 1.0f);
				glEnd();
			}
			else
			{
				glBindTexture(GL_TEXTURE_2D, whichImage+1);
				
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image_width2, image_height2, GL_BGRA, image_type, image[bufferIndex]);
				glBegin(GL_QUADS);
					glTexCoord2f(0.0f, 0.0f);
					glVertex2f(-1.0f, 1.0f);
					
					glTexCoord2f(0.0f, image_ty);
					glVertex2f(-1.0f, -1.0f);
					
					glTexCoord2f(image_tx, image_ty);
					glVertex2f(1.0f, -1.0f);
					
					glTexCoord2f(image_tx, 0.0f);
					glVertex2f(1.0f, 1.0f);
				glEnd();
			}
		}
	
		// FPS Display
		if(ulKeybits&KEY_SHOWFPS)
		{
			int len, i;
			if(szDebugText[0] && ((time(NULL) - tStart) < 2))
			{
				strncpy(szDispBuf, szDebugText, 63);
			}
			else 
			{
				szDebugText[0]=0;
				if (szMenuBuf) {
					strncat(szDispBuf, szMenuBuf, 63 - strlen(szDispBuf));
				}
			}
			
			NSRect rect = [[[self openGLContext] view] bounds];
			len = (int) strlen(szDispBuf);
			
			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			
			gluOrtho2D(0.0, rect.size.width, 0.0, rect.size.height);
			glDisable(rect_texture ? GL_TEXTURE_RECTANGLE_EXT : GL_TEXTURE_2D);

			glColor4f(0.0, 0.0, 0.0, 0.5);
			glRasterPos2f(3.0, rect.size.height - 14.0);
			for (i = 0; i < len; i++) {
				glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, szDispBuf[i]);
			}

			glColor3f(1.0, 1.0, 1.0);
			glRasterPos2f(2.0, rect.size.height - 13.0);
			for (i = 0; i < len; i++) {
				glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, szDispBuf[i]);
			}


			glEnable(rect_texture ? GL_TEXTURE_RECTANGLE_EXT : GL_TEXTURE_2D);
			glPopMatrix();
		}
	
		[[self openGLContext] flushBuffer];
		[NSOpenGLContext clearCurrentContext];
		//[glLock unlock];
	}
}

- (void)loadTextures:(GLboolean)first
{
	GLint i;
	
	//[glLock lock];
	[[self openGLContext] makeCurrentContext];
	
	/*
	printf("Range.x0=%i\n"
			 "Range.x1=%i\n"
			 "Range.y0=%i\n"
			 "Range.y1=%i\n", 
			 PreviousPSXDisplay.Range.x0,
			 PreviousPSXDisplay.Range.x1,
			 PreviousPSXDisplay.Range.y0,
			 PreviousPSXDisplay.Range.y1);

	printf("DisplayMode.x=%d\n"
			 "DisplayMode.y=%d\n",
			 PreviousPSXDisplay.DisplayMode.x,
			 PreviousPSXDisplay.DisplayMode.y);

	printf("DisplayPosition.x=%i\n"
			 "DisplayPosition.y=%i\n",
			 PreviousPSXDisplay.DisplayPosition.x,
			 PreviousPSXDisplay.DisplayPosition.y);

	printf("DisplayEnd.x=%i\n"
			 "DisplayEnd.y=%i\n",
			 PreviousPSXDisplay.DisplayEnd.x,
			 PreviousPSXDisplay.DisplayEnd.y);
	
	printf("Double=%i\n"
			 "Height=%i\n",
			 PreviousPSXDisplay.Double,
			 PreviousPSXDisplay.Height);
	
	printf("Disabled=%i\n", PreviousPSXDisplay.Disabled);
	*/

	image_width = PreviousPSXDisplay.Range.x1;
	image_height = PreviousPSXDisplay.DisplayMode.y;
	if (PSXDisplay.RGB24) {
		image_depth = 32;
		image_type = GL_UNSIGNED_INT_8_8_8_8_REV;
	} else {
		image_depth = 16;
		image_type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
		//image_width >>= 1;
	}

	if (image_width * image_height * (image_depth >> 3) > ((1024*512*2)/3)*4)
		printf("Fatal error: desired dimension are too large! (%ix%i %ibpp)\n",
				 image_width, image_height, image_depth);

	for(i = 0; i < IMAGE_COUNT; i++)
		image[i] = image_base + i * image_width * image_height * (image_depth >> 3);

	if(rect_texture)
	{
		image_width2 = image_width;
		image_height2 = image_height;
		image_tx = (float)image_width;
		image_ty = (float)image_height;

		if(texture_range) glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_EXT, IMAGE_COUNT * image_width * image_height * (image_depth >> 3), image_base);
		else              glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_EXT, 0, NULL);

		for(i = 0; i < IMAGE_COUNT; i++)
		{
			if(!first)
			{
				GLuint dt = i+1;
				glDeleteTextures(1, &dt);
			}

			glDisable(GL_TEXTURE_2D);
			glEnable(GL_TEXTURE_RECTANGLE_EXT);
			glBindTexture(GL_TEXTURE_RECTANGLE_EXT, i+1);

			glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE , texture_hint);
			glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, client_storage);
			glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

			glTexImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA, image_width,
				image_height, 0, GL_BGRA, image_type, image[i]);
		}
	}
	else
	{
		image_width2 = mylog2(image_width);
		image_height2 = mylog2(image_height);
		image_tx = (float)image_width/(float)image_width2;
		image_ty = (float)image_height/(float)image_height2;

		glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_EXT, 0, NULL);
		if(texture_range) glTextureRangeAPPLE(GL_TEXTURE_2D, IMAGE_COUNT * image_width2 * image_height2 * (image_depth >> 3), image_base);
		else              glTextureRangeAPPLE(GL_TEXTURE_2D, 0, NULL);

		for(i = 0; i < IMAGE_COUNT; i++)
		{
			if(!first)
			{
				GLuint dt = i+1;
				glDeleteTextures(1, &dt);
			}

			glDisable(GL_TEXTURE_RECTANGLE_EXT);
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, i+1);

			//if(texture_range) glTextureRangeAPPLE(GL_TEXTURE_2D, IMAGE_COUNT * image_width2 * image_height2 * (image_depth >> 3), image_base);
			//else              glTextureRangeAPPLE(GL_TEXTURE_2D, 0, NULL);

			glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE , texture_hint);
			glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, client_storage);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width2,
				image_height2, 0, GL_BGRA, image_type, image[i]);
		}
	}

	[NSOpenGLContext clearCurrentContext];
	//[glLock unlock];
}

- (void)swapBuffer
{
	unsigned char * surf;
	long x = PSXDisplay.DisplayPosition.x;
	long y = PSXDisplay.DisplayPosition.y;
	unsigned long lu;
	unsigned short row,column;
	unsigned short dx=(unsigned short)PSXDisplay.DisplayEnd.x;//PreviousPSXDisplay.Range.x1;
	unsigned short dy=(unsigned short)PSXDisplay.DisplayEnd.y;//PreviousPSXDisplay.DisplayMode.y;
	long lPitch;

	//printf("y=%i",PSXDisplay.DisplayPosition.y);

	if ([glLock tryLock]) {
		// make sure the texture area is ready to be written to
		glFinishObjectAPPLE(GL_TEXTURE, 2-whichImage);

		if ((image_width != PreviousPSXDisplay.Range.x1) || 
			(image_height != PreviousPSXDisplay.DisplayMode.y) ||
			((PSXDisplay.RGB24 ? 32 : 16) != image_depth)) {
			[self loadTextures:NO];
		}
 
		surf = image[1-whichImage];
		lPitch=image_width2<<(image_depth >> 4);

		if(PreviousPSXDisplay.Range.y0)                       // centering needed?
		{
			surf+=PreviousPSXDisplay.Range.y0*lPitch;
			dy-=PreviousPSXDisplay.Range.y0;
		}

		if(PSXDisplay.RGB24)
		{
			unsigned char * pD;unsigned int startxy;

			surf+=PreviousPSXDisplay.Range.x0<<2;

			for(column=0;column<dy;column++)
			{ 
				startxy = (1024 * (column + y)) + x;
				pD = (unsigned char *)&psxVuw[startxy];

				row = 0;
				// make sure the reads are aligned
				while ((int)pD & 0x3) {
					*((unsigned long *)((surf)+(column*lPitch)+(row<<2))) =
						(*(pD+0)<<16)|(*(pD+1)<<8)|*(pD+2);

					pD+=3;
					row++;
				}

				for(;row<dx;row+=4)
				{
					unsigned long lu1 = *((unsigned long *)pD);
					unsigned long lu2 = *((unsigned long *)pD+1);
					unsigned long lu3 = *((unsigned long *)pD+2);
					unsigned long *dst = ((unsigned long *)((surf)+(column*lPitch)+(row<<2)));
#ifdef __POWERPC__
					*(dst)=
						(((lu1>>24)&0xff)<<16)|(((lu1>>16)&0xff)<<8)|(((lu1>>8)&0xff));
					*(dst+1)=
						(((lu1>>0)&0xff)<<16)|(((lu2>>24)&0xff)<<8)|(((lu2>>16)&0xff));
					*(dst+2)=
						(((lu2>>8)&0xff)<<16)|(((lu2>>0)&0xff)<<8)|(((lu3>>24)&0xff));
					*(dst+3)=
						(((lu3>>16)&0xff)<<16)|(((lu3>>8)&0xff)<<8)|(((lu3>>0)&0xff));
#else
					*(dst)=
						(((lu1>>0)&0xff)<<16)|(((lu1>>8)&0xff)<<8)|(((lu1>>16)&0xff));
					*(dst+1)=
						(((lu1>>24)&0xff)<<16)|(((lu2>>0)&0xff)<<8)|(((lu2>>8)&0xff));
					*(dst+2)=
						(((lu2>>16)&0xff)<<16)|(((lu2>>24)&0xff)<<8)|(((lu3>>0)&0xff));
					*(dst+3)=
						(((lu3>>8)&0xff)<<16)|(((lu3>>16)&0xff)<<8)|(((lu3>>24)&0xff));
#endif
					pD+=12;
				}

				//for(;row<dx;row+=4)
				/*while (pD&0x3) {
					*((unsigned long *)((surf)+(column*lPitch)+(row<<2)))=
						(*(pD+0)<<16)|(*(pD+1)<<8)|(*(pD+2)&0xff));
					pD+=3;
					row++;
				}*/
			}
		}
		else
		{
			int LineOffset,SurfOffset;
			unsigned long * SRCPtr = (unsigned long *)(psxVuw + (y << 10) + x);
			unsigned long * DSTPtr =
				((unsigned long *)surf) + (PreviousPSXDisplay.Range.x0 >> 1);

			dx >>= 1;

			LineOffset = 512 - dx;
			SurfOffset = (lPitch >> 2) - dx;

			for(column=0;column<dy;column++)
			{
				for(row=0;row<dx;row++)
				{
#ifdef __POWERPC__
					lu=GETLE16D(SRCPtr++);
#else
					lu=*SRCPtr++;
#endif
					*DSTPtr++=
						((lu << 10) & 0x7c007c00)|
						((lu) & 0x3e003e0)|
						((lu >> 10) & 0x1f001f);
				}
				SRCPtr += LineOffset;
				DSTPtr += SurfOffset;
			}
		}

	// Swap image buffer
		whichImage = 1 - whichImage;

		[self renderScreen];
		[glLock unlock];
	}
}

- (void)clearBuffer:(BOOL)display
{
	if (display == NO) {
		//[[self openGLContext] makeCurrentContext];
		//glClear(GL_COLOR_BUFFER_BIT);
		//[self loadTextures:NO];
	} else {
		noDisplay = YES;
//		[self setNeedsDisplay:true];
	}
}
/*
- (void)mouseDown:(NSEvent *)theEvent
{
	PluginWindowController *controller = [[self window] windowController];
	
	static unsigned long lastTime = 0;
	unsigned long time;
	
	time = TickCount();
	
	if (lastTime != 0) {
		if (time - lastTime > GetDblTime()) {
			if (isFullscreen) {
				[[self openGLContext] clearDrawable];
			} else {
				[[self openGLContext] setFullScreen];
			}
			isFullscreen = 1-isFullscreen;
			lastTime = 0;
			return;
		}
	}
	
	lastTime = time;
}*/

@end
