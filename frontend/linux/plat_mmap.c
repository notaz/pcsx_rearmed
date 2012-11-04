/*
 * (C) Gražvydas "notaz" Ignotas, 2008-2010
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#define _GNU_SOURCE 1
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

// this is some dupe code to avoid libpicofe dep

//#include "../libpicofe/plat.h"

/* XXX: maybe unhardcode pagesize? */
#define HUGETLB_PAGESIZE (2 * 1024 * 1024)
#define HUGETLB_THRESHOLD (HUGETLB_PAGESIZE / 2)
#ifndef MAP_HUGETLB
#define MAP_HUGETLB 0x40000 /* arch specific */
#endif

void *plat_mmap(unsigned long addr, size_t size, int need_exec, int is_fixed)
{
	static int hugetlb_disabled;
	int prot = PROT_READ | PROT_WRITE;
	int flags = MAP_PRIVATE | MAP_ANONYMOUS;
	void *req, *ret;

	req = (void *)addr;
	if (need_exec)
		prot |= PROT_EXEC;
	if (is_fixed)
		flags |= MAP_FIXED;
	if (size >= HUGETLB_THRESHOLD && !hugetlb_disabled)
		flags |= MAP_HUGETLB;

	ret = mmap(req, size, prot, flags, -1, 0);
	if (ret == MAP_FAILED && (flags & MAP_HUGETLB)) {
		fprintf(stderr,
			"warning: failed to do hugetlb mmap (%p, %zu): %d\n",
			req, size, errno);
		hugetlb_disabled = 1;
		flags &= ~MAP_HUGETLB;
		ret = mmap(req, size, prot, flags, -1, 0);
	}
	if (ret == MAP_FAILED)
		return NULL;

	if (req != NULL && ret != req)
		fprintf(stderr,
			"warning: mmaped to %p, requested %p\n", ret, req);

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
	int ret;

	ret = munmap(ptr, size);
	if (ret != 0 && (size & (HUGETLB_PAGESIZE - 1))) {
		// prehaps an autorounded hugetlb mapping?
		size = (size + HUGETLB_PAGESIZE - 1) & ~(HUGETLB_PAGESIZE - 1);
		ret = munmap(ptr, size);
	}
	if (ret != 0) {
		fprintf(stderr,
			"munmap(%p, %zu) failed: %d\n", ptr, size, errno);
	}
}
