/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
 ***************************************************************************/

/*
* Internal PSX HLE functions.
*/

#include "psxhle.h"

#if 0
#define PSXHLE_LOG SysPrintf
#else
#define PSXHLE_LOG(...)
#endif

static void hleDummy() {
	log_unhandled("hleDummy called @%08x ra=%08x\n",
		psxRegs.pc - 4, psxRegs.GPR.n.ra);
	psxRegs.pc = psxRegs.GPR.n.ra;
	psxRegs.cycle += 1000;

	psxBranchTest();
}

static void hleA0() {
	u32 call = psxRegs.GPR.n.t1 & 0xff;

	if (biosA0[call]) biosA0[call]();

	psxBranchTest();
}

static void hleB0() {
	u32 call = psxRegs.GPR.n.t1 & 0xff;

	if (biosB0[call]) biosB0[call]();

	psxBranchTest();
}

static void hleC0() {
	u32 call = psxRegs.GPR.n.t1 & 0xff;

	if (biosC0[call]) biosC0[call]();

	psxBranchTest();
}

static void hleBootstrap() { // 0xbfc00000
	PSXHLE_LOG("hleBootstrap\n");
	CheckCdrom();
	LoadCdrom();
	PSXHLE_LOG("CdromLabel: \"%s\": PC = %8.8lx (SP = %8.8lx)\n", CdromLabel, psxRegs.pc, psxRegs.GPR.n.sp);
}

typedef struct {                   
	u32 _pc0;      
	u32 gp0;      
	u32 t_addr;   
	u32 t_size;   
	u32 d_addr;   
	u32 d_size;   
	u32 b_addr;   
	u32 b_size;   
	u32 S_addr;
	u32 s_size;
	u32 _sp,_fp,_gp,ret,base;
} EXEC;

static void hleExecRet() {
	EXEC *header = (EXEC*)PSXM(psxRegs.GPR.n.s0);

	PSXHLE_LOG("ExecRet %x: %x\n", psxRegs.GPR.n.s0, header->ret);

	psxRegs.GPR.n.ra = SWAP32(header->ret);
	psxRegs.GPR.n.sp = SWAP32(header->_sp);
	psxRegs.GPR.n.fp = SWAP32(header->_fp);
	psxRegs.GPR.n.gp = SWAP32(header->_gp);
	psxRegs.GPR.n.s0 = SWAP32(header->base);

	psxRegs.GPR.n.v0 = 1;
	psxRegs.pc = psxRegs.GPR.n.ra;
}

void (* const psxHLEt[24])() = {
	hleDummy, hleA0, hleB0, hleC0,
	hleBootstrap, hleExecRet, psxBiosException, hleDummy,
	hleExc0_0_1, hleExc0_0_2,
	hleExc0_1_1, hleExc0_1_2, hleExc0_2_2_syscall,
	hleExc1_0_1, hleExc1_0_2,
	hleExc1_1_1, hleExc1_1_2,
	hleExc1_2_1, hleExc1_2_2,
	hleExc1_3_1, hleExc1_3_2,
	hleExc3_0_2_defint,
	hleExcPadCard1, hleExcPadCard2,
};
