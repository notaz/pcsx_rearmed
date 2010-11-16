/***************************************************************************
                          stdafx.h  -  description
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

#define __X11_C_
#define __inline inline

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <math.h> 
#include <X11/cursorfont.h> 

#define CALLBACK /* */
#define __inline inline

#include "gl_ext.h"

#define SHADETEXBIT(x) ((x>>24) & 0x1)
#define SEMITRANSBIT(x) ((x>>25) & 0x1)

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT GL_BGRA
#endif
#define GL_COLOR_INDEX8_EXT 0x80E5
