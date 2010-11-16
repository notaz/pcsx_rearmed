/***************************************************************************
                           menu.c  -  description
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

#include "stdafx.h"

#define _IN_MENU

#include "externals.h"
#include "draw.h"
#include "menu.h"
#include "gpu.h"

uint32_t       dwCoreFlags=0;
PSXPoint_t     ptCursorPoint[8];
unsigned short usCursorActive=0;

////////////////////////////////////////////////////////////////////////
// field with menu chars... like good old C64 time :)
////////////////////////////////////////////////////////////////////////

GLubyte texrasters[40][12]= {

// 0,0 FPS
{0x00,0x60,0x60,0x60,0x60,0x60,0x7e,0x60,0x60,0x60,0x60,0x7f},
{0x00,0x18,0x18,0x18,0x18,0x18,0x1f,0x18,0x18,0x18,0x18,0x1f},
{0x00,0x03,0x06,0x00,0x00,0x00,0xc3,0x66,0x66,0x66,0x66,0xc3},
{0x00,0xf0,0x18,0x18,0x18,0x18,0xf0,0x00,0x00,0x00,0x18,0xf0},
// 4,0 0
{0x00,0x3c,0x66,0xc3,0xe3,0xf3,0xdb,0xcf,0xc7,0xc3,0x66,0x3c},
// 5,0 1
{0x00,0x7e,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x78,0x38,0x18},
// 6,0 2
{0x00,0xff,0xc0,0xc0,0x60,0x30,0x18,0x0c,0x06,0x03,0xe7,0x7e},
// 7,0 3

{0x00,0x7e,0xe7,0x03,0x03,0x07,0x7e,0x07,0x03,0x03,0xe7,0x7e},
// 0,1 4
{0x00,0x0c,0x0c,0x0c,0x0c,0x0c,0xff,0xcc,0x6c,0x3c,0x1c,0x0c},
// 1,1 5
{0x00,0x7e,0xe7,0x03,0x03,0x07,0xfe,0xc0,0xc0,0xc0,0xc0,0xff},
// 2,1 6
{0x00,0x7e,0xe7,0xc3,0xc3,0xc7,0xfe,0xc0,0xc0,0xc0,0xe7,0x7e},
// 3,1 7
{0x00,0x30,0x30,0x30,0x30,0x18,0x0c,0x06,0x03,0x03,0x03,0xff},
// 4,1 8
{0x00,0x7e,0xe7,0xc3,0xc3,0xe7,0x7e,0xe7,0xc3,0xc3,0xe7,0x7e},
// 5,1 9
{0x00,0x7e,0xe7,0x03,0x03,0x03,0x7f,0xe7,0xc3,0xc3,0xe7,0x7e},
// 6,1 smiley
{0x00,0x3c,0x42,0x99,0xa5,0x81,0xa5,0x81,0x42,0x3c,0x00,0x00},
// 7,1 sun
{0x00,0x08,0x49,0x2a,0x1c,0x7f,0x1c,0x2a,0x49,0x08,0x00,0x00},

// 0,2 fl + empty  box
{0xff,0x81,0x81,0x81,0xff,0x00,0x87,0x84,0x84,0xf4,0x84,0xf8},
// 1,2 fs + grey box
{0xff,0xab,0xd5,0xab,0xff,0x00,0x87,0x81,0x87,0xf4,0x87,0xf8},
// 2,2 od + filled box
{0xff,0xff,0xff,0xff,0xff,0x00,0x66,0x95,0x95,0x95,0x96,0x60},
// 3,2 fi + half grey box
{0xff,0xa1,0xd1,0xa1,0xff,0x00,0x82,0x82,0x82,0xe2,0x82,0xf8},
// 4,2 di + half filled box
{0xff,0xf1,0xf1,0xf1,0xff,0x00,0xe2,0x92,0x92,0x92,0x92,0xe0},
// 5,2 am  + grey box
{0xff,0xab,0xd5,0xab,0xff,0x00,0x95,0x95,0x95,0xf7,0x95,0x60},
// 6,2 ab  + filled box
{0xff,0xff,0xff,0xff,0xff,0x00,0x97,0x95,0x96,0xf5,0x96,0x60},
// 7,2 fa
{0x00,0x00,0x00,0x00,0x00,0x00,0x85,0x85,0x87,0xf5,0x82,0xf8},

// 0,3 fb
{0xff,0x8b,0x85,0x8b,0xff,0x00,0x82,0x82,0x82,0xe2,0x87,0xf8},
// 1,3 gf
{0xff,0x8f,0x8f,0x8f,0xff,0x00,0x74,0x94,0x96,0xb4,0x87,0x70},
// 2,3 D
{0xff,0x00,0xfc,0xc6,0xc3,0xc3,0xc3,0xc3,0xc6,0xfc,0x00,0xff}, 
// 3,3 G
{0xff,0x00,0x3e,0x63,0xc3,0xc7,0xc0,0xc0,0x63,0x3e,0x00,0xff},
// 4,3
{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
// 5,3
{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
// 6,3 tex pal smiley
{0x00,0x3c,0x7e,0xe7,0xdb,0xff,0xdb,0xff,0x7e,0x3c,0x00,0x00},
// 7,3
{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},

// 0,4 subtract blending (moon)
{0x00,0x06,0x1c,0x38,0x78,0x78,0x78,0x38,0x1c,0x06,0x00,0x00},
// 1,4 blurring
{0x00,0x7e,0x93,0xa5,0x93,0xc9,0x93,0xa5,0x93,0x7e,0x00,0x00},
// 2,4 (M)
{0xff,0x00,0xc3,0xc3,0xc3,0xdb,0xff,0xe7,0xc3,0x81,0x00,0xff},
// 3,4 (A)
{0xff,0x00,0xc3,0xc3,0xff,0xc3,0xc3,0x66,0x3c,0x18,0x00,0xff},
// 4,4 blank
{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
// 5,4
{0x00,0xfe,0xc5,0x62,0x35,0x18,0x0c,0xc6,0xc6,0x7c,0x00,0x00},
// 6,4  <-
{0x00,0x00,0x00,0x00,0x00,0x10,0x30,0x7f,0xff,0x7f,0x30,0x10},
// 7,4 .
{0x00,0x38,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
};

////////////////////////////////////////////////////////////////////////
// create lists/stuff for fonts 
// (as a matter of fact: no more display list used, just a texture)
////////////////////////////////////////////////////////////////////////

GLuint gTexFontName=0;
GLuint gTexPicName=0;
GLuint gTexCursorName=0;

void MakeDisplayLists(void)                            // MAKE FONT 
{
 GLubyte TexBytes[64][64][3];                          // we use a 64x64 texture
 int x,y,i,j,n=0; GLubyte col,IB;

 glPixelStorei(GL_UNPACK_ALIGNMENT,1);
 
 memset(TexBytes,0,64*64*3);

 for(y=0;y<5;y++)                                      // create texture out of raster infos
  {
   for(x=0;x<8;x++,n++)
    {
     for(i=0;i<12;i++)
      {
       IB=texrasters[n][i];
       for(j=0;j<8;j++)
        {
         if(IB&(1<<(7-j))) col=255; else col=0;
         TexBytes[y*12+i][x*8+j][0]=col;
         TexBytes[y*12+i][x*8+j][1]=col;
         TexBytes[y*12+i][x*8+j][2]=col;
        }
      }
    }
  }

 glGenTextures(1, &gTexFontName);                      // set tex params for font texture
 glBindTexture(GL_TEXTURE_2D, gTexFontName);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
 glTexImage2D(GL_TEXTURE_2D, 0, 3, 64, 64, 0, GL_RGB,
              GL_UNSIGNED_BYTE,TexBytes);
}

////////////////////////////////////////////////////////////////////////
// kill existing font stuff
////////////////////////////////////////////////////////////////////////

void KillDisplayLists(void)
{
 if(gTexFontName)                                      // del font/info textures
  {glDeleteTextures(1,&gTexFontName);gTexFontName=0;}
 if(gTexPicName) 
  {glDeleteTextures(1,&gTexPicName);gTexPicName=0;}
 if(gTexCursorName) 
  {glDeleteTextures(1,&gTexCursorName);gTexCursorName=0;}
}

////////////////////////////////////////////////////////////////////////
// display text/infos in gpu menu
////////////////////////////////////////////////////////////////////////

#ifdef OWNSCALE
#define DRAWTEXCHAR glTexCoord2f(fX1/256.0f,fY2/256.0f);glVertex3f(fXS1,fYS2,1.0f);glTexCoord2f(fX1/256.0f,fY1/256.0f);glVertex3f(fXS1,fYS1,1.0f);glTexCoord2f(fX2/256.0f,fY1/256.0f);glVertex3f(fXS2,fYS1,1.0f);glTexCoord2f(fX2/256.0f,fY2/256.0f);glVertex3f(fXS2,fYS2,1.0f);
#else
#define DRAWTEXCHAR glTexCoord2f(fX1,fY2);glVertex3f(fXS1,fYS2,1.0f);glTexCoord2f(fX1,fY1);glVertex3f(fXS1,fYS1,1.0f);glTexCoord2f(fX2,fY1);glVertex3f(fXS2,fYS1,1.0f);glTexCoord2f(fX2,fY2);glVertex3f(fXS2,fYS2,1.0f);
#endif

int iMPos=0;

void DisplayText(void)
{
 int iX,iY,i;
 GLfloat fX1,fY1,fX2,fY2,fYS1,fYS2,fXS1,fXS2,fXS,fXSC,fYSC,fYD;

 glDisable(GL_SCISSOR_TEST);                           // disable unwanted ogl states
 glDisable(GL_ALPHA_TEST);
 if(bOldSmoothShaded) {glShadeModel(GL_FLAT);bOldSmoothShaded=FALSE;}
 if(bBlendEnable)     {glDisable(GL_BLEND);bBlendEnable=FALSE;}
 if(!bTexEnabled)     {glEnable(GL_TEXTURE_2D);bTexEnabled=TRUE;}

 gTexName=gTexFontName;
 glBindTexture(GL_TEXTURE_2D,gTexFontName);            // now set font texture

 fYD=fYSC=(GLfloat)PSXDisplay.DisplayMode.y/(GLfloat)iResY; // some pre-calculations
 fYS1=12.0f*fYSC;fYSC*=13.0f;
 fYS2=0.0f;
 fXS= (GLfloat)PSXDisplay.DisplayMode.x/(GLfloat)iResX;
 fXSC= 8.0f*fXS;fXS*=10.0f;
 fXS1=0.0f;
 fXS2=50.0f*fXS;                                       // 3 is one option

#ifdef OWNSCALE
 vertex[0].c.lcol=0xff00ff00;                          // set menu text color
#else
 vertex[0].c.lcol=0xff00ffff;                          // set menu text color
#endif

 SETCOL(vertex[0]); 

 glBegin(GL_QUADS);

#ifdef OWNSCALE
 glTexCoord2f(128.0f/256.0f,240.0f/256.0f);            // make blank (ownscale)
 glVertex3f(fXS1,fYS2,0.99996f);
 glTexCoord2f(128.0f/256.0f,192.0f/256.0f);
 glVertex3f(fXS1,fYSC,0.99996f);
 glTexCoord2f(160.0f/256.0f,192.0f/256.0f);
 glVertex3f(fXS2,fYSC,0.99996f);
 glTexCoord2f(160.0f/256.0f,240.0f/256.0f);
 glVertex3f(fXS2,fYS2,0.99996f);
#else
 glTexCoord2f(128.0f,240.0f);                          // make blank
 glVertex3f(fXS1,fYS2,0.99996f);
 glTexCoord2f(128.0f,192.0f);
 glVertex3f(fXS1,fYSC,0.99996f);
 glTexCoord2f(160.0f,192.0f);
 glVertex3f(fXS2,fYSC,0.99996f);
 glTexCoord2f(160.0f,240.0f);
 glVertex3f(fXS2,fYS2,0.99996f);
#endif

 fXS1=0.0f;fXS2=4.0f*fXSC;                             // draw fps
 fX1=0.0f;  fX2=128.0f;
 fY1=0.0f; fY2=48.0f;
 DRAWTEXCHAR;

 fYSC=fXS1=3.0f*fXS;                                   // start pos of numbers

 i=0;do                                                // paint fps numbers
  {
   iX=4;iY=4;
   if(szDispBuf[i]>='0' && szDispBuf[i]<='3')
    {iX=4+szDispBuf[i]-'0';iY=0;}
   else
   if(szDispBuf[i]>='4' && szDispBuf[i]<='9')
    {iX=szDispBuf[i]-'4';iY=1;}
   else
   if(szDispBuf[i]=='.')
    {iX=7;iY=4;}
   else
   if(szDispBuf[i]==0) break;

   fX1=(GLfloat)iX*32.0f;  fX2=fX1+32.0f;
   fY1=(GLfloat)iY*48.0f; fY2=fY1+48.0f;
   fXS1+=fXS;
   fXS2=fXS1+fXSC;

   DRAWTEXCHAR;

   i++;
  }
 while(i);

 //----------------------------------------------------//
 // draw small chars
 //----------------------------------------------------//

 fXS1=12.0f*fXS;fYS1=6.0f*fYD;
 fY1=120.0f;fY2=144.0f;
 fX1=0.0f;fX2=32.0f;

 for(i=0;i<8;i++)
  {
   fXS2=fXS1+fXSC;
   DRAWTEXCHAR;
   fX1+=32.0f;fX2+=32.0f;fXS1+=fYSC;
  }

 fY1=168.0f;fY2=192.0f;
 fX1=0.0f;fX2=32.0f;

 for(i=0;i<2;i++)
  {
   fXS2=fXS1+fXSC;
   DRAWTEXCHAR;
   fX1+=32.0f;fX2+=32.0f;fXS1+=fYSC;
  }

 //----------------------------------------------------//

 fYSC=fXS+fXS;

 fYS1=12.0f*fYD;

 if(iBlurBuffer && gTexBlurName)                       // blur
  {
   fXS1-=fXS;fY1=192.0f;fY2=240.0f;
   fXS2=fXS1+fXSC;fX1=32.0f;fX2=64.0f;
   DRAWTEXCHAR;
   fXS1+=fXS;
  }

 fY1=48.0f;fY2=96.0f;
 
 if(bGLExt)                                            // packed pixel
  {
   fXS2=fXS1+fXSC;fX1=192.0f;fX2=224.0f;
   DRAWTEXCHAR;
  }

 if(glColorTableEXTEx)                                 // tex wnd pal
  {                                                    
   fY1=144.0f;fY2=192.0f;
   fXS2=fXS1+fXSC;fX1=192.0f;
   if(bGLExt) {fX2=208.0f;fXS2-=fXSC/2.0f;}
   else        fX2=224.0f;
   DRAWTEXCHAR;
   fY1=48.0f;fY2=96.0f;
  }

 if(!bUseMultiPass && glBlendEquationEXTEx)            // multipass blend
  {
   fY1=192.0f;fY2=240.0f;
   fXS1+=fYSC-fXSC;fXS2=fXS1+fXSC;fX1=0.0f;fX2=32.0f;
   DRAWTEXCHAR;
   fXS1+=fXSC;
   fY1=48.0f;fY2=96.0f;
  }
 else fXS1+=fYSC;

 if(bGLBlend)                                         // modulate2x
  {
   fXS2=fXS1+fXSC;fX1=224.0f;fX2=256.0f;
   DRAWTEXCHAR;
  }

 fY1=192.0f;fY2=240.0f;

 if(iHiResTextures)                                    // 2x textures
  {
   fXS1+=fYSC-fXS;fXS2=fXS1+fXSC;
   fX1=160.0f;fX2=192.0f;
   DRAWTEXCHAR;
   fXS1+=fXS;
  }
 else fXS1+=fYSC;

 if(dwCoreFlags&1)                                     //A   
  {
   fXS2=fXS1+fXSC;fX1=96.0f;fX2=128.0f;
   DRAWTEXCHAR;
  }

 if(dwCoreFlags&2)                                     //M
  {
   fXS2=fXS1+fXSC;fX1=64.0f;fX2=96.0f;
   DRAWTEXCHAR;
  }

                                                       // 00 -> digital, 01 -> analog, 02 -> mouse, 03 -> gun
 if(dwCoreFlags&0xff00)                                //A/M/G/D   
  {
   int k;

   fXS2=fXS1+fXSC;

   if((dwCoreFlags&0x0f00)==0x0000)                    // D
    {
     fY1=144.0f;fY2=192.0f;
     fX1=64.0f;fX2=96.0f;
    }
   else
   if((dwCoreFlags&0x0f00)==0x0100)                    // A
    {
     fX1=96.0f;fX2=128.0f;
    }
   else
   if((dwCoreFlags&0x0f00)==0x0200)                    // M
    {
     fX1=64.0f;fX2=96.0f;
    }
   else
   if((dwCoreFlags&0x0f00)==0x0300)                    // G
    {
     fY1=144.0f;fY2=192.0f;
     fX1=96.0f;fX2=128.0f;
    }
   DRAWTEXCHAR;

   k=(dwCoreFlags&0xf000)>>12;                         // number
   fXS1+=fXS;
   fXS2=fXS1+fXSC;
   iX=4;iY=4;
   if(k>=0 && k<=3)
    {iX=4+k;iY=0;}
   else
   if(k>=4 && k<=9)
    {iX=k-4;iY=1;}
   fX1=(GLfloat)iX*32.0f; fX2=fX1+32.0f;
   fY1=(GLfloat)iY*48.0f; fY2=fY1+48.0f;
   DRAWTEXCHAR;
  }

 fXS1+=fYSC;

 if(lSelectedSlot)                                     // save state num
  {
   fXS2=fXS1+fXSC;
   iX=4;iY=4;
   if(lSelectedSlot>=0 && lSelectedSlot<=3)
    {iX=4+lSelectedSlot;iY=0;}
   else
   if(lSelectedSlot>=4 && lSelectedSlot<=9)
    {iX=lSelectedSlot-4;iY=1;}
   fX1=(GLfloat)iX*32.0f; fX2=fX1+32.0f;
   fY1=(GLfloat)iY*48.0f; fY2=fY1+48.0f;
   DRAWTEXCHAR;
  }

 fXS1=(GLfloat)(13+iMPos*3)*fXS;fXS2=fXS1+fXSC;        // arrow
 fX1=192.0f; fX2=224.0f;
 fY1=192.0f; fY2=240.0f;
 DRAWTEXCHAR;

 /////////////////

 fXS1=12.0f*fXS;fXS2=fXS1+fXSC;
 fYS2=6.0f*fYD;fYSC=3.0f*fXS;
 fY1=96.0f;fY2=120.0f;

 if(bUseFrameLimit)                                    // frame limit
  {
   if(iFrameLimit==2) {fX1=64.0f;fX2=96.0f;}
   else               {fX1=32.0f;fX2=64.0f;}
  }
 else                 {fX1=0.0f ;fX2=32.0f;}
 DRAWTEXCHAR; 
 fXS1+=fYSC;fXS2=fXS1+fXSC;

 if(bUseFrameSkip)    {fX1=64.0f;fX2=96.0f;}           // frame skip
 else                 {fX1=0.0f ;fX2=32.0f;}
 DRAWTEXCHAR; 
 fXS1+=fYSC;fXS2=fXS1+fXSC;

 if(iOffscreenDrawing) fX1=(iOffscreenDrawing+2)*32.0f;// offscreen drawing
 else                  fX1=0.0f;
 fX2=fX1+32.0f;
 DRAWTEXCHAR; 
 fXS1+=fYSC;fXS2=fXS1+fXSC;

 if(iFilterType<5) fX1=iFilterType*32.0f;              // texture filter
 else             {fX1=(iFilterType-5)*32.0f;fY1=144.0f;fY2=168.0f;}
 fX2=fX1+32.0f;                             
 DRAWTEXCHAR; 
 if(iFilterType>=5) {fY1=96.0f;fY2=120.0f;}
 fXS1+=fYSC;fXS2=fXS1+fXSC;

 if(bDrawDither)    {fX1=64.0f;fX2=96.0f;}             // dithering
 else               {fX1=0.0f ;fX2=32.0f;}
 DRAWTEXCHAR; 
 fXS1+=fYSC;fXS2=fXS1+fXSC;

 if(bOpaquePass)    {fX1=64.0f;fX2=96.0f;}             // opaque pass
 else              {fX1=0.0f ;fX2=32.0f;}
 DRAWTEXCHAR; 
 fXS1+=fYSC;fXS2=fXS1+fXSC;

 if(bAdvancedBlend) {fX1=64.0f;fX2=96.0f;}             // advanced blend
 else               {fX1=0.0f ;fX2=32.0f;}
 DRAWTEXCHAR; 
 fXS1+=fYSC;fXS2=fXS1+fXSC;

 if(!iFrameReadType) fX1=0.0f;                         // framebuffer reading
 else if(iFrameReadType==2) {fX1=0.0f;fY1=144.0f;fY2=168.0f;}
 else fX1=(iFrameReadType+2)*32.0f;
 fX2=fX1+32.0f;
 DRAWTEXCHAR; 
 if(iFrameReadType==2) {fY1=96.0f;fY2=120.0f;}
 fXS1+=fYSC;fXS2=fXS1+fXSC;

 if(iFrameTexType<2) fX1=iFrameTexType*32.0f;          // frame texture
 else                fX1=iFrameTexType*64.0f;
 fX2=fX1+32.0f;
 DRAWTEXCHAR; 
 fXS1+=fYSC;fXS2=fXS1+fXSC;

 if(dwActFixes)    {fX1=64.0f;fX2=96.0f;}              // game fixes
 else              {fX1=0.0f ;fX2=32.0f;}
 DRAWTEXCHAR; 
 fXS1+=fYSC;fXS2=fXS1+fXSC;

 /////////////////

 glEnd();

 glEnable(GL_ALPHA_TEST);                              // repair needed states
 glEnable(GL_SCISSOR_TEST);                       
}
 
////////////////////////////////////////////////////////////////////////

void HideText(void)
{
 GLfloat fYS1,fYS2,fXS1,fXS2,fXS,fXSC,fYSC;

 glDisable(GL_SCISSOR_TEST);                           // turn off unneeded ogl states
 glDisable(GL_ALPHA_TEST);
 if(bOldSmoothShaded) {glShadeModel(GL_FLAT);bOldSmoothShaded=FALSE;}
 if(bBlendEnable)     {glDisable(GL_BLEND);bBlendEnable=FALSE;}
 if(bTexEnabled)      {glDisable(GL_TEXTURE_2D);bTexEnabled=FALSE;}

 fYSC=(GLfloat)PSXDisplay.DisplayMode.y/(GLfloat)iResY;
 fYS1=12.0f*fYSC;fYSC*=13.0f;
 fYS2=0.0f;
 fXS= (GLfloat)PSXDisplay.DisplayMode.x/(GLfloat)iResX;
 fXSC= 8.0f*fXS;fXS*=10.0f;
 fXS1=0.0f;
 fXS2=50.0f*fXS;                                       

 vertex[0].c.lcol=0xff000000;                          // black color
 SETCOL(vertex[0]); 

 glBegin(GL_QUADS);                                    // make one quad

 glVertex3f(fXS1,fYS2,0.99996f);
 glVertex3f(fXS1,fYSC,0.99996f);
 glVertex3f(fXS2,fYSC,0.99996f);
 glVertex3f(fXS2,fYS2,0.99996f);

 glEnd();
 glEnable(GL_ALPHA_TEST);                              // enable needed ogl states
 glEnable(GL_SCISSOR_TEST);                       
}

////////////////////////////////////////////////////////////////////////
// Build Menu buffer (== Dispbuffer without FPS)
////////////////////////////////////////////////////////////////////////

void BuildDispMenu(int iInc)
{
 if(!(ulKeybits&KEY_SHOWFPS)) return;                  // mmm, cheater ;)

 iMPos+=iInc;                                          // up or down
 if(iMPos<0) iMPos=9;                                  // wrap around
 if(iMPos>9) iMPos=0;                                  
}

////////////////////////////////////////////////////////////////////////
// gpu menu actions...
////////////////////////////////////////////////////////////////////////

void SwitchDispMenu(int iStep)
{
 if(!(ulKeybits&KEY_SHOWFPS)) return;                   // tststs

 switch(iMPos)
  {//////////////////////////////////////////////////////
   case 0:                                             // frame limit
    {
     int iType=0;
     bInitCap = TRUE;

     if(bUseFrameLimit) iType=iFrameLimit;
     iType+=iStep;
     if(iType<0) iType=2;
     if(iType>2) iType=0;
     if(iType==0) bUseFrameLimit=FALSE;
     else
      {
       bUseFrameLimit=TRUE;
       iFrameLimit=iType;
       SetAutoFrameCap();
      }
    }
    break;
   //////////////////////////////////////////////////////
   case 1:                                             // frame skip
    bInitCap = TRUE;
    bUseFrameSkip=!bUseFrameSkip;
    bSkipNextFrame=FALSE;
    break;
   //////////////////////////////////////////////////////
  case 2:                                              // offscreen drawing
    iOffscreenDrawing+=iStep;                          
    if(iOffscreenDrawing>4) iOffscreenDrawing=0;
    if(iOffscreenDrawing<0) iOffscreenDrawing=4;
    break;
   //////////////////////////////////////////////////////
   case 3:                                             // filtering
    ulKeybits|=KEY_RESETTEXSTORE;
    ulKeybits|=KEY_RESETFILTER;
    if(iStep==-1) ulKeybits|=KEY_STEPDOWN;
    break;
   //////////////////////////////////////////////////////
   case 4:                                             // dithering
    ulKeybits|=KEY_RESETTEXSTORE;
    ulKeybits|=KEY_RESETDITHER;
    break;
   //////////////////////////////////////////////////////
   case 5:                                             // alpha multipass
    ulKeybits|=KEY_RESETTEXSTORE;
    ulKeybits|=KEY_RESETOPAQUE;
    break;
   //////////////////////////////////////////////////////
   case 6:                                             // advanced blending
    ulKeybits|=KEY_RESETTEXSTORE;
    ulKeybits|=KEY_RESETADVBLEND;
    break;
   //////////////////////////////////////////////////////
   case 7:                                             // full vram
    ulKeybits|=KEY_RESETTEXSTORE;
    ulKeybits|=KEY_TOGGLEFBREAD;
    if(iStep==-1) ulKeybits|=KEY_STEPDOWN;
    break;
   //////////////////////////////////////////////////////
   case 8:                                             // frame buffer texture
    ulKeybits|=KEY_RESETTEXSTORE;
    ulKeybits|=KEY_TOGGLEFBTEXTURE;
    if(iStep==-1) ulKeybits|=KEY_STEPDOWN;
    break;
   //////////////////////////////////////////////////////
   case 9:                                             // game fixes
    ulKeybits|=KEY_RESETTEXSTORE;
    ulKeybits|=KEY_BLACKWHITE;
    break;
   //////////////////////////////////////////////////////
  }

 BuildDispMenu(0);                                     // update info
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// Here comes my painting zone... just to paint stuff... like 3DStudio ;)
////////////////////////////////////////////////////////////////////////


/*
 12345678
1
2
3
4
5
6
7
8
9
0
1
2
3


{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}


 12345678
3
2
1
0
9
8
7
6
5  111
4  111
3
2
1

{0x00,0x00,0x00,0x38,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}


 12345678
3  1111
2 11  11
111    11
011   111
911  1111
811 11 11
71111  11
6111   11
511    11
4 11  11
3  1111
2
1

// 0
{0x00,0x00,0x3c,0x66,0xc3,0xe3,0xf3,0xdb,0xcf,0xc7,0xc3,0x66,0x3c}
// 1
{0x00,0x00,0x7e,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x78,0x38,0x18}
// 2
{0x00,0x00,0xff,0xc0,0xc0,0x60,0x30,0x18,0x0c,0x06,0x03,0xe7,0x7e}
// 3
{0x00,0x00,0x7e,0xe7,0x03,0x03,0x07,0x7e,0x07,0x03,0x03,0xe7,0x7e}
// 4
{0x00,0x00,0x0c,0x0c,0x0c,0x0c,0x0c,0xff,0xcc,0x6c,0x3c,0x1c,0x0c}
// 5
{0x00,0x00,0x7e,0xe7,0x03,0x03,0x07,0xfe,0xc0,0xc0,0xc0,0xc0,0xff}
// 6
{0x00,0x00,0x7e,0xe7,0xc3,0xc3,0xc7,0xfe,0xc0,0xc0,0xc0,0xe7,0x7e}
// 7
{0x00,0x00,0x30,0x30,0x30,0x30,0x18,0x0c,0x06,0x03,0x03,0x03,0xff}
// 8
{0x00,0x00,0x7e,0xe7,0xc3,0xc3,0xe7,0x7e,0xe7,0xc3,0xc3,0xe7,0x7e}
// 9
{0x00,0x00,0x7e,0xe7,0x03,0x03,0x03,0x7f,0xe7,0xc3,0xc3,0xe7,0x7e}

 12345678123456781234567812345678
3 11111111  1111111    111111
2 11        11    11  11    11
1 11        11    11  11
0 11        11    11  11
9 11        11    11  11
8 111111    1111111    111111
7 11        11              11
6 11        11              11
5 11        11              11
4 11        11        11    11
3 11        11         111111
2          
          
{0x00,0x60,0x60,0x60,0x60,0x60,0x7e,0x60,0x60,0x60,0x60,0x7f},
{0x00,0x18,0x18,0x18,0x18,0x18,0x1f,0x18,0x18,0x18,0x18,0x1f},
{0x00,0x03,0x06,0x00,0x00,0x00,0xc3,0x66,0x66,0x66,0x66,0xc3},
{0x00,0xf0,0x18,0x18,0x18,0x18,0xf0,0x00,0x00,0x00,0x18,0xf0},

 12345678
311111111  0xff
211        0xc0
111        0xc0
011        0xc0
911        0xc0
8111111    0xfc
711        0xc0
611        0xc0
511        0xc0
411        0xc0
311        0xc0
2          0x00
1          0x00

{0x00,0x00,0xc0,0xc0,0xc0,0xc0,0xc0,0x3f,0xc0,0xc0,0xc0,0xc0,0xff}


 12345678
31111111   0xfe
211    11  0xc3
111    11  0xc3
011    11  0xc3
911    11  0xc3
81111111   0xfe
711        0xc0
611        0xc0
511        0xc0
411        0xc0
311        0xc0
2          0x00
1          0x00


{0x00,0x00,0xc0,0xc0,0xc0,0xc0,0xc0,0x7f,0xc3,0xc3,0xc3,0xc3,0x7f}

 12345678
3 111111   0x7e
211    11  0xc3
111        0xc0
011        0xc0
911        0xc0
8 111111   0x7e
7      11  0x03
6      11  0x03
5      11  0x03
411    11  0xc3
3 111111   0x7e
2          0x00
1          0x00

{0x00,0x00,0x7e,0xc3,0x03,0x03,0x03,0x7e,0xc0,0xc0,0xc0,0xc3,0x7e}

 12345678
3          0x00
2 1111111  0x7f
1 11       0x60
0 11       0x60
9 11111    0x7c
8 11       0x60
7 11       0x60
6 11       0x60
5          0x00
4          0x00
3          0x00
2          0x00
1          0x00

{0x00,0x00,0x00,0x00,0x00,0x60,0x60,0x60,0x7c,0x60,0x60,0x7f,0x00}

 12345678
3          0x00
2 1111111  0x7f
1 11       0x60
0 11       0x60
9 11111    0x7c
8 11       0x60
7 11       0x60
6 11       0x60
5          0x00
4    1     0x08
3   111    0x1c
2  11111   0x3e
1 1111111  0x7f

{0x7f,0x3e,0x1c,0x08,0x00,0x60,0x60,0x60,0x7c,0x60,0x60,0x7f,0x00}

 12345678
3          0x00
2 11   11  0x63
1 11   11  0x63
0 11   11  0x63
9 11   11  0x63
8 11 1 11  0x6b
7 1111111  0x7f
6  11 11   0x36
5          0x00
4          0x00
3          0x00
2          0x00
1          0x00

{0x00,0x00,0x00,0x00,0x00,0x36,0x7f,0x6b,0x63,0x63,0x63,0x63,0x00}

 12345678
3          0x00
2 11   11  0x63
1 11   11  0x63
0 11   11  0x63
9 11   11  0x63
8 11 1 11  0x6b
7 1111111  0x7f
6  11 11   0x36
5          0x00
4    1     0x08
3   111    0x1c
2  11111   0x3e
1 1111111  0x7f

{0x7f,0x3e,0x1c,0x08,0x00,0x36,0x7f,0x6b,0x63,0x63,0x63,0x63,0x00}


 12345678
3          0x00
2    1     0x08
1   111    0x1c
0  11 11   0x36
9 11   11  0x63
8 1111111  0x7f
7 11   11  0x63
6 11   11  0x63
5          0x00
4          0x00
3          0x00
2          0x00
1          0x00

{0x00,0x00,0x00,0x00,0x00,0x63,0x63,0x7f,0x63,0x36,0x1c,0x08,0x00}

 12345678
3          0x00
2    1     0x08
1   111    0x1c
0  11 11   0x36
9 11   11  0x63
8 1111111  0x7f
7 11   11  0x63
6 11   11  0x63
5          0x00
4    1     0x08
3   111    0x1c
2  11111   0x3e
1 1111111  0x7f

{0x7f,0x3e,0x1c,0x08,0x00,0x63,0x63,0x7f,0x63,0x36,0x1c,0x08,0x00}

 12345678
3          0x00
2  11111   0x3e
1 11   11  0x63
0 11   11  0x63
9 11   11  0x63
8 11   11  0x63
7 11   11  0x63
6  11111   0x3e
5          0x00
4          0x00
3          0x00
2          0x00
1          0x00

{0x00,0x00,0x00,0x00,0x00,0x3e,0x63,0x63,0x63,0x63,0x63,0x3e,0x00}

 12345678
3          0x00
2  11111   0x3e
1 11   11  0x63
0 11   11  0x63
9 11   11  0x63
8 11   11  0x63
7 11   11  0x63
6  11111   0x3e
5          0x00
4    1     0x08
3   111    0x1c
2  11111   0x3e
1 1111111  0x7f

{0x7f,0x3e,0x1c,0x08,0x00,0x3e,0x63,0x63,0x63,0x63,0x63,0x3e,0x00}

 12345678
3   1      0x10
2  11      0x30
1 111      0x70
011111111  0xff
9 111      0x70
8  11      0x30
7   1      0x10
6          0x00
5          0x00
4          0x00
3          0x00
2          0x00
1          0x00

{0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x30,0x70,0xff,0x70,0x30,0x10}

 12345678
3   1      0x10
2  11      0x30
1 1111111  0x7f
011111111  0xff
9 1111111  0x7f
8  11      0x30
7   1      0x10
6          0x00
5          0x00
4          0x00
3          0x00
2          0x00
1          0x00

{0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x30,0x7f,0xff,0x7f,0x30,0x10}

///////////////////////////////////////////////////////////////////////////////////////

 12345678
3          0x00
211111     0xf8
11    1 1  0x85
01111 1 1  0xf5
91    1 1  0x85
81    1 1  0x85
71     1   0x82
6          0x00
5          0x00
4          0x00
3          0x00
2          0x00
1          0x00

{0x00,0x00,0x00,0x00,0x00,0x00,0x82,0x85,0x85,0xf5,0x85,0xf8,0x00},

///////////////////////////////////////////////////////////////////////////////////////

 12345678
3          0x00
211111     0xf8
11    111  0x87
01111 1    0xf4
91    111  0x87
81      1  0x81
71    111  0x87
6          0x00
5          0x00
4          0x00
3          0x00
2          0x00
1          0x00

{0x00,0x00,0x00,0x00,0x00,0x00,0x87,0x81,0x87,0xf4,0x87,0xf8,0x00},

 12345678
3          0x00
211111     0xf8
11    1    0x84
01111 1    0xf4
91    1    0x84
81    1    0x84
71    111  0x87
6          0x00
5          0x00
4          0x00
3          0x00
2          0x00
1          0x00

{0x00,0x00,0x00,0x00,0x00,0x00,0x87,0x84,0x84,0xf4,0x84,0xf8,0x00},

 12345678
3          0x00
2 11       0x60
11  1 11   0x96
01  1 1 1  0x95
91  1 1 1  0x95
81  1 1 1  0x95
7 11  11   0x66
6          0x00
5          0x00
4          0x00
3          0x00
2          0x00
1          0x00

{0x00,0x00,0x00,0x00,0x00,0x00,0xf6,0x95,0x95,0x95,0x96,0xf0,0x00},

 12345678
3          0x00
211111     0xf8
1  1   1   0x22
0  1  1 1  0x25
9  1  111  0x27
8  1  1 1  0x25
7  1  1 1  0x25
6          0x00
5          0x00
4          0x00
3          0x00
2          0x00
1          0x00

{0x00,0x00,0x00,0x00,0x00,0x00,0x25,0x25,0x27,0x25,0x22,0xf8,0x00},

 12345678
3          0x00
211111     0xf8
11     1   0x82
0111   1   0xe2
91     1   0x82
81     1   0x82
71     1   0x82
6          0x00
5          0x00
4          0x00
3          0x00
2          0x00
1          0x00

{0x00,0x00,0x00,0x00,0x00,0x00,0x82,0x82,0x82,0xe2,0x82,0xf8,0x00},

 12345678
3          0x00
2111       0xe0
11  1  1   0x92
01  1  1   0x92
91  1  1   0x92
81  1  1   0x92
7111   1   0xe2
6          0x00
5          0x00
4          0x00
3          0x00
2          0x00
1          0x00

{0x00,0x00,0x00,0x00,0x00,0x00,0xe2,0x92,0x92,0x92,0x92,0xe0,0x00},

 12345678
3          0x00
211111     0xf8
1 1     1  0x41
0 1 1   1  0x51
9 1 1 1 1  0x55
8 1 11 11  0x5b
7 1 1   1  0x51
6          0x00
5          0x00
4          0x00
3          0x00
2          0x00
1          0x00

{0x00,0x00,0x00,0x00,0x00,0x00,0x51,0x5b,0x55,0x51,0x41,0xf8,0x00},

 12345678
6          0x00
511111111  0xff
4111  111  0xe7
31  11  1  0x99
2111  111  0xe7
111111111  0xff

0xff,0xe7,0x99,0xe7,0xff

 12345678
6          0x00
511111111  0xff
41      1  0x81
31      1  0x81
21      1  0x81
111111111  0xff

0xff,0x81,0x81,0x81,0xff


 12345678
3          0x00
2 11       0x60
11  1 1 1  0x95
01111 111  0xf7
91  1 1 1  0x95
81  1 1 1  0x95
71  1 1 1  0x95
6          0x00
5          0x00
4          0x00
3          0x00
2          0x00
1          0x00

0x95,0x95,0x95,0xf7,0x95,0x60,0x00

 12345678
3          0x00
2          0x00
1  1111    0x3c
0 1    1   0x42
91      1  0x81
81 1  1 1  0xa5
71      1  0x81
61 1  1 1  0xa5
51  11  1  0x99
4 1    1   0x42
3  1111    0x3c
2          0x00
1          0x00

0x00,0x00,0x3c,0x42,0x99,0xa5,0x81,0xa5,0x81,0x42,0x3c,0x00,0x00 

 12345678
3          0x00
2          0x00
1    1     0x08
0 1  1  1  0x49
9  1 1 1   0x2a
8   111    0x1c
7 1111111  0x7f
6   111    0x1c
5  1 1 1   0x2a
4 1  1  1  0x49
3    1     0x08
2          0x00
1          0x00

0x00,0x00,0x08,0x49,0x2a,0x1c,0x7f,0x1c,0x2a,0x49,0x08,0x00,0x00


 12345678
3          0x00
2          0x00
1  11111   0x3e
0  1 1 1   0x2a
9  11 11   0x36
8  1 1 1   0x2a
7  11 11   0x36
6  1 1 1   0x2a
5  11 11   0x36
4  1 1 1   0x2a
3  11111   0x3e
2          0x00
1          0x00

{0x00,0x00,0x3e,0x2a,0x36,0x2a,0x36,0x2a,0x36,0x2a,0x3e,0x00,0x00},

 12345678
3          0x00
2          0x00
1     11   0x06
0   111    0x1c
9  111     0x38
8 1111     0x78
7 1111     0x78  
6 1111     0x78
5  111     0x38
4   111    0x1c
3     11   0x06
2          0x00
1          0x00

{0x00,0x00,0x06,0x1c,0x38,0x78,0x78,0x78,0x38,0x1c,0x06,0x00,0x00},


 12345678
3          0x00
2 11       0x60
11  1 11   0x96
01111 1 1  0xf5
91  1 11   0x96
81  1 1 1  0x95
71  1 111  0x97
6          0x00
5          0x00
4          0x00
3          0x00
2          0x00
1          0x00

0x00,0x00,0x00,0x00,0x00,0x00,0x97,x95,0x96,0xf5,0x96,0x60,0x00

*/

////////////////////////////////////////////////////////////////////////
// texture for gpu picture
////////////////////////////////////////////////////////////////////////

void CreatePic(unsigned char * pMem)
{
 int x,y;
 GLubyte TexBytes[128][128][3];
 memset(TexBytes,0,128*128*3);
 
 for(y=0;y<96;y++)
  {
   for(x=0;x<128;x++)
    {
     TexBytes[y][x][0]=*(pMem+2);
     TexBytes[y][x][1]=*(pMem+1);
     TexBytes[y][x][2]=*(pMem+0);
     pMem+=3;
    }
  }

 glGenTextures(1, &gTexPicName);
 glBindTexture(GL_TEXTURE_2D, gTexPicName);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
 glTexImage2D(GL_TEXTURE_2D, 0, 3, 128, 128, 0, GL_RGB,
              GL_UNSIGNED_BYTE,TexBytes);
}

////////////////////////////////////////////////////////////////////////
// destroy gpu picture texture
////////////////////////////////////////////////////////////////////////

void DestroyPic(void)
{
 if(gTexPicName) 
  {
   GLfloat fYS1,fYS2,fXS1,fXS2,fXS,fYS;

   glDisable(GL_SCISSOR_TEST);                       
   glDisable(GL_ALPHA_TEST);
   if(bOldSmoothShaded) {glShadeModel(GL_FLAT);bOldSmoothShaded=FALSE;}
   if(bBlendEnable)     {glDisable(GL_BLEND);bBlendEnable=FALSE;}
   if(!bTexEnabled)     {glEnable(GL_TEXTURE_2D);bTexEnabled=TRUE;}
   gTexName=0;
   glBindTexture(GL_TEXTURE_2D,0);  
   vertex[0].c.lcol=0xff000000;                      

   fYS=(GLfloat)PSXDisplay.DisplayMode.y/(GLfloat)iResY;
   fXS=(GLfloat)PSXDisplay.DisplayMode.x/(GLfloat)iResX;
   fYS2=96.0f*fYS;
   fYS1=0.0f;
   fXS2=(GLfloat)PSXDisplay.DisplayMode.x;
   fXS1=fXS2-128.0f*fXS;

   SETCOL(vertex[0]); 
   glBegin(GL_QUADS);                                  // paint a black rect to hide texture

   glVertex3f(fXS1,fYS1,0.99996f);
   glVertex3f(fXS1,fYS2,0.99996f);
   glVertex3f(fXS2,fYS2,0.99996f);
   glVertex3f(fXS2,fYS1,0.99996f);

   glEnd();
   glEnable(GL_ALPHA_TEST);
   glEnable(GL_SCISSOR_TEST);                       
   
   glDeleteTextures(1,&gTexPicName);gTexPicName=0;
  }
}

////////////////////////////////////////////////////////////////////////
// display info picture
////////////////////////////////////////////////////////////////////////

void DisplayPic(void)
{
 GLfloat fYS1,fYS2,fXS1,fXS2,fXS,fYS;

 glDisable(GL_SCISSOR_TEST);                       
 glDisable(GL_ALPHA_TEST);
 if(bOldSmoothShaded) {glShadeModel(GL_FLAT);bOldSmoothShaded=FALSE;}
 if(bBlendEnable)     {glDisable(GL_BLEND);bBlendEnable=FALSE;}
 if(!bTexEnabled)     {glEnable(GL_TEXTURE_2D);bTexEnabled=TRUE;}
 gTexName=gTexPicName;
 glBindTexture(GL_TEXTURE_2D,gTexPicName);            // now set font texture

 if(bGLBlend) vertex[0].c.lcol=0xff7f7f7f;                      
 else         vertex[0].c.lcol=0xffffffff;                      

 fYS=(GLfloat)PSXDisplay.DisplayMode.y/(GLfloat)iResY;
 fXS=(GLfloat)PSXDisplay.DisplayMode.x/(GLfloat)iResX;
 fYS2=96.0f*fYS;
 fYS1=0.0f;
 fXS2=(GLfloat)PSXDisplay.DisplayMode.x;
 fXS1=fXS2-128.0f*fXS;

 SETCOL(vertex[0]); 
 glBegin(GL_QUADS);

#ifdef OWNSCALE
 glTexCoord2f(0.0f,0.0f);
 glVertex3f(fXS1,fYS1,0.99996f);
 glTexCoord2f(0.0f,192.0f/256.0f);
 glVertex3f(fXS1,fYS2,0.99996f);
 glTexCoord2f(256.0f/256.0f,192.0f/256.0f);
 glVertex3f(fXS2,fYS2,0.99996f);
 glTexCoord2f(256.0f/256.0f,0.0f);
 glVertex3f(fXS2,fYS1,0.99996f);
#else
 glTexCoord2f(0.0f,0.0f);
 glVertex3f(fXS1,fYS1,0.99996f);
 glTexCoord2f(0.0f,192.0f);
 glVertex3f(fXS1,fYS2,0.99996f);
 glTexCoord2f(256.0f,192.0f);
 glVertex3f(fXS2,fYS2,0.99996f);
 glTexCoord2f(256.0f,0.0f);
 glVertex3f(fXS2,fYS1,0.99996f);
#endif

 glEnd();
 glEnable(GL_ALPHA_TEST);
 glEnable(GL_SCISSOR_TEST);                       
}

////////////////////////////////////////////////////////////////////////
// show gun cursor
////////////////////////////////////////////////////////////////////////

#define TRA 0x00,0x00,0x00,0x00
#define PNT 0xff,0xff,0xff,0xff

GLubyte texcursor[8][32]= 
{
{TRA,TRA,PNT,PNT,PNT,TRA,TRA,TRA},
{TRA,PNT,TRA,TRA,TRA,PNT,TRA,TRA},
{PNT,TRA,TRA,PNT,TRA,TRA,PNT,TRA},
{PNT,TRA,PNT,TRA,PNT,TRA,PNT,TRA},
{PNT,TRA,TRA,PNT,TRA,TRA,PNT,TRA},
{TRA,PNT,TRA,TRA,TRA,PNT,TRA,TRA},
{TRA,TRA,PNT,PNT,PNT,TRA,TRA,TRA},
{TRA,TRA,TRA,TRA,TRA,TRA,TRA,TRA}
};

void ShowGunCursor(void)
{
 int iPlayer;
 GLfloat fX,fY,fDX,fDY,fYS,fXS;
 const uint32_t crCursorColor32[8]={0xff00ff00,0xffff0000,0xff0000ff,0xffff00ff,0xffffff00,0xff00ffff,0xffffffff,0xff7f7f7f};

 if(!gTexCursorName)                                   // create gun cursor texture the first time
  {
   glGenTextures(1, &gTexCursorName);
   glBindTexture(GL_TEXTURE_2D, gTexCursorName);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D, 0, 4, 8, 8, 0, GL_RGBA,
                GL_UNSIGNED_BYTE,texcursor);
  }

 fYS=(GLfloat)PSXDisplay.DisplayMode.y/(GLfloat)iResY;  // some pre-calculations
 fXS=(GLfloat)PSXDisplay.DisplayMode.x/(GLfloat)iResX;

 fDX=fXS*7;
 fDY=fYS*7;

 glDisable(GL_SCISSOR_TEST);                       
 if(bOldSmoothShaded) {glShadeModel(GL_FLAT);bOldSmoothShaded=FALSE;}
 if(bBlendEnable)     {glDisable(GL_BLEND);bBlendEnable=FALSE;}
 if(!bTexEnabled)     {glEnable(GL_TEXTURE_2D);bTexEnabled=TRUE;}

 gTexName=gTexCursorName;
 glBindTexture(GL_TEXTURE_2D,gTexCursorName);          // now set font texture

 for(iPlayer=0;iPlayer<8;iPlayer++)                    // loop all possible players
  {
   if(usCursorActive&(1<<iPlayer))                     // player is active?
    {
     fY=((GLfloat)ptCursorPoint[iPlayer].y*(GLfloat)PSXDisplay.DisplayMode.y)/256.0f;
     fX=((GLfloat)ptCursorPoint[iPlayer].x*(GLfloat)PSXDisplay.DisplayMode.x)/512.0f;

     vertex[0].c.lcol=crCursorColor32[iPlayer];        // -> set player color

     SETCOL(vertex[0]); 

     glBegin(GL_QUADS);

     glTexCoord2f(000.0f,224.0f/255.99f);              // -> paint gun cursor
     glVertex3f(fX-fDX,fY+fDY,0.99996f);
     glTexCoord2f(000.0f,000.0f);
     glVertex3f(fX-fDX,fY-fDY,0.99996f);
     glTexCoord2f(224.0f/255.99f,000.0f);
     glVertex3f(fX+fDX,fY-fDY,0.99996f);
     glTexCoord2f(224.0f/255.99f,224.0f/255.99f);
     glVertex3f(fX+fDX,fY+fDY,0.99996f);

     glEnd();
    }
  }

 glEnable(GL_SCISSOR_TEST);                       
}
