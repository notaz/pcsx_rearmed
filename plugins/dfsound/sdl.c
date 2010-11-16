/* SDL Driver for P.E.Op.S Sound Plugin
 * Copyright (c) 2010, Wei Mingzhi <whistler_wmz@users.sf.net>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA
 */

#include "stdafx.h"

#include "externals.h"
#include <SDL.h>

#define BUFFER_SIZE		22050

short			*pSndBuffer = NULL;
int				iBufSize = 0;
volatile int	iReadPos = 0, iWritePos = 0;

static void SOUND_FillAudio(void *unused, Uint8 *stream, int len) {
	short *p = (short *)stream;

	len /= sizeof(short);

	while (iReadPos != iWritePos && len > 0) {
		*p++ = pSndBuffer[iReadPos++];
		if (iReadPos >= iBufSize) iReadPos = 0;
		--len;
	}

	// Fill remaining space with zero
	while (len > 0) {
		*p++ = 0;
		--len;
	}
}

static void InitSDL() {
	if (SDL_WasInit(SDL_INIT_EVERYTHING)) {
		SDL_InitSubSystem(SDL_INIT_AUDIO);
	} else {
		SDL_Init(SDL_INIT_AUDIO | SDL_INIT_NOPARACHUTE);
	}
}

static void DestroySDL() {
	if (SDL_WasInit(SDL_INIT_EVERYTHING & ~SDL_INIT_AUDIO)) {
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	} else {
		SDL_Quit();
	}
}

void SetupSound(void) {
	SDL_AudioSpec				spec;

	if (pSndBuffer != NULL) return;

	InitSDL();

	spec.freq = 44100;
	spec.format = AUDIO_S16SYS;
	spec.channels = iDisStereo ? 1 : 2;
	spec.samples = 512;
	spec.callback = SOUND_FillAudio;

	if (SDL_OpenAudio(&spec, NULL) < 0) {
		DestroySDL();
		return;
	}

	iBufSize = BUFFER_SIZE;
	if (iDisStereo) iBufSize /= 2;

	pSndBuffer = (short *)malloc(iBufSize * sizeof(short));
	if (pSndBuffer == NULL) {
		SDL_CloseAudio();
		return;
	}

	iReadPos = 0;
	iWritePos = 0;

	SDL_PauseAudio(0);
}

void RemoveSound(void) {
	if (pSndBuffer == NULL) return;

	SDL_CloseAudio();
	DestroySDL();

	free(pSndBuffer);
	pSndBuffer = NULL;
}

unsigned long SoundGetBytesBuffered(void) {
	int size;

	if (pSndBuffer == NULL) return SOUNDSIZE;

	size = iReadPos - iWritePos;
	if (size <= 0) size += iBufSize;

	if (size < iBufSize / 2) return SOUNDSIZE;

	return 0;
}

void SoundFeedStreamData(unsigned char *pSound, long lBytes) {
	short *p = (short *)pSound;

	if (pSndBuffer == NULL) return;

	while (lBytes > 0) {
		if (((iWritePos + 1) % iBufSize) == iReadPos) break;

		pSndBuffer[iWritePos] = *p++;

		++iWritePos;
		if (iWritePos >= iBufSize) iWritePos = 0;

		lBytes -= sizeof(short);
	}
}
