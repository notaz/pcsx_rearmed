//
// Copyright (c) 2008, Wei Mingzhi. All rights reserved.
//
// Use, redistribution and modification of this code is unrestricted
// as long as this notice is preserved.
//
// This code is provided with ABSOLUTELY NO WARRANTY.
//

#ifndef MMAN_H
#define MMAN_H

#include <windows.h>

#define mmap(start, length, prot, flags, fd, offset) \
	((unsigned char *)VirtualAlloc(NULL, (length), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE))

#define munmap(start, length) do { VirtualFree((start), (length), MEM_RELEASE); } while (0)

#endif
