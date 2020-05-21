/*
 * Copyright (C) 2020 Paul Cercueil <paul@crapouillou.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#ifndef __LIGHTREC_REAPER_H__
#define __LIGHTREC_REAPER_H__

struct lightrec_state;
struct reaper;

typedef void (*reap_func_t)(void *);

struct reaper *lightrec_reaper_init(struct lightrec_state *state);
void lightrec_reaper_destroy(struct reaper *reaper);

int lightrec_reaper_add(struct reaper *reaper, reap_func_t f, void *data);
void lightrec_reaper_reap(struct reaper *reaper);

#endif /* __LIGHTREC_REAPER_H__ */
