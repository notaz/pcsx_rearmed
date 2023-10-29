/***************************************************************************
 *   Copyright (C) 2007 PCSX-df Team                                       *
 *   Copyright (C) 2009 Wei Mingzhi                                        *
 *   Copyright (C) 2012 notaz                                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

#include "psxcommon.h"
#include "plugins.h"
#include "cdrom.h"
#include "cdriso.h"
#include "ppf.h"

#include <errno.h>
#include <zlib.h>
#ifdef HAVE_CHD
#include <chd.h>
#endif

#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#if P_HAVE_PTHREAD
#include <pthread.h>
#include <sys/time.h>
#endif
#endif

// to enable the USE_READ_THREAD code, fix:
// - https://github.com/notaz/pcsx_rearmed/issues/257
// - ISOgetBufferSub to not race with async code
#define USE_READ_THREAD 0 //P_HAVE_PTHREAD

#ifdef USE_LIBRETRO_VFS
#include <streams/file_stream_transforms.h>
#undef fseeko
#undef ftello
#undef rewind
#define ftello rftell
#define fseeko rfseek
#define rewind(f_) rfseek(f_, 0, SEEK_SET)
#endif

#define OFF_T_MSB ((off_t)1 << (sizeof(off_t) * 8 - 1))

unsigned int cdrIsoMultidiskCount;
unsigned int cdrIsoMultidiskSelect;

static FILE *cdHandle = NULL;
static FILE *cddaHandle = NULL;
static FILE *subHandle = NULL;

static boolean subChanMixed = FALSE;
static boolean subChanRaw = FALSE;

static boolean multifile = FALSE;

static unsigned char cdbuffer[CD_FRAMESIZE_RAW];
static unsigned char subbuffer[SUB_FRAMESIZE];

static boolean cddaBigEndian = FALSE;
/* Frame offset into CD image where pregap data would be found if it was there.
 * If a game seeks there we must *not* return subchannel data since it's
 * not in the CD image, so that cdrom code can fake subchannel data instead.
 * XXX: there could be multiple pregaps but PSX dumps only have one? */
static unsigned int pregapOffset;

// compressed image stuff
static struct {
	unsigned char buff_raw[16][CD_FRAMESIZE_RAW];
	unsigned char buff_compressed[CD_FRAMESIZE_RAW * 16 + 100];
	off_t *index_table;
	unsigned int index_len;
	unsigned int block_shift;
	unsigned int current_block;
	unsigned int sector_in_blk;
} *compr_img;

#ifdef HAVE_CHD
static struct {
	unsigned char *buffer;
	chd_file* chd;
	const chd_header* header;
	unsigned int sectors_per_hunk;
	unsigned int current_hunk[2];
	unsigned int current_buffer;
	unsigned int sector_in_hunk;
} *chd_img;
#else
#define chd_img 0
#endif

static int (*cdimg_read_func)(FILE *f, unsigned int base, void *dest, int sector);
static int (*cdimg_read_sub_func)(FILE *f, int sector);

char* CALLBACK CDR__getDriveLetter(void);
long CALLBACK CDR__configure(void);
long CALLBACK CDR__test(void);
void CALLBACK CDR__about(void);
long CALLBACK CDR__setfilename(char *filename);

static void DecodeRawSubData(void);

struct trackinfo {
	enum {DATA=1, CDDA} type;
	char start[3];		// MSF-format
	char length[3];		// MSF-format
	FILE *handle;		// for multi-track images CDDA
	unsigned int start_offset; // byte offset from start of above file (chd: sector offset)
};

#define MAXTRACKS 100 /* How many tracks can a CD hold? */

static int numtracks = 0;
static struct trackinfo ti[MAXTRACKS];

// get a sector from a msf-array
static unsigned int msf2sec(char *msf) {
	return ((msf[0] * 60 + msf[1]) * 75) + msf[2];
}

static void sec2msf(unsigned int s, char *msf) {
	msf[0] = s / 75 / 60;
	s = s - msf[0] * 75 * 60;
	msf[1] = s / 75;
	s = s - msf[1] * 75;
	msf[2] = s;
}

// divide a string of xx:yy:zz into m, s, f
static void tok2msf(char *time, char *msf) {
	char *token;

	token = strtok(time, ":");
	if (token) {
		msf[0] = atoi(token);
	}
	else {
		msf[0] = 0;
	}

	token = strtok(NULL, ":");
	if (token) {
		msf[1] = atoi(token);
	}
	else {
		msf[1] = 0;
	}

	token = strtok(NULL, ":");
	if (token) {
		msf[2] = atoi(token);
	}
	else {
		msf[2] = 0;
	}
}

static off_t get_size(FILE *f)
{
	off_t old, size;
#if !defined(USE_LIBRETRO_VFS)
	struct stat st;
	if (fstat(fileno(f), &st) == 0)
		return st.st_size;
#endif
	old = ftello(f);
	fseeko(f, 0, SEEK_END);
	size = ftello(f);
	fseeko(f, old, SEEK_SET);
	return size;
}

// this function tries to get the .toc file of the given .bin
// the necessary data is put into the ti (trackinformation)-array
static int parsetoc(const char *isofile) {
	char			tocname[MAXPATHLEN];
	FILE			*fi;
	char			linebuf[256], tmp[256], name[256];
	char			*token;
	char			time[20], time2[20];
	unsigned int	t, sector_offs, sector_size;
	unsigned int	current_zero_gap = 0;

	numtracks = 0;

	// copy name of the iso and change extension from .bin to .toc
	strncpy(tocname, isofile, sizeof(tocname));
	tocname[MAXPATHLEN - 1] = '\0';
	if (strlen(tocname) >= 4) {
		strcpy(tocname + strlen(tocname) - 4, ".toc");
	}
	else {
		return -1;
	}

	if ((fi = fopen(tocname, "r")) == NULL) {
		// try changing extension to .cue (to satisfy some stupid tutorials)
		strcpy(tocname + strlen(tocname) - 4, ".cue");
		if ((fi = fopen(tocname, "r")) == NULL) {
			// if filename is image.toc.bin, try removing .bin (for Brasero)
			strcpy(tocname, isofile);
			t = strlen(tocname);
			if (t >= 8 && strcmp(tocname + t - 8, ".toc.bin") == 0) {
				tocname[t - 4] = '\0';
				if ((fi = fopen(tocname, "r")) == NULL) {
					return -1;
				}
			}
			else {
				return -1;
			}
		}
		// check if it's really a TOC named as a .cue
		if (fgets(linebuf, sizeof(linebuf), fi) != NULL) {
		token = strtok(linebuf, " ");
		if (token && strncmp(token, "CD", 2) != 0) {
		       // && strcmp(token, "CATALOG") != 0) - valid for a real .cue
			fclose(fi);
			return -1;
		}
		}
		fseek(fi, 0, SEEK_SET);
	}

	memset(&ti, 0, sizeof(ti));
	cddaBigEndian = TRUE; // cdrdao uses big-endian for CD Audio

	sector_size = CD_FRAMESIZE_RAW;
	sector_offs = 2 * 75;

	// parse the .toc file
	while (fgets(linebuf, sizeof(linebuf), fi) != NULL) {
		// search for tracks
		strncpy(tmp, linebuf, sizeof(linebuf));
		token = strtok(tmp, " ");

		if (token == NULL) continue;

		if (!strcmp(token, "TRACK")) {
			sector_offs += current_zero_gap;
			current_zero_gap = 0;

			// get type of track
			token = strtok(NULL, " ");
			numtracks++;

			if (!strncmp(token, "MODE2_RAW", 9)) {
				ti[numtracks].type = DATA;
				sec2msf(2 * 75, ti[numtracks].start); // assume data track on 0:2:0

				// check if this image contains mixed subchannel data
				token = strtok(NULL, " ");
				if (token != NULL && !strncmp(token, "RW", 2)) {
					sector_size = CD_FRAMESIZE_RAW + SUB_FRAMESIZE;
					subChanMixed = TRUE;
					if (!strncmp(token, "RW_RAW", 6))
						subChanRaw = TRUE;
				}
			}
			else if (!strncmp(token, "AUDIO", 5)) {
				ti[numtracks].type = CDDA;
			}
		}
		else if (!strcmp(token, "DATAFILE")) {
			if (ti[numtracks].type == CDDA) {
				sscanf(linebuf, "DATAFILE \"%[^\"]\" #%d %8s", name, &t, time2);
				ti[numtracks].start_offset = t;
				t = t / sector_size + sector_offs;
				sec2msf(t, (char *)&ti[numtracks].start);
				tok2msf((char *)&time2, (char *)&ti[numtracks].length);
			}
			else {
				sscanf(linebuf, "DATAFILE \"%[^\"]\" %8s", name, time);
				tok2msf((char *)&time, (char *)&ti[numtracks].length);
			}
		}
		else if (!strcmp(token, "FILE")) {
			sscanf(linebuf, "FILE \"%[^\"]\" #%d %8s %8s", name, &t, time, time2);
			tok2msf((char *)&time, (char *)&ti[numtracks].start);
			t += msf2sec(ti[numtracks].start) * sector_size;
			ti[numtracks].start_offset = t;
			t = t / sector_size + sector_offs;
			sec2msf(t, (char *)&ti[numtracks].start);
			tok2msf((char *)&time2, (char *)&ti[numtracks].length);
		}
		else if (!strcmp(token, "ZERO") || !strcmp(token, "SILENCE")) {
			// skip unneeded optional fields
			while (token != NULL) {
				token = strtok(NULL, " ");
				if (strchr(token, ':') != NULL)
					break;
			}
			if (token != NULL) {
				tok2msf(token, tmp);
				current_zero_gap = msf2sec(tmp);
			}
			if (numtracks > 1) {
				t = ti[numtracks - 1].start_offset;
				t /= sector_size;
				pregapOffset = t + msf2sec(ti[numtracks - 1].length);
			}
		}
		else if (!strcmp(token, "START")) {
			token = strtok(NULL, " ");
			if (token != NULL && strchr(token, ':')) {
				tok2msf(token, tmp);
				t = msf2sec(tmp);
				ti[numtracks].start_offset += (t - current_zero_gap) * sector_size;
				t = msf2sec(ti[numtracks].start) + t;
				sec2msf(t, (char *)&ti[numtracks].start);
			}
		}
	}

	fclose(fi);

	return 0;
}

// this function tries to get the .cue file of the given .bin
// the necessary data is put into the ti (trackinformation)-array
static int parsecue(const char *isofile) {
	char			cuename[MAXPATHLEN];
	char			filepath[MAXPATHLEN];
	char			*incue_fname;
	FILE			*fi;
	FILE			*ftmp = NULL;
	char			*token;
	char			time[20];
	char			*tmp;
	char			linebuf[256], tmpb[256], dummy[256];
	unsigned int	incue_max_len;
	unsigned int	t, file_len, mode, sector_offs;
	unsigned int	sector_size = 2352;

	numtracks = 0;

	// copy name of the iso and change extension from .bin to .cue
	strncpy(cuename, isofile, sizeof(cuename));
	cuename[MAXPATHLEN - 1] = '\0';
	if (strlen(cuename) < 4)
		return -1;
	if (strcasecmp(cuename + strlen(cuename) - 4, ".cue") == 0) {
		// it's already open as cdHandle
		fi = cdHandle;
	}
	else {
		// If 'isofile' is a '.cd<X>' file, use it as a .cue file
		//  and don't try to search the additional .cue file
		if (strncasecmp(cuename + strlen(cuename) - 4, ".cd", 3) != 0 )
			strcpy(cuename + strlen(cuename) - 4, ".cue");

		if ((ftmp = fopen(cuename, "r")) == NULL)
			return -1;
		fi = ftmp;
	}

	// Some stupid tutorials wrongly tell users to use cdrdao to rip a
	// "bin/cue" image, which is in fact a "bin/toc" image. So let's check
	// that...
	if (fgets(linebuf, sizeof(linebuf), fi) != NULL) {
		if (!strncmp(linebuf, "CD_ROM_XA", 9)) {
			// Don't proceed further, as this is actually a .toc file rather
			// than a .cue file.
			if (ftmp)
				fclose(ftmp);
			return parsetoc(isofile);
		}
		rewind(fi);
	}

	// build a path for files referenced in .cue
	strncpy(filepath, cuename, sizeof(filepath));
	tmp = strrchr(filepath, '/');
	if (tmp == NULL)
		tmp = strrchr(filepath, '\\');
	if (tmp != NULL)
		tmp++;
	else
		tmp = filepath;
	*tmp = 0;
	filepath[sizeof(filepath) - 1] = 0;
	incue_fname = tmp;
	incue_max_len = sizeof(filepath) - (tmp - filepath) - 1;

	memset(&ti, 0, sizeof(ti));

	file_len = 0;
	sector_offs = 2 * 75;

	while (fgets(linebuf, sizeof(linebuf), fi) != NULL) {
		strncpy(dummy, linebuf, sizeof(linebuf));
		token = strtok(dummy, " ");

		if (token == NULL) {
			continue;
		}

		if (!strcmp(token, "TRACK")) {
			numtracks++;

			sector_size = 0;
			if (strstr(linebuf, "AUDIO") != NULL) {
				ti[numtracks].type = CDDA;
				sector_size = 2352;
			}
			else if (sscanf(linebuf, " TRACK %u MODE%u/%u", &t, &mode, &sector_size) == 3)
				ti[numtracks].type = DATA;
			else {
				SysPrintf(".cue: failed to parse TRACK\n");
				ti[numtracks].type = numtracks == 1 ? DATA : CDDA;
			}
			if (sector_size == 0)
				sector_size = 2352;
		}
		else if (!strcmp(token, "INDEX")) {
			if (sscanf(linebuf, " INDEX %02d %8s", &t, time) != 2)
				SysPrintf(".cue: failed to parse INDEX\n");
			tok2msf(time, (char *)&ti[numtracks].start);

			t = msf2sec(ti[numtracks].start);
			ti[numtracks].start_offset = t * sector_size;
			t += sector_offs;
			sec2msf(t, ti[numtracks].start);

			// default track length to file length
			t = file_len - ti[numtracks].start_offset / sector_size;
			sec2msf(t, ti[numtracks].length);

			if (numtracks > 1 && ti[numtracks].handle == NULL) {
				// this track uses the same file as the last,
				// start of this track is last track's end
				t = msf2sec(ti[numtracks].start) - msf2sec(ti[numtracks - 1].start);
				sec2msf(t, ti[numtracks - 1].length);
			}
			if (numtracks > 1 && pregapOffset == -1)
				pregapOffset = ti[numtracks].start_offset / sector_size;
		}
		else if (!strcmp(token, "PREGAP")) {
			if (sscanf(linebuf, " PREGAP %8s", time) == 1) {
				tok2msf(time, dummy);
				sector_offs += msf2sec(dummy);
			}
			pregapOffset = -1; // mark to fill track start_offset
		}
		else if (!strcmp(token, "FILE")) {
			t = sscanf(linebuf, " FILE \"%255[^\"]\"", tmpb);
			if (t != 1)
				sscanf(linebuf, " FILE %255s", tmpb);

			tmp = strrchr(tmpb, '\\');
			if (tmp == NULL)
				tmp = strrchr(tmpb, '/');
			if (tmp != NULL)
				tmp++;
			else
				tmp = tmpb;
			strncpy(incue_fname, tmp, incue_max_len);
			ti[numtracks + 1].handle = fopen(filepath, "rb");

			// update global offset if this is not first file in this .cue
			if (numtracks + 1 > 1) {
				multifile = 1;
				sector_offs += file_len;
			}

			file_len = 0;
			if (ti[numtracks + 1].handle == NULL) {
				SysPrintf(_("\ncould not open: %s\n"), filepath);
				continue;
			}
			file_len = get_size(ti[numtracks + 1].handle) / 2352;
		}
	}

	if (ftmp)
		fclose(ftmp);

	// if there are no tracks detected, then it's not a cue file
	if (!numtracks)
		return -1;

	// the data track handle is always in cdHandle
	if (ti[1].handle) {
		fclose(cdHandle);
		cdHandle = ti[1].handle;
		ti[1].handle = NULL;
	}
	return 0;
}

// this function tries to get the .ccd file of the given .img
// the necessary data is put into the ti (trackinformation)-array
static int parseccd(const char *isofile) {
	char			ccdname[MAXPATHLEN];
	FILE			*fi;
	char			linebuf[256];
	unsigned int	t;

	numtracks = 0;

	// copy name of the iso and change extension from .img to .ccd
	strncpy(ccdname, isofile, sizeof(ccdname));
	ccdname[MAXPATHLEN - 1] = '\0';
	if (strlen(ccdname) >= 4) {
		strcpy(ccdname + strlen(ccdname) - 4, ".ccd");
	}
	else {
		return -1;
	}

	if ((fi = fopen(ccdname, "r")) == NULL) {
		return -1;
	}

	memset(&ti, 0, sizeof(ti));

	while (fgets(linebuf, sizeof(linebuf), fi) != NULL) {
		if (!strncmp(linebuf, "[TRACK", 6)){
			numtracks++;
		}
		else if (!strncmp(linebuf, "MODE=", 5)) {
			sscanf(linebuf, "MODE=%d", &t);
			ti[numtracks].type = ((t == 0) ? CDDA : DATA);
		}
		else if (!strncmp(linebuf, "INDEX 1=", 8)) {
			sscanf(linebuf, "INDEX 1=%d", &t);
			sec2msf(t + 2 * 75, ti[numtracks].start);
			ti[numtracks].start_offset = t * 2352;

			// If we've already seen another track, this is its end
			if (numtracks > 1) {
				t = msf2sec(ti[numtracks].start) - msf2sec(ti[numtracks - 1].start);
				sec2msf(t, ti[numtracks - 1].length);
			}
		}
	}

	fclose(fi);

	// Fill out the last track's end based on size
	if (numtracks >= 1) {
		t = get_size(cdHandle) / 2352 - msf2sec(ti[numtracks].start) + 2 * 75;
		sec2msf(t, ti[numtracks].length);
	}

	return 0;
}

// this function tries to get the .mds file of the given .mdf
// the necessary data is put into the ti (trackinformation)-array
static int parsemds(const char *isofile) {
	char			mdsname[MAXPATHLEN];
	FILE			*fi;
	unsigned int	offset, extra_offset, l, i;
	unsigned short	s;

	numtracks = 0;

	// copy name of the iso and change extension from .mdf to .mds
	strncpy(mdsname, isofile, sizeof(mdsname));
	mdsname[MAXPATHLEN - 1] = '\0';
	if (strlen(mdsname) >= 4) {
		strcpy(mdsname + strlen(mdsname) - 4, ".mds");
	}
	else {
		return -1;
	}

	if ((fi = fopen(mdsname, "rb")) == NULL) {
		return -1;
	}

	memset(&ti, 0, sizeof(ti));

	// check if it's a valid mds file
	if (fread(&i, 1, sizeof(i), fi) != sizeof(i))
		goto fail_io;
	i = SWAP32(i);
	if (i != 0x4944454D) {
		// not an valid mds file
		fclose(fi);
		return -1;
	}

	// get offset to session block
	fseek(fi, 0x50, SEEK_SET);
	if (fread(&offset, 1, sizeof(offset), fi) != sizeof(offset))
		goto fail_io;
	offset = SWAP32(offset);

	// get total number of tracks
	offset += 14;
	fseek(fi, offset, SEEK_SET);
	if (fread(&s, 1, sizeof(s), fi) != sizeof(s))
		goto fail_io;
	s = SWAP16(s);
	numtracks = s;

	// get offset to track blocks
	fseek(fi, 4, SEEK_CUR);
	if (fread(&offset, 1, sizeof(offset), fi) != sizeof(offset))
		goto fail_io;
	offset = SWAP32(offset);

	// skip lead-in data
	while (1) {
		fseek(fi, offset + 4, SEEK_SET);
		if (fgetc(fi) < 0xA0) {
			break;
		}
		offset += 0x50;
	}

	// check if the image contains mixed subchannel data
	fseek(fi, offset + 1, SEEK_SET);
	subChanMixed = subChanRaw = (fgetc(fi) ? TRUE : FALSE);

	// read track data
	for (i = 1; i <= numtracks; i++) {
		fseek(fi, offset, SEEK_SET);

		// get the track type
		ti[i].type = ((fgetc(fi) == 0xA9) ? CDDA : DATA);
		fseek(fi, 8, SEEK_CUR);

		// get the track starting point
		ti[i].start[0] = fgetc(fi);
		ti[i].start[1] = fgetc(fi);
		ti[i].start[2] = fgetc(fi);

		if (fread(&extra_offset, 1, sizeof(extra_offset), fi) != sizeof(extra_offset))
			goto fail_io;
		extra_offset = SWAP32(extra_offset);

		// get track start offset (in .mdf)
		fseek(fi, offset + 0x28, SEEK_SET);
		if (fread(&l, 1, sizeof(l), fi) != sizeof(l))
			goto fail_io;
		l = SWAP32(l);
		ti[i].start_offset = l;

		// get pregap
		fseek(fi, extra_offset, SEEK_SET);
		if (fread(&l, 1, sizeof(l), fi) != sizeof(l))
			goto fail_io;
		l = SWAP32(l);
		if (l != 0 && i > 1)
			pregapOffset = msf2sec(ti[i].start);

		// get the track length
		if (fread(&l, 1, sizeof(l), fi) != sizeof(l))
			goto fail_io;
		l = SWAP32(l);
		sec2msf(l, ti[i].length);

		offset += 0x50;
	}
	fclose(fi);
	return 0;
fail_io:
#ifndef NDEBUG
	SysPrintf(_("File IO error in <%s:%s>.\n"), __FILE__, __func__);
#endif
	fclose(fi);
	return -1;
}

static int handlepbp(const char *isofile) {
	struct {
		unsigned int sig;
		unsigned int dontcare[8];
		unsigned int psar_offs;
	} pbp_hdr;
	struct {
		unsigned char type;
		unsigned char pad0;
		unsigned char track;
		char index0[3];
		char pad1;
		char index1[3];
	} toc_entry;
	struct {
		unsigned int offset;
		unsigned int size;
		unsigned int dontcare[6];
	} index_entry;
	char psar_sig[11];
	off_t psisoimg_offs, cdimg_base;
	unsigned int t, cd_length;
	unsigned int offsettab[8];
	unsigned int psar_offs, index_entry_size, index_entry_offset;
	const char *ext = NULL;
	int i, ret;

	if (strlen(isofile) >= 4)
		ext = isofile + strlen(isofile) - 4;
	if (ext == NULL || strcasecmp(ext, ".pbp") != 0)
		return -1;

	numtracks = 0;

	ret = fread(&pbp_hdr, 1, sizeof(pbp_hdr), cdHandle);
	if (ret != sizeof(pbp_hdr)) {
		SysPrintf("failed to read pbp\n");
		goto fail_io;
	}

	psar_offs = SWAP32(pbp_hdr.psar_offs);

	ret = fseeko(cdHandle, psar_offs, SEEK_SET);
	if (ret != 0) {
		SysPrintf("failed to seek to %x\n", psar_offs);
		goto fail_io;
	}

	psisoimg_offs = psar_offs;
	if (fread(psar_sig, 1, sizeof(psar_sig), cdHandle) != sizeof(psar_sig))
		goto fail_io;
	psar_sig[10] = 0;
	if (strcmp(psar_sig, "PSTITLEIMG") == 0) {
		// multidisk image?
		ret = fseeko(cdHandle, psar_offs + 0x200, SEEK_SET);
		if (ret != 0) {
			SysPrintf("failed to seek to %x\n", psar_offs + 0x200);
			goto fail_io;
		}

		if (fread(&offsettab, 1, sizeof(offsettab), cdHandle) != sizeof(offsettab)) {
			SysPrintf("failed to read offsettab\n");
			goto fail_io;
		}

		for (i = 0; i < sizeof(offsettab) / sizeof(offsettab[0]); i++) {
			if (offsettab[i] == 0)
				break;
		}
		cdrIsoMultidiskCount = i;
		if (cdrIsoMultidiskCount == 0) {
			SysPrintf("multidisk eboot has 0 images?\n");
			goto fail_io;
		}

		if (cdrIsoMultidiskSelect >= cdrIsoMultidiskCount)
			cdrIsoMultidiskSelect = 0;

		psisoimg_offs += SWAP32(offsettab[cdrIsoMultidiskSelect]);

		ret = fseeko(cdHandle, psisoimg_offs, SEEK_SET);
		if (ret != 0) {
			SysPrintf("failed to seek to %llx\n", (long long)psisoimg_offs);
			goto fail_io;
		}

		if (fread(psar_sig, 1, sizeof(psar_sig), cdHandle) != sizeof(psar_sig))
			goto fail_io;
		psar_sig[10] = 0;
	}

	if (strcmp(psar_sig, "PSISOIMG00") != 0) {
		SysPrintf("bad psar_sig: %s\n", psar_sig);
		goto fail_io;
	}

	// seek to TOC
	ret = fseeko(cdHandle, psisoimg_offs + 0x800, SEEK_SET);
	if (ret != 0) {
		SysPrintf("failed to seek to %llx\n", (long long)psisoimg_offs + 0x800);
		goto fail_io;
	}

	// first 3 entries are special
	fseek(cdHandle, sizeof(toc_entry), SEEK_CUR);
	if (fread(&toc_entry, 1, sizeof(toc_entry), cdHandle) != sizeof(toc_entry))
		goto fail_io;
	numtracks = btoi(toc_entry.index1[0]);

	if (fread(&toc_entry, 1, sizeof(toc_entry), cdHandle) != sizeof(toc_entry))
		goto fail_io;
	cd_length = btoi(toc_entry.index1[0]) * 60 * 75 +
		btoi(toc_entry.index1[1]) * 75 + btoi(toc_entry.index1[2]);

	for (i = 1; i <= numtracks; i++) {
		if (fread(&toc_entry, 1, sizeof(toc_entry), cdHandle) != sizeof(toc_entry))
			goto fail_io;

		ti[i].type = (toc_entry.type == 1) ? CDDA : DATA;

		ti[i].start_offset = btoi(toc_entry.index0[0]) * 60 * 75 +
			btoi(toc_entry.index0[1]) * 75 + btoi(toc_entry.index0[2]);
		ti[i].start_offset *= 2352;
		ti[i].start[0] = btoi(toc_entry.index1[0]);
		ti[i].start[1] = btoi(toc_entry.index1[1]);
		ti[i].start[2] = btoi(toc_entry.index1[2]);

		if (i > 1) {
			t = msf2sec(ti[i].start) - msf2sec(ti[i - 1].start);
			sec2msf(t, ti[i - 1].length);
		}
	}
	t = cd_length - ti[numtracks].start_offset / 2352;
	sec2msf(t, ti[numtracks].length);

	// seek to ISO index
	ret = fseeko(cdHandle, psisoimg_offs + 0x4000, SEEK_SET);
	if (ret != 0) {
		SysPrintf("failed to seek to ISO index\n");
		goto fail_io;
	}

	compr_img = calloc(1, sizeof(*compr_img));
	if (compr_img == NULL)
		goto fail_io;

	compr_img->block_shift = 4;
	compr_img->current_block = (unsigned int)-1;

	compr_img->index_len = (0x100000 - 0x4000) / sizeof(index_entry);
	compr_img->index_table = malloc((compr_img->index_len + 1) * sizeof(compr_img->index_table[0]));
	if (compr_img->index_table == NULL)
		goto fail_io;

	cdimg_base = psisoimg_offs + 0x100000;
	for (i = 0; i < compr_img->index_len; i++) {
		ret = fread(&index_entry, 1, sizeof(index_entry), cdHandle);
		if (ret != sizeof(index_entry)) {
			SysPrintf("failed to read index_entry #%d\n", i);
			goto fail_index;
		}

		index_entry_size = SWAP32(index_entry.size);
		index_entry_offset = SWAP32(index_entry.offset);

		if (index_entry_size == 0)
			break;

		compr_img->index_table[i] = cdimg_base + index_entry_offset;
	}
	compr_img->index_table[i] = cdimg_base + index_entry_offset + index_entry_size;

	return 0;

fail_index:
	free(compr_img->index_table);
	compr_img->index_table = NULL;
	goto done;

fail_io:
	SysPrintf(_("File IO error in <%s:%s>.\n"), __FILE__, __func__);
	rewind(cdHandle);

done:
	if (compr_img != NULL) {
		free(compr_img);
		compr_img = NULL;
	}
	return -1;
}

static int handlecbin(const char *isofile) {
	struct
	{
		char magic[4];
		unsigned int header_size;
		unsigned long long total_bytes;
		unsigned int block_size;
		unsigned char ver;		// 1
		unsigned char align;
		unsigned char rsv_06[2];
	} ciso_hdr;
	const char *ext = NULL;
	unsigned int *index_table = NULL;
	unsigned int index = 0, plain;
	int i, ret;

	if (strlen(isofile) >= 5)
		ext = isofile + strlen(isofile) - 5;
	if (ext == NULL || (strcasecmp(ext + 1, ".cbn") != 0 && strcasecmp(ext, ".cbin") != 0))
		return -1;

	ret = fread(&ciso_hdr, 1, sizeof(ciso_hdr), cdHandle);
	if (ret != sizeof(ciso_hdr)) {
		SysPrintf("failed to read ciso header\n");
		goto fail_io;
	}

	if (strncmp(ciso_hdr.magic, "CISO", 4) != 0 || ciso_hdr.total_bytes <= 0 || ciso_hdr.block_size <= 0) {
		SysPrintf("bad ciso header\n");
		goto fail_io;
	}
	if (ciso_hdr.header_size != 0 && ciso_hdr.header_size != sizeof(ciso_hdr)) {
		ret = fseeko(cdHandle, ciso_hdr.header_size, SEEK_SET);
		if (ret != 0) {
			SysPrintf("failed to seek to %x\n", ciso_hdr.header_size);
			goto fail_io;
		}
	}

	compr_img = calloc(1, sizeof(*compr_img));
	if (compr_img == NULL)
		goto fail_io;

	compr_img->block_shift = 0;
	compr_img->current_block = (unsigned int)-1;

	compr_img->index_len = ciso_hdr.total_bytes / ciso_hdr.block_size;
	index_table = malloc((compr_img->index_len + 1) * sizeof(index_table[0]));
	if (index_table == NULL)
		goto fail_io;

	ret = fread(index_table, sizeof(index_table[0]), compr_img->index_len, cdHandle);
	if (ret != compr_img->index_len) {
		SysPrintf("failed to read index table\n");
		goto fail_index;
	}

	compr_img->index_table = malloc((compr_img->index_len + 1) * sizeof(compr_img->index_table[0]));
	if (compr_img->index_table == NULL)
		goto fail_index;

	for (i = 0; i < compr_img->index_len + 1; i++) {
		index = index_table[i];
		plain = index & 0x80000000;
		index &= 0x7fffffff;
		compr_img->index_table[i] = (off_t)index << ciso_hdr.align;
		if (plain)
			compr_img->index_table[i] |= OFF_T_MSB;
	}

	return 0;

fail_index:
	free(index_table);
fail_io:
	if (compr_img != NULL) {
		free(compr_img);
		compr_img = NULL;
	}
	rewind(cdHandle);
	return -1;
}

#ifdef HAVE_CHD
static int handlechd(const char *isofile) {
	int frame_offset = 150;
	int file_offset = 0;

	chd_img = calloc(1, sizeof(*chd_img));
	if (chd_img == NULL)
		goto fail_io;

	if(chd_open(isofile, CHD_OPEN_READ, NULL, &chd_img->chd) != CHDERR_NONE)
		goto fail_io;

	if (Config.CHD_Precache && (chd_precache(chd_img->chd) != CHDERR_NONE))
		goto fail_io;

	chd_img->header = chd_get_header(chd_img->chd);

	chd_img->buffer = malloc(chd_img->header->hunkbytes * 2);
	if (chd_img->buffer == NULL)
		goto fail_io;

	chd_img->sectors_per_hunk = chd_img->header->hunkbytes / (CD_FRAMESIZE_RAW + SUB_FRAMESIZE);
	chd_img->current_hunk[0] = (unsigned int)-1;
	chd_img->current_hunk[1] = (unsigned int)-1;

	cddaBigEndian = TRUE;

	numtracks = 0;
	memset(ti, 0, sizeof(ti));

   while (1)
   {
      struct {
         char type[64];
         char subtype[32];
         char pgtype[32];
         char pgsub[32];
         uint32_t track;
         uint32_t frames;
         uint32_t pregap;
         uint32_t postgap;
      } md = {};
      char meta[256];
      uint32_t meta_size = 0;

      if (chd_get_metadata(chd_img->chd, CDROM_TRACK_METADATA2_TAG, numtracks, meta, sizeof(meta), &meta_size, NULL, NULL) == CHDERR_NONE)
         sscanf(meta, CDROM_TRACK_METADATA2_FORMAT, &md.track, md.type, md.subtype, &md.frames, &md.pregap, md.pgtype, md.pgsub, &md.postgap);
      else if (chd_get_metadata(chd_img->chd, CDROM_TRACK_METADATA_TAG, numtracks, meta, sizeof(meta), &meta_size, NULL, NULL) == CHDERR_NONE)
         sscanf(meta, CDROM_TRACK_METADATA_FORMAT, &md.track, md.type, md.subtype, &md.frames);
      else
         break;

		SysPrintf("chd: %s\n", meta);

		if (md.track == 1) {
			if (!strncmp(md.subtype, "RW", 2)) {
				subChanMixed = TRUE;
				if (!strcmp(md.subtype, "RW_RAW"))
					subChanRaw = TRUE;
			}
		}

		ti[md.track].type = !strncmp(md.type, "AUDIO", 5) ? CDDA : DATA;

		sec2msf(frame_offset + md.pregap, ti[md.track].start);
		sec2msf(md.frames, ti[md.track].length);

		ti[md.track].start_offset = file_offset + md.pregap;

		// XXX: what about postgap?
		frame_offset += md.frames;
		file_offset += md.frames;
		numtracks++;
	}

	if (numtracks)
		return 0;

fail_io:
	if (chd_img != NULL) {
		free(chd_img->buffer);
		free(chd_img);
		chd_img = NULL;
	}
	return -1;
}
#endif

// this function tries to get the .sub file of the given .img
static int opensubfile(const char *isoname) {
	char		subname[MAXPATHLEN];

	// copy name of the iso and change extension from .img to .sub
	strncpy(subname, isoname, sizeof(subname));
	subname[MAXPATHLEN - 1] = '\0';
	if (strlen(subname) >= 4) {
		strcpy(subname + strlen(subname) - 4, ".sub");
	}
	else {
		return -1;
	}

	subHandle = fopen(subname, "rb");
	if (subHandle == NULL) {
		return -1;
	}

	return 0;
}

static int opensbifile(const char *isoname) {
	char		sbiname[MAXPATHLEN], disknum[MAXPATHLEN] = "0";
	int		s;

	strncpy(sbiname, isoname, sizeof(sbiname));
	sbiname[MAXPATHLEN - 1] = '\0';
	if (strlen(sbiname) >= 4) {
		if (cdrIsoMultidiskCount > 1) {
			sprintf(disknum, "_%i.sbi", cdrIsoMultidiskSelect + 1);
			strcpy(sbiname + strlen(sbiname) - 4, disknum);
		}
		else
			strcpy(sbiname + strlen(sbiname) - 4, ".sbi");
	}
	else {
		return -1;
	}

	s = msf2sec(ti[1].length);

	return LoadSBI(sbiname, s);
}

#if !USE_READ_THREAD
static void readThreadStop() {}
static void readThreadStart() {}
#else
static pthread_t read_thread_id;

static pthread_cond_t read_thread_msg_avail;
static pthread_cond_t read_thread_msg_done;
static pthread_mutex_t read_thread_msg_lock;

static pthread_cond_t sectorbuffer_cond;
static pthread_mutex_t sectorbuffer_lock;

static boolean read_thread_running = FALSE;
static int read_thread_sector_start = -1;
static int read_thread_sector_end = -1;

typedef struct {
  int sector;
  long ret;
  unsigned char data[CD_FRAMESIZE_RAW];
} SectorBufferEntry;

#define SECTOR_BUFFER_SIZE 4096

static SectorBufferEntry *sectorbuffer;
static size_t sectorbuffer_index;

int (*sync_cdimg_read_func)(FILE *f, unsigned int base, void *dest, int sector);
unsigned char *(*sync_CDR_getBuffer)(void);

static unsigned char * CALLBACK ISOgetBuffer_async(void);
static int cdread_async(FILE *f, unsigned int base, void *dest, int sector);

static void *readThreadMain(void *param) {
  int max_sector = -1;
  int requested_sector_start = -1;
  int requested_sector_end = -1;
  int last_read_sector = -1;
  int index = 0;

  int ra_sector = -1;
  int max_ra = 128;
  int initial_ra = 1;
  int speedmult_ra = 4;

  int ra_count = 0;
  int how_far_ahead = 0;

  unsigned char tmpdata[CD_FRAMESIZE_RAW];
  long ret;

  max_sector = msf2sec(ti[numtracks].start) + msf2sec(ti[numtracks].length);

  while(1) {
    pthread_mutex_lock(&read_thread_msg_lock);

    // If we don't have readahead and we don't have a sector request, wait for one.
    // If we still have readahead to go, don't block, just keep going.
    // And if we ever have a sector request pending, acknowledge and reset it.

    if (!ra_count) {
      if (read_thread_sector_start == -1 && read_thread_running) {
        pthread_cond_wait(&read_thread_msg_avail, &read_thread_msg_lock);
      }
    }

    if (read_thread_sector_start != -1) {
      requested_sector_start = read_thread_sector_start;
      requested_sector_end = read_thread_sector_end;
      read_thread_sector_start = -1;
      read_thread_sector_end = -1;
      pthread_cond_signal(&read_thread_msg_done);
    }

    pthread_mutex_unlock(&read_thread_msg_lock);

    if (!read_thread_running)
      break;

    // Readahead code, based on the implementation in mednafen psx's cdromif.cpp
    if (requested_sector_start != -1) {
      if (last_read_sector != -1 && last_read_sector == (requested_sector_start - 1)) {
        how_far_ahead = ra_sector - requested_sector_end;

        if(how_far_ahead <= max_ra)
          ra_count = (max_ra - how_far_ahead + 1 ? max_ra - how_far_ahead + 1 : speedmult_ra);
        else
          ra_count++;
      } else if (requested_sector_end != last_read_sector) {
        ra_sector = requested_sector_end;
        ra_count = initial_ra;
      }

      last_read_sector = requested_sector_end;
    }

    index = ra_sector % SECTOR_BUFFER_SIZE;

    // check for end of CD
    if (ra_count && ra_sector >= max_sector) {
      ra_count = 0;
      pthread_mutex_lock(&sectorbuffer_lock);
      sectorbuffer[index].ret = -1;
      sectorbuffer[index].sector = ra_sector;
      pthread_cond_signal(&sectorbuffer_cond);
      pthread_mutex_unlock(&sectorbuffer_lock);
    }

    if (ra_count) {
      pthread_mutex_lock(&sectorbuffer_lock);
      if (sectorbuffer[index].sector != ra_sector) {
        pthread_mutex_unlock(&sectorbuffer_lock);

        ret = sync_cdimg_read_func(cdHandle, 0, tmpdata, ra_sector);

        pthread_mutex_lock(&sectorbuffer_lock);
        sectorbuffer[index].ret = ret;
        sectorbuffer[index].sector = ra_sector;
        memcpy(sectorbuffer[index].data, tmpdata, CD_FRAMESIZE_RAW);
      }
      pthread_cond_signal(&sectorbuffer_cond);
      pthread_mutex_unlock(&sectorbuffer_lock);

      ra_sector++;
      ra_count--;
    }
  }

  return NULL;
}

static void readThreadStop() {
  if (read_thread_running == TRUE) {
    read_thread_running = FALSE;
    pthread_cond_signal(&read_thread_msg_avail);
    pthread_join(read_thread_id, NULL);
  }

  pthread_cond_destroy(&read_thread_msg_done);
  pthread_cond_destroy(&read_thread_msg_avail);
  pthread_mutex_destroy(&read_thread_msg_lock);

  pthread_cond_destroy(&sectorbuffer_cond);
  pthread_mutex_destroy(&sectorbuffer_lock);

  CDR_getBuffer = sync_CDR_getBuffer;
  cdimg_read_func = sync_cdimg_read_func;

  free(sectorbuffer);
  sectorbuffer = NULL;
}

static void readThreadStart() {
  SysPrintf("Starting async CD thread\n");

  if (read_thread_running == TRUE)
    return;

  read_thread_running = TRUE;
  read_thread_sector_start = -1;
  read_thread_sector_end = -1;
  sectorbuffer_index = 0;

  sectorbuffer = calloc(SECTOR_BUFFER_SIZE, sizeof(SectorBufferEntry));
  if(!sectorbuffer)
    goto error;

  sectorbuffer[0].sector = -1; // Otherwise we might think we've already fetched sector 0!

  sync_CDR_getBuffer = CDR_getBuffer;
  CDR_getBuffer = ISOgetBuffer_async;
  sync_cdimg_read_func = cdimg_read_func;
  cdimg_read_func = cdread_async;

  if (pthread_cond_init(&read_thread_msg_avail, NULL) ||
      pthread_cond_init(&read_thread_msg_done, NULL) ||
      pthread_mutex_init(&read_thread_msg_lock, NULL) ||
      pthread_cond_init(&sectorbuffer_cond, NULL) ||
      pthread_mutex_init(&sectorbuffer_lock, NULL) ||
      pthread_create(&read_thread_id, NULL, readThreadMain, NULL))
    goto error;

  return;

 error:
  SysPrintf("Error starting async CD thread\n");
  SysPrintf("Falling back to sync\n");

  readThreadStop();
}
#endif

static int cdread_normal(FILE *f, unsigned int base, void *dest, int sector)
{
	int ret;
	if (!f)
		return -1;
	if (fseeko(f, base + sector * CD_FRAMESIZE_RAW, SEEK_SET))
		goto fail_io;
	ret = fread(dest, 1, CD_FRAMESIZE_RAW, f);
	if (ret <= 0)
		goto fail_io;
	return ret;

fail_io:
	// often happens in cdda gaps of a split cue/bin, so not logged
	//SysPrintf("File IO error %d, base %u, sector %u\n", errno, base, sector);
	return -1;
}

static int cdread_sub_mixed(FILE *f, unsigned int base, void *dest, int sector)
{
	int ret;

	if (!f)
		return -1;
	if (fseeko(f, base + sector * (CD_FRAMESIZE_RAW + SUB_FRAMESIZE), SEEK_SET))
		goto fail_io;
	ret = fread(dest, 1, CD_FRAMESIZE_RAW, f);
	if (ret <= 0)
		goto fail_io;
	return ret;

fail_io:
	//SysPrintf("File IO error %d, base %u, sector %u\n", errno, base, sector);
	return -1;
}

static int cdread_sub_sub_mixed(FILE *f, int sector)
{
	if (!f)
		return -1;
	if (fseeko(f, sector * (CD_FRAMESIZE_RAW + SUB_FRAMESIZE) + CD_FRAMESIZE_RAW, SEEK_SET))
		goto fail_io;
	if (fread(subbuffer, 1, SUB_FRAMESIZE, f) != SUB_FRAMESIZE)
		goto fail_io;

	return SUB_FRAMESIZE;

fail_io:
	SysPrintf("subchannel: file IO error %d, sector %u\n", errno, sector);
	return -1;
}

static int uncompress2_pcsx(void *out, unsigned long *out_size, void *in, unsigned long in_size)
{
	static z_stream z;
	int ret = 0;

	if (z.zalloc == NULL) {
		// XXX: one-time leak here..
		z.next_in = Z_NULL;
		z.avail_in = 0;
		z.zalloc = Z_NULL;
		z.zfree = Z_NULL;
		z.opaque = Z_NULL;
		ret = inflateInit2(&z, -15);
	}
	else
		ret = inflateReset(&z);
	if (ret != Z_OK)
		return ret;

	z.next_in = in;
	z.avail_in = in_size;
	z.next_out = out;
	z.avail_out = *out_size;

	ret = inflate(&z, Z_NO_FLUSH);
	//inflateEnd(&z);

	*out_size -= z.avail_out;
	return ret == 1 ? 0 : ret;
}

static int cdread_compressed(FILE *f, unsigned int base, void *dest, int sector)
{
	unsigned long cdbuffer_size, cdbuffer_size_expect;
	unsigned int size;
	int is_compressed;
	off_t start_byte;
	int ret, block;

	if (!cdHandle)
		return -1;
	if (base)
		sector += base / 2352;

	block = sector >> compr_img->block_shift;
	compr_img->sector_in_blk = sector & ((1 << compr_img->block_shift) - 1);

	if (block == compr_img->current_block) {
		//printf("hit sect %d\n", sector);
		goto finish;
	}

	if (sector >= compr_img->index_len * 16) {
		SysPrintf("sector %d is past img end\n", sector);
		return -1;
	}

	start_byte = compr_img->index_table[block] & ~OFF_T_MSB;
	if (fseeko(cdHandle, start_byte, SEEK_SET) != 0) {
		SysPrintf("seek error for block %d at %llx: ",
			block, (long long)start_byte);
		perror(NULL);
		return -1;
	}

	is_compressed = !(compr_img->index_table[block] & OFF_T_MSB);
	size = (compr_img->index_table[block + 1] & ~OFF_T_MSB) - start_byte;
	if (size > sizeof(compr_img->buff_compressed)) {
		SysPrintf("block %d is too large: %u\n", block, size);
		return -1;
	}

	if (fread(is_compressed ? compr_img->buff_compressed : compr_img->buff_raw[0],
				1, size, cdHandle) != size) {
		SysPrintf("read error for block %d at %x: ", block, start_byte);
		perror(NULL);
		return -1;
	}

	if (is_compressed) {
		cdbuffer_size_expect = sizeof(compr_img->buff_raw[0]) << compr_img->block_shift;
		cdbuffer_size = cdbuffer_size_expect;
		ret = uncompress2_pcsx(compr_img->buff_raw[0], &cdbuffer_size, compr_img->buff_compressed, size);
		if (ret != 0) {
			SysPrintf("uncompress failed with %d for block %d, sector %d\n",
					ret, block, sector);
			return -1;
		}
		if (cdbuffer_size != cdbuffer_size_expect)
			SysPrintf("cdbuffer_size: %lu != %lu, sector %d\n", cdbuffer_size,
					cdbuffer_size_expect, sector);
	}

	// done at last!
	compr_img->current_block = block;

finish:
	if (dest != cdbuffer) // copy avoid HACK
		memcpy(dest, compr_img->buff_raw[compr_img->sector_in_blk],
			CD_FRAMESIZE_RAW);
	return CD_FRAMESIZE_RAW;
}

#ifdef HAVE_CHD
static unsigned char *chd_get_sector(unsigned int current_buffer, unsigned int sector_in_hunk)
{
	return chd_img->buffer
		+ current_buffer * chd_img->header->hunkbytes
		+ sector_in_hunk * (CD_FRAMESIZE_RAW + SUB_FRAMESIZE);
}

static int cdread_chd(FILE *f, unsigned int base, void *dest, int sector)
{
	int hunk;

	sector += base;

	hunk = sector / chd_img->sectors_per_hunk;
	chd_img->sector_in_hunk = sector % chd_img->sectors_per_hunk;

	if (hunk == chd_img->current_hunk[0])
		chd_img->current_buffer = 0;
	else if (hunk == chd_img->current_hunk[1])
		chd_img->current_buffer = 1;
	else
	{
		chd_read(chd_img->chd, hunk, chd_img->buffer +
			chd_img->current_buffer * chd_img->header->hunkbytes);
		chd_img->current_hunk[chd_img->current_buffer] = hunk;
	}

	if (dest != cdbuffer) // copy avoid HACK
		memcpy(dest, chd_get_sector(chd_img->current_buffer, chd_img->sector_in_hunk),
			CD_FRAMESIZE_RAW);
	return CD_FRAMESIZE_RAW;
}

static int cdread_sub_chd(FILE *f, int sector)
{
	unsigned int sector_in_hunk;
	unsigned int buffer;
	int hunk;

	if (!subChanMixed)
		return -1;

	hunk = sector / chd_img->sectors_per_hunk;
	sector_in_hunk = sector % chd_img->sectors_per_hunk;

	if (hunk == chd_img->current_hunk[0])
		buffer = 0;
	else if (hunk == chd_img->current_hunk[1])
		buffer = 1;
	else
	{
		buffer = chd_img->current_buffer ^ 1;
		chd_read(chd_img->chd, hunk, chd_img->buffer +
			buffer * chd_img->header->hunkbytes);
		chd_img->current_hunk[buffer] = hunk;
	}

	memcpy(subbuffer, chd_get_sector(buffer, sector_in_hunk) + CD_FRAMESIZE_RAW, SUB_FRAMESIZE);
	return SUB_FRAMESIZE;
}
#endif

static int cdread_2048(FILE *f, unsigned int base, void *dest, int sector)
{
	int ret;

	if (!f)
		return -1;
	fseeko(f, base + sector * 2048, SEEK_SET);
	ret = fread((char *)dest + 12 * 2, 1, 2048, f);

	// not really necessary, fake mode 2 header
	memset(cdbuffer, 0, 12 * 2);
	sec2msf(sector + 2 * 75, (char *)&cdbuffer[12]);
	cdbuffer[12 + 3] = 1;

	return 12*2 + ret;
}

#if USE_READ_THREAD

static int cdread_async(FILE *f, unsigned int base, void *dest, int sector) {
  boolean found = FALSE;
  int i = sector % SECTOR_BUFFER_SIZE;
  long ret;

  if (f != cdHandle || base != 0 || dest != cdbuffer) {
    // Async reads are only supported for cdbuffer, so call the sync
    // function directly.
    return sync_cdimg_read_func(f, base, dest, sector);
  }

  pthread_mutex_lock(&read_thread_msg_lock);

  // Only wait if we're not trying to read the next sector and
  // sector_start is set (meaning the last request hasn't been
  // processed yet)
  while(read_thread_sector_start != -1 && read_thread_sector_end + 1 != sector) {
    pthread_cond_wait(&read_thread_msg_done, &read_thread_msg_lock);
  }

  if (read_thread_sector_start == -1)
    read_thread_sector_start = sector;

  read_thread_sector_end = sector;
  pthread_cond_signal(&read_thread_msg_avail);
  pthread_mutex_unlock(&read_thread_msg_lock);

  do {
    pthread_mutex_lock(&sectorbuffer_lock);
    if (sectorbuffer[i].sector == sector) {
      sectorbuffer_index = i;
      ret = sectorbuffer[i].ret;
      found = TRUE;
    }

    if (!found) {
      pthread_cond_wait(&sectorbuffer_cond, &sectorbuffer_lock);
    }
    pthread_mutex_unlock(&sectorbuffer_lock);
  } while (!found);

  return ret;
}

#endif

static unsigned char * CALLBACK ISOgetBuffer_compr(void) {
	return compr_img->buff_raw[compr_img->sector_in_blk] + 12;
}

#ifdef HAVE_CHD
static unsigned char * CALLBACK ISOgetBuffer_chd(void) {
	return chd_get_sector(chd_img->current_buffer, chd_img->sector_in_hunk) + 12;
}
#endif

#if USE_READ_THREAD
static unsigned char * CALLBACK ISOgetBuffer_async(void) {
  unsigned char *buffer;
  pthread_mutex_lock(&sectorbuffer_lock);
  buffer = sectorbuffer[sectorbuffer_index].data;
  pthread_mutex_unlock(&sectorbuffer_lock);
  return buffer + 12;
}
#endif

static unsigned char * CALLBACK ISOgetBuffer(void) {
	return cdbuffer + 12;
}

static void PrintTracks(void) {
	int i;

	for (i = 1; i <= numtracks; i++) {
		SysPrintf(_("Track %.2d (%s) - Start %.2d:%.2d:%.2d, Length %.2d:%.2d:%.2d\n"),
			i, (ti[i].type == DATA ? "DATA" :
			    (ti[i].type == CDDA ? "AUDIO" : "UNKNOWN")),
			ti[i].start[0], ti[i].start[1], ti[i].start[2],
			ti[i].length[0], ti[i].length[1], ti[i].length[2]);
	}
}

// This function is invoked by the front-end when opening an ISO
// file for playback
static long CALLBACK ISOopen(void) {
	boolean isMode1ISO = FALSE;
	char alt_bin_filename[MAXPATHLEN];
	const char *bin_filename;
	char image_str[1024];
	off_t size_main;

	if (cdHandle || chd_img) {
		return 0; // it's already open
	}

	cdHandle = fopen(GetIsoFile(), "rb");
	if (cdHandle == NULL) {
		SysPrintf(_("Could't open '%s' for reading: %s\n"),
			GetIsoFile(), strerror(errno));
		return -1;
	}
	size_main = get_size(cdHandle);

	snprintf(image_str, sizeof(image_str) - 6*4 - 1,
		"Loaded CD Image: %s", GetIsoFile());

	cddaBigEndian = FALSE;
	subChanMixed = FALSE;
	subChanRaw = FALSE;
	pregapOffset = 0;
	cdrIsoMultidiskCount = 1;
	multifile = 0;

	CDR_getBuffer = ISOgetBuffer;
	cdimg_read_func = cdread_normal;
	cdimg_read_sub_func = NULL;

	if (parsetoc(GetIsoFile()) == 0) {
		strcat(image_str, "[+toc]");
	}
	else if (parseccd(GetIsoFile()) == 0) {
		strcat(image_str, "[+ccd]");
	}
	else if (parsemds(GetIsoFile()) == 0) {
		strcat(image_str, "[+mds]");
	}
	else if (parsecue(GetIsoFile()) == 0) {
		strcat(image_str, "[+cue]");
	}
	if (handlepbp(GetIsoFile()) == 0) {
		strcat(image_str, "[+pbp]");
		CDR_getBuffer = ISOgetBuffer_compr;
		cdimg_read_func = cdread_compressed;
	}
	else if (handlecbin(GetIsoFile()) == 0) {
		strcat(image_str, "[+cbin]");
		CDR_getBuffer = ISOgetBuffer_compr;
		cdimg_read_func = cdread_compressed;
	}
#ifdef HAVE_CHD
	else if (handlechd(GetIsoFile()) == 0) {
		strcat(image_str, "[+chd]");
		CDR_getBuffer = ISOgetBuffer_chd;
		cdimg_read_func = cdread_chd;
		cdimg_read_sub_func = cdread_sub_chd;
		fclose(cdHandle);
		cdHandle = NULL;
	}
#endif

	if (!subChanMixed && opensubfile(GetIsoFile()) == 0) {
		strcat(image_str, "[+sub]");
	}
	if (opensbifile(GetIsoFile()) == 0) {
		strcat(image_str, "[+sbi]");
	}

	// maybe user selected metadata file instead of main .bin ..
	bin_filename = GetIsoFile();
	if (cdHandle && size_main < 2352 * 0x10) {
		static const char *exts[] = { ".bin", ".BIN", ".img", ".IMG" };
		FILE *tmpf = NULL;
		size_t i;
		char *p;

		strncpy(alt_bin_filename, bin_filename, sizeof(alt_bin_filename));
		alt_bin_filename[MAXPATHLEN - 1] = '\0';
		if (strlen(alt_bin_filename) >= 4) {
			p = alt_bin_filename + strlen(alt_bin_filename) - 4;
			for (i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
				strcpy(p, exts[i]);
				tmpf = fopen(alt_bin_filename, "rb");
				if (tmpf != NULL)
					break;
			}
		}
		if (tmpf != NULL) {
			bin_filename = alt_bin_filename;
			fclose(cdHandle);
			cdHandle = tmpf;
			size_main = get_size(cdHandle);
		}
	}

	// guess whether it is mode1/2048
	if (cdHandle && cdimg_read_func == cdread_normal && size_main % 2048 == 0) {
		unsigned int modeTest = 0;
		if (!fread(&modeTest, sizeof(modeTest), 1, cdHandle)) {
			SysPrintf(_("File IO error in <%s:%s>.\n"), __FILE__, __func__);
		}
		if (SWAP32(modeTest) != 0xffffff00) {
			strcat(image_str, "[2048]");
			isMode1ISO = TRUE;
		}
	}

	SysPrintf("%s.\n", image_str);

	PrintTracks();

	if (subChanMixed && cdimg_read_func == cdread_normal) {
		cdimg_read_func = cdread_sub_mixed;
		cdimg_read_sub_func = cdread_sub_sub_mixed;
	}
	else if (isMode1ISO) {
		cdimg_read_func = cdread_2048;
		cdimg_read_sub_func = NULL;
	}

	if (Config.AsyncCD) {
		readThreadStart();
	}
	return 0;
}

static long CALLBACK ISOclose(void) {
	int i;

	if (cdHandle != NULL) {
		fclose(cdHandle);
		cdHandle = NULL;
	}
	if (subHandle != NULL) {
		fclose(subHandle);
		subHandle = NULL;
	}
	cddaHandle = NULL;

	if (compr_img != NULL) {
		free(compr_img->index_table);
		free(compr_img);
		compr_img = NULL;
	}

#ifdef HAVE_CHD
	if (chd_img != NULL) {
		chd_close(chd_img->chd);
		free(chd_img->buffer);
		free(chd_img);
		chd_img = NULL;
	}
#endif

	for (i = 1; i <= numtracks; i++) {
		if (ti[i].handle != NULL) {
			fclose(ti[i].handle);
			ti[i].handle = NULL;
		}
	}
	numtracks = 0;
	ti[1].type = 0;
	UnloadSBI();

	memset(cdbuffer, 0, sizeof(cdbuffer));
	CDR_getBuffer = ISOgetBuffer;

	if (Config.AsyncCD) {
		readThreadStop();
	}

	return 0;
}

static long CALLBACK ISOinit(void) {
	assert(cdHandle == NULL);
	assert(subHandle == NULL);

	return 0; // do nothing
}

static long CALLBACK ISOshutdown(void) {
	ISOclose();
	return 0;
}

// return Starting and Ending Track
// buffer:
//  byte 0 - start track
//  byte 1 - end track
static long CALLBACK ISOgetTN(unsigned char *buffer) {
	buffer[0] = 1;

	if (numtracks > 0) {
		buffer[1] = numtracks;
	}
	else {
		buffer[1] = 1;
	}

	return 0;
}

// return Track Time
// buffer:
//  byte 0 - frame
//  byte 1 - second
//  byte 2 - minute
static long CALLBACK ISOgetTD(unsigned char track, unsigned char *buffer) {
	if (track == 0) {
		unsigned int sect;
		unsigned char time[3];
		sect = msf2sec(ti[numtracks].start) + msf2sec(ti[numtracks].length);
		sec2msf(sect, (char *)time);
		buffer[2] = time[0];
		buffer[1] = time[1];
		buffer[0] = time[2];
	}
	else if (numtracks > 0 && track <= numtracks) {
		buffer[2] = ti[track].start[0];
		buffer[1] = ti[track].start[1];
		buffer[0] = ti[track].start[2];
	}
	else {
		buffer[2] = 0;
		buffer[1] = 2;
		buffer[0] = 0;
	}

	return 0;
}

// decode 'raw' subchannel data ripped by cdrdao
static void DecodeRawSubData(void) {
	unsigned char subQData[12];
	int i;

	memset(subQData, 0, sizeof(subQData));

	for (i = 0; i < 8 * 12; i++) {
		if (subbuffer[i] & (1 << 6)) { // only subchannel Q is needed
			subQData[i >> 3] |= (1 << (7 - (i & 7)));
		}
	}

	memcpy(&subbuffer[12], subQData, 12);
}

// read track
// time: byte 0 - minute; byte 1 - second; byte 2 - frame
// uses bcd format
static boolean CALLBACK ISOreadTrack(unsigned char *time) {
	int sector = MSF2SECT(btoi(time[0]), btoi(time[1]), btoi(time[2]));
	long ret;

	if (!cdHandle && !chd_img)
		return 0;

	if (pregapOffset && sector >= pregapOffset)
		sector -= 2 * 75;

	ret = cdimg_read_func(cdHandle, 0, cdbuffer, sector);
	if (ret < 12*2 + 2048)
		return 0;

	return 1;
}

// plays cdda audio
// sector: byte 0 - minute; byte 1 - second; byte 2 - frame
// does NOT uses bcd format
static long CALLBACK ISOplay(unsigned char *time) {
	return 0;
}

// stops cdda audio
static long CALLBACK ISOstop(void) {
	return 0;
}

// gets subchannel data
static unsigned char* CALLBACK ISOgetBufferSub(int sector) {
	if (pregapOffset && sector >= pregapOffset) {
		sector -= 2 * 75;
		if (sector < pregapOffset) // ?
			return NULL;
	}

	if (cdimg_read_sub_func != NULL) {
		if (cdimg_read_sub_func(cdHandle, sector) != SUB_FRAMESIZE)
			return NULL;
	}
	else if (subHandle != NULL) {
		if (fseeko(subHandle, sector * SUB_FRAMESIZE, SEEK_SET))
			return NULL;
		if (fread(subbuffer, 1, SUB_FRAMESIZE, subHandle) != SUB_FRAMESIZE)
			return NULL;
	}
	else {
		return NULL;
	}

	if (subChanRaw) DecodeRawSubData();
	return subbuffer;
}

static long CALLBACK ISOgetStatus(struct CdrStat *stat) {
	CDR__getStatus(stat);
	
	// BIOS - boot ID (CD type)
	stat->Type = ti[1].type;
	
	return 0;
}

// read CDDA sector into buffer
long CALLBACK ISOreadCDDA(unsigned char m, unsigned char s, unsigned char f, unsigned char *buffer) {
	unsigned char msf[3] = {m, s, f};
	unsigned int track, track_start = 0;
	FILE *handle = cdHandle;
	unsigned int cddaCurPos;
	int ret;

	cddaCurPos = msf2sec((char *)msf);

	// find current track index
	for (track = numtracks; ; track--) {
		track_start = msf2sec(ti[track].start);
		if (track_start <= cddaCurPos)
			break;
		if (track == 1)
			break;
	}

	// data tracks play silent
	if (ti[track].type != CDDA) {
		memset(buffer, 0, CD_FRAMESIZE_RAW);
		return 0;
	}

	if (multifile) {
		// find the file that contains this track
		unsigned int file;
		for (file = track; file > 1; file--) {
			if (ti[file].handle != NULL) {
				handle = ti[file].handle;
				break;
			}
		}
	}
	if (!handle && !chd_img) {
		memset(buffer, 0, CD_FRAMESIZE_RAW);
		return -1;
	}

	ret = cdimg_read_func(handle, ti[track].start_offset,
		buffer, cddaCurPos - track_start);
	if (ret != CD_FRAMESIZE_RAW) {
		memset(buffer, 0, CD_FRAMESIZE_RAW);
		return -1;
	}

	if (cddaBigEndian) {
		int i;
		unsigned char tmp;

		for (i = 0; i < CD_FRAMESIZE_RAW / 2; i++) {
			tmp = buffer[i * 2];
			buffer[i * 2] = buffer[i * 2 + 1];
			buffer[i * 2 + 1] = tmp;
		}
	}

	return 0;
}

void cdrIsoInit(void) {
	CDR_init = ISOinit;
	CDR_shutdown = ISOshutdown;
	CDR_open = ISOopen;
	CDR_close = ISOclose;
	CDR_getTN = ISOgetTN;
	CDR_getTD = ISOgetTD;
	CDR_readTrack = ISOreadTrack;
	CDR_getBuffer = ISOgetBuffer;
	CDR_play = ISOplay;
	CDR_stop = ISOstop;
	CDR_getBufferSub = ISOgetBufferSub;
	CDR_getStatus = ISOgetStatus;
	CDR_readCDDA = ISOreadCDDA;

	CDR_getDriveLetter = CDR__getDriveLetter;
	CDR_configure = CDR__configure;
	CDR_test = CDR__test;
	CDR_about = CDR__about;
	CDR_setfilename = CDR__setfilename;

	numtracks = 0;
}

int cdrIsoActive(void) {
	return (cdHandle || chd_img);
}
