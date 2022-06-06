/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2022 Paul Cercueil <paul@crapouillou.net>
 */

#ifndef __LIGHTREC_MEM_H__
#define __LIGHTREC_MEM_H__

#ifdef LIGHTREC

#define CODE_BUFFER_SIZE (8 * 1024 * 1024)

extern void *code_buffer;

int lightrec_init_mmap(void);
void lightrec_free_mmap(void);

#else /* if !LIGHTREC */

#define lightrec_init_mmap() -1 /* should not be called */
#define lightrec_free_mmap()

#undef LIGHTREC_CUSTOM_MAP
#define LIGHTREC_CUSTOM_MAP 0

#endif

#endif /* __LIGHTREC_MEM_H__ */
