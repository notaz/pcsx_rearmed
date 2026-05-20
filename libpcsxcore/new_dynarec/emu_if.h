#include "new_dynarec.h"
#include "../r3000a.h"

extern int dynarec_local[];

/* COP2/GTE */
extern const char *gte_opnames[64];
extern const uint64_t gte_reg_reads[64];
extern const uint64_t gte_reg_writes[64];

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
u32  rcnt0_read_count_m0(u32 addr, u32, u32 cycles);
u32  rcnt0_read_count_m1(u32 addr, u32, u32 cycles);
u32  rcnt1_read_count_m0(u32 addr, u32, u32 cycles);
u32  rcnt1_read_count_m1(u32 addr, u32, u32 cycles);
u32  rcnt2_read_count_m0(u32 addr, u32, u32 cycles);
u32  rcnt2_read_count_m1(u32 addr, u32, u32 cycles);

extern unsigned int address;
extern unsigned int hack_addr;
extern void *psxH_ptr;
extern void *zeromem_ptr;
extern void *scratch_buf_ptr;

// same as invalid_code, just a region for ram write checks (inclusive)
// (psx/guest address range)
extern u32 inv_code_start, inv_code_end;

/* cycles/irqs */
extern u32 next_interupt;

/* called by drc */
struct psxRegisters;
void pcsx_mtc0(struct psxRegisters *regs, u32 reg, u32 val);
void pcsx_mtc0_ds(struct psxRegisters *regs, u32 reg, u32 val);

/* misc */
extern void SysPrintf(const char *fmt, ...);

