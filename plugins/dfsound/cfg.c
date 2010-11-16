/***************************************************************************
                            cfg.c  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by Pete Bernert
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

#include "stdafx.h"

#define _IN_CFG

#include "externals.h"

////////////////////////////////////////////////////////////////////////
// LINUX CONFIG/ABOUT HANDLING
////////////////////////////////////////////////////////////////////////

#include <unistd.h>

////////////////////////////////////////////////////////////////////////
// START EXTERNAL CFG TOOL
////////////////////////////////////////////////////////////////////////

void StartCfgTool(char * pCmdLine)
{
 FILE * cf;
 char filename[255];

 strcpy(filename,"cfgDFSound");
 cf=fopen(filename,"rb");
 if(cf!=NULL)
  {
   fclose(cf);
   if(fork()==0)
    {
     execl("./cfgDFSound","cfgDFSound",pCmdLine,NULL);
     exit(0);
    }
  }
 else
  {
   strcpy(filename,"cfg/cfgDFSound");
   cf=fopen(filename,"rb");
   if(cf!=NULL)
    {
     fclose(cf);
     if(fork()==0)
      {
       chdir("cfg");
       execl("./cfgDFSound","cfgDFSound",pCmdLine,NULL);
       exit(0);
      }
    }
   else
    {
     sprintf(filename,"%s/cfgDFSound",getenv("HOME"));
     cf=fopen(filename,"rb");
     if(cf!=NULL)
      {
       fclose(cf);
       if(fork()==0)
       {
        chdir(getenv("HOME"));
        execl("./cfgDFSound","cfgDFSound",pCmdLine,NULL);
        exit(0);
       }
      }
     else printf("Sound error: cfgDFSound not found!\n");
    }
  }
}

/////////////////////////////////////////////////////////
// READ LINUX CONFIG FILE
/////////////////////////////////////////////////////////

void ReadConfigFile(void)
{
 FILE *in;char t[256];int len;
 char * pB, * p;

 strcpy(t,"dfsound.cfg");
 in = fopen(t,"rb"); 
 if(!in) 
  {
   strcpy(t,"cfg/dfsound.cfg");
   in = fopen(t,"rb"); 
   if(!in) 
    {
     sprintf(t,"%s/dfsound.cfg",getenv("HOME")); 
     in = fopen(t,"rb"); 
     if(!in) return;
    }
  }

 pB = (char *)malloc(32767);
 memset(pB,0,32767);

 len = fread(pB, 1, 32767, in);
 fclose(in);

 strcpy(t,"\nVolume");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iVolume=4-atoi(p+len);
 if(iVolume<1) iVolume=1;
 if(iVolume>4) iVolume=4;

 strcpy(t,"\nXAPitch");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
 if(p) iXAPitch=atoi(p+len);
 if(iXAPitch<0) iXAPitch=0;
 if(iXAPitch>1) iXAPitch=1;

 strcpy(t,"\nHighCompMode");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;} 
 if(p)  iUseTimer=atoi(p+len); 
 if(iUseTimer<0) iUseTimer=0; 
 // note: timer mode 1 (win time events) is not supported
 // in linux. But timer mode 2 (spuupdate) is safe to use.
 if(iUseTimer)   iUseTimer=2; 

 strcpy(t,"\nSPUIRQWait");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;} 
 if(p)  iSPUIRQWait=atoi(p+len); 
 if(iSPUIRQWait<0) iSPUIRQWait=0; 
 if(iSPUIRQWait>1) iSPUIRQWait=1; 

 strcpy(t,"\nUseReverb");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;} 
 if(p)  iUseReverb=atoi(p+len); 
 if(iUseReverb<0) iUseReverb=0; 
 if(iUseReverb>2) iUseReverb=2; 

 strcpy(t,"\nUseInterpolation");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;} 
 if(p)  iUseInterpolation=atoi(p+len); 
 if(iUseInterpolation<0) iUseInterpolation=0; 
 if(iUseInterpolation>3) iUseInterpolation=3; 

 strcpy(t,"\nDisStereo");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;} 
 if(p)  iDisStereo=atoi(p+len); 
 if(iDisStereo<0) iDisStereo=0; 
 if(iDisStereo>1) iDisStereo=1; 

 free(pB);
}

/////////////////////////////////////////////////////////
// READ CONFIG called by spu funcs
/////////////////////////////////////////////////////////

void ReadConfig(void)
{
 iVolume=2;
 iXAPitch=0;
 iSPUIRQWait=1;
 iUseTimer=2;
 iUseReverb=2;
 iUseInterpolation=2;
 iDisStereo=0;

 ReadConfigFile();
}
