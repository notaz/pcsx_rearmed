/***************************************************************************
                        macosx.c  -  description
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

#define _IN_MACOSX

#ifdef _MACOSX

#include <Carbon/Carbon.h>
#include "externals.h"

#define kMaxSoundBuffers	20

//static int				macBufferSize = 2, macBufferCount = 36;
//static float			macSoundPitch = 1.0;
static long			   macSoundVolume = 100;
volatile int			soundBufferAt = -1, soundPlayAt = -1, soundQueued = 0;
char			*soundBuffer[kMaxSoundBuffers+1], *emptyBuffer;
SndChannelPtr	sndChannel;
//ExtSoundHeader	sndHeader;
CmpSoundHeader	sndHeader;
SndCallBackUPP	callBackUPP;
static int bufferIndex;

////////////////////////////////////////////////////////////////////////
// small linux time helper... only used for watchdog
////////////////////////////////////////////////////////////////////////

unsigned long timeGetTime()
{
 struct timeval tv;
 gettimeofday(&tv, 0);                                 // well, maybe there are better ways
 return tv.tv_sec * 1000 + tv.tv_usec/1000;            // to do that, but at least it works
}

pascal void MacProcessSound(SndChannelPtr chan, SndCommand *cmd)
{
	#pragma unused (chan, cmd)

	if (soundQueued <= 0)
		sndHeader.samplePtr = emptyBuffer;
	else
	{
		sndHeader.samplePtr = soundBuffer[soundPlayAt];
                soundPlayAt++;
		if (soundPlayAt >= kMaxSoundBuffers/*macBufferCount*/)
			soundPlayAt = 0;
		soundQueued--;
	}
	
	SndCommand buffer   = { bufferCmd, 0, (long) &sndHeader };
	SndDoImmediate(sndChannel, &buffer);

	SndCommand callback = { callBackCmd, 0, 0 };
	SndDoCommand(sndChannel, &callback, true);
}

////////////////////////////////////////////////////////////////////////
// SETUP SOUND
////////////////////////////////////////////////////////////////////////

static int buffer_size;
void SetupSound(void)
{
	int	count;
	
   callBackUPP = NewSndCallBackUPP(MacProcessSound);
   
	if (sndChannel)
	{
		SndDisposeChannel(sndChannel, true);
		sndChannel = nil;
	}
   
   buffer_size = 1;
   while (buffer_size < (44100 / 60))
      buffer_size <<= 1;
	
  	memset(&sndHeader, 0, sizeof(sndHeader));
	sndHeader.numChannels   = (iDisStereo ? 1 : 2);
	sndHeader.sampleRate    = 44100 << 16;
	sndHeader.encode        = cmpSH;
	sndHeader.baseFrequency = kMiddleC;
	sndHeader.numFrames     = buffer_size;
	sndHeader.sampleSize    = 16;
#ifdef __POWERPC__
    sndHeader.format        = k16BitBigEndianFormat;
#else
    sndHeader.format        = k16BitLittleEndianFormat;
#endif
    sndHeader.compressionID = fixedCompression;
   
	if (soundBufferAt != -1)
	{
      free(soundBuffer[0]);
		free(emptyBuffer);
	}
   
   soundBuffer[0] = (char *) calloc(buffer_size << 2, kMaxSoundBuffers);
	for (count = 1; count <= kMaxSoundBuffers; count++)
		soundBuffer[count] = soundBuffer[count-1] + (buffer_size << 2);
	emptyBuffer = (char *) calloc(buffer_size << 2, 1);
	
	soundBufferAt = soundPlayAt = soundQueued = 0;
        bufferIndex = 0;
	
	SndNewChannel(&sndChannel, sampledSynth, initStereo, callBackUPP);

	SndCommand	sndcmd;
	UInt32		volume;
	
	volume = (UInt32) (256.0 * (float) macSoundVolume / 100.0);
	
	sndcmd.cmd = volumeCmd;
   sndcmd.param1 = 0;
   sndcmd.param2 = (volume << 16) | volume;
   SndDoCommand(sndChannel, &sndcmd, true);

   sndcmd.cmd = callBackCmd;
   sndcmd.param1 = 0;
   sndcmd.param2 = 0;	
	SndDoCommand(sndChannel, &sndcmd, true);
}

////////////////////////////////////////////////////////////////////////
// REMOVE SOUND
////////////////////////////////////////////////////////////////////////

void RemoveSound(void)
{
   DisposeSndCallBackUPP(callBackUPP);
}

////////////////////////////////////////////////////////////////////////
// GET BYTES BUFFERED
////////////////////////////////////////////////////////////////////////

unsigned long SoundGetBytesBuffered(void)
{
	int bytes;
	int playAt = soundPlayAt;
	
   if (soundBufferAt < playAt) {
      bytes = (soundBuffer[kMaxSoundBuffers]-soundBuffer[playAt])+
      (soundBuffer[soundBufferAt]-soundBuffer[0]);
   } else {
		bytes = soundBuffer[soundBufferAt]-soundBuffer[playAt];
	}
	//printf("sb=%i\n", bytes);
	
//	if (bytes < SOUNDSIZE/2)
//		return 0;
	
	return bytes;
}

////////////////////////////////////////////////////////////////////////
// FEED SOUND DATA
////////////////////////////////////////////////////////////////////////

void SoundFeedStreamData(unsigned char* pSound,long lBytes)
{
    int rem;
    
   if (lBytes > (buffer_size<<2)*kMaxSoundBuffers) {
      printf("sound feed overflow!\n");
      return;
   }

   rem = soundBuffer[kMaxSoundBuffers]-(soundBuffer[soundBufferAt]+bufferIndex);
   if (lBytes > rem) {
      memcpy(soundBuffer[soundBufferAt]+bufferIndex, pSound, rem);
      lBytes -= rem; pSound += rem;
      soundQueued += kMaxSoundBuffers-soundBufferAt;
      soundBufferAt = 0; bufferIndex = 0;
   }
   memcpy(soundBuffer[soundBufferAt]+bufferIndex, pSound, lBytes);
   soundBufferAt += (lBytes+bufferIndex)/(buffer_size<<2);
   soundQueued += (lBytes+bufferIndex)/(buffer_size<<2);
   bufferIndex = (lBytes+bufferIndex)%(buffer_size<<2);
   
   if (soundQueued >= kMaxSoundBuffers) {
      printf("sound buffer overflow!\n");
   }
}

#endif
