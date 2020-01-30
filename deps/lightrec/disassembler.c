/*
 * Copyright (C) 2014-2020 Paul Cercueil <paul@crapouillou.net>
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

#include "config.h"

#if ENABLE_DISASSEMBLER
#include <dis-asm.h>
#endif
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "disassembler.h"
#include "lightrec-private.h"
#include "memmanager.h"

static bool is_unconditional_jump(const struct opcode *op)
{
	switch (op->i.op) {
	case OP_SPECIAL:
		return op->r.op == OP_SPECIAL_JR || op->r.op == OP_SPECIAL_JALR;
	case OP_J:
	case OP_JAL:
		return true;
	case OP_BEQ:
	case OP_BLEZ:
		return op->i.rs == op->i.rt;
	case OP_REGIMM:
		return (op->r.rt == OP_REGIMM_BGEZ ||
			op->r.rt == OP_REGIMM_BGEZAL) && op->i.rs == 0;
	default:
		return false;
	}
}

static bool is_syscall(const struct opcode *op)
{
	return (op->i.op == OP_SPECIAL && (op->r.op == OP_SPECIAL_SYSCALL ||
					   op->r.op == OP_SPECIAL_BREAK)) ||
		(op->i.op == OP_CP0 && (op->r.rs == OP_CP0_MTC0 ||
					op->r.rs == OP_CP0_CTC0) &&
		 (op->r.rd == 12 || op->r.rd == 13));
}

void lightrec_free_opcode_list(struct lightrec_state *state, struct opcode *list)
{
	struct opcode *next;

	while (list) {
		next = list->next;
		lightrec_free(state, MEM_FOR_IR, sizeof(*list), list);
		list = next;
	}
}

struct opcode * lightrec_disassemble(struct lightrec_state *state,
				     const u32 *src, unsigned int *len)
{
	struct opcode *head = NULL;
	bool stop_next = false;
	struct opcode *curr, *last;
	unsigned int i;

	for (i = 0, last = NULL; ; i++, last = curr) {
		curr = lightrec_calloc(state, MEM_FOR_IR, sizeof(*curr));
		if (!curr) {
			pr_err("Unable to allocate memory\n");
			lightrec_free_opcode_list(state, head);
			return NULL;
		}

		if (!last)
			head = curr;
		else
			last->next = curr;

		/* TODO: Take care of endianness */
		curr->opcode = LE32TOH(*src++);
		curr->offset = i;

		/* NOTE: The block disassembly ends after the opcode that
		 * follows an unconditional jump (delay slot) */
		if (stop_next || is_syscall(curr))
			break;
		else if (is_unconditional_jump(curr))
			stop_next = true;
	}

	if (len)
		*len = (i + 1) * sizeof(u32);

	return head;
}

unsigned int lightrec_cycles_of_opcode(union code code)
{
	switch (code.i.op) {
	case OP_META_REG_UNLOAD:
	case OP_META_SYNC:
		return 0;
	default:
		return 2;
	}
}

#if ENABLE_DISASSEMBLER
void lightrec_print_disassembly(const struct block *block,
				const u32 *code, unsigned int length)
{
	struct disassemble_info info;
	unsigned int i;

	memset(&info, 0, sizeof(info));
	init_disassemble_info(&info, stdout, (fprintf_ftype) fprintf);

	info.buffer = (bfd_byte *) code;
	info.buffer_vma = (bfd_vma)(uintptr_t) code;
	info.buffer_length = length;
	info.flavour = bfd_target_unknown_flavour;
	info.arch = bfd_arch_mips;
	info.mach = bfd_mach_mips3000;
	disassemble_init_for_target(&info);

	for (i = 0; i < length; i += 4) {
		void print_insn_little_mips(bfd_vma, struct disassemble_info *);
		putc('\t', stdout);
		print_insn_little_mips((bfd_vma)(uintptr_t) code++, &info);
		putc('\n', stdout);
	}
}
#endif
