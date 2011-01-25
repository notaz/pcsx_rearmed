/*
 * (C) Gražvydas "notaz" Ignotas, 2010
 *
 * This work is licensed under the terms of GNU GPL version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>

#include "emu_if.h"
#include "pcsxmem.h"
#include "../psxhle.h"
#include "../r3000a.h"
#include "../cdrom.h"
#include "../psxdma.h"
#include "../mdec.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

//#define evprintf printf
#define evprintf(...)

char invalid_code[0x100000];
u32 event_cycles[PSXINT_COUNT];

static void schedule_timeslice(void)
{
	u32 i, c = psxRegs.cycle;
	s32 min, dif;

	min = psxNextsCounter + psxNextCounter - c;
	for (i = 0; i < ARRAY_SIZE(event_cycles); i++) {
		dif = event_cycles[i] - c;
		//evprintf("  ev %d\n", dif);
		if (0 < dif && dif < min)
			min = dif;
	}
	next_interupt = c + min;

#if 0
	static u32 cnt, last_cycle;
	static u64 sum;
	if (last_cycle) {
		cnt++;
		sum += psxRegs.cycle - last_cycle;
		if ((cnt & 0xff) == 0)
			printf("%u\n", (u32)(sum / cnt));
	}
	last_cycle = psxRegs.cycle;
#endif
}

typedef void (irq_func)();

static irq_func * const irq_funcs[] = {
	[PSXINT_SIO]	= sioInterrupt,
	[PSXINT_CDR]	= cdrInterrupt,
	[PSXINT_CDREAD]	= cdrReadInterrupt,
	[PSXINT_GPUDMA]	= gpuInterrupt,
	[PSXINT_MDECOUTDMA] = mdec1Interrupt,
	[PSXINT_SPUDMA]	= spuInterrupt,
	[PSXINT_MDECINDMA] = mdec0Interrupt,
	[PSXINT_GPUOTCDMA] = gpuotcInterrupt,
	[PSXINT_CDRDMA] = cdrDmaInterrupt,
	[PSXINT_CDRLID] = cdrLidSeekInterrupt,
};

/* local dupe of psxBranchTest, using event_cycles */
static void irq_test(void)
{
	u32 irqs = psxRegs.interrupt;
	u32 cycle = psxRegs.cycle;
	u32 irq, irq_bits;

	if ((psxRegs.cycle - psxNextsCounter) >= psxNextCounter)
		psxRcntUpdate();

	// irq_funcs() may queue more irqs
	psxRegs.interrupt = 0;

	for (irq = 0, irq_bits = irqs; irq_bits != 0; irq++, irq_bits >>= 1) {
		if (!(irq_bits & 1))
			continue;
		if ((s32)(cycle - event_cycles[irq]) >= 0) {
			irqs &= ~(1 << irq);
			irq_funcs[irq]();
		}
	}
	psxRegs.interrupt |= irqs;

	if ((psxHu32(0x1070) & psxHu32(0x1074)) && (Status & 0x401) == 0x401) {
		psxException(0x400, 0);
		pending_exception = 1;
	}
}

void gen_interupt()
{
	evprintf("  +ge %08x, %u->%u\n", psxRegs.pc, psxRegs.cycle, next_interupt);
#ifdef DRC_DBG
	psxRegs.cycle += 2;
#endif

	irq_test();
	//psxBranchTest();
	//pending_exception = 1;

	schedule_timeslice();

	evprintf("  -ge %08x, %u->%u (%d)\n", psxRegs.pc, psxRegs.cycle,
		next_interupt, next_interupt - psxRegs.cycle);
}

// from interpreter
extern void MTC0(int reg, u32 val);

void pcsx_mtc0(u32 reg)
{
	evprintf("MTC0 %d #%x @%08x %u\n", reg, readmem_word, psxRegs.pc, psxRegs.cycle);
	MTC0(reg, readmem_word);
	gen_interupt();
}

void pcsx_mtc0_ds(u32 reg)
{
	evprintf("MTC0 %d #%x @%08x %u\n", reg, readmem_word, psxRegs.pc, psxRegs.cycle);
	MTC0(reg, readmem_word);
}

void new_dyna_save(void)
{
	// psxRegs.intCycle is always maintained, no need to convert
}

void new_dyna_restore(void)
{
	int i;
	for (i = 0; i < PSXINT_COUNT; i++)
		event_cycles[i] = psxRegs.intCycle[i].sCycle + psxRegs.intCycle[i].cycle;
}

void *gte_handlers[64];

/* from gte.txt.. not sure if this is any good. */
const char gte_cycletab[64] = {
	/*   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f */
	 0, 15,  0,  0,  0,  0,  8,  0,  0,  0,  0,  0,  6,  0,  0,  0,
	 8,  8,  8, 19, 13,  0, 44,  0,  0,  0,  0, 17, 11,  0, 14,  0,
	30,  0,  0,  0,  0,  0,  0,  0,  5,  8, 17,  0,  0,  5,  6,  0,
	23,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  5,  5, 39,
};

static int ari64_init()
{
	extern void (*psxCP2[64])();
	extern void psxNULL();
	extern void *psxH_ptr;
	size_t i;

	new_dynarec_init();
	new_dyna_pcsx_mem_init();

	for (i = 0; i < ARRAY_SIZE(gte_handlers); i++)
		if (psxCP2[i] != psxNULL)
			gte_handlers[i] = psxCP2[i];

	psxH_ptr = psxH;

	return 0;
}

static void ari64_reset()
{
	printf("ari64_reset\n");
	new_dyna_pcsx_mem_reset();
	invalidate_all_pages();
	pending_exception = 1;
}

// execute until predefined leave points
// (HLE softcall exit and BIOS fastboot end)
static void ari64_execute_until()
{
	schedule_timeslice();

	evprintf("ari64_execute %08x, %u->%u (%d)\n", psxRegs.pc,
		psxRegs.cycle, next_interupt, next_interupt - psxRegs.cycle);

	new_dyna_start();

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

static void ari64_clear(u32 addr, u32 size)
{
	u32 start, end;

	size *= 4; /* PCSX uses DMA units */

	evprintf("ari64_clear %08x %04x\n", addr, size);

	/* check for RAM mirrors */
	if ((addr & ~0xe0600000) < 0x200000) {
		addr &= ~0xe0600000;
		addr |=  0x80000000;
	}

	start = addr >> 12;
	end = (addr + size) >> 12;

	for (; start <= end; start++)
		if (!invalid_code[start])
			invalidate_block(start);
}

static void ari64_shutdown()
{
	new_dynarec_cleanup();
}

extern void intExecute();
extern void intExecuteT();
extern void intExecuteBlock();
extern void intExecuteBlockT();
#ifndef DRC_DBG
#define intExecuteT intExecute
#define intExecuteBlockT intExecuteBlock
#endif

R3000Acpu psxRec = {
	ari64_init,
	ari64_reset,
#if defined(__arm__)
	ari64_execute,
	ari64_execute_until,
#else
	intExecuteT,
	intExecuteBlockT,
#endif
	ari64_clear,
	ari64_shutdown
};

// TODO: rm
#ifndef DRC_DBG
void do_insn_trace() {}
void do_insn_cmp() {}
#endif

#if defined(__x86_64__) || defined(__i386__)
unsigned int address, readmem_word, word;
unsigned short hword;
unsigned char byte;
int pending_exception, stop;
unsigned int next_interupt;
void *psxH_ptr;
void new_dynarec_init() {}
void new_dyna_start() {}
void new_dynarec_cleanup() {}
void invalidate_all_pages() {}
void invalidate_block(unsigned int block) {}
void new_dyna_pcsx_mem_init(void) {}
void new_dyna_pcsx_mem_reset(void) {}
#endif

#ifdef DRC_DBG

#include <stddef.h>
static FILE *f;
extern u32 last_io_addr;

static void dump_mem(const char *fname, void *mem, size_t size)
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

void do_insn_trace(void)
{
	static psxRegisters oldregs;
	static u32 old_io_addr = (u32)-1;
	static u32 old_io_data = 0xbad0c0de;
	u32 *allregs_p = (void *)&psxRegs;
	u32 *allregs_o = (void *)&oldregs;
	u32 io_data;
	int i;
	u8 byte;

//last_io_addr = 0x5e2c8;
	if (f == NULL)
		f = fopen("tracelog", "wb");

	oldregs.code = psxRegs.code; // don't care
	for (i = 0; i < offsetof(psxRegisters, intCycle) / 4; i++) {
		if (allregs_p[i] != allregs_o[i]) {
			fwrite(&i, 1, 1, f);
			fwrite(&allregs_p[i], 1, 4, f);
			allregs_o[i] = allregs_p[i];
		}
	}
	if (old_io_addr != last_io_addr) {
		byte = 0xfd;
		fwrite(&byte, 1, 1, f);
		fwrite(&last_io_addr, 1, 4, f);
		old_io_addr = last_io_addr;
	}
	io_data = memcheck_read(last_io_addr);
	if (old_io_data != io_data) {
		byte = 0xfe;
		fwrite(&byte, 1, 1, f);
		fwrite(&io_data, 1, 4, f);
		old_io_data = io_data;
	}
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
	static psxRegisters rregs;
	static u32 mem_addr, mem_val;
	u32 *allregs_p = (void *)&psxRegs;
	u32 *allregs_e = (void *)&rregs;
	static u32 ppc, failcount;
	int i, ret, bad = 0;
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
		if (code == 0xfd) {
			if ((ret = fread(&mem_addr, 1, 4, f)) <= 0)
				break;
			continue;
		}
		if (code == 0xfe) {
			if ((ret = fread(&mem_val, 1, 4, f)) <= 0)
				break;
			continue;
		}
		if ((ret = fread(&allregs_e[code], 1, 4, f)) <= 0)
			break;
	}

	if (ret <= 0) {
		printf("EOF?\n");
		goto end;
	}

	psxRegs.code = rregs.code; // don't care
	psxRegs.cycle = rregs.cycle;
	psxRegs.CP0.r[9] = rregs.CP0.r[9]; // Count

//if (psxRegs.cycle == 166172) breakme();

	if (memcmp(&psxRegs, &rregs, offsetof(psxRegisters, intCycle)) == 0 &&
			mem_val == memcheck_read(mem_addr)
	   ) {
		failcount = 0;
		goto ok;
	}

	for (i = 0; i < offsetof(psxRegisters, intCycle) / 4; i++) {
		if (allregs_p[i] != allregs_e[i]) {
			miss_log_add(i, allregs_p[i], allregs_e[i], psxRegs.pc, psxRegs.cycle);
			bad++;
		}
	}

	if (mem_val != memcheck_read(mem_addr)) {
		printf("bad mem @%08x: %08x %08x\n", mem_addr, memcheck_read(mem_addr), mem_val);
		goto end;
	}

	if (psxRegs.pc == rregs.pc && bad < 6 && failcount < 32) {
		static int last_mcycle;
		if (last_mcycle != psxRegs.cycle >> 20) {
			printf("%u\n", psxRegs.cycle);
			last_mcycle = psxRegs.cycle >> 20;
		}
		failcount++;
		goto ok;
	}

end:
	for (i = 0; i < miss_log_len; i++, miss_log_i = (miss_log_i + 1) & miss_log_mask)
		printf("bad %5s: %08x %08x, pc=%08x, cycle %u\n",
			regnames[miss_log[miss_log_i].reg], miss_log[miss_log_i].val,
			miss_log[miss_log_i].val_expect, miss_log[miss_log_i].pc, miss_log[miss_log_i].cycle);
	printf("-- %d\n", bad);
	for (i = 0; i < 8; i++)
		printf("r%d=%08x r%2d=%08x r%2d=%08x r%2d=%08x\n", i, allregs_p[i],
			i+8, allregs_p[i+8], i+16, allregs_p[i+16], i+24, allregs_p[i+24]);
	printf("PC: %08x/%08x, cycle %u\n", psxRegs.pc, ppc, psxRegs.cycle);
	dump_mem("/mnt/ntz/dev/pnd/tmp/psxram.dump", psxM, 0x200000);
	dump_mem("/mnt/ntz/dev/pnd/tmp/psxregs.dump", psxH, 0x10000);
	exit(1);
ok:
	psxRegs.cycle = rregs.cycle + 2; // sync timing
	ppc = psxRegs.pc;
}

#endif
