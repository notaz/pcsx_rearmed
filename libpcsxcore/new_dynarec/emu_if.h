#include "new_dynarec.h"
#include "../r3000a.h"

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
extern int reg_cop2d[], reg_cop2c[];
extern void *gte_handlers[64];
extern void *gte_handlers_nf[64];
extern const char *gte_regnames[64];
extern const char gte_cycletab[64];

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

extern void (*readmem[0x10000])();
extern void (*readmemb[0x10000])();
extern void (*readmemh[0x10000])();
extern void (*writemem[0x10000])();
extern void (*writememb[0x10000])();
extern void (*writememh[0x10000])();

extern unsigned int address;
extern unsigned int readmem_word; /* same as readmem_dword */
extern unsigned int word;	/* write */
extern unsigned short hword;
extern unsigned char byte;

extern void *psxH_ptr;

// same as invalid_code, just a region for ram write checks (inclusive)
extern u32 inv_code_start, inv_code_end;

/* cycles/irqs */
extern unsigned int next_interupt;
extern int pending_exception;

/* called by drc */
void pcsx_mtc0(u32 reg);
void pcsx_mtc0_ds(u32 reg);

/* misc */
extern void (*psxHLEt[])();
