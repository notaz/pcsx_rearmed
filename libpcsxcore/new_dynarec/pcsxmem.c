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
#include "../gpu.h"
#include "emu_if.h"
#include "pcsxmem.h"

//#define memprintf printf
#define memprintf(...)

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

u8 zero_mem[0x1000];

u32 read_mem_dummy()
{
	return 0;
}

static void write_mem_dummy(u32 data)
{
	memprintf("unmapped w %08x, %08x @%08x %u\n", address, data, psxRegs.pc, psxRegs.cycle);
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

#ifndef DRC_DBG

static void map_rcnt_rcount0(u32 mode)
{
	if (mode & 0x100) { // pixel clock
		map_item(&mem_iortab[IOMEM32(0x1100)], rcnt0_read_count_m1, 1);
		map_item(&mem_iortab[IOMEM16(0x1100)], rcnt0_read_count_m1, 1);
	}
	else {
		map_item(&mem_iortab[IOMEM32(0x1100)], rcnt0_read_count_m0, 1);
		map_item(&mem_iortab[IOMEM16(0x1100)], rcnt0_read_count_m0, 1);
	}
}

static void map_rcnt_rcount1(u32 mode)
{
	if (mode & 0x100) { // hcnt
		map_item(&mem_iortab[IOMEM32(0x1110)], rcnt1_read_count_m1, 1);
		map_item(&mem_iortab[IOMEM16(0x1110)], rcnt1_read_count_m1, 1);
	}
	else {
		map_item(&mem_iortab[IOMEM32(0x1110)], rcnt1_read_count_m0, 1);
		map_item(&mem_iortab[IOMEM16(0x1110)], rcnt1_read_count_m0, 1);
	}
}

static void map_rcnt_rcount2(u32 mode)
{
	if (mode & 0x01) { // gate
		map_item(&mem_iortab[IOMEM32(0x1120)], &psxH[0x1000], 0);
		map_item(&mem_iortab[IOMEM16(0x1120)], &psxH[0x1000], 0);
	}
	else if (mode & 0x200) { // clk/8
		map_item(&mem_iortab[IOMEM32(0x1120)], rcnt2_read_count_m1, 1);
		map_item(&mem_iortab[IOMEM16(0x1120)], rcnt2_read_count_m1, 1);
	}
	else {
		map_item(&mem_iortab[IOMEM32(0x1120)], rcnt2_read_count_m0, 1);
		map_item(&mem_iortab[IOMEM16(0x1120)], rcnt2_read_count_m0, 1);
	}
}

#else
#define map_rcnt_rcount0(mode)
#define map_rcnt_rcount1(mode)
#define map_rcnt_rcount2(mode)
#endif

#define make_rcnt_funcs(i) \
static u32 io_rcnt_read_count##i()  { return psxRcntRcount(i); } \
static u32 io_rcnt_read_mode##i()   { return psxRcntRmode(i); } \
static u32 io_rcnt_read_target##i() { return psxRcntRtarget(i); } \
static void io_rcnt_write_count##i(u32 val)  { psxRcntWcount(i, val & 0xffff); } \
static void io_rcnt_write_mode##i(u32 val)   { psxRcntWmode(i, val); map_rcnt_rcount##i(val); } \
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

static u32 io_gpu_read_status(void)
{
	// meh2, syncing for img bit, might want to avoid it..
	gpuSyncPluginSR();
	return HW_GPU_STATUS;
}

static void io_gpu_write_status(u32 value)
{
	GPU_writeStatus(value);
	gpuSyncPluginSR();
}

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

void new_dyna_pcsx_mem_load_state(void)
{
	map_rcnt_rcount0(rcnts[0].mode);
	map_rcnt_rcount1(rcnts[1].mode);
	map_rcnt_rcount2(rcnts[2].mode);
}

int pcsxmem_is_handler_dynamic(u_int addr)
{
	if ((addr & 0xfffff000) != 0x1f801000)
		return 0;

	addr &= 0xffff;
	return addr == 0x1100 || addr == 0x1110 || addr == 0x1120;
}

void new_dyna_pcsx_mem_init(void)
{
	int i;

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
		map_l1_mem(mem_readtab, i, 0, 0x1000, zero_mem);
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
	map_item(&mem_iortab[IOMEM32(0x1814)], io_gpu_read_status, 1);
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
	map_item(&mem_iowtab[IOMEM32(0x1814)], io_gpu_write_status, 1);
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

	new_dyna_pcsx_mem_load_state();
}

void new_dyna_pcsx_mem_reset(void)
{
	int i;

	// plugins might change so update the pointers
	map_item(&mem_iortab[IOMEM32(0x1810)], GPU_readData, 1);

	for (i = 0x1c00; i < 0x1e00; i += 2)
		map_item(&mem_iortab[IOMEM16(i)], SPU_readRegister, 1);

	map_item(&mem_iowtab[IOMEM32(0x1810)], GPU_writeData, 1);
}
