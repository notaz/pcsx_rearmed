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

#ifndef __OPTIMIZER_H__
#define __OPTIMIZER_H__

#include "disassembler.h"

struct block;

_Bool opcode_reads_register(union code op, u8 reg);
_Bool opcode_writes_register(union code op, u8 reg);
_Bool has_delay_slot(union code op);
_Bool load_in_delay_slot(union code op);

int lightrec_optimize(struct block *block);

#endif /* __OPTIMIZER_H__ */
