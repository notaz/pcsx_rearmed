/***************************************************************************
                           gpu.c  -  description
                             -------------------
    begin                : Sun Mar 08 2009
    copyright            : (C) 1999-2009 by Pete Bernert
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

// !!! enable this, if Linux XF86VidMode is not supported: 
//#define NOVMODE

#include "stdafx.h"
#include "config.h"

#ifndef NOVMODE
#include <X11/extensions/xf86vmode.h>
static XF86VidModeModeInfo **modes=0;
static int iOldMode=0;
#endif

#define _IN_GPU

#include "externals.h"
#include "gpu.h"
#include "draw.h"
#include "cfg.h"
#include "prim.h"
#include "psemu_plugin_defs.h"
#include "texture.h"
#include "menu.h"
#include "fps.h"
#include "key.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#include <locale.h>
#define _(x)  gettext(x)
#define N_(x) (x)
#else
#define _(x)  (x)
#define N_(x) (x)
#endif
                               
////////////////////////////////////////////////////////////////////////
// PPDK developer must change libraryName field and can change revision and build
////////////////////////////////////////////////////////////////////////

const  unsigned char version  = 1;    // do not touch - library for PSEmu 1.x
const  unsigned char revision = 1;
const  unsigned char build    = 78;

static char *libraryName     = N_("OpenGL Driver");

static char *PluginAuthor    = N_("Pete Bernert");
static char *libraryInfo     = N_("Based on P.E.Op.S. MesaGL Driver V1.78\nCoded by Pete Bernert\n");

////////////////////////////////////////////////////////////////////////
// memory image of the PSX vram
////////////////////////////////////////////////////////////////////////

unsigned char  *psxVSecure;
unsigned char  *psxVub;
signed   char  *psxVsb;
unsigned short *psxVuw;
unsigned short *psxVuw_eom;
signed   short *psxVsw;
uint32_t       *psxVul;
signed   int   *psxVsl;

// macro for easy access to packet information
#define GPUCOMMAND(x) ((x>>24) & 0xff)

GLfloat         gl_z=0.0f;
BOOL            bNeedInterlaceUpdate=FALSE;
BOOL            bNeedRGB24Update=FALSE;
BOOL            bChangeWinMode=FALSE;

uint32_t        ulStatusControl[256];

////////////////////////////////////////////////////////////////////////
// global GPU vars
////////////////////////////////////////////////////////////////////////

static int      GPUdataRet;
int             lGPUstatusRet;
char            szDispBuf[64];

uint32_t        dwGPUVersion = 0;
int             iGPUHeight = 512;
int             iGPUHeightMask = 511;
int             GlobalTextIL = 0;
int             iTileCheat = 0;

static uint32_t      gpuDataM[256];
static unsigned char gpuCommand = 0;
static int           gpuDataC = 0;
static int           gpuDataP = 0;

VRAMLoad_t      VRAMWrite;
VRAMLoad_t      VRAMRead;
int             iDataWriteMode;
int             iDataReadMode;

int             lClearOnSwap;
int             lClearOnSwapColor;
BOOL            bSkipNextFrame = FALSE;
int             iColDepth;
BOOL            bChangeRes;
BOOL            bWindowMode;
int             iWinSize;

// possible psx display widths
short dispWidths[8] = {256,320,512,640,368,384,512,640};

PSXDisplay_t    PSXDisplay;
PSXDisplay_t    PreviousPSXDisplay;
TWin_t          TWin;
short           imageX0,imageX1;
short           imageY0,imageY1;
BOOL            bDisplayNotSet = TRUE;
GLuint          uiScanLine=0;
int             iUseScanLines=0;
int             lSelectedSlot=0;
unsigned char * pGfxCardScreen=0;
int             iBlurBuffer=0;
int             iScanBlend=0;
int             iRenderFVR=0;
int             iNoScreenSaver=0;
uint32_t        ulGPUInfoVals[16];
int             iFakePrimBusy = 0;
int             iRumbleVal    = 0;
int             iRumbleTime   = 0;

////////////////////////////////////////////////////////////////////////
// stuff to make this a true PDK module
////////////////////////////////////////////////////////////////////////

char * CALLBACK PSEgetLibName(void)
{
 return _(libraryName);
}

unsigned long CALLBACK PSEgetLibType(void)
{
 return  PSE_LT_GPU;
}

unsigned long CALLBACK PSEgetLibVersion(void)
{
 return version<<16|revision<<8|build;
}

char * GPUgetLibInfos(void)
{
 return _(libraryInfo);
}

////////////////////////////////////////////////////////////////////////
// snapshot funcs (saves screen to bitmap / text infos into file)
////////////////////////////////////////////////////////////////////////

char * GetConfigInfos(int hW)
{
 char szO[2][4]={"off","on "};
 char szTxt[256];
 char * pB=(char *)malloc(32767);

 if(!pB) return NULL;
 *pB=0;
 //----------------------------------------------------//
 sprintf(szTxt,"Plugin: %s %d.%d.%d\r\n",libraryName,version,revision,build);
 strcat(pB,szTxt);
 sprintf(szTxt,"Author: %s\r\n",PluginAuthor);
 strcat(pB,szTxt);

 sprintf(szTxt,"Card vendor: %s\r\n",(char *)glGetString(GL_VENDOR));
 strcat(pB,szTxt);
 sprintf(szTxt,"GFX card: %s\r\n",(char *)glGetString(GL_RENDERER));
 strcat(pB,szTxt);
 sprintf(szTxt,"OGL version: %s\r\n\r\n",(char *)glGetString(GL_VERSION));
 strcat(pB,szTxt);
 //strcat(pB,(char *)glGetString(GL_EXTENSIONS));
 //strcat(pB,"\r\n\r\n");

 if(hW && bWindowMode)
  sprintf(szTxt,"Resolution/Color:\r\n- %dx%d ",LOWORD(iWinSize),HIWORD(iWinSize));
 else
  sprintf(szTxt,"Resolution/Color:\r\n- %dx%d ",iResX,iResY);
 strcat(pB,szTxt);
 if(bWindowMode) sprintf(szTxt,"Window mode\r\n");
 else
  {
   sprintf(szTxt,"Fullscreen ");
   strcat(pB,szTxt);
   if(bChangeRes) sprintf(szTxt,"- Desktop changing [%d Bit]\r\n",iColDepth);
   else           sprintf(szTxt,"- NO desktop changing\r\n");
  }                                                                                   
 strcat(pB,szTxt);

 if(iForceVSync>=0) sprintf(szTxt,"- V-Sync: %s\r\n",szO[iForceVSync]);
 else               strcpy(szTxt,"- V-Sync: Driver\r\n");
 strcat(pB,szTxt); 
 sprintf(szTxt,"- Keep psx aspect ratio: %s\r\n\r\n",szO[bKeepRatio]);
 strcat(pB,szTxt);
 //----------------------------------------------------//
 strcpy(szTxt,"Textures:\r\n- ");
 if(iTexQuality==0)      strcat(szTxt,"Default");
 else if(iTexQuality==1) strcat(szTxt,"R4G4B4A4");
 else if(iTexQuality==2) strcat(szTxt,"R5G5B5A1");
 else if(iTexQuality==3) strcat(szTxt,"R8G8A8A8");
 else if(iTexQuality==4) strcat(szTxt,"B8G8R8A8");
 if(!hW && bGLExt) strcat(szTxt," (packed pixels)\r\n");
 else              strcat(szTxt,"\r\n");
 strcat(pB,szTxt);
 if(!hW)
  {
   sprintf(szTxt,"- Filtering: %d - edge clamping ",iFilterType);
   if(iClampType==GL_TO_EDGE_CLAMP) strcat(szTxt,"supported\r\n");
   else                             strcat(szTxt,"NOT supported\r\n");
  }
 else sprintf(szTxt,"- iFiltering: %d\r\n",iFilterType);
 strcat(pB,szTxt);
 sprintf(szTxt,"- Hi-Res textures: %d\r\n",iHiResTextures);
 strcat(pB,szTxt); 
 if(!hW)
  {
   sprintf(szTxt,"- Palettized tex windows: %s\r\n",szO[iUsePalTextures]);
   strcat(pB,szTxt); 
  }
 sprintf(szTxt,"- VRam size: %d MBytes",iVRamSize);
 if(!hW)
      sprintf(szTxt+strlen(szTxt)," - %d textures usable\r\n\r\n",iSortTexCnt);
 else strcat(szTxt,"\r\n\r\n");
 strcat(pB,szTxt);
 //----------------------------------------------------//
 sprintf(szTxt,"Framerate:\r\n- FPS limitation: %s\r\n",szO[bUseFrameLimit]);
 strcat(pB,szTxt);
 sprintf(szTxt,"- Frame skipping: %s\r\n",szO[bUseFrameSkip]);
 strcat(pB,szTxt);
 if(iFrameLimit==2)
      strcpy(szTxt,"- FPS limit: Auto\r\n\r\n");
 else sprintf(szTxt,"- FPS limit: %.1f\r\n\r\n",fFrameRate);
 strcat(pB,szTxt);
 //----------------------------------------------------//
 sprintf(szTxt,"Compatibility:\r\n- Offscreen drawing: %d\r\n",iOffscreenDrawing);
 strcat(pB,szTxt);
 sprintf(szTxt,"- Framebuffer texture: %d",iFrameTexType);
 if(!hW && iFrameTexType==2)
  {
   if(gTexFrameName) strcat(szTxt," - texture created\r\n");
   else              strcat(szTxt," - not used yet\r\n");
  }
 else strcat(szTxt,"\r\n");
 strcat(pB,szTxt);
 sprintf(szTxt,"- Framebuffer access: %d\r\n",iFrameReadType);
 strcat(pB,szTxt);
 sprintf(szTxt,"- Alpha multipass: %s\r\n",szO[bOpaquePass]);
 strcat(pB,szTxt);
 sprintf(szTxt,"- Mask bit: %s\r\n",szO[iUseMask]);
 strcat(pB,szTxt);
 sprintf(szTxt,"- Advanced blending: %s",szO[bAdvancedBlend]);
 if(!hW && bAdvancedBlend)
  {
   if(bGLBlend) strcat(szTxt," (hardware)\r\n");
   else         strcat(szTxt," (software)\r\n");
  }
 else strcat(szTxt,"\r\n");
 strcat(pB,szTxt);

 if(!hW)
  {
   strcpy(szTxt,"- Subtractive blending: ");
   if(glBlendEquationEXTEx)
    {
     if(bUseMultiPass) strcat(szTxt,"supported, but not used!");
     else              strcat(szTxt,"activated");
    }
   else strcat(szTxt," NOT supported!");
   strcat(szTxt,"\r\n\r\n");
  }
 else strcpy(szTxt,"\r\n");
 
 strcat(pB,szTxt);             
 //----------------------------------------------------//
 sprintf(szTxt,"Misc:\r\n- Scanlines: %s",szO[iUseScanLines]);
 strcat(pB,szTxt);
 if(iUseScanLines) sprintf(szTxt," [%d]\r\n",iScanBlend);
 else strcpy(szTxt,"\r\n");
 strcat(pB,szTxt);
 sprintf(szTxt,"- Line mode: %s\r\n",szO[bUseLines]);
 strcat(pB,szTxt);
// sprintf(szTxt,"- Line AA: %s\r\n",szO[bUseAntiAlias]);
// fwrite(szTxt,lstrlen(szTxt),1,txtfile);
 sprintf(szTxt,"- Unfiltered FB: %s\r\n",szO[bUseFastMdec]);
 strcat(pB,szTxt);
 sprintf(szTxt,"- 15 bit FB: %s\r\n",szO[bUse15bitMdec]);
 strcat(pB,szTxt);
 sprintf(szTxt,"- Dithering: %s\r\n",szO[bDrawDither]);
 strcat(pB,szTxt);
 sprintf(szTxt,"- Screen smoothing: %s",szO[iBlurBuffer]);
 strcat(pB,szTxt);
 if(!hW && iBlurBuffer) 
  {
   if(gTexBlurName) strcat(pB," - supported\r\n");
   else             strcat(pB," - not supported\r\n");
  }
 else strcat(pB,"\r\n");
 sprintf(szTxt,"- Game fixes: %s [%08x]\r\n",szO[bUseFixes],dwCfgFixes);
 strcat(pB,szTxt);
 //----------------------------------------------------//
 return pB;
}

////////////////////////////////////////////////////////////////////////
// save text infos to file
////////////////////////////////////////////////////////////////////////

void DoTextSnapShot(int iNum)
{
 FILE *txtfile;char szTxt[256];char * pB;

 sprintf(szTxt,"%s/pcsx%04d.txt",getenv("HOME"),iNum);

 if((txtfile=fopen(szTxt,"wb"))==NULL)
  return;                                              

 pB=GetConfigInfos(0);
 if(pB)
  {
   fwrite(pB,strlen(pB),1,txtfile);
   free(pB);
  }
 fclose(txtfile); 
}

////////////////////////////////////////////////////////////////////////
// saves screen bitmap to file
////////////////////////////////////////////////////////////////////////

void DoSnapShot(void)
{
 unsigned char * snapshotdumpmem=NULL,* p,c;
 FILE *bmpfile;char filename[256];
 unsigned char header[0x36];int size;
 unsigned char empty[2]={0,0};int i;
 unsigned int snapshotnr = 0;
 short SnapWidth;
 short SnapHeigth;

 bSnapShot=FALSE;

 SnapWidth  = iResX;
 SnapHeigth = iResY;

 size=SnapWidth * SnapHeigth * 3 + 0x38;

 if((snapshotdumpmem=(unsigned char *)
   malloc(SnapWidth*SnapHeigth*3))==NULL)
  return;
    
  // fill in proper values for BMP
 for(i=0;i<0x36;i++) header[i]=0;
 header[0]='B';
 header[1]='M';
 header[2]=(unsigned char)(size&0xff);
 header[3]=(unsigned char)((size>>8)&0xff);
 header[4]=(unsigned char)((size>>16)&0xff);
 header[5]=(unsigned char)((size>>24)&0xff);
 header[0x0a]=0x36;
 header[0x0e]=0x28;
 header[0x12]=(unsigned char)(SnapWidth%256);
 header[0x13]=(unsigned char)(SnapWidth/256);
 header[0x16]=(unsigned char)(SnapHeigth%256);
 header[0x17]=(unsigned char)(SnapHeigth/256);
 header[0x1a]=0x01;
 header[0x1c]=0x18;
 header[0x26]=0x12;
 header[0x27]=0x0B;
 header[0x2A]=0x12;
 header[0x2B]=0x0B;

 // increment snapshot value
 // get filename
 do
  {
   snapshotnr++;
   sprintf(filename,"%s/pcsx%04d.bmp",getenv("HOME"),snapshotnr);
   bmpfile=fopen(filename,"rb");
   if(bmpfile==NULL)break;
   fclose(bmpfile);
   if(snapshotnr==9999) break;
  }
 while(TRUE);

 // try opening new snapshot file
 if((bmpfile=fopen(filename,"wb"))==NULL)
  {free(snapshotdumpmem);return;}

 fwrite(header,0x36,1,bmpfile);

 glReadPixels(0,0,SnapWidth,SnapHeigth,GL_RGB,
               GL_UNSIGNED_BYTE,snapshotdumpmem);
 p=snapshotdumpmem;
 size=SnapWidth * SnapHeigth;

 for(i=0;i<size;i++,p+=3)
  {c=*p;*p=*(p+2);*(p+2)=c;}

 fwrite(snapshotdumpmem,size*3,1,bmpfile);
 fwrite(empty,0x2,1,bmpfile);
 fclose(bmpfile); 
 free(snapshotdumpmem);

 DoTextSnapShot(snapshotnr);
}       

void CALLBACK GPUmakeSnapshot(void)
{
 bSnapShot = TRUE;
}        

////////////////////////////////////////////////////////////////////////
// GPU INIT... here starts it all (first func called by emu)
////////////////////////////////////////////////////////////////////////

long CALLBACK GPUinit()
{
 memset(ulStatusControl,0,256*sizeof(uint32_t));

 // different ways of accessing PSX VRAM

 psxVSecure=(unsigned char *)malloc((iGPUHeight*2)*1024 + (1024*1024)); // always alloc one extra MB for soft drawing funcs security
 if(!psxVSecure) return -1;

 psxVub=psxVSecure+512*1024;                           // security offset into double sized psx vram!
 psxVsb=(signed char *)psxVub;
 psxVsw=(signed short *)psxVub;
 psxVsl=(signed int *)psxVub;
 psxVuw=(unsigned short *)psxVub;
 psxVul=(uint32_t *)psxVub;

 psxVuw_eom=psxVuw+1024*iGPUHeight;                    // pre-calc of end of vram

 memset(psxVSecure,0x00,(iGPUHeight*2)*1024 + (1024*1024));
 memset(ulGPUInfoVals,0x00,16*sizeof(uint32_t));

 InitFrameCap();                                       // init frame rate stuff

 PSXDisplay.RGB24        = 0;                          // init vars
 PreviousPSXDisplay.RGB24= 0;
 PSXDisplay.Interlaced   = 0;
 PSXDisplay.InterlacedTest=0;
 PSXDisplay.DrawOffset.x = 0;
 PSXDisplay.DrawOffset.y = 0;
 PSXDisplay.DrawArea.x0  = 0;
 PSXDisplay.DrawArea.y0  = 0;
 PSXDisplay.DrawArea.x1  = 320;
 PSXDisplay.DrawArea.y1  = 240;
 PSXDisplay.DisplayMode.x= 320;
 PSXDisplay.DisplayMode.y= 240;
 PSXDisplay.Disabled     = FALSE;
 PreviousPSXDisplay.Range.x0 =0;
 PreviousPSXDisplay.Range.x1 =0;
 PreviousPSXDisplay.Range.y0 =0;
 PreviousPSXDisplay.Range.y1 =0;
 PSXDisplay.Range.x0=0;
 PSXDisplay.Range.x1=0;
 PSXDisplay.Range.y0=0;
 PSXDisplay.Range.y1=0;
 PreviousPSXDisplay.DisplayPosition.x = 1;
 PreviousPSXDisplay.DisplayPosition.y = 1;
 PSXDisplay.DisplayPosition.x = 1;
 PSXDisplay.DisplayPosition.y = 1;
 PreviousPSXDisplay.DisplayModeNew.y=0;
 PSXDisplay.Double=1;
 GPUdataRet=0x400;

 PSXDisplay.DisplayModeNew.x=0;
 PSXDisplay.DisplayModeNew.y=0;

 //PreviousPSXDisplay.Height = PSXDisplay.Height = 239;
 
 iDataWriteMode = DR_NORMAL;

 // Reset transfer values, to prevent mis-transfer of data
 memset(&VRAMWrite,0,sizeof(VRAMLoad_t));
 memset(&VRAMRead,0,sizeof(VRAMLoad_t));
 
 // device initialised already !
 //lGPUstatusRet = 0x74000000;

 STATUSREG = 0x14802000;
 GPUIsIdle;
 GPUIsReadyForCommands;

 return 0;
}                             

////////////////////////////////////////////////////////////////////////
// GPU OPEN: funcs to open up the gpu display (Windows)
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// LINUX GPU OPEN: func to open up the gpu display (X stuff)
// please note: in linux we are creating our own display, and we return
// the display ID to the main emu... that's cleaner
////////////////////////////////////////////////////////////////////////

char * pCaptionText=0;
int    bFullScreen=0;
Display              *display;

static Cursor        cursor;
static XVisualInfo   *myvisual;
static Colormap      colormap;
static Window        window;

static int bModeChanged=0;

typedef struct
{
#define MWM_HINTS_DECORATIONS   2
  long flags;
  long functions;
  long decorations;
  long input_mode;
} MotifWmHints;

static int dbdepat[]={GLX_RGBA,GLX_DOUBLEBUFFER,GLX_DEPTH_SIZE,16,None};
static int dbnodepat[]={GLX_RGBA,GLX_DOUBLEBUFFER,None};
static GLXContext cx;

static int fx=0;

////////////////////////////////////////////////////////////////////////

void osd_close_display (void)                          // close display
{
 if(display)                                           // display exists?
  {
   glXDestroyContext(display,cx);                      // -> kill context
   XFreeColormap(display, colormap);                   // -> kill colormap
   XSync(display,False);                               // -> sync events

#ifndef NOVMODE
   if(bModeChanged)                                    // -> repair screen mode
    {
     int myscreen=DefaultScreen(display);
     XF86VidModeSwitchToMode(display,myscreen,         // --> switch mode back
                             modes[iOldMode]);
     XF86VidModeSetViewPort(display,myscreen,0,0);     // --> set viewport upperleft
     free(modes);                                      // --> finally kill mode infos
     bModeChanged=0;                                   // --> done
    }
#endif

   XCloseDisplay(display);                             // -> close display
  }
}

////////////////////////////////////////////////////////////////////////

void sysdep_create_display(void)                       // create display
{
 XSetWindowAttributes winattr;float fxgamma=2;
 int myscreen;char gammastr[14];
 Screen * screen;XEvent event;
 XSizeHints hints;XWMHints wm_hints;
 MotifWmHints mwmhints;Atom mwmatom;Atom delwindow;
 char *glxfx;

 glxfx=getenv("MESA_GLX_FX");                          // 3dfx mesa fullscreen flag
 if(glxfx)
  {
   if(glxfx[0]=='f')                                   // -> yup, fullscreen needed
    {
     fx=1;                                             // -> raise flag
     putenv("FX_GLIDE_NO_SPLASH=");
     sprintf(gammastr,"SST_GAMMA=%2.1f",fxgamma);      // -> set gamma
     putenv(gammastr);
    }
  }

 display=XOpenDisplay(NULL);                           // open display
 if(!display)                                          // no display?
  {
   fprintf (stderr,"Failed to open display!!!\n");
   osd_close_display();
   return;                                             // -> bye
  }

 myscreen=DefaultScreen(display);                      // get screen id

#ifdef NOVMODE
 if(bFullScreen) {fx=1;bModeChanged=0;}
#else
 if(bFullScreen)
  {
   XF86VidModeModeLine mode;
   int nmodes,iC;
   fx=1;                                               // raise flag
   XF86VidModeGetModeLine(display,myscreen,&iC,&mode); // get actual mode info
   if(mode.privsize) XFree(mode.private);              // no need for private stuff
   bModeChanged=0;                                     // init mode change flag
   if(iResX!=mode.hdisplay || iResY!=mode.vdisplay)    // wanted mode is different?
    {
     XF86VidModeGetAllModeLines(display,myscreen,      // -> enum all mode infos
                                &nmodes,&modes);
     if(modes)                                         // -> infos got?
      {
       for(iC=0;iC<nmodes;++iC)                        // -> loop modes
        {
         if(mode.hdisplay==modes[iC]->hdisplay &&      // -> act mode found?
            mode.vdisplay==modes[iC]->vdisplay)        //    if yes: store mode id
          iOldMode=iC;

         if(iResX==modes[iC]->hdisplay &&              // -> wanted mode found?
            iResY==modes[iC]->vdisplay)
          {
           XF86VidModeSwitchToMode(display,myscreen,   // --> switch to mode
                                   modes[iC]);
           XF86VidModeSetViewPort(display,myscreen,0,0);
           bModeChanged=1;                             // --> raise flag for repairing mode on close
          }
        }

       if(bModeChanged==0)                             // -> no mode found?
        {
         free(modes);                                  // --> free infos
         printf("No proper fullscreen mode found!\n"); // --> some info output
        }
      }
    }
  }
#endif

 screen=DefaultScreenOfDisplay(display);

 if(iZBufferDepth)                                      // visual (with or without zbuffer)
      myvisual=glXChooseVisual(display,myscreen,dbdepat);
 else myvisual=glXChooseVisual(display,myscreen,dbnodepat);

 if(!myvisual)                                         // no visual?
  {
   fprintf(stderr,"Failed to obtain visual!!!\n");     // -> bye
   osd_close_display();
   return;
  }

 cx=glXCreateContext(display,myvisual,0,GL_TRUE);      // create rendering context

 if(!cx)                                               // no context?
  {
   fprintf(stderr,"Failed to create OpenGL context!!!\n");
   osd_close_display();                                // -> bxe
   return;
  }

 // pffff... much work for a simple blank cursor... oh, well...
 if(!bFullScreen) cursor=XCreateFontCursor(display,XC_trek);
 else
  {
   Pixmap p1,p2;XImage * img;
   XColor b,w;unsigned char * idata;
   XGCValues GCv;
   GC        GCc;

   memset(&b,0,sizeof(XColor));
   memset(&w,0,sizeof(XColor));
   idata=(unsigned char *)malloc(8);
   memset(idata,0,8);

   p1=XCreatePixmap(display,RootWindow(display,myvisual->screen),8,8,1);
   p2=XCreatePixmap(display,RootWindow(display,myvisual->screen),8,8,1);

   img = XCreateImage(display,myvisual->visual,
                      1,XYBitmap,0,idata,8,8,8,1);

   GCv.function   = GXcopy;
   GCv.foreground = ~0;
   GCv.background =  0;
   GCv.plane_mask = AllPlanes;
   GCc = XCreateGC(display,p1,
                   (GCFunction|GCForeground|GCBackground|GCPlaneMask),&GCv);

   XPutImage(display, p1,GCc,img,0,0,0,0,8,8);
   XPutImage(display, p2,GCc,img,0,0,0,0,8,8);
   XFreeGC(display, GCc);

   cursor = XCreatePixmapCursor(display,p1,p2,&b,&w,0,0);

   XFreePixmap(display,p1);
   XFreePixmap(display,p2);
   XDestroyImage(img);                                 // will free idata as well
  }

 colormap=XCreateColormap(display,                     // create colormap
                          RootWindow(display,myvisual->screen),
                          myvisual->visual,AllocNone);

 winattr.background_pixel=0;
 winattr.border_pixel=WhitePixelOfScreen(screen);
 winattr.bit_gravity=ForgetGravity;
 winattr.win_gravity=NorthWestGravity;
 winattr.backing_store=NotUseful;
 winattr.override_redirect=False;
 winattr.save_under=False;
 winattr.event_mask=0;
 winattr.do_not_propagate_mask=0;
 winattr.colormap=colormap;
 winattr.cursor=None;

 window=XCreateWindow(display,                         // create own window
             RootWindow(display,DefaultScreen(display)),
             0,0,iResX,iResY,
             0,myvisual->depth,
             InputOutput,myvisual->visual,
             CWBorderPixel | CWBackPixel |
             CWEventMask | CWDontPropagate |
             CWColormap | CWCursor,
             &winattr);

 if(!window)                                           // no window?
  {
   fprintf(stderr,"Failed in XCreateWindow()!!!\n");
   osd_close_display();                                // -> bye
   return;
  }

 delwindow = XInternAtom(display,"WM_DELETE_WINDOW",0);
 XSetWMProtocols(display, window, &delwindow, 1);

 hints.flags=PMinSize|PMaxSize;                        // hints
 if(fx) hints.flags|=USPosition|USSize;
 else   hints.flags|=PSize;

 hints.min_width   = hints.max_width  = hints.base_width  = iResX;
 hints.min_height  = hints.max_height = hints.base_height = iResY;

 wm_hints.input=1;
 wm_hints.flags=InputHint;

 XSetWMHints(display,window,&wm_hints);
 XSetWMNormalHints(display,window,&hints);
 if(pCaptionText)                                      // caption
      XStoreName(display,window,pCaptionText);
 else XStoreName(display,window,"Pete MesaGL PSX Gpu");

 XDefineCursor(display,window,cursor);                 // cursor

 if(fx)                                                // window title bar hack
  {
   mwmhints.flags=MWM_HINTS_DECORATIONS;
   mwmhints.decorations=0;
   mwmatom=XInternAtom(display,"_MOTIF_WM_HINTS",0);
   XChangeProperty(display,window,mwmatom,mwmatom,32,
                   PropModeReplace,(unsigned char *)&mwmhints,4);
  }

 XSelectInput(display,window,                          // input setup
              FocusChangeMask | ExposureMask |
              KeyPressMask | KeyReleaseMask);

 XMapRaised(display,window);
 XClearWindow(display,window);
 XWindowEvent(display,window,ExposureMask,&event);
 glXMakeCurrent(display,window,cx);

/* 
 printf(glGetString(GL_VENDOR));
 printf("\n");
 printf(glGetString(GL_RENDERER));
 printf("\n");
*/

 if (fx)                                               // after make current: fullscreen resize
  {
   XResizeWindow(display,window,screen->width,screen->height);
   hints.min_width   = hints.max_width = hints.base_width = screen->width;
   hints.min_height= hints.max_height = hints.base_height = screen->height;
   XSetWMNormalHints(display,window,&hints);

   // set the window layer for GNOME
   {
    XEvent xev;

    memset(&xev, 0, sizeof(xev));
    xev.xclient.type = ClientMessage;
    xev.xclient.serial = 0;
    xev.xclient.send_event = 1;
    xev.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", 0);
    xev.xclient.window = window;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 1;
    xev.xclient.data.l[1] = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", 0);
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 0;
    xev.xclient.data.l[4] = 0;

    XSendEvent(display, RootWindow(display, DefaultScreen(display)), 0,
      SubstructureRedirectMask | SubstructureNotifyMask, &xev);
   }
  }
}

////////////////////////////////////////////////////////////////////////

long GPUopen(unsigned long * disp,char * CapText,char * CfgFile)
{
 pCaptionText=CapText;
 pConfigFile=CfgFile;

 ReadConfig();                                         // read text file for config

 SetFrameRateConfig();                                 // setup frame rate stuff

 bIsFirstFrame = TRUE;                                 // we have to init later (well, no really... in Linux we do all in GPUopen)

 sysdep_create_display();                              // create display

 InitializeTextureStore();                             // init texture mem

 rRatioRect.left   = rRatioRect.top=0;
 rRatioRect.right  = iResX;
 rRatioRect.bottom = iResY;

 GLinitialize();                                       // init opengl

 if(disp)
  {
   *disp=(unsigned long *)display;                       // return display ID to main emu
  }

 if(display) return 0;
 return -1;
}

////////////////////////////////////////////////////////////////////////
// close
////////////////////////////////////////////////////////////////////////

long GPUclose()                                        // LINUX CLOSE
{
 GLcleanup();                                          // close OGL

 if(pGfxCardScreen) free(pGfxCardScreen);              // free helper memory
 pGfxCardScreen=0;

 osd_close_display();                                  // destroy display

 return 0;
}

////////////////////////////////////////////////////////////////////////
// I shot the sheriff... last function called from emu 
////////////////////////////////////////////////////////////////////////

long CALLBACK GPUshutdown()
{
 if(psxVSecure) free(psxVSecure);                      // kill emulated vram memory
 psxVSecure=0;

 return 0;
}

////////////////////////////////////////////////////////////////////////
// paint it black: simple func to clean up optical border garbage
////////////////////////////////////////////////////////////////////////

void PaintBlackBorders(void)
{
 short s;

 glDisable(GL_SCISSOR_TEST);
 if(bTexEnabled) {glDisable(GL_TEXTURE_2D);bTexEnabled=FALSE;}
 if(bOldSmoothShaded) {glShadeModel(GL_FLAT);bOldSmoothShaded=FALSE;}
 if(bBlendEnable)     {glDisable(GL_BLEND);bBlendEnable=FALSE;}
 glDisable(GL_ALPHA_TEST);

 glBegin(GL_QUADS);

 vertex[0].c.lcol=0xff000000;
 SETCOL(vertex[0]); 

 if(PreviousPSXDisplay.Range.x0)
  {
   s=PreviousPSXDisplay.Range.x0+1;
   glVertex3f(0,0,0.99996f);
   glVertex3f(0,PSXDisplay.DisplayMode.y,0.99996f);
   glVertex3f(s,PSXDisplay.DisplayMode.y,0.99996f);
   glVertex3f(s,0,0.99996f);

   s+=PreviousPSXDisplay.Range.x1-2;

   glVertex3f(s,0,0.99996f);
   glVertex3f(s,PSXDisplay.DisplayMode.y,0.99996f);
   glVertex3f(PSXDisplay.DisplayMode.x,PSXDisplay.DisplayMode.y,0.99996f);
   glVertex3f(PSXDisplay.DisplayMode.x,0,0.99996f);
  }

 if(PreviousPSXDisplay.Range.y0)
  {
   s=PreviousPSXDisplay.Range.y0+1;
   glVertex3f(0,0,0.99996f);
   glVertex3f(0,s,0.99996f);
   glVertex3f(PSXDisplay.DisplayMode.x,s,0.99996f);
   glVertex3f(PSXDisplay.DisplayMode.x,0,0.99996f);
  }

 glEnd();

 glEnable(GL_ALPHA_TEST);
 glEnable(GL_SCISSOR_TEST);
}

////////////////////////////////////////////////////////////////////////
// helper to draw scanlines
////////////////////////////////////////////////////////////////////////

__inline void XPRIMdrawTexturedQuad(OGLVertex* vertex1, OGLVertex* vertex2, 
                                    OGLVertex* vertex3, OGLVertex* vertex4) 
{

 glBegin(GL_QUAD_STRIP);
  glTexCoord2fv(&vertex1->sow);
  glVertex3fv(&vertex1->x);
  
  glTexCoord2fv(&vertex2->sow);
  glVertex3fv(&vertex2->x);
  
  glTexCoord2fv(&vertex4->sow);
  glVertex3fv(&vertex4->x);
  
  glTexCoord2fv(&vertex3->sow);
  glVertex3fv(&vertex3->x);
 glEnd();
}

////////////////////////////////////////////////////////////////////////
// scanlines
////////////////////////////////////////////////////////////////////////

void SetScanLines(void)
{
 glLoadIdentity();
 glOrtho(0,iResX,iResY, 0, -1, 1);

 if(bKeepRatio)
  glViewport(0,0,iResX,iResY);

 glDisable(GL_SCISSOR_TEST);                       
 glDisable(GL_ALPHA_TEST);
 if(bOldSmoothShaded) {glShadeModel(GL_FLAT);bOldSmoothShaded=FALSE;}

 if(iScanBlend<0)                                      // special texture mask scanline mode
  {
   if(!bTexEnabled)    {glEnable(GL_TEXTURE_2D);bTexEnabled=TRUE;}
   gTexName=gTexScanName;
   glBindTexture(GL_TEXTURE_2D, gTexName);
   if(bGLBlend) glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);    
   if(!bBlendEnable)   {glEnable(GL_BLEND);bBlendEnable=TRUE;}
   SetScanTexTrans();

   vertex[0].x=0;
   vertex[0].y=iResY;
   vertex[0].z=0.99996f;

   vertex[1].x=iResX;
   vertex[1].y=iResY;
   vertex[1].z=0.99996f;

   vertex[2].x=iResX;
   vertex[2].y=0;
   vertex[2].z=0.99996f;

   vertex[3].x=0;
   vertex[3].y=0;
   vertex[3].z=0.99996f;

   vertex[0].sow=0;
   vertex[0].tow=0;
   vertex[1].sow=(float)iResX/4.0f;
   vertex[1].tow=0;
   vertex[2].sow=vertex[1].sow;
   vertex[2].tow=(float)iResY/4.0f;
   vertex[3].sow=0;
   vertex[3].tow=vertex[2].tow;

   vertex[0].c.lcol=0xffffffff;
   SETCOL(vertex[0]); 

   XPRIMdrawTexturedQuad(&vertex[0], &vertex[1], &vertex[2], &vertex[3]);

   if(bGLBlend) glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, COMBINE_EXT);    
  }
 else                                                  // typical line mode
  {
   if(bTexEnabled)     {glDisable(GL_TEXTURE_2D);bTexEnabled=FALSE;}

   if(iScanBlend==0)
    {
     if(bBlendEnable)    {glDisable(GL_BLEND);bBlendEnable=FALSE;}
     vertex[0].c.lcol=0xff000000;
    }
   else
    {
     if(!bBlendEnable)   {glEnable(GL_BLEND);bBlendEnable=TRUE;}
     SetScanTrans();
     vertex[0].c.lcol=iScanBlend<<24;
    }

   SETCOL(vertex[0]); 

   glCallList(uiScanLine);
  }

 glLoadIdentity();
 glOrtho(0,PSXDisplay.DisplayMode.x,
         PSXDisplay.DisplayMode.y, 0, -1, 1);

 if(bKeepRatio)
  glViewport(rRatioRect.left,
             iResY-(rRatioRect.top+rRatioRect.bottom),
             rRatioRect.right, 
             rRatioRect.bottom);                         // init viewport

 glEnable(GL_ALPHA_TEST);
 glEnable(GL_SCISSOR_TEST);                       
}

////////////////////////////////////////////////////////////////////////
// blur, babe, blur (heavy performance hit for a so-so fullscreen effect)
////////////////////////////////////////////////////////////////////////

void BlurBackBuffer(void)
{
 if(!gTexBlurName) return;

 if(bKeepRatio) glViewport(0,0,iResX,iResY);

 glDisable(GL_SCISSOR_TEST);
 glDisable(GL_ALPHA_TEST);
 if(bOldSmoothShaded) {glShadeModel(GL_FLAT);bOldSmoothShaded=FALSE;}
 if(bBlendEnable)     {glDisable(GL_BLEND);bBlendEnable=FALSE;}
 if(!bTexEnabled)     {glEnable(GL_TEXTURE_2D);bTexEnabled=TRUE;}
 if(iZBufferDepth)    glDisable(GL_DEPTH_TEST);    
 if(bDrawDither)      glDisable(GL_DITHER); 

 gTexName=gTexBlurName;
 glBindTexture(GL_TEXTURE_2D, gTexName);

 glCopyTexSubImage2D( GL_TEXTURE_2D, 0,                // get back buffer in texture
                      0,
                      0,
                      0,
                      0,
                      iResX,iResY);

 vertex[0].x=0;
 vertex[0].y=PSXDisplay.DisplayMode.y;
 vertex[1].x=PSXDisplay.DisplayMode.x;
 vertex[1].y=PSXDisplay.DisplayMode.y;
 vertex[2].x=PSXDisplay.DisplayMode.x;
 vertex[2].y=0;
 vertex[3].x=0;
 vertex[3].y=0;
 vertex[0].sow=0;
 vertex[0].tow=0;

#ifdef OWNSCALE
 vertex[1].sow=((GLfloat)iFTexA)/256.0f;
 vertex[2].tow=((GLfloat)iFTexB)/256.0f;
#else
 vertex[1].sow=iFTexA;
 vertex[2].tow=iFTexB;
#endif
 vertex[1].tow=0;
 vertex[2].sow=vertex[1].sow;
 vertex[3].sow=0;
 vertex[3].tow=vertex[2].tow;
 
 if(bGLBlend) glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);    
 vertex[0].c.lcol=0x7fffffff;
 SETCOL(vertex[0]); 

 DrawMultiBlur();                                      // draw the backbuffer texture to create blur effect

 glEnable(GL_ALPHA_TEST);
 glEnable(GL_SCISSOR_TEST);
 if(iZBufferDepth)  glEnable(GL_DEPTH_TEST);    
 if(bDrawDither)    glEnable(GL_DITHER);    
 if(bGLBlend) glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, COMBINE_EXT);    

 if(bKeepRatio)
  glViewport(rRatioRect.left,                            // re-init viewport
             iResY-(rRatioRect.top+rRatioRect.bottom),
             rRatioRect.right, 
             rRatioRect.bottom);            
}

////////////////////////////////////////////////////////////////////////
// "unblur" repairs the backbuffer after a blur

void UnBlurBackBuffer(void)
{
 if(!gTexBlurName) return;

 if(bKeepRatio) glViewport(0,0,iResX,iResY);

 glDisable(GL_SCISSOR_TEST);
 glDisable(GL_ALPHA_TEST);
 if(bBlendEnable)    {glDisable(GL_BLEND);bBlendEnable=FALSE;}
 if(!bTexEnabled)    {glEnable(GL_TEXTURE_2D);bTexEnabled=TRUE;}
 if(iZBufferDepth)    glDisable(GL_DEPTH_TEST);    
 if(bDrawDither)      glDisable(GL_DITHER); 

 gTexName=gTexBlurName;
 glBindTexture(GL_TEXTURE_2D, gTexName);

 vertex[0].x=0;
 vertex[0].y=PSXDisplay.DisplayMode.y;
 vertex[1].x=PSXDisplay.DisplayMode.x;
 vertex[1].y=PSXDisplay.DisplayMode.y;
 vertex[2].x=PSXDisplay.DisplayMode.x;
 vertex[2].y=0;
 vertex[3].x=0;
 vertex[3].y=0;
 vertex[0].sow=0;
 vertex[0].tow=0;
#ifdef OWNSCALE
 vertex[1].sow=((GLfloat)iFTexA)/256.0f;
 vertex[2].tow=((GLfloat)iFTexB)/256.0f;
#else
 vertex[1].sow=iFTexA;
 vertex[2].tow=iFTexB;
#endif
 vertex[1].tow=0;
 vertex[2].sow=vertex[1].sow;
 vertex[3].sow=0;
 vertex[3].tow=vertex[2].tow;
 if(bGLBlend) glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);    
 vertex[0].c.lcol=0xffffffff;
 SETCOL(vertex[0]); 

 // simply draw the backbuffer texture (without blur)
 XPRIMdrawTexturedQuad(&vertex[0], &vertex[1], &vertex[2], &vertex[3]);

 glEnable(GL_ALPHA_TEST);
 glEnable(GL_SCISSOR_TEST);
 if(iZBufferDepth)  glEnable(GL_DEPTH_TEST);    
 if(bDrawDither)    glEnable(GL_DITHER);                  // dither mode
 if(bGLBlend) glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, COMBINE_EXT);    

 if(bKeepRatio)
  glViewport(rRatioRect.left,
             iResY-(rRatioRect.top+rRatioRect.bottom),
             rRatioRect.right, 
             rRatioRect.bottom);                         // init viewport
}

////////////////////////////////////////////////////////////////////////
// Update display (swap buffers)... called in interlaced mode on 
// every emulated vsync, otherwise whenever the displayed screen region
// has been changed
////////////////////////////////////////////////////////////////////////

int iLastRGB24=0;                                      // special vars for checking when to skip two display updates
int iSkipTwo=0;

void updateDisplay(void)                               // UPDATE DISPLAY
{
 BOOL bBlur=FALSE;

 bFakeFrontBuffer=FALSE;
 bRenderFrontBuffer=FALSE;

 if(iRenderFVR)                                        // frame buffer read fix mode still active?
  {
   iRenderFVR--;                                       // -> if some frames in a row without read access: turn off mode
   if(!iRenderFVR) bFullVRam=FALSE;
  }

 if(iLastRGB24 && iLastRGB24!=PSXDisplay.RGB24+1)      // (mdec) garbage check
  {
   iSkipTwo=2;                                         // -> skip two frames to avoid garbage if color mode changes
  }
 iLastRGB24=0;

 if(PSXDisplay.RGB24)// && !bNeedUploadAfter)          // (mdec) upload wanted?
  {
   PrepareFullScreenUpload(-1);
   UploadScreen(PSXDisplay.Interlaced);                // -> upload whole screen from psx vram
   bNeedUploadTest=FALSE;
   bNeedInterlaceUpdate=FALSE;
   bNeedUploadAfter=FALSE;
   bNeedRGB24Update=FALSE;
  }
 else
 if(bNeedInterlaceUpdate)                              // smaller upload?
  {
   bNeedInterlaceUpdate=FALSE;
   xrUploadArea=xrUploadAreaIL;                        // -> upload this rect
   UploadScreen(TRUE);
  }

 if(dwActFixes&512) bCheckFF9G4(NULL);                 // special game fix for FF9 

 if(PreviousPSXDisplay.Range.x0||                      // paint black borders around display area, if needed
    PreviousPSXDisplay.Range.y0)
  PaintBlackBorders();

 if(PSXDisplay.Disabled)                               // display disabled?
  {
   // moved here
   glDisable(GL_SCISSOR_TEST);                       
   glClearColor(0,0,0,128);                            // -> clear whole backbuffer
   glClear(uiBufferBits);
   glEnable(GL_SCISSOR_TEST);                       
   gl_z=0.0f;
   bDisplayNotSet = TRUE;
  }

 if(iSkipTwo)                                          // we are in skipping mood?
  {
   iSkipTwo--;
   iDrawnSomething=0;                                  // -> simply lie about something drawn
  }

 if(iBlurBuffer && !bSkipNextFrame)                    // "blur display" activated?
  {BlurBackBuffer();bBlur=TRUE;}                       // -> blur it

 if(iUseScanLines) SetScanLines();                     // "scan lines" activated? do it

 if(usCursorActive) ShowGunCursor();                   // "gun cursor" wanted? show 'em

 if(dwActFixes&128)                                    // special FPS limitation mode?
  {
   if(bUseFrameLimit) PCFrameCap();                    // -> ok, do it
   if(bUseFrameSkip || ulKeybits&KEY_SHOWFPS)  
    PCcalcfps();         
  }

 if(gTexPicName) DisplayPic();                         // some gpu info picture active? display it

 if(bSnapShot) DoSnapShot();                           // snapshot key pressed? cheeeese :)

 if(ulKeybits&KEY_SHOWFPS)                             // wanna see FPS?
  {
   sprintf(szDispBuf,"%06.1f",fps_cur);
   DisplayText();                                      // -> show it
  }

 //----------------------------------------------------//
 // main buffer swapping (well, or skip it)

 if(bUseFrameSkip)                                     // frame skipping active ?
  {
   if(!bSkipNextFrame) 
    {
     if(iDrawnSomething)
      glXSwapBuffers(display,window);
    }
   if(dwActFixes&0x180)                                // -> special old frame skipping: skip max one in a row
    {
     if((fps_skip < fFrameRateHz) && !(bSkipNextFrame)) 
      {bSkipNextFrame = TRUE; fps_skip=fFrameRateHz;}
     else bSkipNextFrame = FALSE;
    }
   else FrameSkip();
  }
 else                                                  // no skip ?
  {
   if(iDrawnSomething)
    glXSwapBuffers(display,window);
  }

 iDrawnSomething=0;

 //----------------------------------------------------//

 if(lClearOnSwap)                                      // clear buffer after swap?
  {
   GLclampf g,b,r;

   if(bDisplayNotSet)                                  // -> set new vals
    SetOGLDisplaySettings(1);

   g=((GLclampf)GREEN(lClearOnSwapColor))/255.0f;      // -> get col
   b=((GLclampf)BLUE(lClearOnSwapColor))/255.0f;
   r=((GLclampf)RED(lClearOnSwapColor))/255.0f;
   
   glDisable(GL_SCISSOR_TEST);                       
   glClearColor(r,g,b,128);                            // -> clear 
   glClear(uiBufferBits);
   glEnable(GL_SCISSOR_TEST);                       
   lClearOnSwap=0;                                     // -> done
  }
 else 
  {
   if(bBlur) UnBlurBackBuffer();                       // unblur buff, if blurred before

   if(iZBufferDepth)                                   // clear zbuffer as well (if activated)
    {
     glDisable(GL_SCISSOR_TEST);                       
     glClear(GL_DEPTH_BUFFER_BIT);
     glEnable(GL_SCISSOR_TEST);                       
    }
  }
 gl_z=0.0f;

 //----------------------------------------------------//
 // additional uploads immediatly after swapping

 if(bNeedUploadAfter)                                  // upload wanted?
  {
   bNeedUploadAfter=FALSE;                           
   bNeedUploadTest=FALSE;
   UploadScreen(-1);                                   // -> upload
  }
 
 if(bNeedUploadTest)
  {
   bNeedUploadTest=FALSE;
   if(PSXDisplay.InterlacedTest &&
      //iOffscreenDrawing>2 &&
      PreviousPSXDisplay.DisplayPosition.x==PSXDisplay.DisplayPosition.x &&
      PreviousPSXDisplay.DisplayEnd.x==PSXDisplay.DisplayEnd.x &&
      PreviousPSXDisplay.DisplayPosition.y==PSXDisplay.DisplayPosition.y &&
      PreviousPSXDisplay.DisplayEnd.y==PSXDisplay.DisplayEnd.y)
    {
     PrepareFullScreenUpload(TRUE);
     UploadScreen(TRUE);
    }
  }

 //----------------------------------------------------//
 // rumbling (main emu pad effect)

 if(iRumbleTime)                                       // shake screen by modifying view port
  {
   int i1=0,i2=0,i3=0,i4=0;

   iRumbleTime--;
   if(iRumbleTime) 
    {
     i1=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2); 
     i2=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2); 
     i3=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2); 
     i4=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2); 
    }

   glViewport(rRatioRect.left+i1,                      
              iResY-(rRatioRect.top+rRatioRect.bottom)+i2,
              rRatioRect.right+i3, 
              rRatioRect.bottom+i4);            
  }

 if(ulKeybits&KEY_RESETTEXSTORE) ResetStuff();         // reset on gpu mode changes? do it before next frame is filled
}

////////////////////////////////////////////////////////////////////////
// update front display: smaller update func, if something has changed 
// in the frontbuffer... dirty, but hey... real men know no pain
////////////////////////////////////////////////////////////////////////

void updateFrontDisplay(void)
{
 if(PreviousPSXDisplay.Range.x0||
    PreviousPSXDisplay.Range.y0)
  PaintBlackBorders();

 if(iBlurBuffer) BlurBackBuffer();

 if(iUseScanLines) SetScanLines();

 if(usCursorActive) ShowGunCursor();

 bFakeFrontBuffer=FALSE;
 bRenderFrontBuffer=FALSE;

 if(gTexPicName) DisplayPic();
 if(ulKeybits&KEY_SHOWFPS) DisplayText();

 if(iDrawnSomething)                                   // linux:
  glXSwapBuffers(display,window);

 if(iBlurBuffer) UnBlurBackBuffer();
}
                                             
////////////////////////////////////////////////////////////////////////
// check if update needed
////////////////////////////////////////////////////////////////////////

void ChangeDispOffsetsX(void)                          // CENTER X
{
 int lx,l;short sO;

 if(!PSXDisplay.Range.x1) return;                      // some range given?

 l=PSXDisplay.DisplayMode.x;

 l*=(int)PSXDisplay.Range.x1;                          // some funky calculation
 l/=2560;lx=l;l&=0xfffffff8;
 
 if(l==PreviousPSXDisplay.Range.x1) return;            // some change?

 sO=PreviousPSXDisplay.Range.x0;                       // store old

 if(lx>=PSXDisplay.DisplayMode.x)                      // range bigger?
  {
   PreviousPSXDisplay.Range.x1=                        // -> take display width
    PSXDisplay.DisplayMode.x;
   PreviousPSXDisplay.Range.x0=0;                      // -> start pos is 0
  }
 else                                                  // range smaller? center it
  {
   PreviousPSXDisplay.Range.x1=l;                      // -> store width (8 pixel aligned)
    PreviousPSXDisplay.Range.x0=                       // -> calc start pos
    (PSXDisplay.Range.x0-500)/8;
   if(PreviousPSXDisplay.Range.x0<0)                   // -> we don't support neg. values yet
    PreviousPSXDisplay.Range.x0=0;

   if((PreviousPSXDisplay.Range.x0+lx)>                // -> uhuu... that's too much
      PSXDisplay.DisplayMode.x)
    {
     PreviousPSXDisplay.Range.x0=                      // -> adjust start
      PSXDisplay.DisplayMode.x-lx;
     PreviousPSXDisplay.Range.x1+=lx-l;                // -> adjust width
    }                   
  }

 if(sO!=PreviousPSXDisplay.Range.x0)                   // something changed?
  {
   bDisplayNotSet=TRUE;                                // -> recalc display stuff
  }
}

////////////////////////////////////////////////////////////////////////

void ChangeDispOffsetsY(void)                          // CENTER Y
{
 int iT;short sO;                                      // store previous y size

 if(PSXDisplay.PAL) iT=48; else iT=28;                 // different offsets on PAL/NTSC

 if(PSXDisplay.Range.y0>=iT)                           // crossed the security line? :)
  {
   PreviousPSXDisplay.Range.y1=                        // -> store width
    PSXDisplay.DisplayModeNew.y;
   
   sO=(PSXDisplay.Range.y0-iT-4)*PSXDisplay.Double;    // -> calc offset
   if(sO<0) sO=0;

   PSXDisplay.DisplayModeNew.y+=sO;                    // -> add offset to y size, too
  }
 else sO=0;                                            // else no offset

 if(sO!=PreviousPSXDisplay.Range.y0)                   // something changed?
  {
   PreviousPSXDisplay.Range.y0=sO;
   bDisplayNotSet=TRUE;                                // -> recalc display stuff
  }
}

////////////////////////////////////////////////////////////////////////
// Aspect ratio of ogl screen: simply adjusting ogl view port
////////////////////////////////////////////////////////////////////////

void SetAspectRatio(void)
{
 float xs,ys,s;RECT r;

 if(!PSXDisplay.DisplayModeNew.x) return;
 if(!PSXDisplay.DisplayModeNew.y) return;

 xs=(float)iResX/(float)PSXDisplay.DisplayModeNew.x;
 ys=(float)iResY/(float)PSXDisplay.DisplayModeNew.y;

 s=min(xs,ys);
 r.right =(int)((float)PSXDisplay.DisplayModeNew.x*s);
 r.bottom=(int)((float)PSXDisplay.DisplayModeNew.y*s);
 if(r.right  > iResX) r.right  = iResX;
 if(r.bottom > iResY) r.bottom = iResY;
 if(r.right  < 1)     r.right  = 1;
 if(r.bottom < 1)     r.bottom = 1;

 r.left = (iResX-r.right)/2;
 r.top  = (iResY-r.bottom)/2;

 if(r.bottom<rRatioRect.bottom ||
    r.right <rRatioRect.right)
  {
   RECT rC;
   glClearColor(0,0,0,128);                         

   if(r.right <rRatioRect.right)
    {
     rC.left=0;
     rC.top=0;
     rC.right=r.left;
     rC.bottom=iResY;
     glScissor(rC.left,rC.top,rC.right,rC.bottom);
     glClear(uiBufferBits);
     rC.left=iResX-rC.right;
     glScissor(rC.left,rC.top,rC.right,rC.bottom);
     glClear(uiBufferBits);
    }

   if(r.bottom <rRatioRect.bottom)
    {
     rC.left=0;
     rC.top=0;
     rC.right=iResX;
     rC.bottom=r.top;
     glScissor(rC.left,rC.top,rC.right,rC.bottom);
     glClear(uiBufferBits);
     rC.top=iResY-rC.bottom;
     glScissor(rC.left,rC.top,rC.right,rC.bottom);
     glClear(uiBufferBits);
    }
   
   bSetClip=TRUE;
   bDisplayNotSet=TRUE;
  }

 rRatioRect=r;


 glViewport(rRatioRect.left,
            iResY-(rRatioRect.top+rRatioRect.bottom),
            rRatioRect.right,
            rRatioRect.bottom);                         // init viewport
}

////////////////////////////////////////////////////////////////////////
// big ass check, if an ogl swap buffer is needed
////////////////////////////////////////////////////////////////////////

void updateDisplayIfChanged(void)
{
 BOOL bUp;

 if ((PSXDisplay.DisplayMode.y == PSXDisplay.DisplayModeNew.y) && 
     (PSXDisplay.DisplayMode.x == PSXDisplay.DisplayModeNew.x))
  {
   if((PSXDisplay.RGB24      == PSXDisplay.RGB24New) && 
      (PSXDisplay.Interlaced == PSXDisplay.InterlacedNew)) 
      return;                                          // nothing has changed? fine, no swap buffer needed
  }
 else                                                  // some res change?
  {
   glLoadIdentity();
   glOrtho(0,PSXDisplay.DisplayModeNew.x,              // -> new psx resolution
             PSXDisplay.DisplayModeNew.y, 0, -1, 1);
   if(bKeepRatio) SetAspectRatio();
  }

 bDisplayNotSet = TRUE;                                // re-calc offsets/display area
 
 bUp=FALSE;
 if(PSXDisplay.RGB24!=PSXDisplay.RGB24New)             // clean up textures, if rgb mode change (usually mdec on/off)
  {
   PreviousPSXDisplay.RGB24=0;                         // no full 24 frame uploaded yet
   ResetTextureArea(FALSE);
   bUp=TRUE;
  }

 PSXDisplay.RGB24         = PSXDisplay.RGB24New;       // get new infos
 PSXDisplay.DisplayMode.y = PSXDisplay.DisplayModeNew.y;
 PSXDisplay.DisplayMode.x = PSXDisplay.DisplayModeNew.x;
 PSXDisplay.Interlaced    = PSXDisplay.InterlacedNew;
    
 PSXDisplay.DisplayEnd.x=                              // calc new ends
  PSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
 PSXDisplay.DisplayEnd.y=
  PSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;
 PreviousPSXDisplay.DisplayEnd.x=
  PreviousPSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
 PreviousPSXDisplay.DisplayEnd.y=
  PreviousPSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;

 ChangeDispOffsetsX();

 if(iFrameLimit==2) SetAutoFrameCap();                 // set new fps limit vals (depends on interlace)

 if(bUp) updateDisplay();                              // yeah, real update (swap buffer)
}

////////////////////////////////////////////////////////////////////////
// swap update check (called by psx vsync function)
////////////////////////////////////////////////////////////////////////

BOOL bSwapCheck(void)
{
 static int iPosCheck=0;
 static PSXPoint_t pO;
 static PSXPoint_t pD;
 static int iDoAgain=0;

 if(PSXDisplay.DisplayPosition.x==pO.x &&
    PSXDisplay.DisplayPosition.y==pO.y &&
    PSXDisplay.DisplayEnd.x==pD.x &&
    PSXDisplay.DisplayEnd.y==pD.y)
      iPosCheck++;
 else iPosCheck=0;

 pO=PSXDisplay.DisplayPosition;
 pD=PSXDisplay.DisplayEnd;

 if(iPosCheck<=4) return FALSE;

 iPosCheck=4;

 if(PSXDisplay.Interlaced) return FALSE;

 if (bNeedInterlaceUpdate||
     bNeedRGB24Update ||
     bNeedUploadAfter|| 
     bNeedUploadTest || 
     iDoAgain
    )
  {
   iDoAgain=0;
   if(bNeedUploadAfter) 
    iDoAgain=1;
   if(bNeedUploadTest && PSXDisplay.InterlacedTest)
    iDoAgain=1;

   bDisplayNotSet = TRUE;
   updateDisplay();

   PreviousPSXDisplay.DisplayPosition.x=PSXDisplay.DisplayPosition.x;
   PreviousPSXDisplay.DisplayPosition.y=PSXDisplay.DisplayPosition.y;
   PreviousPSXDisplay.DisplayEnd.x=PSXDisplay.DisplayEnd.x;
   PreviousPSXDisplay.DisplayEnd.y=PSXDisplay.DisplayEnd.y;
   pO=PSXDisplay.DisplayPosition;
   pD=PSXDisplay.DisplayEnd;

   return TRUE;
  }

 return FALSE;
} 

////////////////////////////////////////////////////////////////////////
// gun cursor func: player=0-7, x=0-511, y=0-255
////////////////////////////////////////////////////////////////////////

void CALLBACK GPUcursor(int iPlayer,int x,int y)
{
 if(iPlayer<0) return;
 if(iPlayer>7) return;

 usCursorActive|=(1<<iPlayer);

 if(x<0)              x=0;
 if(x>iGPUHeightMask) x=iGPUHeightMask;
 if(y<0)              y=0;
 if(y>255)            y=255;

 ptCursorPoint[iPlayer].x=x;
 ptCursorPoint[iPlayer].y=y;
}

////////////////////////////////////////////////////////////////////////
// update lace is called every VSync. Basically we limit frame rate 
// here, and in interlaced mode we swap ogl display buffers.
////////////////////////////////////////////////////////////////////////

static unsigned short usFirstPos=2;

void CALLBACK GPUupdateLace(void)
{
 if(!(dwActFixes&0x1000))                               
  STATUSREG^=0x80000000;                               // interlaced bit toggle, if the CC game fix is not active (see gpuReadStatus)

 if(!(dwActFixes&128))                                 // normal frame limit func
  CheckFrameRate();

 if(iOffscreenDrawing==4)                              // special check if high offscreen drawing is on
  {
   if(bSwapCheck()) return;
  }

 if(PSXDisplay.Interlaced)                             // interlaced mode?
  {
   if(PSXDisplay.DisplayMode.x>0 && PSXDisplay.DisplayMode.y>0)
    {
     updateDisplay();                                  // -> swap buffers (new frame)
    }
  }
 else if(bRenderFrontBuffer)                           // no interlace mode? and some stuff in front has changed?
  {
   updateFrontDisplay();                               // -> update front buffer
  }
 else if(usFirstPos==1)                                // initial updates (after startup)
  {
   updateDisplay();
  }
}

////////////////////////////////////////////////////////////////////////
// process read request from GPU status register
////////////////////////////////////////////////////////////////////////

uint32_t CALLBACK GPUreadStatus(void)
{
 if(dwActFixes&0x1000)                                 // CC game fix
  {
   static int iNumRead=0;
   if((iNumRead++)==2)
    {
     iNumRead=0;
     STATUSREG^=0x80000000;                            // interlaced bit toggle... we do it on every second read status... needed by some games (like ChronoCross)
    }
  }

 if(iFakePrimBusy)                                     // 27.10.2007 - emulating some 'busy' while drawing... pfff... not perfect, but since our emulated dma is not done in an extra thread...
  {
   iFakePrimBusy--;

   if(iFakePrimBusy&1)                                 // we do a busy-idle-busy-idle sequence after/while drawing prims
    {
     GPUIsBusy;
     GPUIsNotReadyForCommands;
    }
   else
    {
     GPUIsIdle;
     GPUIsReadyForCommands;
    }
  }

 return STATUSREG;
}

////////////////////////////////////////////////////////////////////////
// processes data send to GPU status register
// these are always single packet commands.
////////////////////////////////////////////////////////////////////////

void CALLBACK GPUwriteStatus(uint32_t gdata)
{
 uint32_t lCommand=(gdata>>24)&0xff;
 
 ulStatusControl[lCommand]=gdata;

 switch(lCommand)
  {
   //--------------------------------------------------//
   // reset gpu
   case 0x00:
    memset(ulGPUInfoVals, 0x00, 16 * sizeof(uint32_t));
    lGPUstatusRet = 0x14802000;
    PSXDisplay.Disabled=1;
    iDataWriteMode=iDataReadMode=DR_NORMAL;
    PSXDisplay.DrawOffset.x=PSXDisplay.DrawOffset.y=0;
    drawX=drawY=0;drawW=drawH=0;
    sSetMask=0;lSetMask=0;bCheckMask=FALSE;iSetMask=0;
    usMirror=0;
    GlobalTextAddrX=0;GlobalTextAddrY=0;
    GlobalTextTP=0;GlobalTextABR=0;
    PSXDisplay.RGB24=FALSE;
    PSXDisplay.Interlaced=FALSE;
    bUsingTWin = FALSE;
    return;

   // dis/enable display
   case 0x03:  
    PreviousPSXDisplay.Disabled = PSXDisplay.Disabled;
    PSXDisplay.Disabled = (gdata & 1);

    if(PSXDisplay.Disabled) 
         STATUSREG|=GPUSTATUS_DISPLAYDISABLED;
    else STATUSREG&=~GPUSTATUS_DISPLAYDISABLED;

    if (iOffscreenDrawing==4 &&
         PreviousPSXDisplay.Disabled && 
        !(PSXDisplay.Disabled))
     {

      if(!PSXDisplay.RGB24)
       {
        PrepareFullScreenUpload(TRUE);
        UploadScreen(TRUE); 
        updateDisplay();
       }
     }

    return;

   // setting transfer mode
   case 0x04:
    gdata &= 0x03;                                     // only want the lower two bits

    iDataWriteMode=iDataReadMode=DR_NORMAL;
    if(gdata==0x02) iDataWriteMode=DR_VRAMTRANSFER;
    if(gdata==0x03) iDataReadMode =DR_VRAMTRANSFER;

    STATUSREG&=~GPUSTATUS_DMABITS;                     // clear the current settings of the DMA bits
    STATUSREG|=(gdata << 29);                          // set the DMA bits according to the received data

    return;

   // setting display position
   case 0x05: 
    {
     short sx=(short)(gdata & 0x3ff);
     short sy;

     if(iGPUHeight==1024)
      {
       if(dwGPUVersion==2) 
            sy = (short)((gdata>>12)&0x3ff);
       else sy = (short)((gdata>>10)&0x3ff);
      }
     else sy = (short)((gdata>>10)&0x3ff);             // really: 0x1ff, but we adjust it later

     if (sy & 0x200) 
      {
       sy|=0xfc00;
       PreviousPSXDisplay.DisplayModeNew.y=sy/PSXDisplay.Double;
       sy=0;
      }
     else PreviousPSXDisplay.DisplayModeNew.y=0;

     if(sx>1000) sx=0;

     if(usFirstPos)
      {
       usFirstPos--;
       if(usFirstPos)
        {
         PreviousPSXDisplay.DisplayPosition.x = sx;
         PreviousPSXDisplay.DisplayPosition.y = sy;
         PSXDisplay.DisplayPosition.x = sx;
         PSXDisplay.DisplayPosition.y = sy;
        }
      }

     if(dwActFixes&8) 
      {
       if((!PSXDisplay.Interlaced) &&
          PreviousPSXDisplay.DisplayPosition.x == sx  &&
          PreviousPSXDisplay.DisplayPosition.y == sy)
        return;

       PSXDisplay.DisplayPosition.x = PreviousPSXDisplay.DisplayPosition.x;
       PSXDisplay.DisplayPosition.y = PreviousPSXDisplay.DisplayPosition.y;
       PreviousPSXDisplay.DisplayPosition.x = sx;
       PreviousPSXDisplay.DisplayPosition.y = sy;
      }
     else
      {
       if((!PSXDisplay.Interlaced) &&
          PSXDisplay.DisplayPosition.x == sx  &&
          PSXDisplay.DisplayPosition.y == sy)
        return;
       PreviousPSXDisplay.DisplayPosition.x = PSXDisplay.DisplayPosition.x;
       PreviousPSXDisplay.DisplayPosition.y = PSXDisplay.DisplayPosition.y;
       PSXDisplay.DisplayPosition.x = sx;
       PSXDisplay.DisplayPosition.y = sy;
      }

     PSXDisplay.DisplayEnd.x=
      PSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
     PSXDisplay.DisplayEnd.y=
      PSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;

     PreviousPSXDisplay.DisplayEnd.x=
      PreviousPSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
     PreviousPSXDisplay.DisplayEnd.y=
      PreviousPSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;

     bDisplayNotSet = TRUE;

     if (!(PSXDisplay.Interlaced))
      {
       updateDisplay();
      }
     else
     if(PSXDisplay.InterlacedTest && 
        ((PreviousPSXDisplay.DisplayPosition.x != PSXDisplay.DisplayPosition.x)||
         (PreviousPSXDisplay.DisplayPosition.y != PSXDisplay.DisplayPosition.y)))
      PSXDisplay.InterlacedTest--;
 
     return;
    }

   // setting width
   case 0x06:

    PSXDisplay.Range.x0=gdata & 0x7ff;      //0x3ff;
    PSXDisplay.Range.x1=(gdata>>12) & 0xfff;//0x7ff;

    PSXDisplay.Range.x1-=PSXDisplay.Range.x0;

    ChangeDispOffsetsX();

    return;

   // setting height
   case 0x07:

    PreviousPSXDisplay.Height = PSXDisplay.Height;

    PSXDisplay.Range.y0=gdata & 0x3ff;
    PSXDisplay.Range.y1=(gdata>>10) & 0x3ff;

    PSXDisplay.Height = PSXDisplay.Range.y1 - 
                        PSXDisplay.Range.y0 +
                        PreviousPSXDisplay.DisplayModeNew.y;

    if (PreviousPSXDisplay.Height != PSXDisplay.Height)
     {
      PSXDisplay.DisplayModeNew.y=PSXDisplay.Height*PSXDisplay.Double;
      ChangeDispOffsetsY();
      updateDisplayIfChanged();
     }
    return;
 
   // setting display infos
   case 0x08:

    PSXDisplay.DisplayModeNew.x = dispWidths[(gdata & 0x03) | ((gdata & 0x40) >> 4)];

    if (gdata&0x04) PSXDisplay.Double=2;
    else            PSXDisplay.Double=1;
    PSXDisplay.DisplayModeNew.y = PSXDisplay.Height*PSXDisplay.Double;

    ChangeDispOffsetsY();
  
    PSXDisplay.PAL           = (gdata & 0x08)?TRUE:FALSE; // if 1 - PAL mode, else NTSC
    PSXDisplay.RGB24New      = (gdata & 0x10)?TRUE:FALSE; // if 1 - TrueColor
    PSXDisplay.InterlacedNew = (gdata & 0x20)?TRUE:FALSE; // if 1 - Interlace

    STATUSREG&=~GPUSTATUS_WIDTHBITS;                   // clear the width bits

    STATUSREG|=
               (((gdata & 0x03) << 17) | 
               ((gdata & 0x40) << 10));                // set the width bits

    PreviousPSXDisplay.InterlacedNew=FALSE;
    if (PSXDisplay.InterlacedNew)
     {
      if(!PSXDisplay.Interlaced)
       {
        PSXDisplay.InterlacedTest=2;
        PreviousPSXDisplay.DisplayPosition.x = PSXDisplay.DisplayPosition.x;
        PreviousPSXDisplay.DisplayPosition.y = PSXDisplay.DisplayPosition.y;
        PreviousPSXDisplay.InterlacedNew=TRUE;
       }

      STATUSREG|=GPUSTATUS_INTERLACED;
     }
    else 
     {
      PSXDisplay.InterlacedTest=0;
      STATUSREG&=~GPUSTATUS_INTERLACED;
     }

    if (PSXDisplay.PAL)
         STATUSREG|=GPUSTATUS_PAL;
    else STATUSREG&=~GPUSTATUS_PAL;

    if (PSXDisplay.Double==2)
         STATUSREG|=GPUSTATUS_DOUBLEHEIGHT;
    else STATUSREG&=~GPUSTATUS_DOUBLEHEIGHT;

    if (PSXDisplay.RGB24New)
         STATUSREG|=GPUSTATUS_RGB24;
    else STATUSREG&=~GPUSTATUS_RGB24;

    updateDisplayIfChanged();

    return;

   //--------------------------------------------------//
   // ask about GPU version and other stuff
   case 0x10: 

    gdata&=0xff;

    switch(gdata) 
     {
      case 0x02:
       GPUdataRet=ulGPUInfoVals[INFO_TW];              // tw infos
       return;
      case 0x03:
       GPUdataRet=ulGPUInfoVals[INFO_DRAWSTART];       // draw start
       return;
      case 0x04:
       GPUdataRet=ulGPUInfoVals[INFO_DRAWEND];         // draw end
       return;
      case 0x05:
      case 0x06:
       GPUdataRet=ulGPUInfoVals[INFO_DRAWOFF];         // draw offset
       return;
      case 0x07:
       if(dwGPUVersion==2)
            GPUdataRet=0x01;
       else GPUdataRet=0x02;                           // gpu type
       return;
      case 0x08:
      case 0x0F:                                       // some bios addr?
       GPUdataRet=0xBFC03720;
       return;
     }
    return;
   //--------------------------------------------------//
  }
}

////////////////////////////////////////////////////////////////////////
// vram read/write helpers
////////////////////////////////////////////////////////////////////////

BOOL bNeedWriteUpload=FALSE;

__inline void FinishedVRAMWrite(void)
{
 if(bNeedWriteUpload)
  {
   bNeedWriteUpload=FALSE;
   CheckWriteUpdate();
  }

 // set register to NORMAL operation
 iDataWriteMode = DR_NORMAL;

 // reset transfer values, to prevent mis-transfer of data
 VRAMWrite.ColsRemaining = 0;
 VRAMWrite.RowsRemaining = 0;
}

__inline void FinishedVRAMRead(void)
{
 // set register to NORMAL operation
 iDataReadMode = DR_NORMAL;
 // reset transfer values, to prevent mis-transfer of data
 VRAMRead.x = 0;
 VRAMRead.y = 0;
 VRAMRead.Width = 0;
 VRAMRead.Height = 0;
 VRAMRead.ColsRemaining = 0;
 VRAMRead.RowsRemaining = 0;

 // indicate GPU is no longer ready for VRAM data in the STATUS REGISTER
 STATUSREG&=~GPUSTATUS_READYFORVRAM;
}

////////////////////////////////////////////////////////////////////////
// vram read check ex (reading from card's back/frontbuffer if needed...
// slow!)
////////////////////////////////////////////////////////////////////////

void CheckVRamReadEx(int x, int y, int dx, int dy)
{
 unsigned short sArea;
 int ux,uy,udx,udy,wx,wy;
 unsigned short * p1, *p2;
 float XS,YS;
 unsigned char * ps;
 unsigned char * px;
 unsigned short s,sx;

 if(STATUSREG&GPUSTATUS_RGB24) return;

 if(((dx  > PSXDisplay.DisplayPosition.x) &&
     (x   < PSXDisplay.DisplayEnd.x) &&
     (dy  > PSXDisplay.DisplayPosition.y) &&
     (y   < PSXDisplay.DisplayEnd.y)))
  sArea=0;
 else
 if((!(PSXDisplay.InterlacedTest) &&
     (dx  > PreviousPSXDisplay.DisplayPosition.x) &&
     (x   < PreviousPSXDisplay.DisplayEnd.x) &&
     (dy  > PreviousPSXDisplay.DisplayPosition.y) &&
     (y   < PreviousPSXDisplay.DisplayEnd.y)))
  sArea=1;
 else 
  {
   return;
  }

 //////////////

 if(iRenderFVR)
  {
   bFullVRam=TRUE;iRenderFVR=2;return;
  }
 bFullVRam=TRUE;iRenderFVR=2;

 //////////////

 p2=0;

 if(sArea==0)
  {
   ux=PSXDisplay.DisplayPosition.x;
   uy=PSXDisplay.DisplayPosition.y;
   udx=PSXDisplay.DisplayEnd.x-ux;
   udy=PSXDisplay.DisplayEnd.y-uy;
   if((PreviousPSXDisplay.DisplayEnd.x-
       PreviousPSXDisplay.DisplayPosition.x)==udx &&
      (PreviousPSXDisplay.DisplayEnd.y-
       PreviousPSXDisplay.DisplayPosition.y)==udy)
    p2=(psxVuw + (1024*PreviousPSXDisplay.DisplayPosition.y) + 
        PreviousPSXDisplay.DisplayPosition.x);
  }
 else
  {
   ux=PreviousPSXDisplay.DisplayPosition.x;
   uy=PreviousPSXDisplay.DisplayPosition.y;
   udx=PreviousPSXDisplay.DisplayEnd.x-ux;
   udy=PreviousPSXDisplay.DisplayEnd.y-uy;
   if((PSXDisplay.DisplayEnd.x-
       PSXDisplay.DisplayPosition.x)==udx &&
      (PSXDisplay.DisplayEnd.y-
       PSXDisplay.DisplayPosition.y)==udy)
    p2=(psxVuw + (1024*PSXDisplay.DisplayPosition.y) + 
        PSXDisplay.DisplayPosition.x);
  }

 p1=(psxVuw + (1024*uy) + ux);
 if(p1==p2) p2=0;

 x=0;y=0;
 wx=dx=udx;wy=dy=udy;

 if(udx<=0) return;
 if(udy<=0) return;
 if(dx<=0)  return;
 if(dy<=0)  return;
 if(wx<=0)  return;
 if(wy<=0)  return;

 XS=(float)rRatioRect.right/(float)wx;
 YS=(float)rRatioRect.bottom/(float)wy;

 dx=(int)((float)(dx)*XS);
 dy=(int)((float)(dy)*YS);

 if(dx>iResX) dx=iResX;
 if(dy>iResY) dy=iResY;

 if(dx<=0) return;
 if(dy<=0) return;

 // ogl y adjust
 y=iResY-y-dy;

 x+=rRatioRect.left;
 y-=rRatioRect.top;

 if(y<0) y=0; if((y+dy)>iResY) dy=iResY-y;

 if(!pGfxCardScreen)
  {
   glPixelStorei(GL_PACK_ALIGNMENT,1);
   pGfxCardScreen=(unsigned char *)malloc(iResX*iResY*4);
  }

 ps=pGfxCardScreen;
 
 if(!sArea) glReadBuffer(GL_FRONT);

 glReadPixels(x,y,dx,dy,GL_RGB,GL_UNSIGNED_BYTE,ps);
               
 if(!sArea) glReadBuffer(GL_BACK);

 s=0;

 XS=(float)dx/(float)(udx);
 YS=(float)dy/(float)(udy+1);
    
 for(y=udy;y>0;y--)
  {
   for(x=0;x<udx;x++)
    {
     if(p1>=psxVuw && p1<psxVuw_eom)
      {
       px=ps+(3*((int)((float)x * XS))+
             (3*dx)*((int)((float)y*YS)));
       sx=(*px)>>3;px++;
       s=sx;
       sx=(*px)>>3;px++;
       s|=sx<<5;
       sx=(*px)>>3;
       s|=sx<<10;
       s&=~0x8000;
       *p1=s;
      }
     if(p2>=psxVuw && p2<psxVuw_eom) *p2=s;

     p1++;
     if(p2) p2++;
    }

   p1 += 1024 - udx;
   if(p2) p2 += 1024 - udx;
  }
}

////////////////////////////////////////////////////////////////////////
// vram read check (reading from card's back/frontbuffer if needed... 
// slow!)
////////////////////////////////////////////////////////////////////////

void CheckVRamRead(int x, int y, int dx, int dy,BOOL bFront)
{
 unsigned short sArea;unsigned short * p;
 int ux,uy,udx,udy,wx,wy;float XS,YS;
 unsigned char * ps, * px;
 unsigned short s=0,sx;

 if(STATUSREG&GPUSTATUS_RGB24) return;

 if(((dx  > PSXDisplay.DisplayPosition.x) &&
     (x   < PSXDisplay.DisplayEnd.x) &&
     (dy  > PSXDisplay.DisplayPosition.y) &&
     (y   < PSXDisplay.DisplayEnd.y)))
  sArea=0;
 else
 if((!(PSXDisplay.InterlacedTest) &&
     (dx  > PreviousPSXDisplay.DisplayPosition.x) &&
     (x   < PreviousPSXDisplay.DisplayEnd.x) &&
     (dy  > PreviousPSXDisplay.DisplayPosition.y) &&
     (y   < PreviousPSXDisplay.DisplayEnd.y)))
  sArea=1;
 else 
  {
   return;
  }

 if(dwActFixes&0x40)
  {
   if(iRenderFVR)
    {
     bFullVRam=TRUE;iRenderFVR=2;return;
    }
   bFullVRam=TRUE;iRenderFVR=2;
  }

 ux=x;uy=y;udx=dx;udy=dy;

 if(sArea==0)
  {
   x -=PSXDisplay.DisplayPosition.x;
   dx-=PSXDisplay.DisplayPosition.x;
   y -=PSXDisplay.DisplayPosition.y;
   dy-=PSXDisplay.DisplayPosition.y;
   wx=PSXDisplay.DisplayEnd.x-PSXDisplay.DisplayPosition.x;
   wy=PSXDisplay.DisplayEnd.y-PSXDisplay.DisplayPosition.y;
  }
 else
  {
   x -=PreviousPSXDisplay.DisplayPosition.x;
   dx-=PreviousPSXDisplay.DisplayPosition.x;
   y -=PreviousPSXDisplay.DisplayPosition.y;
   dy-=PreviousPSXDisplay.DisplayPosition.y;
   wx=PreviousPSXDisplay.DisplayEnd.x-PreviousPSXDisplay.DisplayPosition.x;
   wy=PreviousPSXDisplay.DisplayEnd.y-PreviousPSXDisplay.DisplayPosition.y;
  }
 if(x<0) {ux-=x;x=0;}
 if(y<0) {uy-=y;y=0;}
 if(dx>wx) {udx-=(dx-wx);dx=wx;}
 if(dy>wy) {udy-=(dy-wy);dy=wy;}
 udx-=ux;
 udy-=uy;
  
 p=(psxVuw + (1024*uy) + ux);

 if(udx<=0) return;
 if(udy<=0) return;
 if(dx<=0)  return;
 if(dy<=0)  return;
 if(wx<=0)  return;
 if(wy<=0)  return;

 XS=(float)rRatioRect.right/(float)wx;
 YS=(float)rRatioRect.bottom/(float)wy;

 dx=(int)((float)(dx)*XS);
 dy=(int)((float)(dy)*YS);
 x=(int)((float)x*XS);
 y=(int)((float)y*YS);

 dx-=x;
 dy-=y;

 if(dx>iResX) dx=iResX;
 if(dy>iResY) dy=iResY;

 if(dx<=0) return;
 if(dy<=0) return;

 // ogl y adjust
 y=iResY-y-dy;

 x+=rRatioRect.left;
 y-=rRatioRect.top;

 if(y<0) y=0; if((y+dy)>iResY) dy=iResY-y;

 if(!pGfxCardScreen)
  {
   glPixelStorei(GL_PACK_ALIGNMENT,1);
   pGfxCardScreen=(unsigned char *)malloc(iResX*iResY*4);
  }

 ps=pGfxCardScreen;
 
 if(bFront) glReadBuffer(GL_FRONT);

 glReadPixels(x,y,dx,dy,GL_RGB,GL_UNSIGNED_BYTE,ps);
               
 if(bFront) glReadBuffer(GL_BACK);

 XS=(float)dx/(float)(udx);
 YS=(float)dy/(float)(udy+1);
    
 for(y=udy;y>0;y--)
  {
   for(x=0;x<udx;x++)
    {
     if(p>=psxVuw && p<psxVuw_eom)
      {
       px=ps+(3*((int)((float)x * XS))+
             (3*dx)*((int)((float)y*YS)));
       sx=(*px)>>3;px++;
       s=sx;
       sx=(*px)>>3;px++;
       s|=sx<<5;
       sx=(*px)>>3;
       s|=sx<<10;
       s&=~0x8000;
       *p=s;
      }
     p++;
    }
   p += 1024 - udx;
  }
}

////////////////////////////////////////////////////////////////////////
// core read from vram
////////////////////////////////////////////////////////////////////////

void CALLBACK GPUreadDataMem(uint32_t *pMem, int iSize)
{
 int i;

 if(iDataReadMode!=DR_VRAMTRANSFER) return;

 GPUIsBusy;

 // adjust read ptr, if necessary
 while(VRAMRead.ImagePtr>=psxVuw_eom)
  VRAMRead.ImagePtr-=iGPUHeight*1024;
 while(VRAMRead.ImagePtr<psxVuw)
  VRAMRead.ImagePtr+=iGPUHeight*1024;

 if((iFrameReadType&1 && iSize>1) &&
    !(iDrawnSomething==2 &&
      VRAMRead.x      == VRAMWrite.x     &&
      VRAMRead.y      == VRAMWrite.y     &&
      VRAMRead.Width  == VRAMWrite.Width &&
      VRAMRead.Height == VRAMWrite.Height))
  CheckVRamRead(VRAMRead.x,VRAMRead.y,
                VRAMRead.x+VRAMRead.RowsRemaining,
                VRAMRead.y+VRAMRead.ColsRemaining,
                TRUE);

 for(i=0;i<iSize;i++)
  {
   // do 2 seperate 16bit reads for compatibility (wrap issues)
   if ((VRAMRead.ColsRemaining > 0) && (VRAMRead.RowsRemaining > 0))
    {
     // lower 16 bit
     GPUdataRet=(uint32_t)*VRAMRead.ImagePtr;

     VRAMRead.ImagePtr++;
     if(VRAMRead.ImagePtr>=psxVuw_eom) VRAMRead.ImagePtr-=iGPUHeight*1024;
     VRAMRead.RowsRemaining --;

     if(VRAMRead.RowsRemaining<=0)
      {
       VRAMRead.RowsRemaining = VRAMRead.Width;
       VRAMRead.ColsRemaining--;
       VRAMRead.ImagePtr += 1024 - VRAMRead.Width;
       if(VRAMRead.ImagePtr>=psxVuw_eom) VRAMRead.ImagePtr-=iGPUHeight*1024;
      }

     // higher 16 bit (always, even if it's an odd width)
     GPUdataRet|=(uint32_t)(*VRAMRead.ImagePtr)<<16;
     *pMem++=GPUdataRet;

     if(VRAMRead.ColsRemaining <= 0)
      {FinishedVRAMRead();goto ENDREAD;}

     VRAMRead.ImagePtr++;
     if(VRAMRead.ImagePtr>=psxVuw_eom) VRAMRead.ImagePtr-=iGPUHeight*1024;
     VRAMRead.RowsRemaining--;
     if(VRAMRead.RowsRemaining<=0)
      {
       VRAMRead.RowsRemaining = VRAMRead.Width;
       VRAMRead.ColsRemaining--;
       VRAMRead.ImagePtr += 1024 - VRAMRead.Width;
       if(VRAMRead.ImagePtr>=psxVuw_eom) VRAMRead.ImagePtr-=iGPUHeight*1024;
      }
     if(VRAMRead.ColsRemaining <= 0)
      {FinishedVRAMRead();goto ENDREAD;}
    }
   else {FinishedVRAMRead();goto ENDREAD;}
  }

ENDREAD:
 GPUIsIdle;
}

uint32_t CALLBACK GPUreadData(void)
{
 uint32_t l;
 GPUreadDataMem(&l,1);
 return GPUdataRet;
}

////////////////////////////////////////////////////////////////////////
// helper table to know how much data is used by drawing commands
////////////////////////////////////////////////////////////////////////

const unsigned char primTableCX[256] =
{
    // 00
    0,0,3,0,0,0,0,0,
    // 08
    0,0,0,0,0,0,0,0,
    // 10
    0,0,0,0,0,0,0,0,
    // 18
    0,0,0,0,0,0,0,0,
    // 20
    4,4,4,4,7,7,7,7,
    // 28
    5,5,5,5,9,9,9,9,
    // 30
    6,6,6,6,9,9,9,9,
    // 38
    8,8,8,8,12,12,12,12,
    // 40
    3,3,3,3,0,0,0,0,
    // 48
//    5,5,5,5,6,6,6,6,      //FLINE
    254,254,254,254,254,254,254,254,
    // 50
    4,4,4,4,0,0,0,0,
    // 58
//    7,7,7,7,9,9,9,9,    //    LINEG3    LINEG4
    255,255,255,255,255,255,255,255,
    // 60
    3,3,3,3,4,4,4,4,    //    TILE    SPRT
    // 68
    2,2,2,2,3,3,3,3,    //    TILE1
    // 70
    2,2,2,2,3,3,3,3,
    // 78
    2,2,2,2,3,3,3,3,
    // 80
    4,0,0,0,0,0,0,0,
    // 88
    0,0,0,0,0,0,0,0,
    // 90
    0,0,0,0,0,0,0,0,
    // 98
    0,0,0,0,0,0,0,0,
    // a0
    3,0,0,0,0,0,0,0,
    // a8
    0,0,0,0,0,0,0,0,
    // b0
    0,0,0,0,0,0,0,0,
    // b8
    0,0,0,0,0,0,0,0,
    // c0
    3,0,0,0,0,0,0,0,
    // c8
    0,0,0,0,0,0,0,0,
    // d0
    0,0,0,0,0,0,0,0,
    // d8
    0,0,0,0,0,0,0,0,
    // e0
    0,1,1,1,1,1,1,0,
    // e8
    0,0,0,0,0,0,0,0,
    // f0
    0,0,0,0,0,0,0,0,
    // f8
    0,0,0,0,0,0,0,0
};

////////////////////////////////////////////////////////////////////////
// processes data send to GPU data register
////////////////////////////////////////////////////////////////////////

void CALLBACK GPUwriteDataMem(uint32_t *pMem, int iSize)
{
 unsigned char command;
 uint32_t gdata=0;
 int i=0;
 GPUIsBusy;
 GPUIsNotReadyForCommands;

STARTVRAM:

 if(iDataWriteMode==DR_VRAMTRANSFER)
  {
   // make sure we are in vram
   while(VRAMWrite.ImagePtr>=psxVuw_eom)
    VRAMWrite.ImagePtr-=iGPUHeight*1024;
   while(VRAMWrite.ImagePtr<psxVuw)
    VRAMWrite.ImagePtr+=iGPUHeight*1024;

   // now do the loop
   while(VRAMWrite.ColsRemaining>0)
    {
     while(VRAMWrite.RowsRemaining>0)
      {
       if(i>=iSize) {goto ENDVRAM;}
       i++;

       gdata=*pMem++;

       *VRAMWrite.ImagePtr++ = (unsigned short)gdata;
       if(VRAMWrite.ImagePtr>=psxVuw_eom) VRAMWrite.ImagePtr-=iGPUHeight*1024;
       VRAMWrite.RowsRemaining --;

       if(VRAMWrite.RowsRemaining <= 0)
        {
         VRAMWrite.ColsRemaining--;
         if (VRAMWrite.ColsRemaining <= 0)             // last pixel is odd width
          {
           gdata=(gdata&0xFFFF)|(((uint32_t)(*VRAMWrite.ImagePtr))<<16);
           FinishedVRAMWrite();
           goto ENDVRAM;
          }
         VRAMWrite.RowsRemaining = VRAMWrite.Width;
         VRAMWrite.ImagePtr += 1024 - VRAMWrite.Width;
        }

       *VRAMWrite.ImagePtr++ = (unsigned short)(gdata>>16);
       if(VRAMWrite.ImagePtr>=psxVuw_eom) VRAMWrite.ImagePtr-=iGPUHeight*1024;
       VRAMWrite.RowsRemaining --;
      }

     VRAMWrite.RowsRemaining = VRAMWrite.Width;
     VRAMWrite.ColsRemaining--;
     VRAMWrite.ImagePtr += 1024 - VRAMWrite.Width;
    }

   FinishedVRAMWrite();
  }

ENDVRAM:

 if(iDataWriteMode==DR_NORMAL)
  {
   void (* *primFunc)(unsigned char *);
   if(bSkipNextFrame) primFunc=primTableSkip;
   else               primFunc=primTableJ;

   for(;i<iSize;)
    {
     if(iDataWriteMode==DR_VRAMTRANSFER) goto STARTVRAM;

     gdata=*pMem++;i++;
 
     if(gpuDataC == 0)
      {
       command = (unsigned char)((gdata>>24) & 0xff);
 
       if(primTableCX[command])
        {
         gpuDataC = primTableCX[command];
         gpuCommand = command;
         gpuDataM[0] = gdata;
         gpuDataP = 1;
        }
       else continue;
      }
     else
      {
       gpuDataM[gpuDataP] = gdata;
       if(gpuDataC>128)
        {
         if((gpuDataC==254 && gpuDataP>=3) ||
            (gpuDataC==255 && gpuDataP>=4 && !(gpuDataP&1)))
          {
           if((gpuDataM[gpuDataP] & 0xF000F000) == 0x50005000)
            gpuDataP=gpuDataC-1;
          }
        }
       gpuDataP++;
      }
 
     if(gpuDataP == gpuDataC)
      {
       gpuDataC=gpuDataP=0;
       primFunc[gpuCommand]((unsigned char *)gpuDataM);

       if(dwEmuFixes&0x0001 || dwActFixes&0x20000)     // hack for emulating "gpu busy" in some games
        iFakePrimBusy=4;
      }
    } 
  }

 GPUdataRet=gdata;

 GPUIsReadyForCommands;
 GPUIsIdle;                
}

////////////////////////////////////////////////////////////////////////

void CALLBACK GPUwriteData(uint32_t gdata)
{
 GPUwriteDataMem(&gdata,1);
}

////////////////////////////////////////////////////////////////////////
// call config dlg
////////////////////////////////////////////////////////////////////////

void StartCfgTool(char *arg) // linux: start external cfg tool
{
	char cfg[256];
	struct stat buf;

	strcpy(cfg, "./cfgpeopsxgl");
	if (stat(cfg, &buf) != -1) {
		if (fork() == 0) {
			execl(cfg, "cfgpeopsxgl", arg, NULL);
			exit(0);
		}
		return;
	}

	strcpy(cfg, "./cfg/cfgpeopsxgl");
	if (stat(cfg, &buf) != -1) {
		if (fork() == 0) {
			execl(cfg, "cfgpeopsxgl", arg, NULL);
			exit(0);
		}
		return;
	}

	sprintf(cfg, "%s/.pcsx/plugins/cfg/cfgpeopsxgl", getenv("HOME"));
	if (stat(cfg, &buf) != -1) {
		if (fork() == 0) {
			execl(cfg, "cfgpeopsxgl", arg, NULL);
			exit(0);
		}
		return;
	}

	printf("ERROR: cfgpeopsxgl file not found!\n");
}

long CALLBACK GPUconfigure(void)
{
 StartCfgTool("CFG");
 return 0;
}

////////////////////////////////////////////////////////////////////////
// sets all kind of act fixes
////////////////////////////////////////////////////////////////////////

void SetFixes(void)
{
 ReInitFrameCap();

 if(dwActFixes & 0x2000) 
      dispWidths[4]=384;
 else dispWidths[4]=368;
}

////////////////////////////////////////////////////////////////////////
// Pete Special: make an 'intelligent' dma chain check (<-Tekken3)
////////////////////////////////////////////////////////////////////////

uint32_t lUsedAddr[3];

__inline BOOL CheckForEndlessLoop(uint32_t laddr)
{
 if(laddr==lUsedAddr[1]) return TRUE;
 if(laddr==lUsedAddr[2]) return TRUE;

 if(laddr<lUsedAddr[0]) lUsedAddr[1]=laddr;
 else                   lUsedAddr[2]=laddr;
 lUsedAddr[0]=laddr;
 return FALSE;
}

////////////////////////////////////////////////////////////////////////
// core gives a dma chain to gpu: same as the gpuwrite interface funcs
////////////////////////////////////////////////////////////////////////

long CALLBACK GPUdmaChain(uint32_t *baseAddrL, uint32_t addr)
{
 uint32_t dmaMem;
 unsigned char * baseAddrB;
 short count;unsigned int DMACommandCounter = 0;

 if(bIsFirstFrame) GLinitialize();

 GPUIsBusy;

 lUsedAddr[0]=lUsedAddr[1]=lUsedAddr[2]=0xffffff;

 baseAddrB = (unsigned char*) baseAddrL;

 do
  {
   if(iGPUHeight==512) addr&=0x1FFFFC;

   if(DMACommandCounter++ > 2000000) break;
   if(CheckForEndlessLoop(addr)) break;

   count = baseAddrB[addr+3];

   dmaMem=addr+4;

   if(count>0) GPUwriteDataMem(&baseAddrL[dmaMem>>2],count);

   addr = baseAddrL[addr>>2]&0xffffff;
  }
 while (addr != 0xffffff);

 GPUIsIdle;

 return 0;
}
           
////////////////////////////////////////////////////////////////////////
// show about dlg
////////////////////////////////////////////////////////////////////////

void CALLBACK GPUabout(void)
{
 StartCfgTool("ABOUT");
}

////////////////////////////////////////////////////////////////////////
// We are ever fine ;)
////////////////////////////////////////////////////////////////////////

long CALLBACK GPUtest(void)
{
 // if test fails this function should return negative value for error (unable to continue)
 // and positive value for warning (can continue but output might be crappy)

 return 0;
}

////////////////////////////////////////////////////////////////////////
// save state funcs
////////////////////////////////////////////////////////////////////////

typedef struct GPUFREEZETAG
{
 uint32_t ulFreezeVersion;      // should be always 1 for now (set by main emu)
 uint32_t ulStatus;             // current gpu status
 uint32_t ulControl[256];       // latest control register values
 unsigned char psxVRam[1024*1024*2]; // current VRam image (full 2 MB for ZN)
} GPUFreeze_t;

////////////////////////////////////////////////////////////////////////

long CALLBACK GPUfreeze(uint32_t ulGetFreezeData,GPUFreeze_t * pF)
{
 if(ulGetFreezeData==2) 
  {
   int lSlotNum=*((int *)pF);
   if(lSlotNum<0) return 0;
   if(lSlotNum>8) return 0;
   lSelectedSlot=lSlotNum+1;
   return 1;
  }

 if(!pF)                    return 0; 
 if(pF->ulFreezeVersion!=1) return 0;

 if(ulGetFreezeData==1)
  {
   pF->ulStatus=STATUSREG;
   memcpy(pF->ulControl,ulStatusControl,256*sizeof(uint32_t));
   memcpy(pF->psxVRam,  psxVub,         1024*iGPUHeight*2);

   return 1;
  }

 if(ulGetFreezeData!=0) return 0;

 STATUSREG=pF->ulStatus;
 memcpy(ulStatusControl,pF->ulControl,256*sizeof(uint32_t));
 memcpy(psxVub,         pF->psxVRam,  1024*iGPUHeight*2);

 ResetTextureArea(TRUE);

 GPUwriteStatus(ulStatusControl[0]);
 GPUwriteStatus(ulStatusControl[1]);
 GPUwriteStatus(ulStatusControl[2]);
 GPUwriteStatus(ulStatusControl[3]);
 GPUwriteStatus(ulStatusControl[8]);
 GPUwriteStatus(ulStatusControl[6]);
 GPUwriteStatus(ulStatusControl[7]);
 GPUwriteStatus(ulStatusControl[5]);
 GPUwriteStatus(ulStatusControl[4]);

 return 1;
}

////////////////////////////////////////////////////////////////////////
// special "emu infos" / "emu effects" functions
////////////////////////////////////////////////////////////////////////

//00 = black
//01 = white
//10 = red
//11 = transparent

unsigned char cFont[10][120]=
{
// 0
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 1
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x05,0x50,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x05,0x55,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 2
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x14,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x01,0x40,0x00,0x00,
 0x80,0x00,0x05,0x00,0x00,0x00,
 0x80,0x00,0x14,0x00,0x00,0x00,
 0x80,0x00,0x15,0x55,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 3
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x01,0x54,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 4
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x14,0x00,0x00,
 0x80,0x00,0x00,0x54,0x00,0x00,
 0x80,0x00,0x01,0x54,0x00,0x00,
 0x80,0x00,0x01,0x54,0x00,0x00,
 0x80,0x00,0x05,0x14,0x00,0x00,
 0x80,0x00,0x14,0x14,0x00,0x00,
 0x80,0x00,0x15,0x55,0x00,0x00,
 0x80,0x00,0x00,0x14,0x00,0x00,
 0x80,0x00,0x00,0x14,0x00,0x00,
 0x80,0x00,0x00,0x55,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 5
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x15,0x55,0x00,0x00,
 0x80,0x00,0x14,0x00,0x00,0x00,
 0x80,0x00,0x14,0x00,0x00,0x00,
 0x80,0x00,0x14,0x00,0x00,0x00,
 0x80,0x00,0x15,0x54,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 6
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x01,0x54,0x00,0x00,
 0x80,0x00,0x05,0x00,0x00,0x00,
 0x80,0x00,0x14,0x00,0x00,0x00,
 0x80,0x00,0x14,0x00,0x00,0x00,
 0x80,0x00,0x15,0x54,0x00,0x00,
 0x80,0x00,0x15,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 7
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x15,0x55,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x00,0x14,0x00,0x00,
 0x80,0x00,0x00,0x14,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x01,0x40,0x00,0x00,
 0x80,0x00,0x01,0x40,0x00,0x00,
 0x80,0x00,0x05,0x00,0x00,0x00,
 0x80,0x00,0x05,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 8
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 9
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x15,0x00,0x00,
 0x80,0x00,0x05,0x55,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x14,0x00,0x00,
 0x80,0x00,0x05,0x50,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
}
};

////////////////////////////////////////////////////////////////////////

void PaintPicDot(unsigned char * p,unsigned char c)
{
 if(c==0) {*p++=0x00;*p++=0x00;*p=0x00;return;}
 if(c==1) {*p++=0xff;*p++=0xff;*p=0xff;return;}
 if(c==2) {*p++=0x00;*p++=0x00;*p=0xff;return;}
}

////////////////////////////////////////////////////////////////////////

void CALLBACK GPUgetScreenPic(unsigned char * pMem)
{
 float XS,YS;int x,y,v;
 unsigned char * ps, * px, * pf;
 unsigned char c;

 if(!pGfxCardScreen)
  {
   glPixelStorei(GL_PACK_ALIGNMENT,1);
   pGfxCardScreen=(unsigned char *)malloc(iResX*iResY*4);
  }

 ps=pGfxCardScreen;

 glReadBuffer(GL_FRONT);

 glReadPixels(0,0,iResX,iResY,GL_RGB,GL_UNSIGNED_BYTE,ps);
               
 glReadBuffer(GL_BACK);

 XS=(float)iResX/128;
 YS=(float)iResY/96;
 pf=pMem;

 for(y=96;y>0;y--)
  {
   for(x=0;x<128;x++)
    {
     px=ps+(3*((int)((float)x * XS))+
           (3*iResX)*((int)((float)y*YS)));
     *(pf+0)=*(px+2);
     *(pf+1)=*(px+1);
     *(pf+2)=*(px+0);
     pf+=3;
    }
  }

 /////////////////////////////////////////////////////////////////////
 // generic number/border painter

 pf=pMem+(103*3);

 for(y=0;y<20;y++)
  {
   for(x=0;x<6;x++)
    {
     c=cFont[lSelectedSlot][x+y*6];
     v=(c&0xc0)>>6;
     PaintPicDot(pf,(unsigned char)v);pf+=3;                // paint the dots into the rect
     v=(c&0x30)>>4;
     PaintPicDot(pf,(unsigned char)v);pf+=3;
     v=(c&0x0c)>>2;
     PaintPicDot(pf,(unsigned char)v);pf+=3;
     v=c&0x03;
     PaintPicDot(pf,(unsigned char)v);pf+=3;
    }
   pf+=104*3;
  }

 pf=pMem;
 for(x=0;x<128;x++)
  {
   *(pf+(95*128*3))=0x00;*pf++=0x00;
   *(pf+(95*128*3))=0x00;*pf++=0x00;
   *(pf+(95*128*3))=0xff;*pf++=0xff;
  }
 pf=pMem;
 for(y=0;y<96;y++)
  {
   *(pf+(127*3))=0x00;*pf++=0x00;
   *(pf+(127*3))=0x00;*pf++=0x00;
   *(pf+(127*3))=0xff;*pf++=0xff;
   pf+=127*3;
  }

}

////////////////////////////////////////////////////////////////////////

void CALLBACK GPUshowScreenPic(unsigned char * pMem)
{
 DestroyPic();
 if(pMem==0) return;
 CreatePic(pMem);
}

////////////////////////////////////////////////////////////////////////

void CALLBACK GPUsetfix(uint32_t dwFixBits)
{
 dwEmuFixes=dwFixBits;
}

////////////////////////////////////////////////////////////////////////
 
void CALLBACK GPUvisualVibration(uint32_t iSmall, uint32_t iBig)
{
 int iVibVal;

 if(PSXDisplay.DisplayModeNew.x)                       // calc min "shake pixel" from screen width
      iVibVal=max(1,iResX/PSXDisplay.DisplayModeNew.x);
 else iVibVal=1;
                                                       // big rumble: 4...15 sp ; small rumble 1...3 sp
 if(iBig) iRumbleVal=max(4*iVibVal,min(15*iVibVal,((int)iBig  *iVibVal)/10));
 else     iRumbleVal=max(1*iVibVal,min( 3*iVibVal,((int)iSmall*iVibVal)/10));

 srand(timeGetTime());                                 // init rand (will be used in BufferSwap)

 iRumbleTime=15;                                       // let the rumble last 16 buffer swaps
}
                                                       
////////////////////////////////////////////////////////////////////////
// main emu can set display infos (A/M/G/D) 
////////////////////////////////////////////////////////////////////////

void CALLBACK GPUdisplayFlags(uint32_t dwFlags)
{
 dwCoreFlags=dwFlags;
}
