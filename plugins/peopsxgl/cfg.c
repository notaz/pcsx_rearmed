/***************************************************************************
                           cfg.c  -  description
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

//*************************************************************************// 
// History of changes:
//
// 2009/03/08 - Pete  
// - generic cleanup for the Peops release
//
//*************************************************************************// 

#define _IN_CFG

#include "stdafx.h"
#include "externals.h"
#include "cfg.h"

char *pConfigFile = NULL;

void ReadConfigFile()
{
 FILE *in = NULL;
 int len;
 char *pB, *p, t[256];

 if (pConfigFile != NULL)
  in = fopen(pConfigFile, "rb");
 else
  in = fopen("gpuPeopsMesaGL.cfg", "rb"); 

 if (in == NULL) return;

 pB=(char *)malloc(32767);                             // buffer for reading config (32k)
 memset(pB, 0, 32767);

 len = fread(pB, 1, 32767, in);                        // read config in buffer
 fclose(in);                                           // close config file

 strcpy(t,"\nResX");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iResX=atoi(p+len);
 if(iResX<10) iResX=10;

 strcpy(t,"\nResY");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iResY=atoi(p+len);
 if(iResY<10) iResY=10;

 strcpy(t,"\nKeepRatio");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) bKeepRatio=atoi(p+len);
 if(bKeepRatio<0) bKeepRatio=0;
 if(bKeepRatio>1) bKeepRatio=1;

 strcpy(t,"\nScreenSmoothing");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iBlurBuffer=atoi(p+len);
 if(iBlurBuffer<0) iBlurBuffer=0;
 if(iBlurBuffer>1) iBlurBuffer=1;

 strcpy(t,"\nHiResTextures");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iHiResTextures=atoi(p+len);
 if(iHiResTextures<0) iHiResTextures=0;
 if(iHiResTextures>2) iHiResTextures=2;

 iSortTexCnt =0;
 strcpy(t,"\nVRamSize");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iVRamSize=atoi(p+len);
 if(iVRamSize<0)    iVRamSize=0;
 if(iVRamSize>1024) iVRamSize=1024;

 strcpy(t,"\nFullScreen");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) bFullScreen=atoi(p+len);
 if(bFullScreen>1) bFullScreen=1;

 strcpy(t,"\nScanLines");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iUseScanLines=atoi(p+len);
 if(iUseScanLines<0) iUseScanLines=0;
 if(iUseScanLines>1) iUseScanLines=1;

 strcpy(t,"\nScanLinesBlend");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iScanBlend=atoi(p+len);
 if(iScanBlend<-1)  iScanBlend=-1;
 if(iScanBlend>255) iScanBlend=255;

 strcpy(t,"\nFrameTextures");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iFrameTexType=atoi(p+len);
 if(iFrameTexType<0) iFrameTexType=0;
 if(iFrameTexType>3) iFrameTexType=3;

 strcpy(t,"\nFrameAccess");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iFrameReadType=atoi(p+len);
 if(iFrameReadType<0) iFrameReadType=0;
 if(iFrameReadType>4) iFrameReadType=4;
 if(iFrameReadType==4) bFullVRam=TRUE;
 else                  bFullVRam=FALSE;

 strcpy(t,"\nTexFilter");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iFilterType=atoi(p+len);
 if(iFilterType<0) iFilterType=0;
 if(iFilterType>6) iFilterType=6;

 strcpy(t,"\nAdvancedBlend");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) bAdvancedBlend=atoi(p+len);
 if(bAdvancedBlend<0) bAdvancedBlend=0;
 if(bAdvancedBlend>1) bAdvancedBlend=1;

 strcpy(t,"\nDithering");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) bDrawDither=atoi(p+len);
 if(bDrawDither<0) bDrawDither=0;
 if(bDrawDither>1) bDrawDither=1;

 strcpy(t,"\nLineMode");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) bUseLines=atoi(p+len);
 if(bUseLines<0) bUseLines=0;
 if(bUseLines>1) bUseLines=1;

 strcpy(t,"\nShowFPS");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iShowFPS=atoi(p+len);
 if(iShowFPS<0) iShowFPS=0;
 if(iShowFPS>1) iShowFPS=1;

 strcpy(t,"\nUseFrameLimit");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) bUseFrameLimit=atoi(p+len);
 if(bUseFrameLimit<0) bUseFrameLimit=0;
 if(bUseFrameLimit>1) bUseFrameLimit=1;

 strcpy(t,"\nUseFrameSkip");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) bUseFrameSkip=atoi(p+len);
 if(bUseFrameSkip<0) bUseFrameSkip=0;
 if(bUseFrameSkip>1) bUseFrameSkip=1;

 strcpy(t,"\nFPSDetection");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iFrameLimit=atoi(p+len)+1;
 if(iFrameLimit<1) iFrameLimit=1;
 if(iFrameLimit>2) iFrameLimit=2;

 strcpy(t,"\nFrameRate");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) fFrameRate=(float)atof(p+len);
 if(fFrameRate<0.0f)    fFrameRate=0.0f;
 if(fFrameRate>1000.0f) fFrameRate=1000.0f;

 strcpy(t,"\nOffscreenDrawing");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iOffscreenDrawing=atoi(p+len);
 if(iOffscreenDrawing<0) iOffscreenDrawing=0;
 if(iOffscreenDrawing>4) iOffscreenDrawing=4;

 strcpy(t,"\nOpaquePass");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) bOpaquePass=atoi(p+len);
 if(bOpaquePass<0) bOpaquePass=0;
 if(bOpaquePass>1) bOpaquePass=1;

 strcpy(t,"\nAntiAlias");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) bUseAntiAlias=atoi(p+len);
 if(bUseAntiAlias<0) bUseAntiAlias=0;
 if(bUseAntiAlias>1) bUseAntiAlias=1;

 strcpy(t,"\nTexQuality");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iTexQuality=atoi(p+len);
 if(iTexQuality<0) iTexQuality=0;
 if(iTexQuality>4) iTexQuality=4;

 strcpy(t,"\n15bitMdec");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) bUse15bitMdec=atoi(p+len);
 if(bUse15bitMdec<0) bUse15bitMdec=0;
 if(bUse15bitMdec>1) bUse15bitMdec=1;

 strcpy(t,"\nMaskDetect");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iUseMask=atoi(p+len);
 if(iUseMask<0) iUseMask=0;
 if(iUseMask>1) iUseMask=1;

 strcpy(t,"\nFastMdec");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) bUseFastMdec=atoi(p+len);
 if(bUseFastMdec<0) bUseFastMdec=0;
 if(bUseFastMdec>1) bUseFastMdec=1;

 strcpy(t,"\nCfgFixes");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) dwCfgFixes=atoi(p+len);

 strcpy(t,"\nUseFixes");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) bUseFixes=atoi(p+len);
 if(bUseFixes<0) bUseFixes=0;
 if(bUseFixes>1) bUseFixes=1;

 strcpy(t,"\nOGLExtensions");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iUseExts=atoi(p+len);
 if(iUseExts>1) iUseExts=1;

 free(pB);
}

void ReadConfig(void)                                  // read config (linux file)
{
 iResX=640;
 iResY=480;
 iColDepth=16;
 bChangeRes=FALSE;
 bWindowMode=TRUE;
 iUseScanLines=0;
 bFullScreen=FALSE;
 bFullVRam=FALSE;
 iFilterType=0;
 bAdvancedBlend=FALSE;
 bDrawDither=FALSE;
 bUseLines=FALSE;
 bUseFrameLimit=TRUE;
 bUseFrameSkip=FALSE;
 iFrameLimit=2;
 fFrameRate=200.0f;
 iOffscreenDrawing=2;
 bOpaquePass=TRUE;
 bUseAntiAlias=FALSE;
 iTexQuality=0;
 iUseMask=0;
 iZBufferDepth=0;
 bUseFastMdec=TRUE;
 dwCfgFixes=0;
 bUseFixes=FALSE;
 iFrameTexType=1;
 iFrameReadType=0;
 bUse15bitMdec=FALSE;
 iShowFPS=0;
 bKeepRatio=FALSE;
 iScanBlend=0;
 iVRamSize=0;
 iTexGarbageCollection=1;
 iBlurBuffer=0;
 iHiResTextures=0;
 iForceVSync=-1;

 ReadConfigFile();                                     // read file

 if(!iColDepth)  iColDepth=32;                         // adjust color info
 if(iUseMask)    iZBufferDepth=16;                     // set zbuffer depth 
 else            iZBufferDepth=0;
 if(bUseFixes)   dwActFixes=dwCfgFixes;                // init game fix global
}
