/***************************************************************************
                            draw.h  -  description
                             -------------------
    begin                : Sun Mar 08 2009
    copyright            : (C) 1999-2009 by Pete Bernert
    web                  : www.pbernert.com   
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

#ifndef _GL_DRAW_H_
#define _GL_DRAW_H_

// internally used defines

#define GPUCOMMAND(x) ((x>>24) & 0xff)
#define RED(x) (x & 0xff)
#define BLUE(x) ((x>>16) & 0xff)
#define GREEN(x) ((x>>8) & 0xff)
#define COLOR(x) (x & 0xffffff)

// prototypes
int  GLinitialize();
void GLcleanup();
BOOL offset2(void);
BOOL offset3(void);
BOOL offset4(void);
BOOL offsetline(void);
void offsetST(void);
void offsetBlk(void);
void offsetScreenUpload(int Position);
void assignTexture3(void);
void assignTexture4(void);
void assignTextureSprite(void);
void assignTextureVRAMWrite(void);
void SetOGLDisplaySettings (BOOL DisplaySet);
void ReadConfig(void);
void WriteConfig(void);
void SetExtGLFuncs(void);

///////////////////////////////////////////////////////////////////////

#endif // _GL_DRAW_H_
