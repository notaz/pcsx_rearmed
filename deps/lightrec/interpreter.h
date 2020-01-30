/*
 * Copyright (C) 2019 Paul Cercueil <paul@crapouillou.net>
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

#ifndef __LIGHTREC_INTERPRETER_H__
#define __LIGHTREC_INTERPRETER_H__

#include "lightrec.h"

struct block;

u32 lightrec_emulate_block(struct block *block, u32 pc);

#endif /* __LIGHTREC_INTERPRETER_H__ */
