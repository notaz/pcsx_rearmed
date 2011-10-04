/*
 * (C) Gražvydas "notaz" Ignotas, 2010-2011
 *
 * This work is licensed under the terms of GNU GPL version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <sys/mman.h>
#include "../psxhw.h"
#include "../cdrom.h"
#include "../mdec.h"
#include "emu_if.h"
#include "pcsxmem.h"

//#define memprintf printf
#define memprintf(...)

static u8 unmapped_mem[0x1000];
int pcsx_ram_is_ro;

static void read_mem8()
{
	memprintf("ari64_read_mem8  %08x @%08x %u\n", address, psxRegs.pc, psxRegs.cycle);
	readmem_word = psxMemRead8(address) & 0xff;
}

static void read_mem16()
{
	memprintf("ari64_read_mem16 %08x @%08x %u\n", address, psxRegs.pc, psxRegs.cycle);
	readmem_word = psxMemRead16(address) & 0xffff;
}

static void read_mem32()
{
	memprintf("ari64_read_mem32 %08x @%08x %u\n", address, psxRegs.pc, psxRegs.cycle);
	readmem_word = psxMemRead32(address);
}

static void write_mem8()
{
	memprintf("ari64_write_mem8  %08x,       %02x @%08x %u\n", address, byte, psxRegs.pc, psxRegs.cycle);
	psxMemWrite8(address, byte);
}

static void write_mem16()
{
	memprintf("ari64_write_mem16 %08x,     %04x @%08x %u\n", address, hword, psxRegs.pc, psxRegs.cycle);
	psxMemWrite16(address, hword);
}

static void write_mem32()
{
	memprintf("ari64_write_mem32 %08x, %08x @%08x %u\n", address, word, psxRegs.pc, psxRegs.cycle);
	psxMemWrite32(address, word);
}

static void read_mem_dummy()
{
	readmem_word = 0;
}

static void write_mem_dummy(u32 data)
{
	memprintf("unmapped w %08x, %08x @%08x %u\n", address, data, psxRegs.pc, psxRegs.cycle);
}

extern void ari_read_ram8();
extern void ari_read_ram16();
extern void ari_read_ram32();
extern void ari_read_ram_mirror8();
extern void ari_read_ram_mirror16();
extern void ari_read_ram_mirror32();
extern void ari_write_ram8();
extern void ari_write_ram16();
extern void ari_write_ram32();
extern void ari_write_ram_mirror8();
extern void ari_write_ram_mirror16();
extern void ari_write_ram_mirror32();
extern void ari_write_ram_mirror_ro32();
extern void ari_read_bios8();
extern void ari_read_bios16();
extern void ari_read_bios32();
extern void ari_read_io8();
extern void ari_read_io16();
extern void ari_read_io32();
extern void ari_write_io8();
extern void ari_write_io16();
extern void ari_write_io32();

void (*readmem[0x10000])();
void (*readmemb[0x10000])();
void (*readmemh[0x10000])();
void (*writemem[0x10000])();
void (*writememb[0x10000])();
void (*writememh[0x10000])();

static void write_biu_()
{
	memprintf("write_biu %08x, %08x @%08x %u\n", address, word, psxRegs.pc, psxRegs.cycle);

	if (address != 0xfffe0130)
		return;

	switch (word) {
	case 0x800: case 0x804:
		pcsx_ram_is_ro = 1;
		break;
	case 0: case 0x1e988:
		pcsx_ram_is_ro = 0;
		break;
	default:
		memprintf("write_biu: unexpected val: %08x\n", word);
		break;
	}
}

/* IO handlers */
static u32 io_read_sio16()
{
	return sioRead8() | (sioRead8() << 8);
}

static u32 io_read_sio32()
{
	return sioRead8() | (sioRead8() << 8) | (sioRead8() << 16) | (sioRead8() << 24);
}

static void io_write_sio16(u32 value)
{
	sioWrite8((unsigned char)value);
	sioWrite8((unsigned char)(value>>8));
}

static void io_write_sio32(u32 value)
{
	sioWrite8((unsigned char)value);
	sioWrite8((unsigned char)(value >>  8));
	sioWrite8((unsigned char)(value >> 16));
	sioWrite8((unsigned char)(value >> 24));
}

#define make_rcnt_funcs(i) \
static u32 io_rcnt_read_count##i()  { return psxRcntRcount(i); } \
static u32 io_rcnt_read_mode##i()   { return psxRcntRmode(i); } \
static u32 io_rcnt_read_target##i() { return psxRcntRtarget(i); } \
static void io_rcnt_write_count##i(u32 val)  { psxRcntWcount(i, val & 0xffff); } \
static void io_rcnt_write_mode##i(u32 val)   { psxRcntWmode(i, val); } \
static void io_rcnt_write_target##i(u32 val) { psxRcntWtarget(i, val & 0xffff); }

make_rcnt_funcs(0)
make_rcnt_funcs(1)
make_rcnt_funcs(2)

static void io_write_ireg16(u32 value)
{
	if (Config.Sio) psxHu16ref(0x1070) |= 0x80;
	if (Config.SpuIrq) psxHu16ref(0x1070) |= 0x200;
	psxHu16ref(0x1070) &= psxHu16(0x1074) & value;
}

static void io_write_imask16(u32 value)
{
	psxHu16ref(0x1074) = value;
	if (psxHu16ref(0x1070) & value)
		new_dyna_set_event(PSXINT_NEWDRC_CHECK, 1);
}

static void io_write_ireg32(u32 value)
{
	if (Config.Sio) psxHu32ref(0x1070) |= 0x80;
	if (Config.SpuIrq) psxHu32ref(0x1070) |= 0x200;
	psxHu32ref(0x1070) &= psxHu32(0x1074) & value;
}

static void io_write_imask32(u32 value)
{
	psxHu32ref(0x1074) = value;
	if (psxHu32ref(0x1070) & value)
		new_dyna_set_event(PSXINT_NEWDRC_CHECK, 1);
}

static void io_write_dma_icr32(u32 value)
{
	u32 tmp = value & 0x00ff803f;
	tmp |= (SWAPu32(HW_DMA_ICR) & ~value) & 0x7f000000;
	if ((tmp & HW_DMA_ICR_GLOBAL_ENABLE && tmp & 0x7f000000)
	    || tmp & HW_DMA_ICR_BUS_ERROR) {
		if (!(SWAPu32(HW_DMA_ICR) & HW_DMA_ICR_IRQ_SENT))
			psxHu32ref(0x1070) |= SWAP32(8);
		tmp |= HW_DMA_ICR_IRQ_SENT;
	}
	HW_DMA_ICR = SWAPu32(tmp);
}

#define make_dma_func(n) \
static void io_write_chcr##n(u32 value) \
{ \
	HW_DMA##n##_CHCR = value; \
	if (value & 0x01000000 && HW_DMA_PCR & (8 << (n * 4))) { \
		psxDma##n(HW_DMA##n##_MADR, HW_DMA##n##_BCR, value); \
	} \
}

make_dma_func(0)
make_dma_func(1)
make_dma_func(2)
make_dma_func(3)
make_dma_func(4)
make_dma_func(6)

static void io_spu_write16(u32 value)
{
	// meh
	SPU_writeRegister(address, value);
}

static void io_spu_write32(u32 value)
{
	SPUwriteRegister wfunc = SPU_writeRegister;
	u32 a = address;

	wfunc(a, value & 0xffff);
	wfunc(a + 2, value >> 16);
}

/* IO tables for 1000-1880 */
#define IOADR8(a)  ((a) & 0xfff)
#define IOADR16(a) (((a) & 0xfff) >> 1)
#define IOADR32(a) (((a) & 0xfff) >> 2)

static const void *io_read8 [0x880] = {
	[IOADR8(0x1040)] = sioRead8,
	[IOADR8(0x1800)] = cdrRead0,
	[IOADR8(0x1801)] = cdrRead1,
	[IOADR8(0x1802)] = cdrRead2,
	[IOADR8(0x1803)] = cdrRead3,
};
static const void *io_read16[0x880/2] = {
	[IOADR16(0x1040)] = io_read_sio16,
	[IOADR16(0x1044)] = sioReadStat16,
	[IOADR16(0x1048)] = sioReadMode16,
	[IOADR16(0x104a)] = sioReadCtrl16,
	[IOADR16(0x104e)] = sioReadBaud16,
	[IOADR16(0x1100)] = io_rcnt_read_count0,
	[IOADR16(0x1104)] = io_rcnt_read_mode0,
	[IOADR16(0x1108)] = io_rcnt_read_target0,
	[IOADR16(0x1110)] = io_rcnt_read_count1,
	[IOADR16(0x1114)] = io_rcnt_read_mode1,
	[IOADR16(0x1118)] = io_rcnt_read_target1,
	[IOADR16(0x1120)] = io_rcnt_read_count2,
	[IOADR16(0x1124)] = io_rcnt_read_mode2,
	[IOADR16(0x1128)] = io_rcnt_read_target2,
};
static const void *io_read32[0x880/4] = {
	[IOADR32(0x1040)] = io_read_sio32,
	[IOADR32(0x1100)] = io_rcnt_read_count0,
	[IOADR32(0x1104)] = io_rcnt_read_mode0,
	[IOADR32(0x1108)] = io_rcnt_read_target0,
	[IOADR32(0x1110)] = io_rcnt_read_count1,
	[IOADR32(0x1114)] = io_rcnt_read_mode1,
	[IOADR32(0x1118)] = io_rcnt_read_target1,
	[IOADR32(0x1120)] = io_rcnt_read_count2,
	[IOADR32(0x1124)] = io_rcnt_read_mode2,
	[IOADR32(0x1128)] = io_rcnt_read_target2,
//	[IOADR32(0x1810)] = GPU_readData,
//	[IOADR32(0x1814)] = GPU_readStatus,
	[IOADR32(0x1820)] = mdecRead0,
	[IOADR32(0x1824)] = mdecRead1,
};
// write(u32 val)
static const void *io_write8 [0x880] = {
	[IOADR8(0x1040)] = sioWrite8,
	[IOADR8(0x1800)] = cdrWrite0,
	[IOADR8(0x1801)] = cdrWrite1,
	[IOADR8(0x1802)] = cdrWrite2,
	[IOADR8(0x1803)] = cdrWrite3,
};
static const void *io_write16[0x880/2] = {
	[IOADR16(0x1040)] = io_write_sio16,
	[IOADR16(0x1044)] = sioWriteStat16,
	[IOADR16(0x1048)] = sioWriteMode16,
	[IOADR16(0x104a)] = sioWriteCtrl16,
	[IOADR16(0x104e)] = sioWriteBaud16,
	[IOADR16(0x1070)] = io_write_ireg16,
	[IOADR16(0x1074)] = io_write_imask16,
	[IOADR16(0x1100)] = io_rcnt_write_count0,
	[IOADR16(0x1104)] = io_rcnt_write_mode0,
	[IOADR16(0x1108)] = io_rcnt_write_target0,
	[IOADR16(0x1110)] = io_rcnt_write_count1,
	[IOADR16(0x1114)] = io_rcnt_write_mode1,
	[IOADR16(0x1118)] = io_rcnt_write_target1,
	[IOADR16(0x1120)] = io_rcnt_write_count2,
	[IOADR16(0x1124)] = io_rcnt_write_mode2,
	[IOADR16(0x1128)] = io_rcnt_write_target2,
};
static const void *io_write32[0x880/4] = {
	[IOADR32(0x1040)] = io_write_sio32,
	[IOADR32(0x1070)] = io_write_ireg32,
	[IOADR32(0x1074)] = io_write_imask32,
	[IOADR32(0x1088)] = io_write_chcr0,
	[IOADR32(0x1098)] = io_write_chcr1,
	[IOADR32(0x10a8)] = io_write_chcr2,
	[IOADR32(0x10b8)] = io_write_chcr3,
	[IOADR32(0x10c8)] = io_write_chcr4,
	[IOADR32(0x10e8)] = io_write_chcr6,
	[IOADR32(0x10f4)] = io_write_dma_icr32,
	[IOADR32(0x1100)] = io_rcnt_write_count0,
	[IOADR32(0x1104)] = io_rcnt_write_mode0,
	[IOADR32(0x1108)] = io_rcnt_write_target0,
	[IOADR32(0x1110)] = io_rcnt_write_count1,
	[IOADR32(0x1114)] = io_rcnt_write_mode1,
	[IOADR32(0x1118)] = io_rcnt_write_target1,
	[IOADR32(0x1120)] = io_rcnt_write_count2,
	[IOADR32(0x1124)] = io_rcnt_write_mode2,
	[IOADR32(0x1128)] = io_rcnt_write_target2,
//	[IOADR32(0x1810)] = GPU_writeData,
//	[IOADR32(0x1814)] = GPU_writeStatus,
	[IOADR32(0x1820)] = mdecWrite0,
	[IOADR32(0x1824)] = mdecWrite1,
};

// this has to be in .bss to link into dynarec_local
struct {
	void *tab_read8;
	void *tab_read16;
	void *tab_read32;
	void *tab_write8;
	void *tab_write16;
	void *tab_write32;
	void *spu_readf;
	void *spu_writef;
} nd_pcsx_io;

static u32 *mem_readtab;
static u32 *mem_writetab;
static u32 mem_iortab[(1+2+4) * 0x1000 / 4];
static u32 mem_iowtab[(1+2+4) * 0x1000 / 4];
static u32 mem_ffwtab[(1+2+4) * 0x1000 / 4];
//static u32 mem_unmrtab[(1+2+4) * 0x1000 / 4];
static u32 mem_unmwtab[(1+2+4) * 0x1000 / 4];

static void map_item(u32 *out, const void *h, u32 flag)
{
	u32 hv = (u32)h;
	if (hv & 1)
		fprintf(stderr, "%p has LSB set\n", h);
	*out = (hv >> 1) | (flag << 31);
}

// size must be power of 2, at least 4k
#define map_l1_mem(tab, i, addr, size, base) \
	map_item(&tab[((addr)>>12) + i], (u8 *)(base) - (u32)(addr) - ((i << 12) & ~(size - 1)), 0)

#define IOMEM32(a) (((a) & 0xfff) / 4)
#define IOMEM16(a) (0x1000/4 + (((a) & 0xfff) / 2))
#define IOMEM8(a)  (0x1000/4 + 0x1000/2 + ((a) & 0xfff))

static void map_ram_write(void)
{
	int i;

	for (i = 0; i < (0x800000 >> 12); i++) {
		map_l1_mem(mem_writetab, i, 0x80000000, 0x200000, psxM);
		map_l1_mem(mem_writetab, i, 0x00000000, 0x200000, psxM);
		map_l1_mem(mem_writetab, i, 0xa0000000, 0x200000, psxM);
	}
}

static void unmap_ram_write(void)
{
	int i;

	for (i = 0; i < (0x800000 >> 12); i++) {
		map_item(&mem_writetab[0x80000|i], mem_unmwtab, 1);
		map_item(&mem_writetab[0x00000|i], mem_unmwtab, 1);
		map_item(&mem_writetab[0xa0000|i], mem_unmwtab, 1);
	}
}

static void write_biu(u32 value)
{
	memprintf("write_biu %08x, %08x @%08x %u\n", address, value, psxRegs.pc, psxRegs.cycle);

	if (address != 0xfffe0130)
		return;

	switch (value) {
	case 0x800: case 0x804:
		unmap_ram_write();
		break;
	case 0: case 0x1e988:
		map_ram_write();
		break;
	default:
		printf("write_biu: unexpected val: %08x\n", value);
		break;
	}
}

void new_dyna_pcsx_mem_init(void)
{
	int i;
#if 1
	// have to map these further to keep tcache close to .text
	mem_readtab = mmap((void *)0x08000000, 0x200000 * 4, PROT_READ | PROT_WRITE,
		MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mem_readtab == MAP_FAILED) {
		fprintf(stderr, "failed to map mem tables\n");
		exit(1);
	}
	mem_writetab = mem_readtab + 0x100000;

	// 1st level lookup:
	//   0: direct mem
	//   1: use 2nd lookup
	// 2nd level lookup:
	//   0: direct mem variable
	//   1: memhandler

	// default/unmapped memhandlers
	for (i = 0; i < 0x100000; i++) {
		//map_item(&mem_readtab[i], mem_unmrtab, 1);
		map_l1_mem(mem_readtab, i, 0, 0x1000, unmapped_mem);
		map_item(&mem_writetab[i], mem_unmwtab, 1);
	}

	// RAM and it's mirrors
	for (i = 0; i < (0x800000 >> 12); i++) {
		map_l1_mem(mem_readtab,  i, 0x80000000, 0x200000, psxM);
		map_l1_mem(mem_readtab,  i, 0x00000000, 0x200000, psxM);
		map_l1_mem(mem_readtab,  i, 0xa0000000, 0x200000, psxM);
	}
	map_ram_write();

	// BIOS and it's mirrors
	for (i = 0; i < (0x80000 >> 12); i++) {
		map_l1_mem(mem_readtab, i, 0x1fc00000, 0x80000, psxR);
		map_l1_mem(mem_readtab, i, 0xbfc00000, 0x80000, psxR);
	}

	// scratchpad
	map_l1_mem(mem_readtab, 0, 0x1f800000, 0x1000, psxH);
	map_l1_mem(mem_writetab, 0, 0x1f800000, 0x1000, psxH);

	// I/O
	map_item(&mem_readtab[0x1f801000 >> 12], mem_iortab, 1);
	map_item(&mem_writetab[0x1f801000 >> 12], mem_iowtab, 1);

	// L2
	// unmapped tables
	for (i = 0; i < (1+2+4) * 0x1000 / 4; i++)
		map_item(&mem_unmwtab[i], write_mem_dummy, 1);

	// fill IO tables
	for (i = 0; i < 0x1000/4; i++) {
		map_item(&mem_iortab[i], &psxH[0x1000], 0);
		map_item(&mem_iowtab[i], &psxH[0x1000], 0);
	}
	for (; i < 0x1000/4 + 0x1000/2; i++) {
		map_item(&mem_iortab[i], &psxH[0x1000], 0);
		map_item(&mem_iowtab[i], &psxH[0x1000], 0);
	}
	for (; i < 0x1000/4 + 0x1000/2 + 0x1000; i++) {
		map_item(&mem_iortab[i], &psxH[0x1000], 0);
		map_item(&mem_iowtab[i], &psxH[0x1000], 0);
	}

	map_item(&mem_iortab[IOMEM32(0x1040)], io_read_sio32, 1);
	map_item(&mem_iortab[IOMEM32(0x1100)], io_rcnt_read_count0, 1);
	map_item(&mem_iortab[IOMEM32(0x1104)], io_rcnt_read_mode0, 1);
	map_item(&mem_iortab[IOMEM32(0x1108)], io_rcnt_read_target0, 1);
	map_item(&mem_iortab[IOMEM32(0x1110)], io_rcnt_read_count1, 1);
	map_item(&mem_iortab[IOMEM32(0x1114)], io_rcnt_read_mode1, 1);
	map_item(&mem_iortab[IOMEM32(0x1118)], io_rcnt_read_target1, 1);
	map_item(&mem_iortab[IOMEM32(0x1120)], io_rcnt_read_count2, 1);
	map_item(&mem_iortab[IOMEM32(0x1124)], io_rcnt_read_mode2, 1);
	map_item(&mem_iortab[IOMEM32(0x1128)], io_rcnt_read_target2, 1);
//	map_item(&mem_iortab[IOMEM32(0x1810)], GPU_readData, 1);
//	map_item(&mem_iortab[IOMEM32(0x1814)], GPU_readStatus, 1);
	map_item(&mem_iortab[IOMEM32(0x1820)], mdecRead0, 1);
	map_item(&mem_iortab[IOMEM32(0x1824)], mdecRead1, 1);

	map_item(&mem_iortab[IOMEM16(0x1040)], io_read_sio16, 1);
	map_item(&mem_iortab[IOMEM16(0x1044)], sioReadStat16, 1);
	map_item(&mem_iortab[IOMEM16(0x1048)], sioReadMode16, 1);
	map_item(&mem_iortab[IOMEM16(0x104a)], sioReadCtrl16, 1);
	map_item(&mem_iortab[IOMEM16(0x104e)], sioReadBaud16, 1);
	map_item(&mem_iortab[IOMEM16(0x1100)], io_rcnt_read_count0, 1);
	map_item(&mem_iortab[IOMEM16(0x1104)], io_rcnt_read_mode0, 1);
	map_item(&mem_iortab[IOMEM16(0x1108)], io_rcnt_read_target0, 1);
	map_item(&mem_iortab[IOMEM16(0x1110)], io_rcnt_read_count1, 1);
	map_item(&mem_iortab[IOMEM16(0x1114)], io_rcnt_read_mode1, 1);
	map_item(&mem_iortab[IOMEM16(0x1118)], io_rcnt_read_target1, 1);
	map_item(&mem_iortab[IOMEM16(0x1120)], io_rcnt_read_count2, 1);
	map_item(&mem_iortab[IOMEM16(0x1124)], io_rcnt_read_mode2, 1);
	map_item(&mem_iortab[IOMEM16(0x1128)], io_rcnt_read_target2, 1);

	map_item(&mem_iortab[IOMEM8(0x1040)], sioRead8, 1);
	map_item(&mem_iortab[IOMEM8(0x1800)], cdrRead0, 1);
	map_item(&mem_iortab[IOMEM8(0x1801)], cdrRead1, 1);
	map_item(&mem_iortab[IOMEM8(0x1802)], cdrRead2, 1);
	map_item(&mem_iortab[IOMEM8(0x1803)], cdrRead3, 1);

	// write(u32 data)
	map_item(&mem_iowtab[IOMEM32(0x1040)], io_write_sio32, 1);
	map_item(&mem_iowtab[IOMEM32(0x1070)], io_write_ireg32, 1);
	map_item(&mem_iowtab[IOMEM32(0x1074)], io_write_imask32, 1);
	map_item(&mem_iowtab[IOMEM32(0x1088)], io_write_chcr0, 1);
	map_item(&mem_iowtab[IOMEM32(0x1098)], io_write_chcr1, 1);
	map_item(&mem_iowtab[IOMEM32(0x10a8)], io_write_chcr2, 1);
	map_item(&mem_iowtab[IOMEM32(0x10b8)], io_write_chcr3, 1);
	map_item(&mem_iowtab[IOMEM32(0x10c8)], io_write_chcr4, 1);
	map_item(&mem_iowtab[IOMEM32(0x10e8)], io_write_chcr6, 1);
	map_item(&mem_iowtab[IOMEM32(0x10f4)], io_write_dma_icr32, 1);
	map_item(&mem_iowtab[IOMEM32(0x1100)], io_rcnt_write_count0, 1);
	map_item(&mem_iowtab[IOMEM32(0x1104)], io_rcnt_write_mode0, 1);
	map_item(&mem_iowtab[IOMEM32(0x1108)], io_rcnt_write_target0, 1);
	map_item(&mem_iowtab[IOMEM32(0x1110)], io_rcnt_write_count1, 1);
	map_item(&mem_iowtab[IOMEM32(0x1114)], io_rcnt_write_mode1, 1);
	map_item(&mem_iowtab[IOMEM32(0x1118)], io_rcnt_write_target1, 1);
	map_item(&mem_iowtab[IOMEM32(0x1120)], io_rcnt_write_count2, 1);
	map_item(&mem_iowtab[IOMEM32(0x1124)], io_rcnt_write_mode2, 1);
	map_item(&mem_iowtab[IOMEM32(0x1128)], io_rcnt_write_target2, 1);
//	map_item(&mem_iowtab[IOMEM32(0x1810)], GPU_writeData, 1);
//	map_item(&mem_iowtab[IOMEM32(0x1814)], GPU_writeStatus, 1);
	map_item(&mem_iowtab[IOMEM32(0x1820)], mdecWrite0, 1);
	map_item(&mem_iowtab[IOMEM32(0x1824)], mdecWrite1, 1);

	map_item(&mem_iowtab[IOMEM16(0x1040)], io_write_sio16, 1);
	map_item(&mem_iowtab[IOMEM16(0x1044)], sioWriteStat16, 1);
	map_item(&mem_iowtab[IOMEM16(0x1048)], sioWriteMode16, 1);
	map_item(&mem_iowtab[IOMEM16(0x104a)], sioWriteCtrl16, 1);
	map_item(&mem_iowtab[IOMEM16(0x104e)], sioWriteBaud16, 1);
	map_item(&mem_iowtab[IOMEM16(0x1070)], io_write_ireg16, 1);
	map_item(&mem_iowtab[IOMEM16(0x1074)], io_write_imask16, 1);
	map_item(&mem_iowtab[IOMEM16(0x1100)], io_rcnt_write_count0, 1);
	map_item(&mem_iowtab[IOMEM16(0x1104)], io_rcnt_write_mode0, 1);
	map_item(&mem_iowtab[IOMEM16(0x1108)], io_rcnt_write_target0, 1);
	map_item(&mem_iowtab[IOMEM16(0x1110)], io_rcnt_write_count1, 1);
	map_item(&mem_iowtab[IOMEM16(0x1114)], io_rcnt_write_mode1, 1);
	map_item(&mem_iowtab[IOMEM16(0x1118)], io_rcnt_write_target1, 1);
	map_item(&mem_iowtab[IOMEM16(0x1120)], io_rcnt_write_count2, 1);
	map_item(&mem_iowtab[IOMEM16(0x1124)], io_rcnt_write_mode2, 1);
	map_item(&mem_iowtab[IOMEM16(0x1128)], io_rcnt_write_target2, 1);

	map_item(&mem_iowtab[IOMEM8(0x1040)], sioWrite8, 1);
	map_item(&mem_iowtab[IOMEM8(0x1800)], cdrWrite0, 1);
	map_item(&mem_iowtab[IOMEM8(0x1801)], cdrWrite1, 1);
	map_item(&mem_iowtab[IOMEM8(0x1802)], cdrWrite2, 1);
	map_item(&mem_iowtab[IOMEM8(0x1803)], cdrWrite3, 1);

	for (i = 0x1c00; i < 0x1e00; i += 2) {
		map_item(&mem_iowtab[IOMEM16(i)], io_spu_write16, 1);
		map_item(&mem_iowtab[IOMEM32(i)], io_spu_write32, 1);
	}

	// misc
	map_item(&mem_writetab[0xfffe0130 >> 12], mem_ffwtab, 1);
	for (i = 0; i < 0x1000/4 + 0x1000/2 + 0x1000; i++)
		map_item(&mem_ffwtab[i], write_biu, 1);

	mem_rtab = mem_readtab;
	mem_wtab = mem_writetab;
#endif
///
	// default/unmapped handlers
	for (i = 0; i < 0x10000; i++) {
		readmemb[i] = read_mem8;
		readmemh[i] = read_mem16;
		readmem[i] = read_mem32;
		writememb[i] = write_mem8;
		writememh[i] = write_mem16;
		writemem[i] = write_mem32;
#if 1
		readmemb[i] = readmemh[i] = readmem[i] = read_mem_dummy;
		writememb[i] = writememh[i] = writemem[i] = write_mem_dummy;
#endif
	}

#if 1
	// RAM mirrors
	for (i = 0; i < 0x80; i++) {
		readmemb[i] = readmemb[0x8000|i] = readmemb[0xa000|i] = ari_read_ram_mirror8;
		readmemh[i] = readmemh[0x8000|i] = readmemh[0xa000|i] = ari_read_ram_mirror16;
		readmem[i]  = readmem [0x8000|i] = readmem [0xa000|i] = ari_read_ram_mirror32;
		writememb[i] = writememb[0x8000|i] = writememb[0xa000|i] = ari_write_ram_mirror8;
		writememh[i] = writememh[0x8000|i] = writememh[0xa000|i] = ari_write_ram_mirror16;
		writemem[i]  = writemem [0x8000|i] = writemem [0xa000|i] = ari_write_ram_mirror32;
	}

	// stupid BIOS RAM check
	writemem[0] = ari_write_ram_mirror_ro32;
	pcsx_ram_is_ro = 0;

	// RAM direct
	for (i = 0x8000; i < 0x8020; i++) {
		readmemb[i] = ari_read_ram8;
		readmemh[i] = ari_read_ram16;
		readmem[i] = ari_read_ram32;
	}

	// BIOS and it's mirrors
	for (i = 0x1fc0; i < 0x1fc8; i++) {
		readmemb[i] = readmemb[0x8000|i] = readmemb[0xa000|i] = ari_read_bios8;
		readmemh[i] = readmemh[0x8000|i] = readmemh[0xa000|i] = ari_read_bios16;
		readmem[i]  = readmem[0x8000|i]  = readmem[0xa000|i]  = ari_read_bios32;
	}

	// I/O
	readmemb[0x1f80] = ari_read_io8;
	readmemh[0x1f80] = ari_read_io16;
	readmem[0x1f80]  = ari_read_io32;
	writememb[0x1f80] = ari_write_io8;
	writememh[0x1f80] = ari_write_io16;
	writemem[0x1f80]  = ari_write_io32;

	writemem[0xfffe] = write_biu_;
#endif

	// fill IO tables
	nd_pcsx_io.tab_read8 = io_read8;
	nd_pcsx_io.tab_read16 = io_read16;
	nd_pcsx_io.tab_read32 = io_read32;
	nd_pcsx_io.tab_write8 = io_write8;
	nd_pcsx_io.tab_write16 = io_write16;
	nd_pcsx_io.tab_write32 = io_write32;
}

void new_dyna_pcsx_mem_reset(void)
{
	int i;

	// plugins might change so update the pointers
	map_item(&mem_iortab[IOMEM32(0x1810)], GPU_readData, 1);
	map_item(&mem_iortab[IOMEM32(0x1814)], GPU_readStatus, 1);

	for (i = 0x1c00; i < 0x1e00; i += 2)
		map_item(&mem_iortab[IOMEM16(i)], SPU_readRegister, 1);

	map_item(&mem_iowtab[IOMEM32(0x1810)], GPU_writeData, 1);
	map_item(&mem_iowtab[IOMEM32(0x1814)], GPU_writeStatus, 1);

	nd_pcsx_io.spu_readf = SPU_readRegister;
	nd_pcsx_io.spu_writef = SPU_writeRegister;

	io_read32[IOADR32(0x1810)] = GPU_readData;
	io_read32[IOADR32(0x1814)] = GPU_readStatus;
	io_write32[IOADR32(0x1810)] = GPU_writeData;
	io_write32[IOADR32(0x1814)] = GPU_writeStatus;
}

