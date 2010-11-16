/***************************************************************************
                         soft.h  -  description
                             -------------------
    begin                : Sun Oct 28 2001
    copyright            : (C) 2001 by Pete Bernert
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

//*************************************************************************// 
// History of changes:
//
// 2001/10/28 - Pete  
// - generic cleanup for the Peops release
//
//*************************************************************************// 

#ifndef _GPU_SOFT_H_
#define _GPU_SOFT_H_

// internally used defines

#define RED(x) (x & 0xff)
#define BLUE(x) ((x>>16) & 0xff)
#define GREEN(x) ((x>>8) & 0xff)
#define COLOR(x) (x & 0xffffff)

///////////////////////////////////////////////////////////////////////

void offsetPSXLine(void);
void offsetPSX2(void);
void offsetPSX3(void);
void offsetPSX4(void);

void FillSoftwareAreaTrans(short x0, short y0, short x1, short y1, unsigned short col);
void FillSoftwareArea(short x0, short y0, short x1, short y1, unsigned short col);
void drawPoly3G(int rgb1, int rgb2, int rgb3);
void drawPoly4G(int rgb1, int rgb2, int rgb3, int rgb4);
void drawPoly3F(int rgb);
void drawPoly4F(int rgb);
void drawPoly4FT(unsigned char *baseAddr);
void drawPoly4GT(unsigned char *baseAddr);
void drawPoly3FT(unsigned char *baseAddr);
void drawPoly3GT(unsigned char *baseAddr);
void DrawSoftwareSprite(unsigned char *baseAddr, short w, short h, int tx, int ty);
void DrawSoftwareSpriteTWin(unsigned char *baseAddr, int w, int h);
void DrawSoftwareSpriteMirror(unsigned char *baseAddr, int w, int h);

#endif // _GPU_SOFT_H_
