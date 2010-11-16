/***************************************************************************
                          cfg.c  -  description
                             -------------------
    begin                : Sun Oct 28 2001
    copyright            : (C) 2001 by Pete Bernert
    email                : BlackDove@addcom.de
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

#define _IN_CFG

#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#undef FALSE
#undef TRUE
#define MAKELONG(low,high)     ((unsigned long)(((unsigned short)(low)) | (((unsigned long)((unsigned short)(high))) << 16)))

#include "externals.h"
#include "cfg.h"
#include "gpu.h"

char * pConfigFile = NULL;

// CONFIG FILE helpers....
// some helper macros:

#define GetValue(name, var) \
 p = strstr(pB, name); \
 if (p != NULL) { \
  p+=strlen(name); \
  while ((*p == ' ') || (*p == '=')) p++; \
  if (*p != '\n') var = atoi(p); \
 }

#define GetFloatValue(name, var) \
 p = strstr(pB, name); \
 if (p != NULL) { \
  p+=strlen(name); \
  while ((*p == ' ') || (*p == '=')) p++; \
  if (*p != '\n') var = (float)atof(p); \
 }

#define SetValue(name, var) \
 p = strstr(pB, name); \
 if (p != NULL) { \
  p+=strlen(name); \
  while ((*p == ' ') || (*p == '=')) p++; \
  if (*p != '\n') { \
   len = sprintf(t1, "%d", var); \
   strncpy(p, t1, len); \
   if (p[len] != ' ' && p[len] != '\n' && p[len] != 0) p[len] = ' '; \
  } \
 } \
 else { \
  size+=sprintf(pB+size, "%s = %d\n", name, var); \
 }

#define SetFloatValue(name, var) \
 p = strstr(pB, name); \
 if (p != NULL) { \
  p+=strlen(name); \
  while ((*p == ' ') || (*p == '=')) p++; \
  if (*p != '\n') { \
   len = sprintf(t1, "%.1f", (double)var); \
   strncpy(p, t1, len); \
   if (p[len] != ' ' && p[len] != '\n' && p[len] != 0) p[len] = ' '; \
  } \
 } \
 else { \
  size+=sprintf(pB+size, "%s = %.1f\n", name, (double)var); \
 }

void ReadConfigFile()
{
 struct stat buf;
 FILE *in;char t[256];int len, size;
 char * pB, * p;

 if(pConfigFile)
      strcpy(t,pConfigFile);
 else
  {
   strcpy(t,"dfxvideo.cfg");
   in = fopen(t,"rb");
   if (!in)
    {
     strcpy(t,"cfg/dfxvideo.cfg");
     in = fopen(t,"rb");
     if(!in) sprintf(t,"%s/.pcsx/plugins/dfxvideo.cfg",getenv("HOME"));
     else    fclose(in);
    }
   else     fclose(in);
  }

 if (stat(t, &buf) == -1) return;
 size = buf.st_size;

 in = fopen(t,"rb");
 if (!in) return;

 pB=(char *)malloc(size + 1);
 memset(pB,0,size + 1);

 len = fread(pB, 1, size, in);
 fclose(in);

 GetValue("ResX", iResX);
 if(iResX<20) iResX=20;
 iResX=(iResX/4)*4;

 GetValue("ResY", iResY);
 if(iResY<20) iResY=20;
 iResY=(iResY/4)*4;

 iWinSize=MAKELONG(iResX,iResY);

 GetValue("NoStretch", iUseNoStretchBlt);

 GetValue("Dithering", iUseDither);

 GetValue("FullScreen", iWindowMode);
 if(iWindowMode!=0) iWindowMode=0;
 else               iWindowMode=1;

 GetValue("ShowFPS", iShowFPS);
 if(iShowFPS<0) iShowFPS=0;
 if(iShowFPS>1) iShowFPS=1;

 GetValue("Maintain43", iMaintainAspect);
 if(iMaintainAspect<0) iMaintainAspect=0;
 if(iMaintainAspect>1) iMaintainAspect=1;

 GetValue("UseFrameLimit", UseFrameLimit);
 if(UseFrameLimit<0) UseFrameLimit=0;
 if(UseFrameLimit>1) UseFrameLimit=1;

 GetValue("UseFrameSkip", UseFrameSkip);
 if(UseFrameSkip<0) UseFrameSkip=0;
 if(UseFrameSkip>1) UseFrameSkip=1;

 GetValue("FPSDetection", iFrameLimit);
 if(iFrameLimit<1) iFrameLimit=1;
 if(iFrameLimit>2) iFrameLimit=2;

 GetFloatValue("FrameRate", fFrameRate);
 fFrameRate/=10;
 if(fFrameRate<10.0f)   fFrameRate=10.0f;
 if(fFrameRate>1000.0f) fFrameRate=1000.0f;

 GetValue("CfgFixes", dwCfgFixes);

 GetValue("UseFixes", iUseFixes);
 if(iUseFixes<0) iUseFixes=0;
 if(iUseFixes>1) iUseFixes=1;

 free(pB);
}

void ExecCfg(char *arg) {
	char cfg[256];
	struct stat buf;

	strcpy(cfg, "./cfgDFXVideo");
	if (stat(cfg, &buf) != -1) {
		if (fork() == 0) {
			execl(cfg, "cfgDFXVideo", arg, NULL);
			exit(0);
		}
		return;
	}

	strcpy(cfg, "./cfg/cfgDFXVideo");
	if (stat(cfg, &buf) != -1) {
		if (fork() == 0) {
			execl(cfg, "cfgDFXVideo", arg, NULL);
			exit(0);
		}
		return;
	}

	sprintf(cfg, "%s/.pcsx/plugins/cfg/cfgDFXVideo", getenv("HOME"));
	if (stat(cfg, &buf) != -1) {
		if (fork() == 0) {
			execl(cfg, "cfgDFXVideo", arg, NULL);
			exit(0);
		}
		return;
	}

	printf("ERROR: cfgDFXVideo file not found!\n");
}

void SoftDlgProc(void)
{
	ExecCfg("CFG");
}

void AboutDlgProc(void)
{
	char args[256];

	sprintf(args, "ABOUT");
	ExecCfg(args);
}

void ReadConfig(void)
{
 // defaults
 iResX=640;iResY=480;
 iWinSize=MAKELONG(iResX,iResY);
 iColDepth=32;
 iWindowMode=1;
 iMaintainAspect=0;
 UseFrameLimit=1;
 UseFrameSkip=0;
 iFrameLimit=2;
 fFrameRate=200.0f;
 dwCfgFixes=0;
 iUseFixes=0;
 iUseNoStretchBlt=0;
 iUseDither=0;
 iShowFPS=0;

 // read sets
 ReadConfigFile();

 // additional checks
 if(!iColDepth)       iColDepth=32;
 if(iUseFixes)        dwActFixes=dwCfgFixes;
 SetFixes();
}

void WriteConfig(void) {

 struct stat buf;
 FILE *out;char t[256];int len, size;
 char * pB, * p; char t1[8];

 if(pConfigFile) 
      strcpy(t,pConfigFile);
 else 
  {
   strcpy(t,"dfxvideo.cfg");
   out = fopen(t,"rb");
   if (!out) 
    {
     strcpy(t,"cfg/dfxvideo.cfg");
     out = fopen(t,"rb");
     if(!out) sprintf(t,"%s/.pcsx/plugins/dfxvideo.cfg",getenv("HOME"));
     else     fclose(out);
    }
   else     fclose(out);
  }

 if (stat(t, &buf) != -1) size = buf.st_size;
 else size = 0;

 out = fopen(t,"rb");
 if (!out) {
  // defaults
  iResX=640;iResY=480;
  iColDepth=32;
  iWindowMode=1;
  iMaintainAspect=0;
  UseFrameLimit=0;
  UseFrameSkip=0;
  iFrameLimit=2;
  fFrameRate=200.0f;
  dwCfgFixes=0;
  iUseFixes=0;
  iUseNoStretchBlt=0;
  iUseDither=0;
  iShowFPS=0;

  size = 0;
  pB=(char *)malloc(4096);
  memset(pB,0,4096);
 }
 else {
  pB=(char *)malloc(size+4096);
  memset(pB,0,size+4096);

  len = fread(pB, 1, size, out);
  fclose(out);
 }

 SetValue("ResX", iResX);
 SetValue("ResY", iResY);
 SetValue("NoStretch", iUseNoStretchBlt);
 SetValue("Dithering", iUseDither);
 SetValue("FullScreen", !iWindowMode);
 SetValue("ShowFPS", iShowFPS);
 SetValue("Maintain43", iMaintainAspect);
 SetValue("UseFrameLimit", UseFrameLimit);
 SetValue("UseFrameSkip", UseFrameSkip);
 SetValue("FPSDetection", iFrameLimit);
 SetFloatValue("FrameRate", fFrameRate);
 SetValue("CfgFixes", (unsigned int)dwCfgFixes);
 SetValue("UseFixes", iUseFixes);

 out = fopen(t,"wb");
 if (!out) return;

 len = fwrite(pB, 1, size, out);
 fclose(out);

 free(pB);
}
