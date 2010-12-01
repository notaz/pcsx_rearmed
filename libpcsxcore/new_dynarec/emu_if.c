// pending_exception?
// swi 0 in do_unalignedwritestub?
#include <stdio.h>

#include "emu_if.h"
#include "../psxmem.h"
#include "../psxhle.h"

//#define memprintf printf
#define memprintf(...)
//#define evprintf printf
#define evprintf(...)

//#define DRC_DBG

char invalid_code[0x100000];

void MTC0_()
{
	extern void psxMTC0();

	printf("ari64 MTC0 %08x\n", psxRegs.code);
	psxMTC0();
	pending_exception = 1; /* FIXME? */
}

void gen_interupt()
{
	evprintf("ari64_gen_interupt\n");
	evprintf("  +ge %08x, %d->%d\n", psxRegs.pc, psxRegs.cycle, next_interupt);
#ifdef DRC_DBG
	psxRegs.cycle += 2;
#endif

	psxBranchTest();

	next_interupt = psxNextsCounter + psxNextCounter;
	evprintf("  -ge %08x, %d->%d\n", psxRegs.pc, psxRegs.cycle, next_interupt);

	pending_exception = 1; /* FIXME */
}

void check_interupt()
{
	printf("ari64_check_interupt\n");
}

void read_nomem_new()
{
	printf("ari64_read_nomem_new\n");
}

static void read_mem8()
{
	memprintf("ari64_read_mem8  %08x, PC~=%08x\n", address, psxRegs.pc);
	readmem_word = psxMemRead8(address) & 0xff;
}

static void read_mem16()
{
	memprintf("ari64_read_mem16 %08x, PC~=%08x\n", address, psxRegs.pc);
	readmem_word = psxMemRead16(address) & 0xffff;
}

static void read_mem32()
{
	memprintf("ari64_read_mem32 %08x, PC~=%08x\n", address, psxRegs.pc);
	readmem_word = psxMemRead32(address);
}

static void write_mem8()
{
	memprintf("ari64_write_mem8  %08x,       %02x, PC~=%08x\n", address, byte, psxRegs.pc);
	psxMemWrite8(address, byte);
}

static void write_mem16()
{
	memprintf("ari64_write_mem16 %08x,     %04x, PC~=%08x\n", address, hword, psxRegs.pc);
	psxMemWrite16(address, hword);
}

static void write_mem32()
{
	memprintf("ari64_write_mem32 %08x, %08x, PC~=%08x\n", address, word, psxRegs.pc);
	psxMemWrite32(address, word);
}

void (*readmem[0x10000])();
void (*readmemb[0x10000])();
void (*readmemh[0x10000])();
void (*writemem[0x10000])();
void (*writememb[0x10000])();
void (*writememh[0x10000])();


static int ari64_init()
{
	size_t i;
	new_dynarec_init();

	for (i = 0; i < sizeof(readmem) / sizeof(readmem[0]); i++) {
		readmemb[i] = read_mem8;
		readmemh[i] = read_mem16;
		readmem[i] = read_mem32;
		writememb[i] = write_mem8;
		writememh[i] = write_mem16;
		writemem[i] = write_mem32;
	}

	psxHLEt_addr = (void *)psxHLEt;
}

static void ari64_reset()
{
	/* hmh */
	printf("ari64_reset\n");
}

static void ari64_execute()
{
	next_interupt = psxNextsCounter + psxNextCounter;

	evprintf("psxNextsCounter %d, psxNextCounter %d, Count %d\n", psxNextsCounter, psxNextCounter, psxRegs.CP0.r[9]);
	evprintf("ari64_execute %08x, %d->%d\n", psxRegs.pc, psxRegs.cycle, next_interupt);
	new_dyna_start(psxRegs.pc);
	evprintf("ari64_execute end %08x, %d->%d\n", psxRegs.pc, psxRegs.cycle, next_interupt);
}

static void ari64_clear(u32 Addr, u32 Size)
{
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
#if 1
	ari64_execute,
	ari64_execute,
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
int pending_exception;
unsigned int next_interupt;
void *psxHLEt_addr;
void new_dynarec_init() {}
int  new_dyna_start() {}
void new_dynarec_cleanup() {}
#endif

#ifdef DRC_DBG

#include <stddef.h>
static FILE *f;
extern u32 last_io_addr;

static void dump_mem(const char *fname, void *mem, size_t size)
{
	FILE *f1 = fopen(fname, "wb");
	fwrite(mem, 1, size, f1);
	fclose(f1);
}

void do_insn_trace(void)
{
	static psxRegisters oldregs;
	static u32 old_io_addr = (u32)-1;
	static u32 old_io_data = 0xbad0c0de;
	u32 *allregs_p = (void *)&psxRegs;
	u32 *allregs_o = (void *)&oldregs;
	u32 *io_data;
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
	io_data = (void *)(psxM + (last_io_addr&0x1ffffc));
	if (old_io_data != *io_data) {
		byte = 0xfe;
		fwrite(&byte, 1, 1, f);
		fwrite(io_data, 1, 4, f);
		old_io_data = *io_data;
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
//if (psxRegs.cycle > 11296376) printf("pc=%08x %u  %08x\n", psxRegs.pc, psxRegs.cycle, psxRegs.interrupt);

	mem_addr &= 0x1ffffc;

	if (memcmp(&psxRegs, &rregs, offsetof(psxRegisters, intCycle)) == 0 &&
			mem_val == *(u32 *)(psxM + mem_addr)
	   ) {
		failcount = 0;
		goto ok;
	}

	for (i = 0; i < offsetof(psxRegisters, intCycle) / 4; i++) {
		if (allregs_p[i] != allregs_e[i]) {
			printf("bad %5s: %08x %08x, pc=%08x, cycle %u\n",
				regnames[i], allregs_p[i], allregs_e[i], psxRegs.pc, psxRegs.cycle);
			bad++;
		}
	}

	if (mem_val != *(u32 *)(psxM + mem_addr)) {
		printf("bad mem @%08x: %08x %08x\n", mem_addr, *(u32 *)(psxM + mem_addr), mem_val);
		goto end;
	}

	if (psxRegs.pc == rregs.pc && bad < 6 && failcount < 32) {
		printf("-- %d\n", bad);
		failcount++;
		goto ok;
	}

end:
	printf("PC: %08x/%08x, cycle %u\n", psxRegs.pc, ppc, psxRegs.cycle);
	dump_mem("/mnt/ntz/dev/pnd/tmp/psxram.dump", psxM, 0x200000);
	dump_mem("/mnt/ntz/dev/pnd/tmp/psxregs.dump", psxH, 0x10000);
	exit(1);
ok:
	psxRegs.cycle = rregs.cycle + 2; // sync timing
	ppc = psxRegs.pc;
}

#endif
