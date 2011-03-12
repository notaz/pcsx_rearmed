#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define CD_FRAMESIZE_RAW 2352

struct ztab_entry {
	unsigned int offset;
	unsigned short size;
} __attribute__((packed));

int main(int argc, char *argv[])
{
	unsigned char outbuf[CD_FRAMESIZE_RAW * 2];
	unsigned char inbuf[CD_FRAMESIZE_RAW];
	struct ztab_entry *ztable;
	char *out_basename, *out_fname, *out_tfname;
	FILE *fin, *fout;
	long in_bytes, out_bytes;
	long s, total_sectors;
	int ret, len;

	if (argc < 2) {
		fprintf(stderr, "usage:\n%s <cd_img> [out_basename]\n", argv[0]);
		return 1;
	}

	fin = fopen(argv[1], "rb");
	if (fin == NULL) {
		fprintf(stderr, "fopen %s: ", argv[1]);
		perror(NULL);
		return 1;
	}

	if (argv[2] != NULL)
		out_basename = argv[2];
	else
		out_basename = argv[1];

	len = strlen(out_basename) + 3;
	out_fname = malloc(len);
	if (out_fname == NULL) {
		fprintf(stderr, "OOM\n");
		return 1;
	}
	snprintf(out_fname, len, "%s.Z", out_basename);

	fout = fopen(out_fname, "wb");
	if (fout == NULL) {
		fprintf(stderr, "fopen %s: ", out_fname);
		perror(NULL);
		return 1;
	}

	if (fseek(fin, 0, SEEK_END) != 0) {
		fprintf(stderr, "fseek failed: ");
		perror(NULL);
		return 1;
	}

	in_bytes = ftell(fin);
	if (in_bytes % CD_FRAMESIZE_RAW) {
		fprintf(stderr, "warning: input size %ld is not "
				"multiple of sector size\n", in_bytes);
	}
	total_sectors = in_bytes / CD_FRAMESIZE_RAW;
	fseek(fin, 0, SEEK_SET);

	ztable = calloc(total_sectors, sizeof(ztable[0]));
	if (ztable == NULL) {
		fprintf(stderr, "OOM\n");
		return 1;
	}

	out_bytes = 0;
	for (s = 0; s < total_sectors; s++) {
		uLongf dest_len = sizeof(outbuf);

		ret = fread(inbuf, 1, sizeof(inbuf), fin);
		if (ret != sizeof(inbuf)) {
			printf("\n");
			fprintf(stderr, "fread returned %d\n", ret);
			return 1;
		}

		ret = compress2(outbuf, &dest_len, inbuf, sizeof(inbuf), 9);
		if (ret != Z_OK) {
			printf("\n");
			fprintf(stderr, "compress2 failed: %d\n", ret);
			return 1;
		}

		ret = fwrite(outbuf, 1, dest_len, fout);
		if (ret != dest_len) {
			printf("\n");
			fprintf(stderr, "fwrite returned %d\n", ret);
			return 1;
		}

		ztable[s].offset = out_bytes;
		ztable[s].size = dest_len;
		out_bytes += dest_len;

		// print progress
		if ((s & 0x1ff) == 0) {
			printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
			printf("%3ld%% %ld/%ld", s * 100 / total_sectors, s, total_sectors);
			fflush(stdout);
		}
	}

	fclose(fin);
	fclose(fout);

	// write .table
	len = strlen(out_fname) + 7;
	out_tfname = malloc(len);
	if (out_tfname == NULL) {
		printf("\n");
		fprintf(stderr, "OOM\n");
		return 1;
	}
	snprintf(out_tfname, len, "%s.table", out_fname);

	fout = fopen(out_tfname, "wb");
	if (fout == NULL) {
		fprintf(stderr, "fopen %s: ", out_tfname);
		perror(NULL);
		return 1;
	}

	ret = fwrite(ztable, sizeof(ztable[0]), total_sectors, fout);
	if (ret != total_sectors) {
		printf("\n");
		fprintf(stderr, "fwrite returned %d\n", ret);
		return 1;
	}
	fclose(fout);

	printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
	printf("%3ld%% %ld/%ld\n", s * 100 / total_sectors, s, total_sectors);
	printf("%ld bytes from %ld (%.1f%%)\n", out_bytes, in_bytes,
		(double)out_bytes * 100.0 / in_bytes);

	return 0;
}
