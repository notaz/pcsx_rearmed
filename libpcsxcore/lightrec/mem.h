/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2022 Paul Cercueil <paul@crapouillou.net>
 */

#ifndef __LIGHTREC_MEM_H__
#define __LIGHTREC_MEM_H__

int lightrec_init_mmap(void);
void lightrec_free_mmap(void);

#endif /* __LIGHTREC_MEM_H__ */
