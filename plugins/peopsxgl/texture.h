/***************************************************************************
                          texture.h  -  description
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

#ifndef _GPU_TEXTURE_H_
#define _GPU_TEXTURE_H_

#define TEXTUREPAGESIZE 256 * 256

void           InitializeTextureStore();
void           CleanupTextureStore();
GLuint         LoadTextureWnd(int pageid, int TextureMode, uint32_t GivenClutId);
GLuint         LoadTextureMovie(void);
void           InvalidateTextureArea(int imageX0, int imageY0, int imageX1, int imageY1);
void           InvalidateTextureAreaEx(void);
void           LoadTexturePage(int pageid, int mode, short cx, short cy);
void           ResetTextureArea(BOOL bDelTex);
GLuint         SelectSubTextureS(int TextureMode, uint32_t GivenClutId);
void           CheckTextureMemory(void);

void           LoadSubTexturePage(int pageid, int mode, short cx, short cy);
void           LoadSubTexturePageSort(int pageid, int mode, short cx, short cy);
void           LoadPackedSubTexturePage(int pageid, int mode, short cx, short cy);
void           LoadPackedSubTexturePageSort(int pageid, int mode, short cx, short cy);
uint32_t       XP8RGBA(uint32_t BGR);
uint32_t       XP8RGBAEx(uint32_t BGR);
uint32_t       XP8RGBA_0(uint32_t BGR);
uint32_t       XP8RGBAEx_0(uint32_t BGR);
uint32_t       XP8BGRA_0(uint32_t BGR);
uint32_t       XP8BGRAEx_0(uint32_t BGR);
uint32_t       XP8RGBA_1(uint32_t BGR);
uint32_t       XP8RGBAEx_1(uint32_t BGR);
uint32_t       XP8BGRA_1(uint32_t BGR);
uint32_t       XP8BGRAEx_1(uint32_t BGR);
uint32_t       P8RGBA(uint32_t BGR);
uint32_t       P8BGRA(uint32_t BGR);
uint32_t       CP8RGBA_0(uint32_t BGR);
uint32_t       CP8RGBAEx_0(uint32_t BGR);
uint32_t       CP8BGRA_0(uint32_t BGR);
uint32_t       CP8BGRAEx_0(uint32_t BGR);
uint32_t       CP8RGBA(uint32_t BGR);
uint32_t       CP8RGBAEx(uint32_t BGR);
unsigned short XP5RGBA (unsigned short BGR);
unsigned short XP5RGBA_0 (unsigned short BGR);
unsigned short XP5RGBA_1 (unsigned short BGR);
unsigned short P5RGBA (unsigned short BGR);
unsigned short CP5RGBA_0 (unsigned short BGR);
unsigned short XP4RGBA (unsigned short BGR);
unsigned short XP4RGBA_0 (unsigned short BGR);
unsigned short XP4RGBA_1 (unsigned short BGR);
unsigned short P4RGBA (unsigned short BGR);
unsigned short CP4RGBA_0 (unsigned short BGR);

#endif // _TEXTURE_H_
