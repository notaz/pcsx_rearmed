/***************************************************************************
                            oss.c  -  description
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

#define _IN_OSS

#include "externals.h"

////////////////////////////////////////////////////////////////////////
// oss globals
////////////////////////////////////////////////////////////////////////

#define OSS_MODE_STEREO	    1
#define OSS_MODE_MONO       0

#define OSS_SPEED_44100     44100

static int oss_audio_fd = -1;
extern int errno;

////////////////////////////////////////////////////////////////////////
// SETUP SOUND
////////////////////////////////////////////////////////////////////////

void SetupSound(void)
{
 int pspeed=44100;
 int pstereo;
 int format;
 int fragsize = 0;
 int myfrag;
 int oss_speed, oss_stereo;

 if(iDisStereo) pstereo=OSS_MODE_MONO;
 else           pstereo=OSS_MODE_STEREO;

 oss_speed = pspeed;
 oss_stereo = pstereo;

 if((oss_audio_fd=open("/dev/dsp",O_WRONLY,0))==-1)
  {
   printf("Sound device not available!\n");
   return;
  }

 if(ioctl(oss_audio_fd,SNDCTL_DSP_RESET,0)==-1)
  {
   printf("Sound reset failed\n");
   return;
  }

 // we use 64 fragments with 1024 bytes each

 fragsize=10;
 myfrag=(63<<16)|fragsize;

 if(ioctl(oss_audio_fd,SNDCTL_DSP_SETFRAGMENT,&myfrag)==-1)
  {
   printf("Sound set fragment failed!\n");
   return;        
  }

 format = AFMT_S16_NE;

 if(ioctl(oss_audio_fd,SNDCTL_DSP_SETFMT,&format) == -1)
  {
   printf("Sound format not supported!\n");
   return;
  }

 if(format!=AFMT_S16_NE)
  {
   printf("Sound format not supported!\n");
   return;
  }

 if(ioctl(oss_audio_fd,SNDCTL_DSP_STEREO,&oss_stereo)==-1)
  {
   printf("Stereo mode not supported!\n");
   return;
  }

 if(oss_stereo!=1)
  {
   iDisStereo=1;
  }

 if(ioctl(oss_audio_fd,SNDCTL_DSP_SPEED,&oss_speed)==-1)
  {
   printf("Sound frequency not supported\n");
   return;
  }

 if(oss_speed!=pspeed)
  {
   printf("Sound frequency not supported\n");
   return;
  }
}

////////////////////////////////////////////////////////////////////////
// REMOVE SOUND
////////////////////////////////////////////////////////////////////////

void RemoveSound(void)
{
 if(oss_audio_fd != -1 )
  {
   close(oss_audio_fd);
   oss_audio_fd = -1;
  }
}

////////////////////////////////////////////////////////////////////////
// GET BYTES BUFFERED
////////////////////////////////////////////////////////////////////////

unsigned long SoundGetBytesBuffered(void)
{
 audio_buf_info info;
 unsigned long l;

 if(oss_audio_fd == -1) return SOUNDSIZE;
 if(ioctl(oss_audio_fd,SNDCTL_DSP_GETOSPACE,&info)==-1)
  l=0;
 else
  {
   if(info.fragments<(info.fragstotal>>1))             // can we write in at least the half of fragments?
        l=SOUNDSIZE;                                   // -> no? wait
   else l=0;                                           // -> else go on
  }

 return l;
}

////////////////////////////////////////////////////////////////////////
// FEED SOUND DATA
////////////////////////////////////////////////////////////////////////

void SoundFeedStreamData(unsigned char* pSound,long lBytes)
{
 if(oss_audio_fd == -1) return;
 write(oss_audio_fd,pSound,lBytes);
}
