/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2014-2021 Paul Cercueil <paul@crapouillou.net>
 */

#ifndef __OPTIMIZER_H__
#define __OPTIMIZER_H__

#include "disassembler.h"

struct block;
struct opcode;

_Bool opcode_reads_register(union code op, u8 reg);
_Bool opcode_writes_register(union code op, u8 reg);
_Bool has_delay_slot(union code op);
_Bool is_delay_slot(const struct opcode *list, unsigned int offset);
_Bool load_in_delay_slot(union code op);
_Bool opcode_is_io(union code op);
_Bool is_unconditional_jump(union code c);
_Bool is_syscall(union code c);

_Bool should_emulate(const struct opcode *op);

int lightrec_optimize(struct lightrec_state *state, struct block *block);

#endif /* __OPTIMIZER_H__ */
