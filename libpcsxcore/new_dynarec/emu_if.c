/*
 * (C) Gražvydas "notaz" Ignotas, 2010-2011
 *
 * This work is licensed under the terms of GNU GPL version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>

#include "emu_if.h"
#include "pcsxmem.h"
#include "events.h"
#include "../psxhle.h"
#include "../psxinterpreter.h"
#include "../r3000a.h"
#include "../gte_arm.h"
#include "../gte_neon.h"
#define FLAGLESS
#include "../gte.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

//#define evprintf printf
#define evprintf(...)

void pcsx_mtc0(u32 reg, u32 val)
{
	evprintf("MTC0 %d #%x @%08x %u\n", reg, val, psxRegs.pc, psxRegs.cycle);
	MTC0(&psxRegs, reg, val);
	gen_interupt(&psxRegs.CP0);
	if (psxRegs.CP0.n.Cause & psxRegs.CP0.n.SR & 0x0300) // possible sw irq
		pending_exception = 1;
}

void pcsx_mtc0_ds(u32 reg, u32 val)
{
	evprintf("MTC0 %d #%x @%08x %u\n", reg, val, psxRegs.pc, psxRegs.cycle);
	MTC0(&psxRegs, reg, val);
}

static void new_dyna_restore(void)
{
	int i;
	for (i = 0; i < PSXINT_COUNT; i++)
		event_cycles[i] = psxRegs.intCycle[i].sCycle + psxRegs.intCycle[i].cycle;

	event_cycles[PSXINT_RCNT] = psxNextsCounter + psxNextCounter;
	psxRegs.interrupt |=  1 << PSXINT_RCNT;
	psxRegs.interrupt &= (1 << PSXINT_COUNT) - 1;

	new_dyna_pcsx_mem_load_state();
}

void new_dyna_freeze(void *f, int mode)
{
	const char header_save[8] = "ariblks";
	uint32_t addrs[1024 * 4];
	int32_t size = 0;
	int bytes;
	char header[8];

	if (mode != 0) { // save
		size = new_dynarec_save_blocks(addrs, sizeof(addrs));
		if (size == 0)
			return;

		SaveFuncs.write(f, header_save, sizeof(header_save));
		SaveFuncs.write(f, &size, sizeof(size));
		SaveFuncs.write(f, addrs, size);
	}
	else {
		new_dyna_restore();

		bytes = SaveFuncs.read(f, header, sizeof(header));
		if (bytes != sizeof(header) || strcmp(header, header_save)) {
			if (bytes > 0)
				SaveFuncs.seek(f, -bytes, SEEK_CUR);
			return;
		}
		SaveFuncs.read(f, &size, sizeof(size));
		if (size <= 0)
			return;
		if (size > sizeof(addrs)) {
			bytes = size - sizeof(addrs);
			SaveFuncs.seek(f, bytes, SEEK_CUR);
			size = sizeof(addrs);
		}
		bytes = SaveFuncs.read(f, addrs, size);
		if (bytes != size)
			return;

		if (psxCpu != &psxInt)
			new_dynarec_load_blocks(addrs, size);
	}

	//printf("drc: %d block info entries %s\n", size/8, mode ? "saved" : "loaded");
}

#if !defined(DRC_DISABLE) && !defined(LIGHTREC)

/* GTE stuff */
void *gte_handlers[64];

void *gte_handlers_nf[64] = {
	NULL      , gteRTPS_nf , NULL       , NULL      , NULL     , NULL       , gteNCLIP_nf, NULL      , // 00
	NULL      , NULL       , NULL       , NULL      , gteOP_nf , NULL       , NULL       , NULL      , // 08
	gteDPCS_nf, gteINTPL_nf, gteMVMVA_nf, gteNCDS_nf, gteCDP_nf, NULL       , gteNCDT_nf , NULL      , // 10
	NULL      , NULL       , NULL       , gteNCCS_nf, gteCC_nf , NULL       , gteNCS_nf  , NULL      , // 18
	gteNCT_nf , NULL       , NULL       , NULL      , NULL     , NULL       , NULL       , NULL      , // 20
	gteSQR_nf , gteDCPL_nf , gteDPCT_nf , NULL      , NULL     , gteAVSZ3_nf, gteAVSZ4_nf, NULL      , // 28 
	gteRTPT_nf, NULL       , NULL       , NULL      , NULL     , NULL       , NULL       , NULL      , // 30
	NULL      , NULL       , NULL       , NULL      , NULL     , gteGPF_nf  , gteGPL_nf  , gteNCCT_nf, // 38
};

const char *gte_regnames[64] = {
	NULL  , "RTPS" , NULL   , NULL  , NULL , NULL   , "NCLIP", NULL  , // 00
	NULL  , NULL   , NULL   , NULL  , "OP" , NULL   , NULL   , NULL  , // 08
	"DPCS", "INTPL", "MVMVA", "NCDS", "CDP", NULL   , "NCDT" , NULL  , // 10
	NULL  , NULL   , NULL   , "NCCS", "CC" , NULL   , "NCS"  , NULL  , // 18
	"NCT" , NULL   , NULL   , NULL  , NULL , NULL   , NULL   , NULL  , // 20
	"SQR" , "DCPL" , "DPCT" , NULL  , NULL , "AVSZ3", "AVSZ4", NULL  , // 28 
	"RTPT", NULL   , NULL   , NULL  , NULL , NULL   , NULL   , NULL  , // 30
	NULL  , NULL   , NULL   , NULL  , NULL , "GPF"  , "GPL"  , "NCCT", // 38
};

#define GCBIT(x) \
	(1ll << (32+x))
#define GDBIT(x) \
	(1ll << (x))
#define GCBITS3(b0,b1,b2) \
	(GCBIT(b0) | GCBIT(b1) | GCBIT(b2))
#define GDBITS2(b0,b1) \
	(GDBIT(b0) | GDBIT(b1))
#define GDBITS3(b0,b1,b2) \
	(GDBITS2(b0,b1) | GDBIT(b2))
#define GDBITS4(b0,b1,b2,b3) \
	(GDBITS3(b0,b1,b2) | GDBIT(b3))
#define GDBITS5(b0,b1,b2,b3,b4) \
	(GDBITS4(b0,b1,b2,b3) | GDBIT(b4))
#define GDBITS6(b0,b1,b2,b3,b4,b5) \
	(GDBITS5(b0,b1,b2,b3,b4) | GDBIT(b5))
#define GDBITS7(b0,b1,b2,b3,b4,b5,b6) \
	(GDBITS6(b0,b1,b2,b3,b4,b5) | GDBIT(b6))
#define GDBITS8(b0,b1,b2,b3,b4,b5,b6,b7) \
	(GDBITS7(b0,b1,b2,b3,b4,b5,b6) | GDBIT(b7))
#define GDBITS9(b0,b1,b2,b3,b4,b5,b6,b7,b8) \
	(GDBITS8(b0,b1,b2,b3,b4,b5,b6,b7) | GDBIT(b8))
#define GDBITS10(b0,b1,b2,b3,b4,b5,b6,b7,b8,b9) \
	(GDBITS9(b0,b1,b2,b3,b4,b5,b6,b7,b8) | GDBIT(b9))

const uint64_t gte_reg_reads[64] = {
	[GTE_RTPS]  = 0x1f0000ff00000000ll | GDBITS7(0,1,13,14,17,18,19),
	[GTE_NCLIP] =                        GDBITS3(12,13,14),
	[GTE_OP]    = GCBITS3(0,2,4)       | GDBITS3(9,10,11),
	[GTE_DPCS]  = GCBITS3(21,22,23)    | GDBITS4(6,8,21,22),
	[GTE_INTPL] = GCBITS3(21,22,23)    | GDBITS7(6,8,9,10,11,21,22),
	[GTE_MVMVA] = 0x00ffffff00000000ll | GDBITS9(0,1,2,3,4,5,9,10,11), // XXX: maybe decode further?
	[GTE_NCDS]  = 0x00ffff0000000000ll | GDBITS6(0,1,6,8,21,22),
	[GTE_CDP]   = 0x00ffe00000000000ll | GDBITS7(6,8,9,10,11,21,22),
	[GTE_NCDT]  = 0x00ffff0000000000ll | GDBITS8(0,1,2,3,4,5,6,8),
	[GTE_NCCS]  = 0x001fff0000000000ll | GDBITS5(0,1,6,21,22),
	[GTE_CC]    = 0x001fe00000000000ll | GDBITS6(6,9,10,11,21,22),
	[GTE_NCS]   = 0x001fff0000000000ll | GDBITS5(0,1,6,21,22),
	[GTE_NCT]   = 0x001fff0000000000ll | GDBITS7(0,1,2,3,4,5,6),
	[GTE_SQR]   =                        GDBITS3(9,10,11),
	[GTE_DCPL]  = GCBITS3(21,22,23)    | GDBITS7(6,8,9,10,11,21,22),
	[GTE_DPCT]  = GCBITS3(21,22,23)    | GDBITS4(8,20,21,22),
	[GTE_AVSZ3] = GCBIT(29)            | GDBITS3(17,18,19),
	[GTE_AVSZ4] = GCBIT(30)            | GDBITS4(16,17,18,19),
	[GTE_RTPT]  = 0x1f0000ff00000000ll | GDBITS7(0,1,2,3,4,5,19),
	[GTE_GPF]   =                        GDBITS7(6,8,9,10,11,21,22),
	[GTE_GPL]   =                        GDBITS10(6,8,9,10,11,21,22,25,26,27),
	[GTE_NCCT]  = 0x001fff0000000000ll | GDBITS7(0,1,2,3,4,5,6),
};

// note: this excludes gteFLAG that is always written to
const uint64_t gte_reg_writes[64] = {
	[GTE_RTPS]  = 0x0f0f7f00ll,
	[GTE_NCLIP] = GDBIT(24),
	[GTE_OP]    = GDBITS6(9,10,11,25,26,27),
	[GTE_DPCS]  = GDBITS9(9,10,11,20,21,22,25,26,27),
	[GTE_INTPL] = GDBITS9(9,10,11,20,21,22,25,26,27),
	[GTE_MVMVA] = GDBITS6(9,10,11,25,26,27),
	[GTE_NCDS]  = GDBITS9(9,10,11,20,21,22,25,26,27),
	[GTE_CDP]   = GDBITS9(9,10,11,20,21,22,25,26,27),
	[GTE_NCDT]  = GDBITS9(9,10,11,20,21,22,25,26,27),
	[GTE_NCCS]  = GDBITS9(9,10,11,20,21,22,25,26,27),
	[GTE_CC]    = GDBITS9(9,10,11,20,21,22,25,26,27),
	[GTE_NCS]   = GDBITS9(9,10,11,20,21,22,25,26,27),
	[GTE_NCT]   = GDBITS9(9,10,11,20,21,22,25,26,27),
	[GTE_SQR]   = GDBITS6(9,10,11,25,26,27),
	[GTE_DCPL]  = GDBITS9(9,10,11,20,21,22,25,26,27),
	[GTE_DPCT]  = GDBITS9(9,10,11,20,21,22,25,26,27),
	[GTE_AVSZ3] = GDBITS2(7,24),
	[GTE_AVSZ4] = GDBITS2(7,24),
	[GTE_RTPT]  = 0x0f0f7f00ll,
	[GTE_GPF]   = GDBITS9(9,10,11,20,21,22,25,26,27),
	[GTE_GPL]   = GDBITS9(9,10,11,20,21,22,25,26,27),
	[GTE_NCCT]  = GDBITS9(9,10,11,20,21,22,25,26,27),
};

static int ari64_init()
{
	static u32 scratch_buf[8*8*2] __attribute__((aligned(64)));
	size_t i;

	new_dynarec_init();
	new_dyna_pcsx_mem_init();

	for (i = 0; i < ARRAY_SIZE(gte_handlers); i++)
		if (psxCP2[i] != gteNULL)
			gte_handlers[i] = psxCP2[i];

#if defined(__arm__) && !defined(DRC_DBG)
	gte_handlers[0x06] = gteNCLIP_arm;
#ifdef HAVE_ARMV5
	gte_handlers_nf[0x01] = gteRTPS_nf_arm;
	gte_handlers_nf[0x30] = gteRTPT_nf_arm;
#endif
#ifdef __ARM_NEON__
	// compiler's _nf version is still a lot slower than neon
	// _nf_arm RTPS is roughly the same, RTPT slower
	gte_handlers[0x01] = gte_handlers_nf[0x01] = gteRTPS_neon;
	gte_handlers[0x30] = gte_handlers_nf[0x30] = gteRTPT_neon;
#endif
#endif
#ifdef DRC_DBG
	memcpy(gte_handlers_nf, gte_handlers, sizeof(gte_handlers_nf));
#endif
	psxH_ptr = psxH;
	zeromem_ptr = zero_mem;
	scratch_buf_ptr = scratch_buf;

	return 0;
}

static void ari64_reset()
{
	new_dyna_pcsx_mem_reset();
	new_dynarec_invalidate_all_pages();
	new_dyna_restore();
	pending_exception = 1;
}

// execute until predefined leave points
// (HLE softcall exit and BIOS fastboot end)
static void ari64_execute_until()
{
	schedule_timeslice();

	evprintf("ari64_execute %08x, %u->%u (%d)\n", psxRegs.pc,
		psxRegs.cycle, next_interupt, next_interupt - psxRegs.cycle);

	new_dyna_start(dynarec_local);

	evprintf("ari64_execute end %08x, %u->%u (%d)\n", psxRegs.pc,
		psxRegs.cycle, next_interupt, next_interupt - psxRegs.cycle);
}

static void ari64_execute()
{
	while (!stop) {
		ari64_execute_until();
		evprintf("drc left @%08x\n", psxRegs.pc);
	}
}

static void ari64_execute_block(enum blockExecCaller caller)
{
	if (caller == EXEC_CALLER_BOOT)
		stop++;

	ari64_execute_until();

	if (caller == EXEC_CALLER_BOOT)
		stop--;
}

static void ari64_clear(u32 addr, u32 size)
{
	size *= 4; /* PCSX uses DMA units (words) */

	evprintf("ari64_clear %08x %04x\n", addr, size);

	new_dynarec_invalidate_range(addr, addr + size);
}

static void ari64_notify(enum R3000Anote note, void *data) {
	switch (note)
	{
	case R3000ACPU_NOTIFY_CACHE_UNISOLATED:
	case R3000ACPU_NOTIFY_CACHE_ISOLATED:
		new_dyna_pcsx_mem_isolate(note == R3000ACPU_NOTIFY_CACHE_ISOLATED);
		break;
	case R3000ACPU_NOTIFY_BEFORE_SAVE:
		break;
	case R3000ACPU_NOTIFY_AFTER_LOAD:
		ari64_reset();
		break;
	}
}

static void ari64_apply_config()
{
	intApplyConfig();

	if (Config.DisableStalls)
		new_dynarec_hacks |= NDHACK_NO_STALLS;
	else
		new_dynarec_hacks &= ~NDHACK_NO_STALLS;

	if (Config.cycle_multiplier != cycle_multiplier_old
	    || new_dynarec_hacks != new_dynarec_hacks_old)
	{
		new_dynarec_clear_full();
	}
}

static void ari64_shutdown()
{
	new_dynarec_cleanup();
	new_dyna_pcsx_mem_shutdown();
}

R3000Acpu psxRec = {
	ari64_init,
	ari64_reset,
	ari64_execute,
	ari64_execute_block,
	ari64_clear,
	ari64_notify,
	ari64_apply_config,
	ari64_shutdown
};

#else // if DRC_DISABLE

unsigned int address;
int pending_exception, stop;
u32 next_interupt;
int new_dynarec_did_compile;
int cycle_multiplier_old;
int new_dynarec_hacks_pergame;
int new_dynarec_hacks_old;
int new_dynarec_hacks;
void *psxH_ptr;
void *zeromem_ptr;
u32 zero_mem[0x1000/4];
void *mem_rtab;
void *scratch_buf_ptr;
void new_dynarec_init() {}
void new_dyna_start(void *context) {}
void new_dynarec_cleanup() {}
void new_dynarec_clear_full() {}
void new_dynarec_invalidate_all_pages() {}
void new_dynarec_invalidate_range(unsigned int start, unsigned int end) {}
void new_dyna_pcsx_mem_init(void) {}
void new_dyna_pcsx_mem_reset(void) {}
void new_dyna_pcsx_mem_load_state(void) {}
void new_dyna_pcsx_mem_isolate(int enable) {}
void new_dyna_pcsx_mem_shutdown(void) {}
int  new_dynarec_save_blocks(void *save, int size) { return 0; }
void new_dynarec_load_blocks(const void *save, int size) {}
#endif

#ifdef DRC_DBG

#include <stddef.h>
static FILE *f;
u32 irq_test_cycle;
u32 handler_cycle;
u32 last_io_addr;

void dump_mem(const char *fname, void *mem, size_t size)
{
	FILE *f1 = fopen(fname, "wb");
	if (f1 == NULL)
		f1 = fopen(strrchr(fname, '/') + 1, "wb");
	fwrite(mem, 1, size, f1);
	fclose(f1);
}

static u32 memcheck_read(u32 a)
{
	if ((a >> 16) == 0x1f80)
		// scratchpad/IO
		return *(u32 *)(psxH + (a & 0xfffc));

	if ((a >> 16) == 0x1f00)
		// parallel
		return *(u32 *)(psxP + (a & 0xfffc));

//	if ((a & ~0xe0600000) < 0x200000)
	// RAM
	return *(u32 *)(psxM + (a & 0x1ffffc));
}

#if 0
void do_insn_trace(void)
{
	static psxRegisters oldregs;
	static u32 event_cycles_o[PSXINT_COUNT];
	u32 *allregs_p = (void *)&psxRegs;
	u32 *allregs_o = (void *)&oldregs;
	u32 io_data;
	int i;
	u8 byte;

	//last_io_addr = 0x5e2c8;
	if (f == NULL)
		f = fopen("tracelog", "wb");

	// log reg changes
	oldregs.code = psxRegs.code; // don't care
	for (i = 0; i < offsetof(psxRegisters, intCycle) / 4; i++) {
		if (allregs_p[i] != allregs_o[i]) {
			fwrite(&i, 1, 1, f);
			fwrite(&allregs_p[i], 1, 4, f);
			allregs_o[i] = allregs_p[i];
		}
	}
	// log event changes
	for (i = 0; i < PSXINT_COUNT; i++) {
		if (event_cycles[i] != event_cycles_o[i]) {
			byte = 0xf8;
			fwrite(&byte, 1, 1, f);
			fwrite(&i, 1, 1, f);
			fwrite(&event_cycles[i], 1, 4, f);
			event_cycles_o[i] = event_cycles[i];
		}
	}
	#define SAVE_IF_CHANGED(code_, name_) { \
		static u32 old_##name_ = 0xbad0c0de; \
		if (old_##name_ != name_) { \
			byte = code_; \
			fwrite(&byte, 1, 1, f); \
			fwrite(&name_, 1, 4, f); \
			old_##name_ = name_; \
		} \
	}
	SAVE_IF_CHANGED(0xfb, irq_test_cycle);
	SAVE_IF_CHANGED(0xfc, handler_cycle);
	SAVE_IF_CHANGED(0xfd, last_io_addr);
	io_data = memcheck_read(last_io_addr);
	SAVE_IF_CHANGED(0xfe, io_data);
	byte = 0xff;
	fwrite(&byte, 1, 1, f);

#if 0
	if (psxRegs.cycle == 190230) {
		dump_mem("/mnt/ntz/dev/pnd/tmp/psxram_i.dump", psxM, 0x200000);
		dump_mem("/mnt/ntz/dev/pnd/tmp/psxregs_i.dump", psxH, 0x10000);
		printf("dumped\n");
		exit(1);
	}
#endif
}
#endif

static const char *regnames[offsetof(psxRegisters, intCycle) / 4] = {
	"r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
	"r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",
	"r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
	"r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
	"lo",  "hi",
	"C0_0",  "C0_1",  "C0_2",  "C0_3",  "C0_4",  "C0_5",  "C0_6",  "C0_7",
	"C0_8",  "C0_9",  "C0_10", "C0_11", "C0_12", "C0_13", "C0_14", "C0_15",
	"C0_16", "C0_17", "C0_18", "C0_19", "C0_20", "C0_21", "C0_22", "C0_23",
	"C0_24", "C0_25", "C0_26", "C0_27", "C0_28", "C0_29", "C0_30", "C0_31",

	"C2D0",  "C2D1",  "C2D2",  "C2D3",  "C2D4",  "C2D5",  "C2D6",  "C2D7",
	"C2D8",  "C2D9",  "C2D10", "C2D11", "C2D12", "C2D13", "C2D14", "C2D15",
	"C2D16", "C2D17", "C2D18", "C2D19", "C2D20", "C2D21", "C2D22", "C2D23",
	"C2D24", "C2D25", "C2D26", "C2D27", "C2D28", "C2D29", "C2D30", "C2D31",

	"C2C0",  "C2C1",  "C2C2",  "C2C3",  "C2C4",  "C2C5",  "C2C6",  "C2C7",
	"C2C8",  "C2C9",  "C2C10", "C2C11", "C2C12", "C2C13", "C2C14", "C2C15",
	"C2C16", "C2C17", "C2C18", "C2C19", "C2C20", "C2C21", "C2C22", "C2C23",
	"C2C24", "C2C25", "C2C26", "C2C27", "C2C28", "C2C29", "C2C30", "C2C31",

	"PC", "code", "cycle", "interrupt",
};

static struct {
	int reg;
	u32 val, val_expect;
	u32 pc, cycle;
} miss_log[64];
static int miss_log_i;
#define miss_log_len (sizeof(miss_log)/sizeof(miss_log[0]))
#define miss_log_mask (miss_log_len-1)

static void miss_log_add(int reg, u32 val, u32 val_expect, u32 pc, u32 cycle)
{
	miss_log[miss_log_i].reg = reg;
	miss_log[miss_log_i].val = val;
	miss_log[miss_log_i].val_expect = val_expect;
	miss_log[miss_log_i].pc = pc;
	miss_log[miss_log_i].cycle = cycle;
	miss_log_i = (miss_log_i + 1) & miss_log_mask;
}

void breakme() {}

void do_insn_cmp(void)
{
	extern int last_count;
	static psxRegisters rregs;
	static u32 mem_addr, mem_val;
	static u32 irq_test_cycle_intr;
	static u32 handler_cycle_intr;
	u32 *allregs_p = (void *)&psxRegs;
	u32 *allregs_e = (void *)&rregs;
	static u32 ppc, failcount;
	int i, ret, bad = 0, fatal = 0, which_event = -1;
	u32 ev_cycles = 0;
	u8 code;

	if (f == NULL)
		f = fopen("tracelog", "rb");

	while (1) {
		if ((ret = fread(&code, 1, 1, f)) <= 0)
			break;
		if (ret <= 0)
			break;
		if (code == 0xff)
			break;
		switch (code) {
		case 0xf8:
			which_event = 0;
			fread(&which_event, 1, 1, f);
			fread(&ev_cycles, 1, 4, f);
			continue;
		case 0xfb:
			fread(&irq_test_cycle_intr, 1, 4, f);
			continue;
		case 0xfc:
			fread(&handler_cycle_intr, 1, 4, f);
			continue;
		case 0xfd:
			fread(&mem_addr, 1, 4, f);
			continue;
		case 0xfe:
			fread(&mem_val, 1, 4, f);
			continue;
		}
		assert(code < offsetof(psxRegisters, intCycle) / 4);
		fread(&allregs_e[code], 1, 4, f);
	}

	if (ret <= 0) {
		printf("EOF?\n");
		exit(1);
	}

	psxRegs.code = rregs.code; // don't care
	psxRegs.cycle += last_count;
	//psxRegs.cycle = rregs.cycle; // needs reload in _cmp
	psxRegs.CP0.r[9] = rregs.CP0.r[9]; // Count

	//if (psxRegs.cycle == 166172) breakme();

	if (which_event >= 0 && event_cycles[which_event] != ev_cycles) {
		printf("bad ev_cycles #%d: %u %u / %u\n", which_event,
			event_cycles[which_event], ev_cycles, psxRegs.cycle);
		fatal = 1;
	}

	if (irq_test_cycle > irq_test_cycle_intr) {
		printf("bad irq_test_cycle: %u %u\n", irq_test_cycle, irq_test_cycle_intr);
		fatal = 1;
	}

	if (handler_cycle != handler_cycle_intr) {
		printf("bad handler_cycle: %u %u\n", handler_cycle, handler_cycle_intr);
		fatal = 1;
	}

	if (mem_val != memcheck_read(mem_addr)) {
		printf("bad mem @%08x: %08x %08x\n", mem_addr, memcheck_read(mem_addr), mem_val);
		fatal = 1;
	}

	if (!fatal && !memcmp(&psxRegs, &rregs, offsetof(psxRegisters, intCycle))) {
		failcount = 0;
		goto ok;
	}

	for (i = 0; i < offsetof(psxRegisters, intCycle) / 4; i++) {
		if (allregs_p[i] != allregs_e[i]) {
			miss_log_add(i, allregs_p[i], allregs_e[i], psxRegs.pc, psxRegs.cycle);
			bad++;
			if (i > 32+2)
				fatal = 1;
		}
	}

	if (!fatal && psxRegs.pc == rregs.pc && bad < 6 && failcount < 32) {
		static int last_mcycle;
		if (last_mcycle != psxRegs.cycle >> 20) {
			printf("%u\n", psxRegs.cycle);
			last_mcycle = psxRegs.cycle >> 20;
		}
		failcount++;
		goto ok;
	}

	for (i = 0; i < miss_log_len; i++, miss_log_i = (miss_log_i + 1) & miss_log_mask)
		printf("bad %5s: %08x %08x, pc=%08x, cycle %u\n",
			regnames[miss_log[miss_log_i].reg], miss_log[miss_log_i].val,
			miss_log[miss_log_i].val_expect, miss_log[miss_log_i].pc, miss_log[miss_log_i].cycle);
	printf("-- %d\n", bad);
	for (i = 0; i < 8; i++)
		printf("r%d=%08x r%2d=%08x r%2d=%08x r%2d=%08x\n", i, allregs_p[i],
			i+8, allregs_p[i+8], i+16, allregs_p[i+16], i+24, allregs_p[i+24]);
	printf("PC: %08x/%08x, cycle %u, next %u\n", psxRegs.pc, ppc, psxRegs.cycle, next_interupt);
	//dump_mem("/tmp/psxram.dump", psxM, 0x200000);
	//dump_mem("/mnt/ntz/dev/pnd/tmp/psxregs.dump", psxH, 0x10000);
	exit(1);
ok:
	//psxRegs.cycle = rregs.cycle + 2; // sync timing
	ppc = psxRegs.pc;
}

#endif
