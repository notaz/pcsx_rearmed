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

#ifdef _MACOSX

#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOCDMediaBSDClient.h>
#include <CoreFoundation/CoreFoundation.h>

int cdHandle = -1;
char cdDevice[4096] = "";

static int IsPsxDisc(const char *dev) {
	int fd;
	char buf[CD_FRAMESIZE_RAW];
	dk_cd_read_t r;

	fd = open(dev, O_RDONLY, 0);
	if (fd < 0) return 0;

	memset(&r, 0, sizeof(r));

	r.offset = msf_to_lba(0, 2, 4) * CD_FRAMESIZE_RAW;
	r.sectorArea = 0xF8;
	r.sectorType = kCDSectorTypeUnknown;
	r.bufferLength = CD_FRAMESIZE_RAW;
	r.buffer = buf;

	if (ioctl(fd, DKIOCCDREAD, &r) != kIOReturnSuccess) {
		close(fd);
		return 0;
	}

	close(fd);

	if (strncmp(buf + 56, "Sony Computer Entertainment", 27) == 0) {
		return 1;
	}

	return 0;
}

static void FindCdDevice(char *dev) {
	io_object_t   next_media;
	kern_return_t kern_result;
	io_iterator_t media_iterator;
	CFMutableDictionaryRef classes_to_match;
	const char *name, *cd = kIOCDMediaClass, *dvd = kIODVDMediaClass;

	dev[0] = '\0';
	name = cd;

start:
	classes_to_match = IOServiceMatching(name);
	if (classes_to_match == NULL) goto end;

	CFDictionarySetValue(classes_to_match, CFSTR(kIOMediaEjectableKey),
		kCFBooleanTrue);

	kern_result = IOServiceGetMatchingServices(kIOMasterPortDefault, 
		classes_to_match, &media_iterator);

	if (kern_result != KERN_SUCCESS) goto end;

	next_media = IOIteratorNext(media_iterator);
	if (next_media != 0) {
		char psz_buf[0x32];
		size_t dev_path_length;
		CFTypeRef str_bsd_path;

		do {
			str_bsd_path = IORegistryEntryCreateCFProperty(next_media,
				CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);

			if (str_bsd_path == NULL) {
				IOObjectRelease(next_media);
				continue;
			}

			strcpy(psz_buf, "/dev/r");
			dev_path_length = strlen(psz_buf);

			if (CFStringGetCString(str_bsd_path, (char *)&psz_buf + dev_path_length,
				sizeof(psz_buf) - dev_path_length, kCFStringEncodingASCII))
			{
				strcpy(dev, psz_buf);

				if (IsPsxDisc(dev)) {
					CFRelease(str_bsd_path);
					IOObjectRelease(next_media);
					IOObjectRelease(media_iterator);
					return;
				}
			}

			CFRelease(str_bsd_path);
			IOObjectRelease(next_media);
		} while ((next_media = IOIteratorNext(media_iterator)) != 0);
	}

	IOObjectRelease(media_iterator);

end:
	if (dev[0] == '\0') {
		if (name == cd) {
			name = dvd; // Is this really necessary or correct? Dunno...
			goto start;
		}
	}
}

int OpenCdHandle(const char *dev) {
	if (dev != NULL && dev[0] != '\0') strcpy(cdDevice, dev);
	else if (cdDevice[0] == '\0') FindCdDevice(cdDevice);

	cdHandle = open(cdDevice, O_RDONLY, 0);
	if (cdHandle < 0) return -1;

	if (CdrSpeed > 0) {
		u_int16_t speed = kCDSpeedMin * CdrSpeed;
		ioctl(cdHandle, DKIOCCDSETSPEED, &speed);
	}

	return 0;
}

void CloseCdHandle() {
	if (cdHandle != -1) close(cdHandle);
	cdHandle = -1;
}

int IsCdHandleOpen() {
	return 1;
}

long GetTN(unsigned char *buffer) {
	if (cdHandle < 0) return -1;

	// TODO
	buffer[0] = 1;
	buffer[1] = 1;

	return 0;
}

long GetTD(unsigned char track, unsigned char *buffer) {
	if (cdHandle < 0) return -1;

	// TODO
	memset(buffer + 1, 0, 3);
	return 0;
}

long GetTE(unsigned char track, unsigned char *m, unsigned char *s, unsigned char *f) {
	return -1; // TODO
}

long ReadSector(crdata *cr) {
	int lba;
	dk_cd_read_t r;

	if (cdHandle < 0) return -1;

	lba = msf_to_lba(cr->msf.cdmsf_min0, cr->msf.cdmsf_sec0, cr->msf.cdmsf_frame0);

	memset(&r, 0, sizeof(r));

	r.offset = lba * CD_FRAMESIZE_RAW;
	r.sectorArea = 0xF8;
	r.sectorType = kCDSectorTypeUnknown;
	r.bufferLength = CD_FRAMESIZE_RAW;
	r.buffer = cr->buf;

	if (ioctl(cdHandle, DKIOCCDREAD, &r) != kIOReturnSuccess) {
		return -1;
	}

	return 0;
}

long PlayCDDA(unsigned char *sector) {
	return 0; // TODO
}

long StopCDDA() {
	return 0; // TODO
}

long GetStatus(int playing, struct CdrStat *stat) {
	memset(stat, 0, sizeof(struct CdrStat));
	stat->Type = 0x01;

	// Close and reopen the CD handle. If opening failed,
	// then there is no CD in drive.
	// Note that this WILL be screwed if user inserted another
	// removable device such as USB stick when tray is open.
	// There may be a better way, but this should do the job.
	if (cdHandle >= 0) {
		close(cdHandle);
		cdHandle = -1;
	}

	cdHandle = open(cdDevice, O_RDONLY, 0);
	if (cdHandle < 0) {
		// No CD in drive
		stat->Type = 0xff;
		stat->Status |= 0x10;
	} else {
		if (CdrSpeed > 0) {
			u_int16_t speed = kCDSpeedMin * CdrSpeed;
			ioctl(cdHandle, DKIOCCDSETSPEED, &speed);
		}
	}

	return 0;
}

unsigned char *ReadSub(const unsigned char *time) {
	return NULL; // TODO
}

char *CDRgetDriveLetter(void) {
	return cdDevice;
}

#endif
