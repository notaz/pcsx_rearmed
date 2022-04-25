/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2014-2021 Paul Cercueil <paul@crapouillou.net>
 */

#ifndef __DISASSEMBLER_H__
#define __DISASSEMBLER_H__

#include "debug.h"
#include "lightrec.h"

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#define BIT(x) (1ULL << (x))

/* Flags for all opcodes */
#define LIGHTREC_NO_DS		BIT(0)
#define LIGHTREC_UNLOAD_RS	BIT(1)
#define LIGHTREC_UNLOAD_RT	BIT(2)
#define LIGHTREC_UNLOAD_RD	BIT(3)
#define LIGHTREC_SYNC		BIT(4)

/* Flags for load/store opcodes */
#define LIGHTREC_SMC		BIT(5)
#define LIGHTREC_NO_INVALIDATE	BIT(6)
#define LIGHTREC_NO_MASK	BIT(7)

/* I/O mode for load/store opcodes */
#define LIGHTREC_IO_MODE_LSB	8
#define LIGHTREC_IO_MODE(x)	((x) << LIGHTREC_IO_MODE_LSB)
#define LIGHTREC_IO_UNKNOWN	0x0
#define LIGHTREC_IO_DIRECT	0x1
#define LIGHTREC_IO_HW		0x2
#define LIGHTREC_IO_RAM		0x3
#define LIGHTREC_IO_BIOS	0x4
#define LIGHTREC_IO_SCRATCH	0x5
#define LIGHTREC_IO_MASK	LIGHTREC_IO_MODE(0x7)
#define LIGHTREC_FLAGS_GET_IO_MODE(x) \
	(((x) & LIGHTREC_IO_MASK) >> LIGHTREC_IO_MODE_LSB)

/* Flags for branches */
#define LIGHTREC_EMULATE_BRANCH	BIT(5)
#define LIGHTREC_LOCAL_BRANCH	BIT(6)

/* Flags for div/mult opcodes */
#define LIGHTREC_NO_LO		BIT(5)
#define LIGHTREC_NO_HI		BIT(6)
#define LIGHTREC_NO_DIV_CHECK	BIT(7)

struct block;

enum standard_opcodes {
	OP_SPECIAL		= 0x00,
	OP_REGIMM		= 0x01,
	OP_J			= 0x02,
	OP_JAL			= 0x03,
	OP_BEQ			= 0x04,
	OP_BNE			= 0x05,
	OP_BLEZ			= 0x06,
	OP_BGTZ			= 0x07,
	OP_ADDI			= 0x08,
	OP_ADDIU		= 0x09,
	OP_SLTI			= 0x0a,
	OP_SLTIU		= 0x0b,
	OP_ANDI			= 0x0c,
	OP_ORI			= 0x0d,
	OP_XORI			= 0x0e,
	OP_LUI			= 0x0f,
	OP_CP0			= 0x10,
	OP_CP2			= 0x12,
	OP_LB			= 0x20,
	OP_LH			= 0x21,
	OP_LWL			= 0x22,
	OP_LW			= 0x23,
	OP_LBU			= 0x24,
	OP_LHU			= 0x25,
	OP_LWR			= 0x26,
	OP_SB			= 0x28,
	OP_SH			= 0x29,
	OP_SWL			= 0x2a,
	OP_SW			= 0x2b,
	OP_SWR			= 0x2e,
	OP_LWC2			= 0x32,
	OP_SWC2			= 0x3a,

	OP_META_MOV		= 0x16,

	OP_META_EXTC		= 0x17,
	OP_META_EXTS		= 0x18,
};

enum special_opcodes {
	OP_SPECIAL_SLL		= 0x00,
	OP_SPECIAL_SRL		= 0x02,
	OP_SPECIAL_SRA		= 0x03,
	OP_SPECIAL_SLLV		= 0x04,
	OP_SPECIAL_SRLV		= 0x06,
	OP_SPECIAL_SRAV		= 0x07,
	OP_SPECIAL_JR		= 0x08,
	OP_SPECIAL_JALR		= 0x09,
	OP_SPECIAL_SYSCALL	= 0x0c,
	OP_SPECIAL_BREAK	= 0x0d,
	OP_SPECIAL_MFHI		= 0x10,
	OP_SPECIAL_MTHI		= 0x11,
	OP_SPECIAL_MFLO		= 0x12,
	OP_SPECIAL_MTLO		= 0x13,
	OP_SPECIAL_MULT		= 0x18,
	OP_SPECIAL_MULTU	= 0x19,
	OP_SPECIAL_DIV		= 0x1a,
	OP_SPECIAL_DIVU		= 0x1b,
	OP_SPECIAL_ADD		= 0x20,
	OP_SPECIAL_ADDU		= 0x21,
	OP_SPECIAL_SUB		= 0x22,
	OP_SPECIAL_SUBU		= 0x23,
	OP_SPECIAL_AND		= 0x24,
	OP_SPECIAL_OR		= 0x25,
	OP_SPECIAL_XOR		= 0x26,
	OP_SPECIAL_NOR		= 0x27,
	OP_SPECIAL_SLT		= 0x2a,
	OP_SPECIAL_SLTU		= 0x2b,
};

enum regimm_opcodes {
	OP_REGIMM_BLTZ		= 0x00,
	OP_REGIMM_BGEZ		= 0x01,
	OP_REGIMM_BLTZAL	= 0x10,
	OP_REGIMM_BGEZAL	= 0x11,
};

enum cp0_opcodes {
	OP_CP0_MFC0		= 0x00,
	OP_CP0_CFC0		= 0x02,
	OP_CP0_MTC0		= 0x04,
	OP_CP0_CTC0		= 0x06,
	OP_CP0_RFE		= 0x10,
};

enum cp2_opcodes {
	OP_CP2_BASIC		= 0x00,
};

enum cp2_basic_opcodes {
	OP_CP2_BASIC_MFC2	= 0x00,
	OP_CP2_BASIC_CFC2	= 0x02,
	OP_CP2_BASIC_MTC2	= 0x04,
	OP_CP2_BASIC_CTC2	= 0x06,
};

struct opcode_r {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	u32 zero :6;
	u32 rs   :5;
	u32 rt   :5;
	u32 rd   :5;
	u32 imm  :5;
	u32 op   :6;
#else
	u32 op   :6;
	u32 imm  :5;
	u32 rd   :5;
	u32 rt   :5;
	u32 rs   :5;
	u32 zero :6;
#endif
} __packed;

struct opcode_i {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	u32 op  :6;
	u32 rs  :5;
	u32 rt  :5;
	u32 imm :16;
#else
	u32 imm :16;
	u32 rt  :5;
	u32 rs  :5;
	u32 op  :6;
#endif
} __packed;

struct opcode_j {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	u32 op  :6;
	u32 imm :26;
#else
	u32 imm :26;
	u32 op  :6;
#endif
} __packed;

union code {
	/* Keep in sync with struct opcode */
	u32 opcode;
	struct opcode_r r;
	struct opcode_i i;
	struct opcode_j j;
};

struct opcode {
	/* Keep this union at the first position */
	union {
		union code c;

		/* Keep in sync with union code */
		u32 opcode;
		struct opcode_r r;
		struct opcode_i i;
		struct opcode_j j;
	};
	u16 flags;
};

void lightrec_print_disassembly(const struct block *block, const u32 *code);

#endif /* __DISASSEMBLER_H__ */
