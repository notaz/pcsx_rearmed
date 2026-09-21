/*
 * mkm2cd - generate a zero-filled Mode 2 Form 1 CD-ROM cue/bin image.
 *
 * Sector layout (2352 bytes, Mode 2 Form 1):
 *   0x000  12  sync    00 FF FF FF FF FF FF FF FF FF FF 00
 *   0x00C   3  address MM:SS:FF in BCD (LBA + 150)
 *   0x00F   1  mode    0x02
 *   0x010   8  subheader, two identical 4-byte copies:
 *               file=0 channel=0 submode=0x08 (data) coding=0
 *   0x018 2048 user data (zero filled here)
 *   0x818   4  EDC over bytes 0x010..0x817
 *   0x81C 172  P parity
 *   0x8C8 104  Q parity
 *
 * The EDC is a CRC-32 (poly x^32+x^31+x^16+x^15+x^4+x^3+x+1, LSB first,
 * zero init, no final inversion) over the subheader plus the user data.
 * Both are constant here, so the EDC is the same for every sector and is
 * simply hardcoded below.
 *
 * The P/Q parity is left zeroed, i.e. deliberately invalid: readers only
 * consult it when the EDC check fails, which it will not here.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define SECTOR_SIZE  2352
#define LBA_OFFSET    150       /* 2 second pregap, LBA 0 == 00:02:00 */
#define MAX_SECTORS  (100 * 60 * 75 - LBA_OFFSET)

static const unsigned char sync_pattern[12] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00
};

static const unsigned char subheader[8] = {
	0x00, 0x00, 0x08, 0x00,	/* file, channel, submode (data), coding info */
	0x00, 0x00, 0x08, 0x00	/* identical copy */
};

/*
 * EDC for a sector with the subheader above and 2048 zero user data bytes:
 * 0x9481880b, stored little endian.
 */
static const unsigned char edc[4] = { 0x0b, 0x88, 0x81, 0x94 };

static unsigned char to_bcd(unsigned int v)
{
	return (unsigned char)(((v / 10) << 4) | (v % 10));
}

static void make_header(unsigned char *sect, unsigned int lba)
{
	unsigned int amsf = lba + LBA_OFFSET;

	sect[12] = to_bcd(amsf / (60 * 75));
	sect[13] = to_bcd(amsf / 75 % 60);
	sect[14] = to_bcd(amsf % 75);
	sect[15] = 0x02;		/* mode 2 */
}

static int write_cue(const char *cue_name, const char *bin_name)
{
	FILE *f = fopen(cue_name, "w");
	if (f == NULL) {
		fprintf(stderr, "%s: %s\n", cue_name, strerror(errno));
		return -1;
	}

	fprintf(f, "FILE \"%s\" BINARY\n", bin_name);
	fprintf(f, "  TRACK 01 MODE2/2352\n");
	fprintf(f, "    INDEX 01 00:00:00\n");

	if (fclose(f) != 0) {
		fprintf(stderr, "%s: %s\n", cue_name, strerror(errno));
		return -1;
	}
	return 0;
}

static int write_bin(const char *bin_name, unsigned long sectors)
{
	unsigned char sect[SECTOR_SIZE];
	unsigned int i;
	int ret = 0;
	FILE *f;

	f = fopen(bin_name, "wb");
	if (f == NULL) {
		fprintf(stderr, "%s: %s\n", bin_name, strerror(errno));
		return -1;
	}

	memset(sect, 0, sizeof(sect));
	memcpy(sect + 0, sync_pattern, sizeof(sync_pattern));
	memcpy(sect + 16, subheader, sizeof(subheader));
	memcpy(sect + 2072, edc, sizeof(edc));

	for (i = 0; i < sectors; i++) {
		make_header(sect, i);
		if (fwrite(sect, 1, SECTOR_SIZE, f) != SECTOR_SIZE) {
			fprintf(stderr, "%s: write failed at sector %u: %s\n",
				bin_name, i, strerror(errno));
			ret = -1;
			break;
		}
	}

	if (fclose(f) != 0) {
		fprintf(stderr, "%s: %s\n", bin_name, strerror(errno));
		ret = -1;
	}
	return ret;
}

int main(int argc, char *argv[])
{
	const char *base = "image";
	char bin_name[256], cue_name[256];
	unsigned long sectors;
	char *end;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <basename> <sector_count>\n", argv[0]);
		return 1;
	}

	base = argv[1];
	errno = 0;
	sectors = strtoul(argv[2], &end, 0);
	if (errno != 0 || end == argv[2] || *end != '\0') {
		fprintf(stderr, "invalid sector count: %s\n", argv[2]);
		return 1;
	}
	if (sectors == 0 || sectors > MAX_SECTORS) {
		fprintf(stderr, "sector count must be 1..%d\n", MAX_SECTORS);
		return 1;
	}

	if (snprintf(bin_name, sizeof(bin_name), "%s.bin", base) >= (int)sizeof(bin_name) ||
	    snprintf(cue_name, sizeof(cue_name), "%s.cue", base) >= (int)sizeof(cue_name)) {
		fprintf(stderr, "basename too long\n");
		return 1;
	}

	if (write_bin(bin_name, sectors) != 0)
		return 1;
	if (write_cue(cue_name, bin_name) != 0)
		return 1;

	printf("%s: %lu sectors, %lu bytes\n", bin_name, sectors,
	       sectors * SECTOR_SIZE);
	return 0;
}
