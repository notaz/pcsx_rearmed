/*
 * Copyright (c) 2010, Wei Mingzhi <whistler@openoffice.org>.
 * All Rights Reserved.
 *
 * Based on: Cdrom for Psemu Pro like Emulators
 * By: linuzappz <linuzappz@hotmail.com>
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
 * along with this program; if not, see <http://www.gnu.org/licenses>.
 */

#include "cdr.h"

#ifndef USE_NULL
static char *LibName = N_("CD-ROM Drive Reader");
#else
static char *LibName = N_("CDR NULL Plugin");
#endif

int initial_time = 0;

pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

CacheData *cdcache;
unsigned char *cdbuffer;
int cacheaddr;

crdata cr;

unsigned char lastTime[3];
pthread_t thread;
int subqread;
volatile int stopth, found, locked, playing;

long (*ReadTrackT[])() = {
	ReadNormal,
	ReadThreaded,
};

unsigned char* (*GetBufferT[])() = {
	GetBNormal,
	GetBThreaded,
};

long (*fReadTrack)();
unsigned char* (*fGetBuffer)();

void *CdrThread(void *arg);

long CDRinit(void) {
	thread = (pthread_t)-1;
	return 0;
}

long CDRshutdown(void) {
	return 0;
}

long CDRopen(void) {
	LoadConf();

#ifndef _MACOSX
	if (IsCdHandleOpen())
		return 0; // it's already open
#endif

	if (OpenCdHandle(CdromDev) == -1) { // if we can't open the cdrom we'll works as a null plugin
		fprintf(stderr, "CDR: Could not open %s\n", CdromDev);
	}

	fReadTrack = ReadTrackT[ReadMode];
	fGetBuffer = GetBufferT[ReadMode];

	if (ReadMode == THREADED) {
		cdcache = (CacheData *)malloc(CacheSize * sizeof(CacheData));
		if (cdcache == NULL) return -1;
		memset(cdcache, 0, CacheSize * sizeof(CacheData));

		found = 0;
	} else {
		cdbuffer = cr.buf + 12; /* skip sync data */
	}

	if (ReadMode == THREADED) {
		pthread_attr_t attr;

		pthread_mutex_init(&mut, NULL);
		pthread_cond_init(&cond, NULL);
		locked = 0;

		pthread_attr_init(&attr);
		pthread_create(&thread, &attr, CdrThread, NULL);

		cacheaddr = -1;
	} else thread = (pthread_t)-1;

	playing = 0;
	stopth = 0;
	initial_time = 0;

	return 0;
}

long CDRclose(void) {
	if (!IsCdHandleOpen()) return 0;

	if (playing) CDRstop();

	CloseCdHandle();

	if (thread != (pthread_t)-1) {
		if (locked == 0) {
			stopth = 1;
			while (locked == 0) usleep(5000);
		}

		stopth = 2;
		pthread_mutex_lock(&mut);
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mut);

		pthread_join(thread, NULL);
		pthread_mutex_destroy(&mut);
		pthread_cond_destroy(&cond);
	}

	if (ReadMode == THREADED) {
		free(cdcache);
	}

	return 0;
}

// return Starting and Ending Track
// buffer:
//  byte 0 - start track
//  byte 1 - end track
long CDRgetTN(unsigned char *buffer) {
	long ret;

	if (!IsCdHandleOpen()) {
		buffer[0] = 1;
		buffer[1] = 1;
		return 0;
	}

	if (ReadMode == THREADED) pthread_mutex_lock(&mut);
	ret = GetTN(buffer);
	if (ReadMode == THREADED) pthread_mutex_unlock(&mut);

	return ret;
}

// return Track Time
// buffer:
//  byte 0 - frame
//  byte 1 - second
//  byte 2 - minute
long CDRgetTD(unsigned char track, unsigned char *buffer) {
	long ret;

	if (!IsCdHandleOpen()) {
		memset(buffer + 1, 0, 3);
		return 0;
	}

	if (ReadMode == THREADED) pthread_mutex_lock(&mut);
	ret = GetTD(track, buffer);
	if (ReadMode == THREADED) pthread_mutex_unlock(&mut);

	return ret;
}

// normal reading
long ReadNormal() {
	if (ReadSector(&cr) == -1)
		return -1;

	return 0;
}

unsigned char* GetBNormal() {
	return cdbuffer;
}

// threaded reading (with cache)
long ReadThreaded() {
	int addr = msf_to_lba(cr.msf.cdmsf_min0, cr.msf.cdmsf_sec0, cr.msf.cdmsf_frame0);
	int i;

	if (addr >= cacheaddr && addr < (cacheaddr + CacheSize) && cacheaddr != -1) {
		i = addr - cacheaddr;
		PRINTF("found %d\n", (addr - cacheaddr));
		cdbuffer = cdcache[i].cr.buf + 12;
		while (btoi(cdbuffer[0]) != cr.msf.cdmsf_min0 ||
			   btoi(cdbuffer[1]) != cr.msf.cdmsf_sec0 ||
			   btoi(cdbuffer[2]) != cr.msf.cdmsf_frame0) {
			if (locked == 1) {
				if (cdcache[i].ret == 0) break;
				return -1;
			}
			usleep(5000);
		}
		PRINTF("%x:%x:%x, %p, %p\n", cdbuffer[0], cdbuffer[1], cdbuffer[2], cdbuffer, cdcache);
		found = 1;

		return 0;
	} else found = 0;

	if (locked == 0) {
		stopth = 1;
		while (locked == 0) { usleep(5000); }
		stopth = 0;
	}

	// not found in cache
	locked = 0;
	pthread_mutex_lock(&mut);
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mut);

	return 0;
}

unsigned char* GetBThreaded() {
	PRINTF("threadc %d\n", found);

	if (found == 1) return cdbuffer;
	cdbuffer = cdcache[0].cr.buf + 12;
	while (btoi(cdbuffer[0]) != cr.msf.cdmsf_min0 ||
		   btoi(cdbuffer[1]) != cr.msf.cdmsf_sec0 ||
		   btoi(cdbuffer[2]) != cr.msf.cdmsf_frame0) {
		if (locked == 1) return NULL;
		usleep(5000);
	}
	if (cdcache[0].ret == -1) return NULL;

	return cdbuffer;
}

void *CdrThread(void *arg) {
	unsigned char curTime[3];
	int i;

	for (;;) {
		locked = 1;
		pthread_mutex_lock(&mut);
		pthread_cond_wait(&cond, &mut);

		if (stopth == 2) pthread_exit(NULL);
		// refill the buffer
		cacheaddr = msf_to_lba(cr.msf.cdmsf_min0, cr.msf.cdmsf_sec0, cr.msf.cdmsf_frame0);

		memcpy(curTime, &cr.msf, 3);

		PRINTF("start thc %d:%d:%d\n", curTime[0], curTime[1], curTime[2]);

		for (i = 0; i < CacheSize; i++) {
			memcpy(&cdcache[i].cr.msf, curTime, 3);
			PRINTF("reading %d:%d:%d\n", curTime[0], curTime[1], curTime[2]);
			cdcache[i].ret = ReadSector(&cdcache[i].cr);

			PRINTF("readed %x:%x:%x\n", cdcache[i].cr.buf[12], cdcache[i].cr.buf[13], cdcache[i].cr.buf[14]);
			if (cdcache[i].ret == -1) break;

			curTime[2]++;
			if (curTime[2] == 75) {
				curTime[2] = 0;
				curTime[1]++;
				if (curTime[1] == 60) {
					curTime[1] = 0;
					curTime[0]++;
				}
			}

			if (stopth) break;
		}

		pthread_mutex_unlock(&mut);
	}

	return NULL;
}

// read track
// time:
//  byte 0 - minute
//  byte 1 - second
//  byte 2 - frame
// uses bcd format
long CDRreadTrack(unsigned char *time) {
	if (!IsCdHandleOpen()) {
		memset(cr.buf, 0, DATA_SIZE);
		return 0;
	}

	PRINTF("CDRreadTrack %d:%d:%d\n", btoi(time[0]), btoi(time[1]), btoi(time[2]));

	if (UseSubQ) memcpy(lastTime, time, 3);
	subqread = 0;

	cr.msf.cdmsf_min0 = btoi(time[0]);
	cr.msf.cdmsf_sec0 = btoi(time[1]);
	cr.msf.cdmsf_frame0 = btoi(time[2]);

	return fReadTrack();
}

// return readed track
unsigned char *CDRgetBuffer(void) {
	return fGetBuffer();
}

// plays cdda audio
// sector:
//  byte 0 - minute
//  byte 1 - second
//  byte 2 - frame
// does NOT uses bcd format
long CDRplay(unsigned char *sector) {
	long ret;

	if (!IsCdHandleOpen())
		return 0;

	// If play was called with the same time as the previous call,
	// don't restart it. Of course, if play is called with a different
	// track, stop playing the current stream.
	if (playing) {
		if (msf_to_lba(sector[0], sector[1], sector[2]) == initial_time)
			return 0;
		else
			CDRstop();
	}

	initial_time = msf_to_lba(sector[0], sector[1], sector[2]);

	if (ReadMode == THREADED) pthread_mutex_lock(&mut);
	ret = PlayCDDA(sector);
	if (ReadMode == THREADED) pthread_mutex_unlock(&mut);

	if (ret == 0) {
		playing = 1;
		return 0;
	}

	return -1;
}

// stops cdda audio
long CDRstop(void) {
	long ret;

	if (!IsCdHandleOpen())
		return 0;

	if (ReadMode == THREADED) pthread_mutex_lock(&mut);
	ret = StopCDDA();
	if (ReadMode == THREADED) pthread_mutex_unlock(&mut);

	if (ret == 0) {
		playing = 0;
		initial_time = 0;

		return 0;
	}

	return -1;
}

// reads cdr status
// type:
//  0x00 - unknown
//  0x01 - data
//  0x02 - audio
//  0xff - no cdrom
// status: (only shell open supported)
//  0x00 - unknown
//  0x01 - error
//  0x04 - seek error
//  0x10 - shell open
//  0x20 - reading
//  0x40 - seeking
//  0x80 - playing
// time:
//  byte 0 - minute
//  byte 1 - second
//  byte 2 - frame

long CDRgetStatus(struct CdrStat *stat) {
	long ret;

	if (!IsCdHandleOpen())
		return -1;

	if (ReadMode == THREADED) pthread_mutex_lock(&mut);
	ret = GetStatus(playing, stat);
	if (ReadMode == THREADED) pthread_mutex_unlock(&mut);

	return ret;
}

unsigned char *CDRgetBufferSub(void) {
	static unsigned char *p = NULL;

	if (!UseSubQ) return NULL;
	if (subqread) return p;

	if (ReadMode == THREADED) pthread_mutex_lock(&mut);
	p = ReadSub(lastTime);
	if (ReadMode == THREADED) pthread_mutex_unlock(&mut);

	if (p != NULL) subqread = 1;

	return p;
}

// read CDDA sector into buffer
long CDRreadCDDA(unsigned char m, unsigned char s, unsigned char f, unsigned char *buffer) {
	unsigned char msf[3] = {m, s, f};
	unsigned char *p;

	if (CDRreadTrack(msf) != 0) return -1;

	p = CDRgetBuffer();
	if (p == NULL) return -1;

	memcpy(buffer, p - 12, CD_FRAMESIZE_RAW); // copy from the beginning of the sector
	return 0;
}

// get Track End Time
long CDRgetTE(unsigned char track, unsigned char *m, unsigned char *s, unsigned char *f) {
	long ret;

	if (!IsCdHandleOpen()) return -1;

	if (ReadMode == THREADED) pthread_mutex_lock(&mut);
	ret = GetTE(track, m, s, f);
	if (ReadMode == THREADED) pthread_mutex_unlock(&mut);

	return ret;
}

#ifndef _MACOSX

void ExecCfg(char *arg) {
	char cfg[256];
	struct stat buf;

	strcpy(cfg, "./cfgDFCdrom");
	if (stat(cfg, &buf) != -1) {
		if (fork() == 0) {
			execl(cfg, "cfgDFCdrom", arg, NULL);
			exit(0);
		}
		return;
	}

	strcpy(cfg, "./cfg/DFCdrom");
	if (stat(cfg, &buf) != -1) {
		if (fork() == 0) {
			execl(cfg, "cfgDFCdrom", arg, NULL);
			exit(0);
		}
		return;
	}

	fprintf(stderr, "cfgDFCdrom file not found!\n");
}

long CDRconfigure() {
#ifndef USE_NULL
	ExecCfg("configure");
#endif
	return 0;
}

void CDRabout() {
	ExecCfg("about");
}

#endif

long CDRtest(void) {
#ifndef USE_NULL
	if (OpenCdHandle(CdromDev) == -1)
		return -1;
	CloseCdHandle();
#endif
	return 0;
}

char *PSEgetLibName(void) {
	return _(LibName);
}

unsigned long PSEgetLibType(void) {
	return PSE_LT_CDR;
}

unsigned long PSEgetLibVersion(void) {
	return 1 << 16;
}
