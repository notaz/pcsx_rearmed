#include <stdio.h>

#include "emu_if.h"

char invalid_code[0x100000];

void MFC0(void)
{
	printf("MFC0!\n");
}

void gen_interupt()
{
	printf("gen_interupt\n");
}

void check_interupt()
{
	printf("check_interupt\n");
}

void read_nomem_new()
{
	printf("read_nomem_new\n");
}

static void read_mem()
{
	printf("read_mem %08x\n", address);
}

static void write_mem()
{
	printf("write_mem %08x\n", address);
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
		readmem[i] = read_mem;
		writemem[i] = write_mem;
	}
	memcpy(readmemb, readmem, sizeof(readmem));
	memcpy(readmemh, readmem, sizeof(readmem));
	memcpy(writememb, writemem, sizeof(writemem));
	memcpy(writememh, writemem, sizeof(writemem));
}

static void ari64_reset()
{
	/* hmh */
	printf("ari64_reset\n");
}

static void ari64_execute()
{
/*
	FILE *f = fopen("/mnt/ntz/dev/pnd/tmp/ram.dump", "wb");
	fwrite((void *)0x80000000, 1, 0x200000, f);
	fclose(f);
	exit(1);
*/
	printf("psxNextsCounter %d, psxNextCounter %d\n", psxNextsCounter, psxNextCounter);
	printf("ari64_execute %08x\n", psxRegs.pc);
	new_dyna_start(psxRegs.pc);
}

static void ari64_clear(u32 Addr, u32 Size)
{
}

static void ari64_shutdown()
{
	new_dynarec_cleanup();
}

R3000Acpu psxRec = {
	ari64_init,
	ari64_reset,
	ari64_execute,
// TODO	recExecuteBlock,
	ari64_clear,
	ari64_shutdown
};
