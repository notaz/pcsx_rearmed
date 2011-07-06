/*
 * (C) Gražvydas "notaz" Ignotas, 2008-2010
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>

#include "../common/plat.h"


int plat_is_dir(const char *path)
{
	DIR *dir;
	if ((dir = opendir(path))) {
		closedir(dir);
		return 1;
	}
	return 0;
}

int plat_get_root_dir(char *dst, int len)
{
	int j, ret;

	ret = readlink("/proc/self/exe", dst, len - 1);
	if (ret < 0) {
		perror("readlink");
		ret = 0;
	}
	dst[ret] = 0;

	for (j = strlen(dst); j > 0; j--)
		if (dst[j] == '/') {
			dst[++j] = 0;
			break;
		}

	return j;
}

#ifdef __GP2X__
/* Wiz has a borked gettimeofday().. */
#define plat_get_ticks_ms plat_get_ticks_ms_good
#define plat_get_ticks_us plat_get_ticks_us_good
#endif

unsigned int plat_get_ticks_ms(void)
{
	struct timeval tv;
	unsigned int ret;

	gettimeofday(&tv, NULL);

	ret = (unsigned)tv.tv_sec * 1000;
	/* approximate /= 1000 */
	ret += ((unsigned)tv.tv_usec * 4195) >> 22;

	return ret;
}

unsigned int plat_get_ticks_us(void)
{
	struct timeval tv;
	unsigned int ret;

	gettimeofday(&tv, NULL);

	ret = (unsigned)tv.tv_sec * 1000000;
	ret += (unsigned)tv.tv_usec;

	return ret;
}

void plat_sleep_ms(int ms)
{
	usleep(ms * 1000);
}

int plat_wait_event(int *fds_hnds, int count, int timeout_ms)
{
	struct timeval tv, *timeout = NULL;
	int i, ret, fdmax = -1;
	fd_set fdset;

	if (timeout_ms >= 0) {
		tv.tv_sec = timeout_ms / 1000;
		tv.tv_usec = (timeout_ms % 1000) * 1000;
		timeout = &tv;
	}

	FD_ZERO(&fdset);
	for (i = 0; i < count; i++) {
		if (fds_hnds[i] > fdmax) fdmax = fds_hnds[i];
		FD_SET(fds_hnds[i], &fdset);
	}

	ret = select(fdmax + 1, &fdset, NULL, NULL, timeout);
	if (ret == -1)
	{
		perror("plat_wait_event: select failed");
		sleep(1);
		return -1;
	}

	if (ret == 0)
		return -1; /* timeout */

	ret = -1;
	for (i = 0; i < count; i++)
		if (FD_ISSET(fds_hnds[i], &fdset))
			ret = fds_hnds[i];

	return ret;
}

void *plat_mmap(unsigned long addr, size_t size)
{
	void *req, *ret;

	req = (void *)addr;
	ret = mmap(req, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (ret == MAP_FAILED)
		return NULL;
	if (ret != req)
		printf("warning: mmaped to %p, requested %p\n", ret, req);

	return ret;
}

void *plat_mremap(void *ptr, size_t oldsize, size_t newsize)
{
	void *ret;

	ret = mremap(ptr, oldsize, newsize, MREMAP_MAYMOVE);
	if (ret == MAP_FAILED)
		return NULL;
	if (ret != ptr)
		printf("warning: mremap moved: %p -> %p\n", ptr, ret);

	return ret;
}

void plat_munmap(void *ptr, size_t size)
{
	munmap(ptr, size);
}

/* lprintf */
void lprintf(const char *fmt, ...)
{
	va_list vl;

	va_start(vl, fmt);
	vprintf(fmt, vl);
	va_end(vl);
}

