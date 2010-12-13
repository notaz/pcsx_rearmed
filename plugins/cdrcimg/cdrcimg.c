/*
 * (C) Gražvydas "notaz" Ignotas, 2010
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>

#include "cdrcimg.h"

#define CD_FRAMESIZE_RAW 2352

static const char *cd_fname;
static unsigned int *cd_index_table;
static unsigned int  cd_index_len;
static FILE *cd_file;

static unsigned char cdbuffer[CD_FRAMESIZE_RAW];
static unsigned char cdbuffer_compressed[CD_FRAMESIZE_RAW + 100];
static int current_sector;

struct CdrStat;
extern long CDR__getStatus(struct CdrStat *stat);

struct CdrStat
{
	unsigned long Type;
	unsigned long Status;
	unsigned char Time[3]; // current playing time
};

struct trackinfo {
	enum {DATA, CDDA} type;
	char start[3];		// MSF-format
	char length[3];		// MSF-format
};

#define MAXTRACKS 100 /* How many tracks can a CD hold? */

static int numtracks = 0;

#define btoi(b)           ((b) / 16 * 10 + (b) % 16) /* BCD to u_char */
#define MSF2SECT(m, s, f) (((m) * 60 + (s) - 2) * 75 + (f))

// return Starting and Ending Track
// buffer:
//  byte 0 - start track
//  byte 1 - end track
static long CDRgetTN(unsigned char *buffer)
{
	buffer[0] = 1;
	buffer[1] = numtracks > 0 ? numtracks : 1;

	return 0;
}

// return Track Time
// buffer:
//  byte 0 - frame
//  byte 1 - second
//  byte 2 - minute
static long CDRgetTD(unsigned char track, unsigned char *buffer)
{
	buffer[2] = 0;
	buffer[1] = 2;
	buffer[0] = 0;

	return 0;
}

// read track
// time: byte 0 - minute; byte 1 - second; byte 2 - frame
// uses bcd format
static long CDRreadTrack(unsigned char *time)
{
	unsigned int start_byte, size;
	unsigned long cdbuffer_size;
	int ret, sector;

	if (cd_file == NULL)
		return -1;

	sector = MSF2SECT(btoi(time[0]), btoi(time[1]), btoi(time[2]));
	if (sector == current_sector) {
		// it's already there, nothing to do
		//printf("hit sect %d\n", sector);
		return 0;
	}

	if (sector >= cd_index_len) {
		fprintf(stderr, "sector %d is past track end\n", sector);
		return -1;
	}

	start_byte = cd_index_table[sector];
	if (fseek(cd_file, start_byte, SEEK_SET) != 0) {
		fprintf(stderr, "seek error for sector %d at %x: ",
			sector, start_byte);
		perror(NULL);
		return -1;
	}

	size = cd_index_table[sector + 1] - start_byte;
	if (size > sizeof(cdbuffer_compressed)) {
		fprintf(stderr, "sector %d is too large: %u\n", sector, size);
		return -1;
	}

	if (fread(cdbuffer_compressed, 1, size, cd_file) != size) {
		fprintf(stderr, "read error for sector %d at %x: ",
			sector, start_byte);
		perror(NULL);
		return -1;
	}

	cdbuffer_size = sizeof(cdbuffer);
	ret = uncompress(cdbuffer, &cdbuffer_size, cdbuffer_compressed, size);
	if (ret != 0) {
		fprintf(stderr, "uncompress failed with %d for sector %d\n", ret, sector);
		return -1;
	}
	if (cdbuffer_size != sizeof(cdbuffer))
		printf("%lu != %d\n", cdbuffer_size, sizeof(cdbuffer));

	// done at last!
	current_sector = sector;
	return 0;
}

// return read track
static unsigned char *CDRgetBuffer(void)
{
	return cdbuffer + 12;
}

// plays cdda audio
// sector: byte 0 - minute; byte 1 - second; byte 2 - frame
// does NOT uses bcd format
static long CDRplay(unsigned char *time)
{
	return 0;
}

// stops cdda audio
static long CDRstop(void)
{
	return 0;
}

// gets subchannel data
static unsigned char* CDRgetBufferSub(void)
{
	return NULL;
}

static long CDRgetStatus(struct CdrStat *stat) {
	CDR__getStatus(stat);

	stat->Type = 0x01;

	return 0;
}

static long CDRclose(void)
{
	if (cd_file != NULL) {
		fclose(cd_file);
		cd_file = NULL;
	}
	if (cd_index_table != NULL) {
		free(cd_index_table);
		cd_index_table = NULL;
	}
	return 0;
}

static long CDRshutdown(void)
{
	return CDRclose();
}

static long CDRinit(void)
{
	return 0; // do nothing
}

// This function is invoked by the front-end when opening an ISO
// file for playback
static long CDRopen(void)
{
	// a Z.table file is binary where each element represents
	// one compressed frame.  
	//    4 bytes: the offset of the frame in the .Z file
	//    2 bytes: the length of the compressed frame
	struct {
		unsigned int offset;
		unsigned short size;
	} __attribute__((packed)) ztab_entry;
	char table_fname[256];
	long table_size;
	int i, ret;
	FILE *f;

	if (cd_file != NULL)
		return 0; // it's already open

	numtracks = 0;

	if (cd_fname == NULL)
		return -1;

	snprintf(table_fname, sizeof(table_fname), "%s.table", cd_fname);
	f = fopen(table_fname, "rb");
	if (f == NULL) {
		fprintf(stderr, "missing file: %s: ", table_fname);
		perror(NULL);
		return -1;
	}

	ret = fseek(f, 0, SEEK_END);
	if (ret != 0) {
		fprintf(stderr, "failed to seek\n");
		goto fail_table_io;
	}
	table_size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (table_size > 2 * 1024 * 1024) {
		fprintf(stderr, ".table too large\n");
		goto fail_table_io;
	}

	cd_index_len = table_size / 6;
	cd_index_table = malloc((cd_index_len + 1) * sizeof(cd_index_table[0]));
	if (cd_index_table == NULL)
		goto fail_table_io;

	for (i = 0; i < cd_index_len; i++) {
		ret = fread(&ztab_entry, 1, sizeof(ztab_entry), f);
		if (ret != sizeof(ztab_entry)) {
			fprintf(stderr, ".table read failed on entry %d/%d\n", i, cd_index_len);
			goto fail_table_io_read;
		}
		cd_index_table[i] = ztab_entry.offset;
	}
	// fake entry, so that we know last compressed block size
	cd_index_table[i] = ztab_entry.offset + ztab_entry.size;

	cd_file = fopen(cd_fname, "rb");
	if (cd_file == NULL) {
		fprintf(stderr, "faied to open: %s: ", table_fname);
		perror(NULL);
		goto fail_img;
	}
	fclose(f);

	printf("Loaded compressed CD Image: %s.\n", cd_fname);
	current_sector = -1;

	return 0;

fail_img:
fail_table_io_read:
	free(cd_index_table);
	cd_index_table = NULL;
fail_table_io:
	fclose(f);
	return -1;
}

#define FUNC(n) { #n, n }

static const struct {
	const char *name;
	void *func;
} plugin_funcs[] = {
	/* CDR */
	FUNC(CDRinit),
	FUNC(CDRshutdown),
	FUNC(CDRopen),
	FUNC(CDRclose),
	FUNC(CDRgetTN),
	FUNC(CDRgetTD),
	FUNC(CDRreadTrack),
	FUNC(CDRgetBuffer),
	FUNC(CDRgetBufferSub),
	FUNC(CDRplay),
	FUNC(CDRstop),
	FUNC(CDRgetStatus),
};

void cdrcimg_set_fname(const char *fname)
{
	cd_fname = fname;
}

void *cdrcimg_get_sym(const char *sym)
{
	int i;
	for (i = 0; i < sizeof(plugin_funcs) / sizeof(plugin_funcs[0]); i++)
		if (strcmp(plugin_funcs[i].name, sym) == 0)
			return plugin_funcs[i].func;
	return NULL;
}

