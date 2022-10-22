// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Copyright (C) 2014-2021 Paul Cercueil <paul@crapouillou.net>
 */

#include "lightrec-config.h"
#include "disassembler.h"
#include "lightrec.h"
#include "memmanager.h"
#include "optimizer.h"
#include "regcache.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define IF_OPT(opt, ptr) ((opt) ? (ptr) : NULL)

struct optimizer_list {
	void (**optimizers)(struct opcode *);
	unsigned int nb_optimizers;
};

static bool is_nop(union code op);

bool is_unconditional_jump(union code c)
{
	switch (c.i.op) {
	case OP_SPECIAL:
		return c.r.op == OP_SPECIAL_JR || c.r.op == OP_SPECIAL_JALR;
	case OP_J:
	case OP_JAL:
		return true;
	case OP_BEQ:
	case OP_BLEZ:
		return c.i.rs == c.i.rt;
	case OP_REGIMM:
		return (c.r.rt == OP_REGIMM_BGEZ ||
			c.r.rt == OP_REGIMM_BGEZAL) && c.i.rs == 0;
	default:
		return false;
	}
}

bool is_syscall(union code c)
{
	return (c.i.op == OP_SPECIAL && c.r.op == OP_SPECIAL_SYSCALL) ||
		(c.i.op == OP_CP0 && (c.r.rs == OP_CP0_MTC0 ||
					c.r.rs == OP_CP0_CTC0) &&
		 (c.r.rd == 12 || c.r.rd == 13));
}

static u64 opcode_read_mask(union code op)
{
	switch (op.i.op) {
	case OP_SPECIAL:
		switch (op.r.op) {
		case OP_SPECIAL_SYSCALL:
		case OP_SPECIAL_BREAK:
			return 0;
		case OP_SPECIAL_JR:
		case OP_SPECIAL_JALR:
		case OP_SPECIAL_MTHI:
		case OP_SPECIAL_MTLO:
			return BIT(op.r.rs);
		case OP_SPECIAL_MFHI:
			return BIT(REG_HI);
		case OP_SPECIAL_MFLO:
			return BIT(REG_LO);
		case OP_SPECIAL_SLL:
			if (!op.r.imm)
				return 0;
			fallthrough;
		case OP_SPECIAL_SRL:
		case OP_SPECIAL_SRA:
			return BIT(op.r.rt);
		default:
			return BIT(op.r.rs) | BIT(op.r.rt);
		}
	case OP_CP0:
		switch (op.r.rs) {
		case OP_CP0_MTC0:
		case OP_CP0_CTC0:
			return BIT(op.r.rt);
		default:
			return 0;
		}
	case OP_CP2:
		if (op.r.op == OP_CP2_BASIC) {
			switch (op.r.rs) {
			case OP_CP2_BASIC_MTC2:
			case OP_CP2_BASIC_CTC2:
				return BIT(op.r.rt);
			default:
				break;
			}
		}
		return 0;
	case OP_J:
	case OP_JAL:
	case OP_LUI:
		return 0;
	case OP_BEQ:
		if (op.i.rs == op.i.rt)
			return 0;
		fallthrough;
	case OP_BNE:
	case OP_LWL:
	case OP_LWR:
	case OP_SB:
	case OP_SH:
	case OP_SWL:
	case OP_SW:
	case OP_SWR:
		return BIT(op.i.rs) | BIT(op.i.rt);
	default:
		return BIT(op.i.rs);
	}
}

static u64 mult_div_write_mask(union code op)
{
	u64 flags;

	if (!OPT_FLAG_MULT_DIV)
		return BIT(REG_LO) | BIT(REG_HI);

	if (op.r.rd)
		flags = BIT(op.r.rd);
	else
		flags = BIT(REG_LO);
	if (op.r.imm)
		flags |= BIT(op.r.imm);
	else
		flags |= BIT(REG_HI);

	return flags;
}

static u64 opcode_write_mask(union code op)
{
	switch (op.i.op) {
	case OP_META_MULT2:
	case OP_META_MULTU2:
		return mult_div_write_mask(op);
	case OP_SPECIAL:
		switch (op.r.op) {
		case OP_SPECIAL_JR:
		case OP_SPECIAL_SYSCALL:
		case OP_SPECIAL_BREAK:
			return 0;
		case OP_SPECIAL_MULT:
		case OP_SPECIAL_MULTU:
		case OP_SPECIAL_DIV:
		case OP_SPECIAL_DIVU:
			return mult_div_write_mask(op);
		case OP_SPECIAL_MTHI:
			return BIT(REG_HI);
		case OP_SPECIAL_MTLO:
			return BIT(REG_LO);
		case OP_SPECIAL_SLL:
			if (!op.r.imm)
				return 0;
			fallthrough;
		default:
			return BIT(op.r.rd);
		}
	case OP_ADDI:
	case OP_ADDIU:
	case OP_SLTI:
	case OP_SLTIU:
	case OP_ANDI:
	case OP_ORI:
	case OP_XORI:
	case OP_LUI:
	case OP_LB:
	case OP_LH:
	case OP_LWL:
	case OP_LW:
	case OP_LBU:
	case OP_LHU:
	case OP_LWR:
	case OP_META_EXTC:
	case OP_META_EXTS:
		return BIT(op.i.rt);
	case OP_JAL:
		return BIT(31);
	case OP_CP0:
		switch (op.r.rs) {
		case OP_CP0_MFC0:
		case OP_CP0_CFC0:
			return BIT(op.i.rt);
		default:
			return 0;
		}
	case OP_CP2:
		if (op.r.op == OP_CP2_BASIC) {
			switch (op.r.rs) {
			case OP_CP2_BASIC_MFC2:
			case OP_CP2_BASIC_CFC2:
				return BIT(op.i.rt);
			default:
				break;
			}
		}
		return 0;
	case OP_REGIMM:
		switch (op.r.rt) {
		case OP_REGIMM_BLTZAL:
		case OP_REGIMM_BGEZAL:
			return BIT(31);
		default:
			return 0;
		}
	case OP_META_MOV:
		return BIT(op.r.rd);
	default:
		return 0;
	}
}

bool opcode_reads_register(union code op, u8 reg)
{
	return opcode_read_mask(op) & BIT(reg);
}

bool opcode_writes_register(union code op, u8 reg)
{
	return opcode_write_mask(op) & BIT(reg);
}

static int find_prev_writer(const struct opcode *list, unsigned int offset, u8 reg)
{
	union code c;
	unsigned int i;

	if (op_flag_sync(list[offset].flags))
		return -1;

	for (i = offset; i > 0; i--) {
		c = list[i - 1].c;

		if (opcode_writes_register(c, reg)) {
			if (i > 1 && has_delay_slot(list[i - 2].c))
				break;

			return i - 1;
		}

		if (op_flag_sync(list[i - 1].flags) ||
		    has_delay_slot(c) ||
		    opcode_reads_register(c, reg))
			break;
	}

	return -1;
}

static int find_next_reader(const struct opcode *list, unsigned int offset, u8 reg)
{
	unsigned int i;
	union code c;

	if (op_flag_sync(list[offset].flags))
		return -1;

	for (i = offset; ; i++) {
		c = list[i].c;

		if (opcode_reads_register(c, reg)) {
			if (i > 0 && has_delay_slot(list[i - 1].c))
				break;

			return i;
		}

		if (op_flag_sync(list[i].flags) ||
		    has_delay_slot(c) || opcode_writes_register(c, reg))
			break;
	}

	return -1;
}

static bool reg_is_dead(const struct opcode *list, unsigned int offset, u8 reg)
{
	unsigned int i;

	if (op_flag_sync(list[offset].flags))
		return false;

	for (i = offset + 1; ; i++) {
		if (opcode_reads_register(list[i].c, reg))
			return false;

		if (opcode_writes_register(list[i].c, reg))
			return true;

		if (has_delay_slot(list[i].c)) {
			if (op_flag_no_ds(list[i].flags) ||
			    opcode_reads_register(list[i + 1].c, reg))
				return false;

			return opcode_writes_register(list[i + 1].c, reg);
		}
	}
}

static bool reg_is_read(const struct opcode *list,
			unsigned int a, unsigned int b, u8 reg)
{
	/* Return true if reg is read in one of the opcodes of the interval
	 * [a, b[ */
	for (; a < b; a++) {
		if (!is_nop(list[a].c) && opcode_reads_register(list[a].c, reg))
			return true;
	}

	return false;
}

static bool reg_is_written(const struct opcode *list,
			   unsigned int a, unsigned int b, u8 reg)
{
	/* Return true if reg is written in one of the opcodes of the interval
	 * [a, b[ */

	for (; a < b; a++) {
		if (!is_nop(list[a].c) && opcode_writes_register(list[a].c, reg))
			return true;
	}

	return false;
}

static bool reg_is_read_or_written(const struct opcode *list,
				   unsigned int a, unsigned int b, u8 reg)
{
	return reg_is_read(list, a, b, reg) || reg_is_written(list, a, b, reg);
}

static bool opcode_is_load(union code op)
{
	switch (op.i.op) {
	case OP_LB:
	case OP_LH:
	case OP_LWL:
	case OP_LW:
	case OP_LBU:
	case OP_LHU:
	case OP_LWR:
	case OP_LWC2:
		return true;
	default:
		return false;
	}
}

static bool opcode_is_store(union code op)
{
	switch (op.i.op) {
	case OP_SB:
	case OP_SH:
	case OP_SW:
	case OP_SWL:
	case OP_SWR:
	case OP_SWC2:
		return true;
	default:
		return false;
	}
}

static u8 opcode_get_io_size(union code op)
{
	switch (op.i.op) {
	case OP_LB:
	case OP_LBU:
	case OP_SB:
		return 8;
	case OP_LH:
	case OP_LHU:
	case OP_SH:
		return 16;
	default:
		return 32;
	}
}

bool opcode_is_io(union code op)
{
	return opcode_is_load(op) || opcode_is_store(op);
}

/* TODO: Complete */
static bool is_nop(union code op)
{
	if (opcode_writes_register(op, 0)) {
		switch (op.i.op) {
		case OP_CP0:
			return op.r.rs != OP_CP0_MFC0;
		case OP_LB:
		case OP_LH:
		case OP_LWL:
		case OP_LW:
		case OP_LBU:
		case OP_LHU:
		case OP_LWR:
			return false;
		default:
			return true;
		}
	}

	switch (op.i.op) {
	case OP_SPECIAL:
		switch (op.r.op) {
		case OP_SPECIAL_AND:
			return op.r.rd == op.r.rt && op.r.rd == op.r.rs;
		case OP_SPECIAL_ADD:
		case OP_SPECIAL_ADDU:
			return (op.r.rd == op.r.rt && op.r.rs == 0) ||
				(op.r.rd == op.r.rs && op.r.rt == 0);
		case OP_SPECIAL_SUB:
		case OP_SPECIAL_SUBU:
			return op.r.rd == op.r.rs && op.r.rt == 0;
		case OP_SPECIAL_OR:
			if (op.r.rd == op.r.rt)
				return op.r.rd == op.r.rs || op.r.rs == 0;
			else
				return (op.r.rd == op.r.rs) && op.r.rt == 0;
		case OP_SPECIAL_SLL:
		case OP_SPECIAL_SRA:
		case OP_SPECIAL_SRL:
			return op.r.rd == op.r.rt && op.r.imm == 0;
		case OP_SPECIAL_MFHI:
		case OP_SPECIAL_MFLO:
			return op.r.rd == 0;
		default:
			return false;
		}
	case OP_ORI:
	case OP_ADDI:
	case OP_ADDIU:
		return op.i.rt == op.i.rs && op.i.imm == 0;
	case OP_BGTZ:
		return (op.i.rs == 0 || op.i.imm == 1);
	case OP_REGIMM:
		return (op.i.op == OP_REGIMM_BLTZ ||
				op.i.op == OP_REGIMM_BLTZAL) &&
			(op.i.rs == 0 || op.i.imm == 1);
	case OP_BNE:
		return (op.i.rs == op.i.rt || op.i.imm == 1);
	default:
		return false;
	}
}

bool load_in_delay_slot(union code op)
{
	switch (op.i.op) {
	case OP_CP0:
		switch (op.r.rs) {
		case OP_CP0_MFC0:
		case OP_CP0_CFC0:
			return true;
		default:
			break;
		}

		break;
	case OP_CP2:
		if (op.r.op == OP_CP2_BASIC) {
			switch (op.r.rs) {
			case OP_CP2_BASIC_MFC2:
			case OP_CP2_BASIC_CFC2:
				return true;
			default:
				break;
			}
		}

		break;
	case OP_LB:
	case OP_LH:
	case OP_LW:
	case OP_LWL:
	case OP_LWR:
	case OP_LBU:
	case OP_LHU:
		return true;
	default:
		break;
	}

	return false;
}

static u32 lightrec_propagate_consts(const struct opcode *op,
				     const struct opcode *prev,
				     u32 known, u32 *v)
{
	union code c = prev->c;

	/* Register $zero is always, well, zero */
	known |= BIT(0);
	v[0] = 0;

	if (op_flag_sync(op->flags))
		return BIT(0);

	switch (c.i.op) {
	case OP_SPECIAL:
		switch (c.r.op) {
		case OP_SPECIAL_SLL:
			if (known & BIT(c.r.rt)) {
				known |= BIT(c.r.rd);
				v[c.r.rd] = v[c.r.rt] << c.r.imm;
			} else {
				known &= ~BIT(c.r.rd);
			}
			break;
		case OP_SPECIAL_SRL:
			if (known & BIT(c.r.rt)) {
				known |= BIT(c.r.rd);
				v[c.r.rd] = v[c.r.rt] >> c.r.imm;
			} else {
				known &= ~BIT(c.r.rd);
			}
			break;
		case OP_SPECIAL_SRA:
			if (known & BIT(c.r.rt)) {
				known |= BIT(c.r.rd);
				v[c.r.rd] = (s32)v[c.r.rt] >> c.r.imm;
			} else {
				known &= ~BIT(c.r.rd);
			}
			break;
		case OP_SPECIAL_SLLV:
			if (known & BIT(c.r.rt) && known & BIT(c.r.rs)) {
				known |= BIT(c.r.rd);
				v[c.r.rd] = v[c.r.rt] << (v[c.r.rs] & 0x1f);
			} else {
				known &= ~BIT(c.r.rd);
			}
			break;
		case OP_SPECIAL_SRLV:
			if (known & BIT(c.r.rt) && known & BIT(c.r.rs)) {
				known |= BIT(c.r.rd);
				v[c.r.rd] = v[c.r.rt] >> (v[c.r.rs] & 0x1f);
			} else {
				known &= ~BIT(c.r.rd);
			}
			break;
		case OP_SPECIAL_SRAV:
			if (known & BIT(c.r.rt) && known & BIT(c.r.rs)) {
				known |= BIT(c.r.rd);
				v[c.r.rd] = (s32)v[c.r.rt]
					  >> (v[c.r.rs] & 0x1f);
			} else {
				known &= ~BIT(c.r.rd);
			}
			break;
		case OP_SPECIAL_ADD:
		case OP_SPECIAL_ADDU:
			if (known & BIT(c.r.rt) && known & BIT(c.r.rs)) {
				known |= BIT(c.r.rd);
				v[c.r.rd] = (s32)v[c.r.rt] + (s32)v[c.r.rs];
			} else {
				known &= ~BIT(c.r.rd);
			}
			break;
		case OP_SPECIAL_SUB:
		case OP_SPECIAL_SUBU:
			if (known & BIT(c.r.rt) && known & BIT(c.r.rs)) {
				known |= BIT(c.r.rd);
				v[c.r.rd] = v[c.r.rt] - v[c.r.rs];
			} else {
				known &= ~BIT(c.r.rd);
			}
			break;
		case OP_SPECIAL_AND:
			if (known & BIT(c.r.rt) && known & BIT(c.r.rs)) {
				known |= BIT(c.r.rd);
				v[c.r.rd] = v[c.r.rt] & v[c.r.rs];
			} else {
				known &= ~BIT(c.r.rd);
			}
			break;
		case OP_SPECIAL_OR:
			if (known & BIT(c.r.rt) && known & BIT(c.r.rs)) {
				known |= BIT(c.r.rd);
				v[c.r.rd] = v[c.r.rt] | v[c.r.rs];
			} else {
				known &= ~BIT(c.r.rd);
			}
			break;
		case OP_SPECIAL_XOR:
			if (known & BIT(c.r.rt) && known & BIT(c.r.rs)) {
				known |= BIT(c.r.rd);
				v[c.r.rd] = v[c.r.rt] ^ v[c.r.rs];
			} else {
				known &= ~BIT(c.r.rd);
			}
			break;
		case OP_SPECIAL_NOR:
			if (known & BIT(c.r.rt) && known & BIT(c.r.rs)) {
				known |= BIT(c.r.rd);
				v[c.r.rd] = ~(v[c.r.rt] | v[c.r.rs]);
			} else {
				known &= ~BIT(c.r.rd);
			}
			break;
		case OP_SPECIAL_SLT:
			if (known & BIT(c.r.rt) && known & BIT(c.r.rs)) {
				known |= BIT(c.r.rd);
				v[c.r.rd] = (s32)v[c.r.rs] < (s32)v[c.r.rt];
			} else {
				known &= ~BIT(c.r.rd);
			}
			break;
		case OP_SPECIAL_SLTU:
			if (known & BIT(c.r.rt) && known & BIT(c.r.rs)) {
				known |= BIT(c.r.rd);
				v[c.r.rd] = v[c.r.rs] < v[c.r.rt];
			} else {
				known &= ~BIT(c.r.rd);
			}
			break;
		case OP_SPECIAL_MULT:
		case OP_SPECIAL_MULTU:
		case OP_SPECIAL_DIV:
		case OP_SPECIAL_DIVU:
			if (OPT_FLAG_MULT_DIV && c.r.rd)
				known &= ~BIT(c.r.rd);
			if (OPT_FLAG_MULT_DIV && c.r.imm)
				known &= ~BIT(c.r.imm);
			break;
		case OP_SPECIAL_MFLO:
		case OP_SPECIAL_MFHI:
			known &= ~BIT(c.r.rd);
			break;
		default:
			break;
		}
		break;
	case OP_META_MULT2:
	case OP_META_MULTU2:
		if (OPT_FLAG_MULT_DIV && (known & BIT(c.r.rs))) {
			if (c.r.rd) {
				known |= BIT(c.r.rd);

				if (c.r.op < 32)
					v[c.r.rd] = v[c.r.rs] << c.r.op;
				else
					v[c.r.rd] = 0;
			}

			if (c.r.imm) {
				known |= BIT(c.r.imm);

				if (c.r.op >= 32)
					v[c.r.imm] = v[c.r.rs] << (c.r.op - 32);
				else if (c.i.op == OP_META_MULT2)
					v[c.r.imm] = (s32) v[c.r.rs] >> (32 - c.r.op);
				else
					v[c.r.imm] = v[c.r.rs] >> (32 - c.r.op);
			}
		} else {
			if (OPT_FLAG_MULT_DIV && c.r.rd)
				known &= ~BIT(c.r.rd);
			if (OPT_FLAG_MULT_DIV && c.r.imm)
				known &= ~BIT(c.r.imm);
		}
		break;
	case OP_REGIMM:
		break;
	case OP_ADDI:
	case OP_ADDIU:
		if (known & BIT(c.i.rs)) {
			known |= BIT(c.i.rt);
			v[c.i.rt] = v[c.i.rs] + (s32)(s16)c.i.imm;
		} else {
			known &= ~BIT(c.i.rt);
		}
		break;
	case OP_SLTI:
		if (known & BIT(c.i.rs)) {
			known |= BIT(c.i.rt);
			v[c.i.rt] = (s32)v[c.i.rs] < (s32)(s16)c.i.imm;
		} else {
			known &= ~BIT(c.i.rt);
		}
		break;
	case OP_SLTIU:
		if (known & BIT(c.i.rs)) {
			known |= BIT(c.i.rt);
			v[c.i.rt] = v[c.i.rs] < (u32)(s32)(s16)c.i.imm;
		} else {
			known &= ~BIT(c.i.rt);
		}
		break;
	case OP_ANDI:
		if (known & BIT(c.i.rs)) {
			known |= BIT(c.i.rt);
			v[c.i.rt] = v[c.i.rs] & c.i.imm;
		} else {
			known &= ~BIT(c.i.rt);
		}
		break;
	case OP_ORI:
		if (known & BIT(c.i.rs)) {
			known |= BIT(c.i.rt);
			v[c.i.rt] = v[c.i.rs] | c.i.imm;
		} else {
			known &= ~BIT(c.i.rt);
		}
		break;
	case OP_XORI:
		if (known & BIT(c.i.rs)) {
			known |= BIT(c.i.rt);
			v[c.i.rt] = v[c.i.rs] ^ c.i.imm;
		} else {
			known &= ~BIT(c.i.rt);
		}
		break;
	case OP_LUI:
		known |= BIT(c.i.rt);
		v[c.i.rt] = c.i.imm << 16;
		break;
	case OP_CP0:
		switch (c.r.rs) {
		case OP_CP0_MFC0:
		case OP_CP0_CFC0:
			known &= ~BIT(c.r.rt);
			break;
		}
		break;
	case OP_CP2:
		if (c.r.op == OP_CP2_BASIC) {
			switch (c.r.rs) {
			case OP_CP2_BASIC_MFC2:
			case OP_CP2_BASIC_CFC2:
				known &= ~BIT(c.r.rt);
				break;
			}
		}
		break;
	case OP_LB:
	case OP_LH:
	case OP_LWL:
	case OP_LW:
	case OP_LBU:
	case OP_LHU:
	case OP_LWR:
	case OP_LWC2:
		known &= ~BIT(c.i.rt);
		break;
	case OP_META_MOV:
		if (known & BIT(c.r.rs)) {
			known |= BIT(c.r.rd);
			v[c.r.rd] = v[c.r.rs];
		} else {
			known &= ~BIT(c.r.rd);
		}
		break;
	case OP_META_EXTC:
		if (known & BIT(c.i.rs)) {
			known |= BIT(c.i.rt);
			v[c.i.rt] = (s32)(s8)v[c.i.rs];
		} else {
			known &= ~BIT(c.i.rt);
		}
		break;
	case OP_META_EXTS:
		if (known & BIT(c.i.rs)) {
			known |= BIT(c.i.rt);
			v[c.i.rt] = (s32)(s16)v[c.i.rs];
		} else {
			known &= ~BIT(c.i.rt);
		}
		break;
	default:
		break;
	}

	return known;
}

static void lightrec_optimize_sll_sra(struct opcode *list, unsigned int offset)
{
	struct opcode *prev, *prev2 = NULL, *curr = &list[offset];
	struct opcode *to_change, *to_nop;
	int idx, idx2;

	if (curr->r.imm != 24 && curr->r.imm != 16)
		return;

	idx = find_prev_writer(list, offset, curr->r.rt);
	if (idx < 0)
		return;

	prev = &list[idx];

	if (prev->i.op != OP_SPECIAL || prev->r.op != OP_SPECIAL_SLL ||
	    prev->r.imm != curr->r.imm || prev->r.rd != curr->r.rt)
		return;

	if (prev->r.rd != prev->r.rt && curr->r.rd != curr->r.rt) {
		/* sll rY, rX, 16
		 * ...
		 * srl rZ, rY, 16 */

		if (!reg_is_dead(list, offset, curr->r.rt) ||
		    reg_is_read_or_written(list, idx, offset, curr->r.rd))
			return;

		/* If rY is dead after the SRL, and rZ is not used after the SLL,
		 * we can change rY to rZ */

		pr_debug("Detected SLL/SRA with middle temp register\n");
		prev->r.rd = curr->r.rd;
		curr->r.rt = prev->r.rd;
	}

	/* We got a SLL/SRA combo. If imm #16, that's a cast to u16.
	 * If imm #24 that's a cast to u8.
	 *
	 * First of all, make sure that the target register of the SLL is not
	 * read before the SRA. */

	if (prev->r.rd == prev->r.rt) {
		/* sll rX, rX, 16
		 * ...
		 * srl rY, rX, 16 */
		to_change = curr;
		to_nop = prev;

		/* rX is used after the SRA - we cannot convert it. */
		if (prev->r.rd != curr->r.rd && !reg_is_dead(list, offset, prev->r.rd))
			return;
	} else {
		/* sll rY, rX, 16
		 * ...
		 * srl rY, rY, 16 */
		to_change = prev;
		to_nop = curr;
	}

	idx2 = find_prev_writer(list, idx, prev->r.rt);
	if (idx2 >= 0) {
		/* Note that PSX games sometimes do casts after
		 * a LHU or LBU; in this case we can change the
		 * load opcode to a LH or LB, and the cast can
		 * be changed to a MOV or a simple NOP. */

		prev2 = &list[idx2];

		if (curr->r.rd != prev2->i.rt &&
		    !reg_is_dead(list, offset, prev2->i.rt))
			prev2 = NULL;
		else if (curr->r.imm == 16 && prev2->i.op == OP_LHU)
			prev2->i.op = OP_LH;
		else if (curr->r.imm == 24 && prev2->i.op == OP_LBU)
			prev2->i.op = OP_LB;
		else
			prev2 = NULL;

		if (prev2) {
			if (curr->r.rd == prev2->i.rt) {
				to_change->opcode = 0;
			} else if (reg_is_dead(list, offset, prev2->i.rt) &&
				   !reg_is_read_or_written(list, idx2 + 1, offset, curr->r.rd)) {
				/* The target register of the SRA is dead after the
				 * LBU/LHU; we can change the target register of the
				 * LBU/LHU to the one of the SRA. */
				prev2->i.rt = curr->r.rd;
				to_change->opcode = 0;
			} else {
				to_change->i.op = OP_META_MOV;
				to_change->r.rd = curr->r.rd;
				to_change->r.rs = prev2->i.rt;
			}

			if (to_nop->r.imm == 24)
				pr_debug("Convert LBU+SLL+SRA to LB\n");
			else
				pr_debug("Convert LHU+SLL+SRA to LH\n");
		}
	}

	if (!prev2) {
		pr_debug("Convert SLL/SRA #%u to EXT%c\n",
			 prev->r.imm,
			 prev->r.imm == 24 ? 'C' : 'S');

		if (to_change == prev) {
			to_change->i.rs = prev->r.rt;
			to_change->i.rt = curr->r.rd;
		} else {
			to_change->i.rt = curr->r.rd;
			to_change->i.rs = prev->r.rt;
		}

		if (to_nop->r.imm == 24)
			to_change->i.op = OP_META_EXTC;
		else
			to_change->i.op = OP_META_EXTS;
	}

	to_nop->opcode = 0;
}

static void lightrec_remove_useless_lui(struct block *block, unsigned int offset,
					u32 known, u32 *values)
{
	struct opcode *list = block->opcode_list,
		      *op = &block->opcode_list[offset];
	int reader;

	if (!op_flag_sync(op->flags) && (known & BIT(op->i.rt)) &&
	    values[op->i.rt] == op->i.imm << 16) {
		pr_debug("Converting duplicated LUI to NOP\n");
		op->opcode = 0x0;
		return;
	}

	if (op->i.imm != 0 || op->i.rt == 0)
		return;

	reader = find_next_reader(list, offset + 1, op->i.rt);
	if (reader <= 0)
		return;

	if (opcode_writes_register(list[reader].c, op->i.rt) ||
	    reg_is_dead(list, reader, op->i.rt)) {
		pr_debug("Removing useless LUI 0x0\n");

		if (list[reader].i.rs == op->i.rt)
			list[reader].i.rs = 0;
		if (list[reader].i.op == OP_SPECIAL &&
		    list[reader].i.rt == op->i.rt)
			list[reader].i.rt = 0;
		op->opcode = 0x0;
	}
}

static void lightrec_modify_lui(struct block *block, unsigned int offset)
{
	union code c, *lui = &block->opcode_list[offset].c;
	bool stop = false, stop_next = false;
	unsigned int i;

	for (i = offset + 1; !stop && i < block->nb_ops; i++) {
		c = block->opcode_list[i].c;
		stop = stop_next;

		if ((opcode_is_store(c) && c.i.rt == lui->i.rt)
		    || (!opcode_is_load(c) && opcode_reads_register(c, lui->i.rt)))
			break;

		if (opcode_writes_register(c, lui->i.rt)) {
			pr_debug("Convert LUI at offset 0x%x to kuseg\n",
				 i - 1 << 2);
			lui->i.imm = kunseg(lui->i.imm << 16) >> 16;
			break;
		}

		if (has_delay_slot(c))
			stop_next = true;
	}
}

static int lightrec_transform_branches(struct lightrec_state *state,
				       struct block *block)
{
	struct opcode *op;
	unsigned int i;
	s32 offset;

	for (i = 0; i < block->nb_ops; i++) {
		op = &block->opcode_list[i];

		switch (op->i.op) {
		case OP_J:
			/* Transform J opcode into BEQ $zero, $zero if possible. */
			offset = (s32)((block->pc & 0xf0000000) >> 2 | op->j.imm)
				- (s32)(block->pc >> 2) - (s32)i - 1;

			if (offset == (s16)offset) {
				pr_debug("Transform J into BEQ $zero, $zero\n");
				op->i.op = OP_BEQ;
				op->i.rs = 0;
				op->i.rt = 0;
				op->i.imm = offset;

			}
			fallthrough;
		default:
			break;
		}
	}

	return 0;
}

static inline bool is_power_of_two(u32 value)
{
	return popcount32(value) == 1;
}

static int lightrec_transform_ops(struct lightrec_state *state, struct block *block)
{
	struct opcode *list = block->opcode_list;
	struct opcode *prev, *op = NULL;
	u32 known = BIT(0);
	u32 values[32] = { 0 };
	unsigned int i;
	u8 tmp;

	for (i = 0; i < block->nb_ops; i++) {
		prev = op;
		op = &list[i];

		if (prev)
			known = lightrec_propagate_consts(op, prev, known, values);

		/* Transform all opcodes detected as useless to real NOPs
		 * (0x0: SLL r0, r0, #0) */
		if (op->opcode != 0 && is_nop(op->c)) {
			pr_debug("Converting useless opcode 0x%08x to NOP\n",
					op->opcode);
			op->opcode = 0x0;
		}

		if (!op->opcode)
			continue;

		switch (op->i.op) {
		case OP_BEQ:
			if (op->i.rs == op->i.rt) {
				op->i.rs = 0;
				op->i.rt = 0;
			} else if (op->i.rs == 0) {
				op->i.rs = op->i.rt;
				op->i.rt = 0;
			}
			break;

		case OP_BNE:
			if (op->i.rs == 0) {
				op->i.rs = op->i.rt;
				op->i.rt = 0;
			}
			break;

		case OP_LUI:
			if (!prev || !has_delay_slot(prev->c))
				lightrec_modify_lui(block, i);
			lightrec_remove_useless_lui(block, i, known, values);
			break;

		/* Transform ORI/ADDI/ADDIU with imm #0 or ORR/ADD/ADDU/SUB/SUBU
		 * with register $zero to the MOV meta-opcode */
		case OP_ORI:
		case OP_ADDI:
		case OP_ADDIU:
			if (op->i.imm == 0) {
				pr_debug("Convert ORI/ADDI/ADDIU #0 to MOV\n");
				op->i.op = OP_META_MOV;
				op->r.rd = op->i.rt;
			}
			break;
		case OP_SPECIAL:
			switch (op->r.op) {
			case OP_SPECIAL_SRA:
				if (op->r.imm == 0) {
					pr_debug("Convert SRA #0 to MOV\n");
					op->i.op = OP_META_MOV;
					op->r.rs = op->r.rt;
					break;
				}

				lightrec_optimize_sll_sra(block->opcode_list, i);
				break;
			case OP_SPECIAL_SLL:
			case OP_SPECIAL_SRL:
				if (op->r.imm == 0) {
					pr_debug("Convert SLL/SRL #0 to MOV\n");
					op->i.op = OP_META_MOV;
					op->r.rs = op->r.rt;
				}
				break;
			case OP_SPECIAL_MULT:
			case OP_SPECIAL_MULTU:
				if ((known & BIT(op->r.rs)) &&
				    is_power_of_two(values[op->r.rs])) {
					tmp = op->c.i.rs;
					op->c.i.rs = op->c.i.rt;
					op->c.i.rt = tmp;
				} else if (!(known & BIT(op->r.rt)) ||
					   !is_power_of_two(values[op->r.rt])) {
					break;
				}

				pr_debug("Multiply by power-of-two: %u\n",
					 values[op->r.rt]);

				if (op->r.op == OP_SPECIAL_MULT)
					op->i.op = OP_META_MULT2;
				else
					op->i.op = OP_META_MULTU2;

				op->r.op = ctz32(values[op->r.rt]);
				break;
			case OP_SPECIAL_OR:
			case OP_SPECIAL_ADD:
			case OP_SPECIAL_ADDU:
				if (op->r.rs == 0) {
					pr_debug("Convert OR/ADD $zero to MOV\n");
					op->i.op = OP_META_MOV;
					op->r.rs = op->r.rt;
				}
				fallthrough;
			case OP_SPECIAL_SUB:
			case OP_SPECIAL_SUBU:
				if (op->r.rt == 0) {
					pr_debug("Convert OR/ADD/SUB $zero to MOV\n");
					op->i.op = OP_META_MOV;
				}
				fallthrough;
			default:
				break;
			}
			fallthrough;
		default:
			break;
		}
	}

	return 0;
}

static bool lightrec_can_switch_delay_slot(union code op, union code next_op)
{
	switch (op.i.op) {
	case OP_SPECIAL:
		switch (op.r.op) {
		case OP_SPECIAL_JALR:
			if (opcode_reads_register(next_op, op.r.rd) ||
			    opcode_writes_register(next_op, op.r.rd))
				return false;
			fallthrough;
		case OP_SPECIAL_JR:
			if (opcode_writes_register(next_op, op.r.rs))
				return false;
			fallthrough;
		default:
			break;
		}
		fallthrough;
	case OP_J:
		break;
	case OP_JAL:
		if (opcode_reads_register(next_op, 31) ||
		    opcode_writes_register(next_op, 31))
			return false;;

		break;
	case OP_BEQ:
	case OP_BNE:
		if (op.i.rt && opcode_writes_register(next_op, op.i.rt))
			return false;
		fallthrough;
	case OP_BLEZ:
	case OP_BGTZ:
		if (op.i.rs && opcode_writes_register(next_op, op.i.rs))
			return false;
		break;
	case OP_REGIMM:
		switch (op.r.rt) {
		case OP_REGIMM_BLTZAL:
		case OP_REGIMM_BGEZAL:
			if (opcode_reads_register(next_op, 31) ||
			    opcode_writes_register(next_op, 31))
				return false;
			fallthrough;
		case OP_REGIMM_BLTZ:
		case OP_REGIMM_BGEZ:
			if (op.i.rs && opcode_writes_register(next_op, op.i.rs))
				return false;
			break;
		}
		fallthrough;
	default:
		break;
	}

	return true;
}

static int lightrec_switch_delay_slots(struct lightrec_state *state, struct block *block)
{
	struct opcode *list, *next = &block->opcode_list[0];
	unsigned int i;
	union code op, next_op;
	u32 flags;

	for (i = 0; i < block->nb_ops - 1; i++) {
		list = next;
		next = &block->opcode_list[i + 1];
		next_op = next->c;
		op = list->c;

		if (!has_delay_slot(op) || op_flag_no_ds(list->flags) ||
		    op_flag_emulate_branch(list->flags) ||
		    op.opcode == 0 || next_op.opcode == 0)
			continue;

		if (i && has_delay_slot(block->opcode_list[i - 1].c) &&
		    !op_flag_no_ds(block->opcode_list[i - 1].flags))
			continue;

		if (op_flag_sync(next->flags))
			continue;

		if (!lightrec_can_switch_delay_slot(list->c, next_op))
			continue;

		pr_debug("Swap branch and delay slot opcodes "
			 "at offsets 0x%x / 0x%x\n",
			 i << 2, (i + 1) << 2);

		flags = next->flags | (list->flags & LIGHTREC_SYNC);
		list->c = next_op;
		next->c = op;
		next->flags = (list->flags | LIGHTREC_NO_DS) & ~LIGHTREC_SYNC;
		list->flags = flags | LIGHTREC_NO_DS;
	}

	return 0;
}

static int shrink_opcode_list(struct lightrec_state *state, struct block *block, u16 new_size)
{
	struct opcode_list *list, *old_list;

	if (new_size >= block->nb_ops) {
		pr_err("Invalid shrink size (%u vs %u)\n",
		       new_size, block->nb_ops);
		return -EINVAL;
	}

	list = lightrec_malloc(state, MEM_FOR_IR,
			       sizeof(*list) + sizeof(struct opcode) * new_size);
	if (!list) {
		pr_err("Unable to allocate memory\n");
		return -ENOMEM;
	}

	old_list = container_of(block->opcode_list, struct opcode_list, ops);
	memcpy(list->ops, old_list->ops, sizeof(struct opcode) * new_size);

	lightrec_free_opcode_list(state, block->opcode_list);
	list->nb_ops = new_size;
	block->nb_ops = new_size;
	block->opcode_list = list->ops;

	pr_debug("Shrunk opcode list of block PC 0x%08x to %u opcodes\n",
		 block->pc, new_size);

	return 0;
}

static int lightrec_detect_impossible_branches(struct lightrec_state *state,
					       struct block *block)
{
	struct opcode *op, *list = block->opcode_list, *next = &list[0];
	unsigned int i;
	int ret = 0;
	s16 offset;

	for (i = 0; i < block->nb_ops - 1; i++) {
		op = next;
		next = &list[i + 1];

		if (!has_delay_slot(op->c) ||
		    (!load_in_delay_slot(next->c) &&
		     !has_delay_slot(next->c) &&
		     !(next->i.op == OP_CP0 && next->r.rs == OP_CP0_RFE)))
			continue;

		if (op->c.opcode == next->c.opcode) {
			/* The delay slot is the exact same opcode as the branch
			 * opcode: this is effectively a NOP */
			next->c.opcode = 0;
			continue;
		}

		offset = i + 1 + (s16)op->i.imm;
		if (load_in_delay_slot(next->c) &&
		    (offset >= 0 && offset < block->nb_ops) &&
		    !opcode_reads_register(list[offset].c, next->c.i.rt)) {
			/* The 'impossible' branch is a local branch - we can
			 * verify here that the first opcode of the target does
			 * not use the target register of the delay slot */

			pr_debug("Branch at offset 0x%x has load delay slot, "
				 "but is local and dest opcode does not read "
				 "dest register\n", i << 2);
			continue;
		}

		op->flags |= LIGHTREC_EMULATE_BRANCH;

		if (op == list) {
			pr_debug("First opcode of block PC 0x%08x is an impossible branch\n",
				 block->pc);

			/* If the first opcode is an 'impossible' branch, we
			 * only keep the first two opcodes of the block (the
			 * branch itself + its delay slot) */
			if (block->nb_ops > 2)
				ret = shrink_opcode_list(state, block, 2);
			break;
		}
	}

	return ret;
}

static int lightrec_local_branches(struct lightrec_state *state, struct block *block)
{
	struct opcode *list;
	unsigned int i;
	s32 offset;

	for (i = 0; i < block->nb_ops; i++) {
		list = &block->opcode_list[i];

		if (should_emulate(list))
			continue;

		switch (list->i.op) {
		case OP_BEQ:
		case OP_BNE:
		case OP_BLEZ:
		case OP_BGTZ:
		case OP_REGIMM:
			offset = i + 1 + (s16)list->i.imm;
			if (offset >= 0 && offset < block->nb_ops)
				break;
			fallthrough;
		default:
			continue;
		}

		pr_debug("Found local branch to offset 0x%x\n", offset << 2);

		if (should_emulate(&block->opcode_list[offset])) {
			pr_debug("Branch target must be emulated - skip\n");
			continue;
		}

		if (offset && has_delay_slot(block->opcode_list[offset - 1].c)) {
			pr_debug("Branch target is a delay slot - skip\n");
			continue;
		}

		pr_debug("Adding sync at offset 0x%x\n", offset << 2);

		block->opcode_list[offset].flags |= LIGHTREC_SYNC;
		list->flags |= LIGHTREC_LOCAL_BRANCH;
	}

	return 0;
}

bool has_delay_slot(union code op)
{
	switch (op.i.op) {
	case OP_SPECIAL:
		switch (op.r.op) {
		case OP_SPECIAL_JR:
		case OP_SPECIAL_JALR:
			return true;
		default:
			return false;
		}
	case OP_J:
	case OP_JAL:
	case OP_BEQ:
	case OP_BNE:
	case OP_BLEZ:
	case OP_BGTZ:
	case OP_REGIMM:
		return true;
	default:
		return false;
	}
}

bool should_emulate(const struct opcode *list)
{
	return op_flag_emulate_branch(list->flags) && has_delay_slot(list->c);
}

static bool op_writes_rd(union code c)
{
	switch (c.i.op) {
	case OP_SPECIAL:
	case OP_META_MOV:
		return true;
	default:
		return false;
	}
}

static void lightrec_add_reg_op(struct opcode *op, u8 reg, u32 reg_op)
{
	if (op_writes_rd(op->c) && reg == op->r.rd)
		op->flags |= LIGHTREC_REG_RD(reg_op);
	else if (op->i.rs == reg)
		op->flags |= LIGHTREC_REG_RS(reg_op);
	else if (op->i.rt == reg)
		op->flags |= LIGHTREC_REG_RT(reg_op);
	else
		pr_debug("Cannot add unload/clean/discard flag: "
			 "opcode does not touch register %s!\n",
			 lightrec_reg_name(reg));
}

static void lightrec_add_unload(struct opcode *op, u8 reg)
{
	lightrec_add_reg_op(op, reg, LIGHTREC_REG_UNLOAD);
}

static void lightrec_add_discard(struct opcode *op, u8 reg)
{
	lightrec_add_reg_op(op, reg, LIGHTREC_REG_DISCARD);
}

static void lightrec_add_clean(struct opcode *op, u8 reg)
{
	lightrec_add_reg_op(op, reg, LIGHTREC_REG_CLEAN);
}

static void
lightrec_early_unload_sync(struct opcode *list, s16 *last_r, s16 *last_w)
{
	unsigned int reg;
	s16 offset;

	for (reg = 0; reg < 34; reg++) {
		offset = s16_max(last_w[reg], last_r[reg]);

		if (offset >= 0)
			lightrec_add_unload(&list[offset], reg);
	}

	memset(last_r, 0xff, sizeof(*last_r) * 34);
	memset(last_w, 0xff, sizeof(*last_w) * 34);
}

static int lightrec_early_unload(struct lightrec_state *state, struct block *block)
{
	u16 i, offset;
	struct opcode *op;
	s16 last_r[34], last_w[34], last_sync = 0, next_sync = 0;
	u64 mask_r, mask_w, dirty = 0, loaded = 0;
	u8 reg;

	memset(last_r, 0xff, sizeof(last_r));
	memset(last_w, 0xff, sizeof(last_w));

	/*
	 * Clean if:
	 * - the register is dirty, and is read again after a branch opcode
	 *
	 * Unload if:
	 * - the register is dirty or loaded, and is not read again
	 * - the register is dirty or loaded, and is written again after a branch opcode
	 * - the next opcode has the SYNC flag set
	 *
	 * Discard if:
	 * - the register is dirty or loaded, and is written again
	 */

	for (i = 0; i < block->nb_ops; i++) {
		op = &block->opcode_list[i];

		if (op_flag_sync(op->flags) || should_emulate(op)) {
			/* The next opcode has the SYNC flag set, or is a branch
			 * that should be emulated: unload all registers. */
			lightrec_early_unload_sync(block->opcode_list, last_r, last_w);
			dirty = 0;
			loaded = 0;
		}

		if (next_sync == i) {
			last_sync = i;
			pr_debug("Last sync: 0x%x\n", last_sync << 2);
		}

		if (has_delay_slot(op->c)) {
			next_sync = i + 1 + !op_flag_no_ds(op->flags);
			pr_debug("Next sync: 0x%x\n", next_sync << 2);
		}

		mask_r = opcode_read_mask(op->c);
		mask_w = opcode_write_mask(op->c);

		for (reg = 0; reg < 34; reg++) {
			if (mask_r & BIT(reg)) {
				if (dirty & BIT(reg) && last_w[reg] < last_sync) {
					/* The register is dirty, and is read
					 * again after a branch: clean it */

					lightrec_add_clean(&block->opcode_list[last_w[reg]], reg);
					dirty &= ~BIT(reg);
					loaded |= BIT(reg);
				}

				last_r[reg] = i;
			}

			if (mask_w & BIT(reg)) {
				if ((dirty & BIT(reg) && last_w[reg] < last_sync) ||
				    (loaded & BIT(reg) && last_r[reg] < last_sync)) {
					/* The register is dirty or loaded, and
					 * is written again after a branch:
					 * unload it */

					offset = s16_max(last_w[reg], last_r[reg]);
					lightrec_add_unload(&block->opcode_list[offset], reg);
					dirty &= ~BIT(reg);
					loaded &= ~BIT(reg);
				} else if (!(mask_r & BIT(reg)) &&
					   ((dirty & BIT(reg) && last_w[reg] > last_sync) ||
					   (loaded & BIT(reg) && last_r[reg] > last_sync))) {
					/* The register is dirty or loaded, and
					 * is written again: discard it */

					offset = s16_max(last_w[reg], last_r[reg]);
					lightrec_add_discard(&block->opcode_list[offset], reg);
					dirty &= ~BIT(reg);
					loaded &= ~BIT(reg);
				}

				last_w[reg] = i;
			}

		}

		dirty |= mask_w;
		loaded |= mask_r;
	}

	/* Unload all registers that are dirty or loaded at the end of block. */
	lightrec_early_unload_sync(block->opcode_list, last_r, last_w);

	return 0;
}

static int lightrec_flag_io(struct lightrec_state *state, struct block *block)
{
	struct opcode *prev = NULL, *list = NULL;
	enum psx_map psx_map;
	u32 known = BIT(0);
	u32 values[32] = { 0 };
	unsigned int i;
	u32 val, kunseg_val;
	bool no_mask;

	for (i = 0; i < block->nb_ops; i++) {
		prev = list;
		list = &block->opcode_list[i];

		if (prev)
			known = lightrec_propagate_consts(list, prev, known, values);

		switch (list->i.op) {
		case OP_SB:
		case OP_SH:
		case OP_SW:
			if (OPT_FLAG_STORES) {
				/* Mark all store operations that target $sp or $gp
				 * as not requiring code invalidation. This is based
				 * on the heuristic that stores using one of these
				 * registers as address will never hit a code page. */
				if (list->i.rs >= 28 && list->i.rs <= 29 &&
				    !state->maps[PSX_MAP_KERNEL_USER_RAM].ops) {
					pr_debug("Flaging opcode 0x%08x as not "
						 "requiring invalidation\n",
						 list->opcode);
					list->flags |= LIGHTREC_NO_INVALIDATE;
					list->flags |= LIGHTREC_IO_MODE(LIGHTREC_IO_DIRECT);
				}

				/* Detect writes whose destination address is inside the
				 * current block, using constant propagation. When these
				 * occur, we mark the blocks as not compilable. */
				if ((known & BIT(list->i.rs)) &&
				    kunseg(values[list->i.rs]) >= kunseg(block->pc) &&
				    kunseg(values[list->i.rs]) < (kunseg(block->pc) +
								  block->nb_ops * 4)) {
					pr_debug("Self-modifying block detected\n");
					block_set_flags(block, BLOCK_NEVER_COMPILE);
					list->flags |= LIGHTREC_SMC;
				}
			}
			fallthrough;
		case OP_SWL:
		case OP_SWR:
		case OP_SWC2:
		case OP_LB:
		case OP_LBU:
		case OP_LH:
		case OP_LHU:
		case OP_LW:
		case OP_LWL:
		case OP_LWR:
		case OP_LWC2:
			if (OPT_FLAG_IO && (known & BIT(list->i.rs))) {
				val = values[list->i.rs] + (s16) list->i.imm;
				kunseg_val = kunseg(val);
				psx_map = lightrec_get_map_idx(state, kunseg_val);

				list->flags &= ~LIGHTREC_IO_MASK;
				no_mask = val == kunseg_val;

				switch (psx_map) {
				case PSX_MAP_KERNEL_USER_RAM:
					if (no_mask)
						list->flags |= LIGHTREC_NO_MASK;
					fallthrough;
				case PSX_MAP_MIRROR1:
				case PSX_MAP_MIRROR2:
				case PSX_MAP_MIRROR3:
					pr_debug("Flaging opcode %u as RAM access\n", i);
					list->flags |= LIGHTREC_IO_MODE(LIGHTREC_IO_RAM);
					if (no_mask && state->mirrors_mapped)
						list->flags |= LIGHTREC_NO_MASK;
					break;
				case PSX_MAP_BIOS:
					pr_debug("Flaging opcode %u as BIOS access\n", i);
					list->flags |= LIGHTREC_IO_MODE(LIGHTREC_IO_BIOS);
					if (no_mask)
						list->flags |= LIGHTREC_NO_MASK;
					break;
				case PSX_MAP_SCRATCH_PAD:
					pr_debug("Flaging opcode %u as scratchpad access\n", i);
					list->flags |= LIGHTREC_IO_MODE(LIGHTREC_IO_SCRATCH);
					if (no_mask)
						list->flags |= LIGHTREC_NO_MASK;

					/* Consider that we're never going to run code from
					 * the scratchpad. */
					list->flags |= LIGHTREC_NO_INVALIDATE;
					break;
				case PSX_MAP_HW_REGISTERS:
					if (state->ops.hw_direct &&
					    state->ops.hw_direct(kunseg_val,
								 opcode_is_store(list->c),
								 opcode_get_io_size(list->c))) {
						pr_debug("Flagging opcode %u as direct I/O access\n",
							 i);
						list->flags |= LIGHTREC_IO_MODE(LIGHTREC_IO_DIRECT_HW);

						if (no_mask)
							list->flags |= LIGHTREC_NO_MASK;
						break;
					}
					fallthrough;
				default:
					pr_debug("Flagging opcode %u as I/O access\n",
						 i);
					list->flags |= LIGHTREC_IO_MODE(LIGHTREC_IO_HW);
					break;
				}
			}
			fallthrough;
		default:
			break;
		}
	}

	return 0;
}

static u8 get_mfhi_mflo_reg(const struct block *block, u16 offset,
			    const struct opcode *last,
			    u32 mask, bool sync, bool mflo, bool another)
{
	const struct opcode *op, *next = &block->opcode_list[offset];
	u32 old_mask;
	u8 reg2, reg = mflo ? REG_LO : REG_HI;
	u16 branch_offset;
	unsigned int i;

	for (i = offset; i < block->nb_ops; i++) {
		op = next;
		next = &block->opcode_list[i + 1];
		old_mask = mask;

		/* If any other opcode writes or reads to the register
		 * we'd use, then we cannot use it anymore. */
		mask |= opcode_read_mask(op->c);
		mask |= opcode_write_mask(op->c);

		if (op_flag_sync(op->flags))
			sync = true;

		switch (op->i.op) {
		case OP_BEQ:
		case OP_BNE:
		case OP_BLEZ:
		case OP_BGTZ:
		case OP_REGIMM:
			/* TODO: handle backwards branches too */
			if (!last && op_flag_local_branch(op->flags) &&
			    (s16)op->c.i.imm >= 0) {
				branch_offset = i + 1 + (s16)op->c.i.imm
					- !!op_flag_no_ds(op->flags);

				reg = get_mfhi_mflo_reg(block, branch_offset, NULL,
							mask, sync, mflo, false);
				reg2 = get_mfhi_mflo_reg(block, offset + 1, next,
							 mask, sync, mflo, false);
				if (reg > 0 && reg == reg2)
					return reg;
				if (!reg && !reg2)
					return 0;
			}

			return mflo ? REG_LO : REG_HI;
		case OP_META_MULT2:
		case OP_META_MULTU2:
			return 0;
		case OP_SPECIAL:
			switch (op->r.op) {
			case OP_SPECIAL_MULT:
			case OP_SPECIAL_MULTU:
			case OP_SPECIAL_DIV:
			case OP_SPECIAL_DIVU:
				return 0;
			case OP_SPECIAL_MTHI:
				if (!mflo)
					return 0;
				continue;
			case OP_SPECIAL_MTLO:
				if (mflo)
					return 0;
				continue;
			case OP_SPECIAL_JR:
				if (op->r.rs != 31)
					return reg;

				if (!sync && !op_flag_no_ds(op->flags) &&
				    (next->i.op == OP_SPECIAL) &&
				    ((!mflo && next->r.op == OP_SPECIAL_MFHI) ||
				    (mflo && next->r.op == OP_SPECIAL_MFLO)))
					return next->r.rd;

				return 0;
			case OP_SPECIAL_JALR:
				return reg;
			case OP_SPECIAL_MFHI:
				if (!mflo) {
					if (another)
						return op->r.rd;
					/* Must use REG_HI if there is another MFHI target*/
					reg2 = get_mfhi_mflo_reg(block, i + 1, next,
							 0, sync, mflo, true);
					if (reg2 > 0 && reg2 != REG_HI)
						return REG_HI;

					if (!sync && !(old_mask & BIT(op->r.rd)))
						return op->r.rd;
					else
						return REG_HI;
				}
				continue;
			case OP_SPECIAL_MFLO:
				if (mflo) {
					if (another)
						return op->r.rd;
					/* Must use REG_LO if there is another MFLO target*/
					reg2 = get_mfhi_mflo_reg(block, i + 1, next,
							 0, sync, mflo, true);
					if (reg2 > 0 && reg2 != REG_LO)
						return REG_LO;

					if (!sync && !(old_mask & BIT(op->r.rd)))
						return op->r.rd;
					else
						return REG_LO;
				}
				continue;
			default:
				break;
			}

			fallthrough;
		default:
			continue;
		}
	}

	return reg;
}

static void lightrec_replace_lo_hi(struct block *block, u16 offset,
				   u16 last, bool lo)
{
	unsigned int i;
	u32 branch_offset;

	/* This function will remove the following MFLO/MFHI. It must be called
	 * only if get_mfhi_mflo_reg() returned a non-zero value. */

	for (i = offset; i < last; i++) {
		struct opcode *op = &block->opcode_list[i];

		switch (op->i.op) {
		case OP_BEQ:
		case OP_BNE:
		case OP_BLEZ:
		case OP_BGTZ:
		case OP_REGIMM:
			/* TODO: handle backwards branches too */
			if (op_flag_local_branch(op->flags) && (s16)op->c.i.imm >= 0) {
				branch_offset = i + 1 + (s16)op->c.i.imm
					- !!op_flag_no_ds(op->flags);

				lightrec_replace_lo_hi(block, branch_offset, last, lo);
				lightrec_replace_lo_hi(block, i + 1, branch_offset, lo);
			}
			break;

		case OP_SPECIAL:
			if (lo && op->r.op == OP_SPECIAL_MFLO) {
				pr_debug("Removing MFLO opcode at offset 0x%x\n",
					 i << 2);
				op->opcode = 0;
				return;
			} else if (!lo && op->r.op == OP_SPECIAL_MFHI) {
				pr_debug("Removing MFHI opcode at offset 0x%x\n",
					 i << 2);
				op->opcode = 0;
				return;
			}

			fallthrough;
		default:
			break;
		}
	}
}

static bool lightrec_always_skip_div_check(void)
{
#ifdef __mips__
	return true;
#else
	return false;
#endif
}

static int lightrec_flag_mults_divs(struct lightrec_state *state, struct block *block)
{
	struct opcode *prev, *list = NULL;
	u8 reg_hi, reg_lo;
	unsigned int i;
	u32 known = BIT(0);
	u32 values[32] = { 0 };

	for (i = 0; i < block->nb_ops - 1; i++) {
		prev = list;
		list = &block->opcode_list[i];

		if (prev)
			known = lightrec_propagate_consts(list, prev, known, values);

		switch (list->i.op) {
		case OP_SPECIAL:
			switch (list->r.op) {
			case OP_SPECIAL_DIV:
			case OP_SPECIAL_DIVU:
				/* If we are dividing by a non-zero constant, don't
				 * emit the div-by-zero check. */
				if (lightrec_always_skip_div_check() ||
				    ((known & BIT(list->c.r.rt)) && values[list->c.r.rt]))
					list->flags |= LIGHTREC_NO_DIV_CHECK;
				fallthrough;
			case OP_SPECIAL_MULT:
			case OP_SPECIAL_MULTU:
				break;
			default:
				continue;
			}
			fallthrough;
		case OP_META_MULT2:
		case OP_META_MULTU2:
			break;
		default:
			continue;
		}

		/* Don't support opcodes in delay slots */
		if ((i && has_delay_slot(block->opcode_list[i - 1].c)) ||
		    op_flag_no_ds(list->flags)) {
			continue;
		}

		reg_lo = get_mfhi_mflo_reg(block, i + 1, NULL, 0, false, true, false);
		if (reg_lo == 0) {
			pr_debug("Mark MULT(U)/DIV(U) opcode at offset 0x%x as"
				 " not writing LO\n", i << 2);
			list->flags |= LIGHTREC_NO_LO;
		}

		reg_hi = get_mfhi_mflo_reg(block, i + 1, NULL, 0, false, false, false);
		if (reg_hi == 0) {
			pr_debug("Mark MULT(U)/DIV(U) opcode at offset 0x%x as"
				 " not writing HI\n", i << 2);
			list->flags |= LIGHTREC_NO_HI;
		}

		if (!reg_lo && !reg_hi) {
			pr_debug("Both LO/HI unused in this block, they will "
				 "probably be used in parent block - removing "
				 "flags.\n");
			list->flags &= ~(LIGHTREC_NO_LO | LIGHTREC_NO_HI);
		}

		if (reg_lo > 0 && reg_lo != REG_LO) {
			pr_debug("Found register %s to hold LO (rs = %u, rt = %u)\n",
				 lightrec_reg_name(reg_lo), list->r.rs, list->r.rt);

			lightrec_replace_lo_hi(block, i + 1, block->nb_ops, true);
			list->r.rd = reg_lo;
		} else {
			list->r.rd = 0;
		}

		if (reg_hi > 0 && reg_hi != REG_HI) {
			pr_debug("Found register %s to hold HI (rs = %u, rt = %u)\n",
				 lightrec_reg_name(reg_hi), list->r.rs, list->r.rt);

			lightrec_replace_lo_hi(block, i + 1, block->nb_ops, false);
			list->r.imm = reg_hi;
		} else {
			list->r.imm = 0;
		}
	}

	return 0;
}

static bool remove_div_sequence(struct block *block, unsigned int offset)
{
	struct opcode *op;
	unsigned int i, found = 0;

	/*
	 * Scan for the zero-checking sequence that GCC automatically introduced
	 * after most DIV/DIVU opcodes. This sequence checks the value of the
	 * divisor, and if zero, executes a BREAK opcode, causing the BIOS
	 * handler to crash the PS1.
	 *
	 * For DIV opcodes, this sequence additionally checks that the signed
	 * operation does not overflow.
	 *
	 * With the assumption that the games never crashed the PS1, we can
	 * therefore assume that the games never divided by zero or overflowed,
	 * and these sequences can be removed.
	 */

	for (i = offset; i < block->nb_ops; i++) {
		op = &block->opcode_list[i];

		if (!found) {
			if (op->i.op == OP_SPECIAL &&
			    (op->r.op == OP_SPECIAL_DIV || op->r.op == OP_SPECIAL_DIVU))
				break;

			if ((op->opcode & 0xfc1fffff) == 0x14000002) {
				/* BNE ???, zero, +8 */
				found++;
			} else {
				offset++;
			}
		} else if (found == 1 && !op->opcode) {
			/* NOP */
			found++;
		} else if (found == 2 && op->opcode == 0x0007000d) {
			/* BREAK 0x1c00 */
			found++;
		} else if (found == 3 && op->opcode == 0x2401ffff) {
			/* LI at, -1 */
			found++;
		} else if (found == 4 && (op->opcode & 0xfc1fffff) == 0x14010004) {
			/* BNE ???, at, +16 */
			found++;
		} else if (found == 5 && op->opcode == 0x3c018000) {
			/* LUI at, 0x8000 */
			found++;
		} else if (found == 6 && (op->opcode & 0x141fffff) == 0x14010002) {
			/* BNE ???, at, +16 */
			found++;
		} else if (found == 7 && !op->opcode) {
			/* NOP */
			found++;
		} else if (found == 8 && op->opcode == 0x0006000d) {
			/* BREAK 0x1800 */
			found++;
			break;
		} else {
			break;
		}
	}

	if (found >= 3) {
		if (found != 9)
			found = 3;

		pr_debug("Removing DIV%s sequence at offset 0x%x\n",
			 found == 9 ? "" : "U", offset << 2);

		for (i = 0; i < found; i++)
			block->opcode_list[offset + i].opcode = 0;

		return true;
	}

	return false;
}

static int lightrec_remove_div_by_zero_check_sequence(struct lightrec_state *state,
						      struct block *block)
{
	struct opcode *op;
	unsigned int i;

	for (i = 0; i < block->nb_ops; i++) {
		op = &block->opcode_list[i];

		if (op->i.op == OP_SPECIAL &&
		    (op->r.op == OP_SPECIAL_DIVU || op->r.op == OP_SPECIAL_DIV) &&
		    remove_div_sequence(block, i + 1))
			op->flags |= LIGHTREC_NO_DIV_CHECK;
	}

	return 0;
}

static const u32 memset_code[] = {
	0x10a00006,	// beqz		a1, 2f
	0x24a2ffff,	// addiu	v0,a1,-1
	0x2403ffff,	// li		v1,-1
	0xac800000,	// 1: sw	zero,0(a0)
	0x2442ffff,	// addiu	v0,v0,-1
	0x1443fffd,	// bne		v0,v1, 1b
	0x24840004,	// addiu	a0,a0,4
	0x03e00008,	// 2: jr	ra
	0x00000000,	// nop
};

static int lightrec_replace_memset(struct lightrec_state *state, struct block *block)
{
	unsigned int i;
	union code c;

	for (i = 0; i < block->nb_ops; i++) {
		c = block->opcode_list[i].c;

		if (c.opcode != memset_code[i])
			return 0;

		if (i == ARRAY_SIZE(memset_code) - 1) {
			/* success! */
			pr_debug("Block at PC 0x%x is a memset\n", block->pc);
			block_set_flags(block,
					BLOCK_IS_MEMSET | BLOCK_NEVER_COMPILE);

			/* Return non-zero to skip other optimizers. */
			return 1;
		}
	}

	return 0;
}

static int (*lightrec_optimizers[])(struct lightrec_state *state, struct block *) = {
	IF_OPT(OPT_REMOVE_DIV_BY_ZERO_SEQ, &lightrec_remove_div_by_zero_check_sequence),
	IF_OPT(OPT_REPLACE_MEMSET, &lightrec_replace_memset),
	IF_OPT(OPT_DETECT_IMPOSSIBLE_BRANCHES, &lightrec_detect_impossible_branches),
	IF_OPT(OPT_TRANSFORM_OPS, &lightrec_transform_branches),
	IF_OPT(OPT_LOCAL_BRANCHES, &lightrec_local_branches),
	IF_OPT(OPT_TRANSFORM_OPS, &lightrec_transform_ops),
	IF_OPT(OPT_SWITCH_DELAY_SLOTS, &lightrec_switch_delay_slots),
	IF_OPT(OPT_FLAG_IO || OPT_FLAG_STORES, &lightrec_flag_io),
	IF_OPT(OPT_FLAG_MULT_DIV, &lightrec_flag_mults_divs),
	IF_OPT(OPT_EARLY_UNLOAD, &lightrec_early_unload),
};

int lightrec_optimize(struct lightrec_state *state, struct block *block)
{
	unsigned int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(lightrec_optimizers); i++) {
		if (lightrec_optimizers[i]) {
			ret = (*lightrec_optimizers[i])(state, block);
			if (ret)
				return ret;
		}
	}

	return 0;
}
