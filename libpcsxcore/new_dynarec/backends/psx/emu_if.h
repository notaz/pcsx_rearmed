#include "../../new_dynarec.h"
#include "../../../r3000a.h"

extern char invalid_code[0x100000];

/* weird stuff */
#define EAX 0
#define ECX 1

/* same as psxRegs */
extern int reg[];

/* same as psxRegs.GPR.n.* */
extern int hi, lo;

/* same as psxRegs.CP0.n.* */
extern int reg_cop0[];
#define Status   psxRegs.CP0.n.Status
#define Cause    psxRegs.CP0.n.Cause
#define EPC      psxRegs.CP0.n.EPC
#define BadVAddr psxRegs.CP0.n.BadVAddr
#define Context  psxRegs.CP0.n.Context
#define EntryHi  psxRegs.CP0.n.EntryHi
#define Count    psxRegs.cycle // psxRegs.CP0.n.Count

/* COP2/GTE */
enum gte_opcodes {
	GTE_RTPS	= 0x01,
	GTE_NCLIP	= 0x06,
	GTE_OP		= 0x0c,
	GTE_DPCS	= 0x10,
	GTE_INTPL	= 0x11,
	GTE_MVMVA	= 0x12,
	GTE_NCDS	= 0x13,
	GTE_CDP		= 0x14,
	GTE_NCDT	= 0x16,
	GTE_NCCS	= 0x1b,
	GTE_CC		= 0x1c,
	GTE_NCS		= 0x1e,
	GTE_NCT		= 0x20,
	GTE_SQR		= 0x28,
	GTE_DCPL	= 0x29,
	GTE_DPCT	= 0x2a,
	GTE_AVSZ3	= 0x2d,
	GTE_AVSZ4	= 0x2e,
	GTE_RTPT	= 0x30,
	GTE_GPF		= 0x3d,
	GTE_GPL		= 0x3e,
	GTE_NCCT	= 0x3f,
};

extern int reg_cop2d[], reg_cop2c[];
extern void *gte_handlers[64];
extern void *gte_handlers_nf[64];
extern const char *gte_regnames[64];
extern const char gte_cycletab[64];
extern const uint64_t gte_reg_reads[64];
extern const uint64_t gte_reg_writes[64];

/* dummy */
extern int FCR0, FCR31;

/* mem */
extern void *mem_rtab;
extern void *mem_wtab;

void jump_handler_read8(u32 addr, u32 *table, u32 cycles);
void jump_handler_read16(u32 addr, u32 *table, u32 cycles);
void jump_handler_read32(u32 addr, u32 *table, u32 cycles);
void jump_handler_write8(u32 addr, u32 data, u32 cycles, u32 *table);
void jump_handler_write16(u32 addr, u32 data, u32 cycles, u32 *table);
void jump_handler_write32(u32 addr, u32 data, u32 cycles, u32 *table);
void jump_handler_write_h(u32 addr, u32 data, u32 cycles, void *handler);
void jump_handle_swl(u32 addr, u32 data, u32 cycles);
void jump_handle_swr(u32 addr, u32 data, u32 cycles);
void rcnt0_read_count_m0(u32 addr, u32, u32 cycles);
void rcnt0_read_count_m1(u32 addr, u32, u32 cycles);
void rcnt1_read_count_m0(u32 addr, u32, u32 cycles);
void rcnt1_read_count_m1(u32 addr, u32, u32 cycles);
void rcnt2_read_count_m0(u32 addr, u32, u32 cycles);
void rcnt2_read_count_m1(u32 addr, u32, u32 cycles);

extern unsigned int address;
extern void *psxH_ptr;
extern void *zeromem_ptr;
extern void *scratch_buf_ptr;

// same as invalid_code, just a region for ram write checks (inclusive)
extern u32 inv_code_start, inv_code_end;

/* cycles/irqs */
extern u32 next_interupt;
extern int pending_exception;

/* called by drc */
void pcsx_mtc0(u32 reg, u32 val);
void pcsx_mtc0_ds(u32 reg, u32 val);

/* misc */
extern void (*psxHLEt[])();

extern void SysPrintf(const char *fmt, ...);

#ifdef RAM_FIXED
#define rdram ((u_int)0x80000000)
#else
#define rdram ((u_int)psxM)
#endif
