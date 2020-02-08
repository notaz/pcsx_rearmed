/*
 * Copyright (C) 2014 Paul Cercueil <paul@crapouillou.net>
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

#ifndef __EMITTER_H__
#define __EMITTER_H__

#include "lightrec.h"

struct block;
struct opcode;

void lightrec_rec_opcode(const struct block *block,
			 const struct opcode *op, u32 pc);
void lightrec_emit_eob(const struct block *block,
		       const struct opcode *op, u32 pc);

#endif /* __EMITTER_H__ */
