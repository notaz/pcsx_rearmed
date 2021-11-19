/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - new_dynarec.c                                           *
 *   Copyright (C) 2009-2011 Ari64                                         *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdlib.h>
#include <stdint.h> //include for uint64_t
#include <assert.h>
#include <errno.h>
#include <sys/mman.h>
#ifdef __MACH__
#include <libkern/OSCacheControl.h>
#endif
#ifdef _3DS
#include <3ds_utils.h>
#endif
#ifdef VITA
#include <psp2/kernel/sysmem.h>
static int sceBlock;
#endif

#include "new_dynarec_config.h"
#include "../psxhle.h"
#include "../psxinterpreter.h"
#include "../gte.h"
#include "emu_if.h" // emulator interface

#define noinline __attribute__((noinline,noclone))
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif
#ifndef min
#define min(a, b) ((b) < (a) ? (b) : (a))
#endif

//#define DISASM
//#define assem_debug printf
//#define inv_debug printf
#define assem_debug(...)
#define inv_debug(...)

#ifdef __i386__
#include "assem_x86.h"
#endif
#ifdef __x86_64__
#include "assem_x64.h"
#endif
#ifdef __arm__
#include "assem_arm.h"
#endif
#ifdef __aarch64__
#include "assem_arm64.h"
#endif

#define RAM_SIZE 0x200000
#define MAXBLOCK 4096
#define MAX_OUTPUT_BLOCK_SIZE 262144

struct ndrc_mem
{
  u_char translation_cache[1 << TARGET_SIZE_2];
  struct
  {
    struct tramp_insns ops[2048 / sizeof(struct tramp_insns)];
    const void *f[2048 / sizeof(void *)];
  } tramp;
};

#ifdef BASE_ADDR_DYNAMIC
static struct ndrc_mem *ndrc;
#else
static struct ndrc_mem ndrc_ __attribute__((aligned(4096)));
static struct ndrc_mem *ndrc = &ndrc_;
#endif

// stubs
enum stub_type {
  CC_STUB = 1,
  FP_STUB = 2,
  LOADB_STUB = 3,
  LOADH_STUB = 4,
  LOADW_STUB = 5,
  LOADD_STUB = 6,
  LOADBU_STUB = 7,
  LOADHU_STUB = 8,
  STOREB_STUB = 9,
  STOREH_STUB = 10,
  STOREW_STUB = 11,
  STORED_STUB = 12,
  STORELR_STUB = 13,
  INVCODE_STUB = 14,
};

struct regstat
{
  signed char regmap_entry[HOST_REGS];
  signed char regmap[HOST_REGS];
  uint64_t wasdirty;
  uint64_t dirty;
  uint64_t u;
  u_int wasconst;
  u_int isconst;
  u_int loadedconst;             // host regs that have constants loaded
  u_int waswritten;              // MIPS regs that were used as store base before
};

// note: asm depends on this layout
struct ll_entry
{
  u_int vaddr;
  u_int reg_sv_flags;
  void *addr;
  struct ll_entry *next;
};

struct ht_entry
{
  u_int vaddr[2];
  void *tcaddr[2];
};

struct code_stub
{
  enum stub_type type;
  void *addr;
  void *retaddr;
  u_int a;
  uintptr_t b;
  uintptr_t c;
  u_int d;
  u_int e;
};

struct link_entry
{
  void *addr;
  u_int target;
  u_int ext;
};

  // used by asm:
  u_char *out;
  struct ht_entry hash_table[65536]  __attribute__((aligned(16)));
  struct ll_entry *jump_in[4096] __attribute__((aligned(16)));
  struct ll_entry *jump_dirty[4096];

  static struct ll_entry *jump_out[4096];
  static u_int start;
  static u_int *source;
  static char insn[MAXBLOCK][10];
  static u_char itype[MAXBLOCK];
  static u_char opcode[MAXBLOCK];
  static u_char opcode2[MAXBLOCK];
  static u_char bt[MAXBLOCK];
  static u_char rs1[MAXBLOCK];
  static u_char rs2[MAXBLOCK];
  static u_char rt1[MAXBLOCK];
  static u_char rt2[MAXBLOCK];
  static u_char dep1[MAXBLOCK];
  static u_char dep2[MAXBLOCK];
  static u_char lt1[MAXBLOCK];
  static uint64_t gte_rs[MAXBLOCK]; // gte: 32 data and 32 ctl regs
  static uint64_t gte_rt[MAXBLOCK];
  static uint64_t gte_unneeded[MAXBLOCK];
  static u_int smrv[32]; // speculated MIPS register values
  static u_int smrv_strong; // mask or regs that are likely to have correct values
  static u_int smrv_weak; // same, but somewhat less likely
  static u_int smrv_strong_next; // same, but after current insn executes
  static u_int smrv_weak_next;
  static int imm[MAXBLOCK];
  static u_int ba[MAXBLOCK];
  static char likely[MAXBLOCK];
  static char is_ds[MAXBLOCK];
  static char ooo[MAXBLOCK];
  static uint64_t unneeded_reg[MAXBLOCK];
  static uint64_t branch_unneeded_reg[MAXBLOCK];
  static signed char regmap_pre[MAXBLOCK][HOST_REGS]; // pre-instruction i?
  // contains 'real' consts at [i] insn, but may differ from what's actually
  // loaded in host reg as 'final' value is always loaded, see get_final_value()
  static uint32_t current_constmap[HOST_REGS];
  static uint32_t constmap[MAXBLOCK][HOST_REGS];
  static struct regstat regs[MAXBLOCK];
  static struct regstat branch_regs[MAXBLOCK];
  static signed char minimum_free_regs[MAXBLOCK];
  static u_int needed_reg[MAXBLOCK];
  static u_int wont_dirty[MAXBLOCK];
  static u_int will_dirty[MAXBLOCK];
  static int ccadj[MAXBLOCK];
  static int slen;
  static void *instr_addr[MAXBLOCK];
  static struct link_entry link_addr[MAXBLOCK];
  static int linkcount;
  static struct code_stub stubs[MAXBLOCK*3];
  static int stubcount;
  static u_int literals[1024][2];
  static int literalcount;
  static int is_delayslot;
  static char shadow[1048576]  __attribute__((aligned(16)));
  static void *copy;
  static int expirep;
  static u_int stop_after_jal;
#ifndef RAM_FIXED
  static uintptr_t ram_offset;
#else
  static const uintptr_t ram_offset=0;
#endif

  int new_dynarec_hacks;
  int new_dynarec_hacks_pergame;
  int new_dynarec_did_compile;

  #define HACK_ENABLED(x) ((new_dynarec_hacks | new_dynarec_hacks_pergame) & (x))

  extern int cycle_count; // ... until end of the timeslice, counts -N -> 0
  extern int last_count;  // last absolute target, often = next_interupt
  extern int pcaddr;
  extern int pending_exception;
  extern int branch_target;
  extern uintptr_t mini_ht[32][2];
  extern u_char restore_candidate[512];

  /* registers that may be allocated */
  /* 1-31 gpr */
#define LOREG 32 // lo
#define HIREG 33 // hi
//#define FSREG 34 // FPU status (FCSR)
#define CSREG 35 // Coprocessor status
#define CCREG 36 // Cycle count
#define INVCP 37 // Pointer to invalid_code
//#define MMREG 38 // Pointer to memory_map
//#define ROREG 39 // ram offset (if rdram!=0x80000000)
#define TEMPREG 40
#define FTEMP 40 // FPU temporary register
#define PTEMP 41 // Prefetch temporary register
//#define TLREG 42 // TLB mapping offset
#define RHASH 43 // Return address hash
#define RHTBL 44 // Return address hash table address
#define RTEMP 45 // JR/JALR address register
#define MAXREG 45
#define AGEN1 46 // Address generation temporary register
//#define AGEN2 47 // Address generation temporary register
//#define MGEN1 48 // Maptable address generation temporary register
//#define MGEN2 49 // Maptable address generation temporary register
#define BTREG 50 // Branch target temporary register

  /* instruction types */
#define NOP 0     // No operation
#define LOAD 1    // Load
#define STORE 2   // Store
#define LOADLR 3  // Unaligned load
#define STORELR 4 // Unaligned store
#define MOV 5     // Move
#define ALU 6     // Arithmetic/logic
#define MULTDIV 7 // Multiply/divide
#define SHIFT 8   // Shift by register
#define SHIFTIMM 9// Shift by immediate
#define IMM16 10  // 16-bit immediate
#define RJUMP 11  // Unconditional jump to register
#define UJUMP 12  // Unconditional jump
#define CJUMP 13  // Conditional branch (BEQ/BNE/BGTZ/BLEZ)
#define SJUMP 14  // Conditional branch (regimm format)
#define COP0 15   // Coprocessor 0
#define COP1 16   // Coprocessor 1
#define C1LS 17   // Coprocessor 1 load/store
//#define FJUMP 18  // Conditional branch (floating point)
//#define FLOAT 19  // Floating point unit
//#define FCONV 20  // Convert integer to float
//#define FCOMP 21  // Floating point compare (sets FSREG)
#define SYSCALL 22// SYSCALL
#define OTHER 23  // Other
#define SPAN 24   // Branch/delay slot spans 2 pages
#define NI 25     // Not implemented
#define HLECALL 26// PCSX fake opcodes for HLE
#define COP2 27   // Coprocessor 2 move
#define C2LS 28   // Coprocessor 2 load/store
#define C2OP 29   // Coprocessor 2 operation
#define INTCALL 30// Call interpreter to handle rare corner cases

  /* branch codes */
#define TAKEN 1
#define NOTTAKEN 2
#define NULLDS 3

#define DJT_1 (void *)1l // no function, just a label in assem_debug log
#define DJT_2 (void *)2l

// asm linkage
int new_recompile_block(u_int addr);
void *get_addr_ht(u_int vaddr);
void invalidate_block(u_int block);
void invalidate_addr(u_int addr);
void remove_hash(int vaddr);
void dyna_linker();
void dyna_linker_ds();
void verify_code();
void verify_code_ds();
void cc_interrupt();
void fp_exception();
void fp_exception_ds();
void jump_to_new_pc();
void call_gteStall();
void new_dyna_leave();

// Needed by assembler
static void wb_register(signed char r,signed char regmap[],uint64_t dirty);
static void wb_dirtys(signed char i_regmap[],uint64_t i_dirty);
static void wb_needed_dirtys(signed char i_regmap[],uint64_t i_dirty,int addr);
static void load_all_regs(signed char i_regmap[]);
static void load_needed_regs(signed char i_regmap[],signed char next_regmap[]);
static void load_regs_entry(int t);
static void load_all_consts(signed char regmap[],u_int dirty,int i);
static u_int get_host_reglist(const signed char *regmap);

static int verify_dirty(const u_int *ptr);
static int get_final_value(int hr, int i, int *value);
static void add_stub(enum stub_type type, void *addr, void *retaddr,
  u_int a, uintptr_t b, uintptr_t c, u_int d, u_int e);
static void add_stub_r(enum stub_type type, void *addr, void *retaddr,
  int i, int addr_reg, const struct regstat *i_regs, int ccadj, u_int reglist);
static void add_to_linker(void *addr, u_int target, int ext);
static void *emit_fastpath_cmp_jump(int i,int addr,int *addr_reg_override);
static void *get_direct_memhandler(void *table, u_int addr,
  enum stub_type type, uintptr_t *addr_host);
static void cop2_call_stall_check(u_int op, int i, const struct regstat *i_regs, u_int reglist);
static void pass_args(int a0, int a1);
static void emit_far_jump(const void *f);
static void emit_far_call(const void *f);

static void mprotect_w_x(void *start, void *end, int is_x)
{
#ifdef NO_WRITE_EXEC
  #if defined(VITA)
  // *Open* enables write on all memory that was
  // allocated by sceKernelAllocMemBlockForVM()?
  if (is_x)
    sceKernelCloseVMDomain();
  else
    sceKernelOpenVMDomain();
  #else
  u_long mstart = (u_long)start & ~4095ul;
  u_long mend = (u_long)end;
  if (mprotect((void *)mstart, mend - mstart,
               PROT_READ | (is_x ? PROT_EXEC : PROT_WRITE)) != 0)
    SysPrintf("mprotect(%c) failed: %s\n", is_x ? 'x' : 'w', strerror(errno));
  #endif
#endif
}

static void start_tcache_write(void *start, void *end)
{
  mprotect_w_x(start, end, 0);
}

static void end_tcache_write(void *start, void *end)
{
#if defined(__arm__) || defined(__aarch64__)
  size_t len = (char *)end - (char *)start;
  #if   defined(__BLACKBERRY_QNX__)
  msync(start, len, MS_SYNC | MS_CACHE_ONLY | MS_INVALIDATE_ICACHE);
  #elif defined(__MACH__)
  sys_cache_control(kCacheFunctionPrepareForExecution, start, len);
  #elif defined(VITA)
  sceKernelSyncVMDomain(sceBlock, start, len);
  #elif defined(_3DS)
  ctr_flush_invalidate_cache();
  #elif defined(__aarch64__)
  // as of 2021, __clear_cache() is still broken on arm64
  // so here is a custom one :(
  clear_cache_arm64(start, end);
  #else
  __clear_cache(start, end);
  #endif
  (void)len;
#endif

  mprotect_w_x(start, end, 1);
}

static void *start_block(void)
{
  u_char *end = out + MAX_OUTPUT_BLOCK_SIZE;
  if (end > ndrc->translation_cache + sizeof(ndrc->translation_cache))
    end = ndrc->translation_cache + sizeof(ndrc->translation_cache);
  start_tcache_write(out, end);
  return out;
}

static void end_block(void *start)
{
  end_tcache_write(start, out);
}

// also takes care of w^x mappings when patching code
static u_int needs_clear_cache[1<<(TARGET_SIZE_2-17)];

static void mark_clear_cache(void *target)
{
  uintptr_t offset = (u_char *)target - ndrc->translation_cache;
  u_int mask = 1u << ((offset >> 12) & 31);
  if (!(needs_clear_cache[offset >> 17] & mask)) {
    char *start = (char *)((uintptr_t)target & ~4095l);
    start_tcache_write(start, start + 4095);
    needs_clear_cache[offset >> 17] |= mask;
  }
}

// Clearing the cache is rather slow on ARM Linux, so mark the areas
// that need to be cleared, and then only clear these areas once.
static void do_clear_cache(void)
{
  int i, j;
  for (i = 0; i < (1<<(TARGET_SIZE_2-17)); i++)
  {
    u_int bitmap = needs_clear_cache[i];
    if (!bitmap)
      continue;
    for (j = 0; j < 32; j++)
    {
      u_char *start, *end;
      if (!(bitmap & (1<<j)))
        continue;

      start = ndrc->translation_cache + i*131072 + j*4096;
      end = start + 4095;
      for (j++; j < 32; j++) {
        if (!(bitmap & (1<<j)))
          break;
        end += 4096;
      }
      end_tcache_write(start, end);
    }
    needs_clear_cache[i] = 0;
  }
}

//#define DEBUG_CYCLE_COUNT 1

#define NO_CYCLE_PENALTY_THR 12

int cycle_multiplier; // 100 for 1.0
int cycle_multiplier_override;

static int CLOCK_ADJUST(int x)
{
  int m = cycle_multiplier_override
        ? cycle_multiplier_override : cycle_multiplier;
  int s=(x>>31)|1;
  return (x * m + s * 50) / 100;
}

// is the op an unconditional jump?
static int is_ujump(int i)
{
  return itype[i] == UJUMP || itype[i] == RJUMP
    || (source[i] >> 16) == 0x1000; // beq r0, r0, offset // b offset
}

static int is_jump(int i)
{
  return itype[i] == RJUMP || itype[i] == UJUMP || itype[i] == CJUMP || itype[i] == SJUMP;
}

static u_int get_page(u_int vaddr)
{
  u_int page=vaddr&~0xe0000000;
  if (page < 0x1000000)
    page &= ~0x0e00000; // RAM mirrors
  page>>=12;
  if(page>2048) page=2048+(page&2047);
  return page;
}

// no virtual mem in PCSX
static u_int get_vpage(u_int vaddr)
{
  return get_page(vaddr);
}

static struct ht_entry *hash_table_get(u_int vaddr)
{
  return &hash_table[((vaddr>>16)^vaddr)&0xFFFF];
}

static void hash_table_add(struct ht_entry *ht_bin, u_int vaddr, void *tcaddr)
{
  ht_bin->vaddr[1] = ht_bin->vaddr[0];
  ht_bin->tcaddr[1] = ht_bin->tcaddr[0];
  ht_bin->vaddr[0] = vaddr;
  ht_bin->tcaddr[0] = tcaddr;
}

// some messy ari64's code, seems to rely on unsigned 32bit overflow
static int doesnt_expire_soon(void *tcaddr)
{
  u_int diff = (u_int)((u_char *)tcaddr - out) << (32-TARGET_SIZE_2);
  return diff > (u_int)(0x60000000 + (MAX_OUTPUT_BLOCK_SIZE << (32-TARGET_SIZE_2)));
}

// Get address from virtual address
// This is called from the recompiled JR/JALR instructions
void noinline *get_addr(u_int vaddr)
{
  u_int page=get_page(vaddr);
  u_int vpage=get_vpage(vaddr);
  struct ll_entry *head;
  //printf("TRACE: count=%d next=%d (get_addr %x,page %d)\n",Count,next_interupt,vaddr,page);
  head=jump_in[page];
  while(head!=NULL) {
    if(head->vaddr==vaddr) {
  //printf("TRACE: count=%d next=%d (get_addr match %x: %p)\n",Count,next_interupt,vaddr,head->addr);
      hash_table_add(hash_table_get(vaddr), vaddr, head->addr);
      return head->addr;
    }
    head=head->next;
  }
  head=jump_dirty[vpage];
  while(head!=NULL) {
    if(head->vaddr==vaddr) {
      //printf("TRACE: count=%d next=%d (get_addr match dirty %x: %p)\n",Count,next_interupt,vaddr,head->addr);
      // Don't restore blocks which are about to expire from the cache
      if (doesnt_expire_soon(head->addr))
      if (verify_dirty(head->addr)) {
        //printf("restore candidate: %x (%d) d=%d\n",vaddr,page,invalid_code[vaddr>>12]);
        invalid_code[vaddr>>12]=0;
        inv_code_start=inv_code_end=~0;
        if(vpage<2048) {
          restore_candidate[vpage>>3]|=1<<(vpage&7);
        }
        else restore_candidate[page>>3]|=1<<(page&7);
        struct ht_entry *ht_bin = hash_table_get(vaddr);
        if (ht_bin->vaddr[0] == vaddr)
          ht_bin->tcaddr[0] = head->addr; // Replace existing entry
        else
          hash_table_add(ht_bin, vaddr, head->addr);

        return head->addr;
      }
    }
    head=head->next;
  }
  //printf("TRACE: count=%d next=%d (get_addr no-match %x)\n",Count,next_interupt,vaddr);
  int r=new_recompile_block(vaddr);
  if(r==0) return get_addr(vaddr);
  // Execute in unmapped page, generate pagefault execption
  Status|=2;
  Cause=(vaddr<<31)|0x8;
  EPC=(vaddr&1)?vaddr-5:vaddr;
  BadVAddr=(vaddr&~1);
  Context=(Context&0xFF80000F)|((BadVAddr>>9)&0x007FFFF0);
  EntryHi=BadVAddr&0xFFFFE000;
  return get_addr_ht(0x80000000);
}
// Look up address in hash table first
void *get_addr_ht(u_int vaddr)
{
  //printf("TRACE: count=%d next=%d (get_addr_ht %x)\n",Count,next_interupt,vaddr);
  const struct ht_entry *ht_bin = hash_table_get(vaddr);
  if (ht_bin->vaddr[0] == vaddr) return ht_bin->tcaddr[0];
  if (ht_bin->vaddr[1] == vaddr) return ht_bin->tcaddr[1];
  return get_addr(vaddr);
}

void clear_all_regs(signed char regmap[])
{
  int hr;
  for (hr=0;hr<HOST_REGS;hr++) regmap[hr]=-1;
}

static signed char get_reg(const signed char regmap[],int r)
{
  int hr;
  for (hr=0;hr<HOST_REGS;hr++) if(hr!=EXCLUDE_REG&&regmap[hr]==r) return hr;
  return -1;
}

// Find a register that is available for two consecutive cycles
static signed char get_reg2(signed char regmap1[], const signed char regmap2[], int r)
{
  int hr;
  for (hr=0;hr<HOST_REGS;hr++) if(hr!=EXCLUDE_REG&&regmap1[hr]==r&&regmap2[hr]==r) return hr;
  return -1;
}

int count_free_regs(signed char regmap[])
{
  int count=0;
  int hr;
  for(hr=0;hr<HOST_REGS;hr++)
  {
    if(hr!=EXCLUDE_REG) {
      if(regmap[hr]<0) count++;
    }
  }
  return count;
}

void dirty_reg(struct regstat *cur,signed char reg)
{
  int hr;
  if(!reg) return;
  for (hr=0;hr<HOST_REGS;hr++) {
    if((cur->regmap[hr]&63)==reg) {
      cur->dirty|=1<<hr;
    }
  }
}

static void set_const(struct regstat *cur, signed char reg, uint32_t value)
{
  int hr;
  if(!reg) return;
  for (hr=0;hr<HOST_REGS;hr++) {
    if(cur->regmap[hr]==reg) {
      cur->isconst|=1<<hr;
      current_constmap[hr]=value;
    }
  }
}

static void clear_const(struct regstat *cur, signed char reg)
{
  int hr;
  if(!reg) return;
  for (hr=0;hr<HOST_REGS;hr++) {
    if((cur->regmap[hr]&63)==reg) {
      cur->isconst&=~(1<<hr);
    }
  }
}

static int is_const(struct regstat *cur, signed char reg)
{
  int hr;
  if(reg<0) return 0;
  if(!reg) return 1;
  for (hr=0;hr<HOST_REGS;hr++) {
    if((cur->regmap[hr]&63)==reg) {
      return (cur->isconst>>hr)&1;
    }
  }
  return 0;
}

static uint32_t get_const(struct regstat *cur, signed char reg)
{
  int hr;
  if(!reg) return 0;
  for (hr=0;hr<HOST_REGS;hr++) {
    if(cur->regmap[hr]==reg) {
      return current_constmap[hr];
    }
  }
  SysPrintf("Unknown constant in r%d\n",reg);
  abort();
}

// Least soon needed registers
// Look at the next ten instructions and see which registers
// will be used.  Try not to reallocate these.
void lsn(u_char hsn[], int i, int *preferred_reg)
{
  int j;
  int b=-1;
  for(j=0;j<9;j++)
  {
    if(i+j>=slen) {
      j=slen-i-1;
      break;
    }
    if (is_ujump(i+j))
    {
      // Don't go past an unconditonal jump
      j++;
      break;
    }
  }
  for(;j>=0;j--)
  {
    if(rs1[i+j]) hsn[rs1[i+j]]=j;
    if(rs2[i+j]) hsn[rs2[i+j]]=j;
    if(rt1[i+j]) hsn[rt1[i+j]]=j;
    if(rt2[i+j]) hsn[rt2[i+j]]=j;
    if(itype[i+j]==STORE || itype[i+j]==STORELR) {
      // Stores can allocate zero
      hsn[rs1[i+j]]=j;
      hsn[rs2[i+j]]=j;
    }
    // On some architectures stores need invc_ptr
    #if defined(HOST_IMM8)
    if(itype[i+j]==STORE || itype[i+j]==STORELR || (opcode[i+j]&0x3b)==0x39 || (opcode[i+j]&0x3b)==0x3a) {
      hsn[INVCP]=j;
    }
    #endif
    if(i+j>=0&&(itype[i+j]==UJUMP||itype[i+j]==CJUMP||itype[i+j]==SJUMP))
    {
      hsn[CCREG]=j;
      b=j;
    }
  }
  if(b>=0)
  {
    if(ba[i+b]>=start && ba[i+b]<(start+slen*4))
    {
      // Follow first branch
      int t=(ba[i+b]-start)>>2;
      j=7-b;if(t+j>=slen) j=slen-t-1;
      for(;j>=0;j--)
      {
        if(rs1[t+j]) if(hsn[rs1[t+j]]>j+b+2) hsn[rs1[t+j]]=j+b+2;
        if(rs2[t+j]) if(hsn[rs2[t+j]]>j+b+2) hsn[rs2[t+j]]=j+b+2;
        //if(rt1[t+j]) if(hsn[rt1[t+j]]>j+b+2) hsn[rt1[t+j]]=j+b+2;
        //if(rt2[t+j]) if(hsn[rt2[t+j]]>j+b+2) hsn[rt2[t+j]]=j+b+2;
      }
    }
    // TODO: preferred register based on backward branch
  }
  // Delay slot should preferably not overwrite branch conditions or cycle count
  if (i > 0 && is_jump(i-1)) {
    if(rs1[i-1]) if(hsn[rs1[i-1]]>1) hsn[rs1[i-1]]=1;
    if(rs2[i-1]) if(hsn[rs2[i-1]]>1) hsn[rs2[i-1]]=1;
    hsn[CCREG]=1;
    // ...or hash tables
    hsn[RHASH]=1;
    hsn[RHTBL]=1;
  }
  // Coprocessor load/store needs FTEMP, even if not declared
  if(itype[i]==C1LS||itype[i]==C2LS) {
    hsn[FTEMP]=0;
  }
  // Load L/R also uses FTEMP as a temporary register
  if(itype[i]==LOADLR) {
    hsn[FTEMP]=0;
  }
  // Also SWL/SWR/SDL/SDR
  if(opcode[i]==0x2a||opcode[i]==0x2e||opcode[i]==0x2c||opcode[i]==0x2d) {
    hsn[FTEMP]=0;
  }
  // Don't remove the miniht registers
  if(itype[i]==UJUMP||itype[i]==RJUMP)
  {
    hsn[RHASH]=0;
    hsn[RHTBL]=0;
  }
}

// We only want to allocate registers if we're going to use them again soon
int needed_again(int r, int i)
{
  int j;
  int b=-1;
  int rn=10;

  if (i > 0 && is_ujump(i-1))
  {
    if(ba[i-1]<start || ba[i-1]>start+slen*4-4)
      return 0; // Don't need any registers if exiting the block
  }
  for(j=0;j<9;j++)
  {
    if(i+j>=slen) {
      j=slen-i-1;
      break;
    }
    if (is_ujump(i+j))
    {
      // Don't go past an unconditonal jump
      j++;
      break;
    }
    if(itype[i+j]==SYSCALL||itype[i+j]==HLECALL||itype[i+j]==INTCALL||((source[i+j]&0xfc00003f)==0x0d))
    {
      break;
    }
  }
  for(;j>=1;j--)
  {
    if(rs1[i+j]==r) rn=j;
    if(rs2[i+j]==r) rn=j;
    if((unneeded_reg[i+j]>>r)&1) rn=10;
    if(i+j>=0&&(itype[i+j]==UJUMP||itype[i+j]==CJUMP||itype[i+j]==SJUMP))
    {
      b=j;
    }
  }
  /*
  if(b>=0)
  {
    if(ba[i+b]>=start && ba[i+b]<(start+slen*4))
    {
      // Follow first branch
      int o=rn;
      int t=(ba[i+b]-start)>>2;
      j=7-b;if(t+j>=slen) j=slen-t-1;
      for(;j>=0;j--)
      {
        if(!((unneeded_reg[t+j]>>r)&1)) {
          if(rs1[t+j]==r) if(rn>j+b+2) rn=j+b+2;
          if(rs2[t+j]==r) if(rn>j+b+2) rn=j+b+2;
        }
        else rn=o;
      }
    }
  }*/
  if(rn<10) return 1;
  (void)b;
  return 0;
}

// Try to match register allocations at the end of a loop with those
// at the beginning
int loop_reg(int i, int r, int hr)
{
  int j,k;
  for(j=0;j<9;j++)
  {
    if(i+j>=slen) {
      j=slen-i-1;
      break;
    }
    if (is_ujump(i+j))
    {
      // Don't go past an unconditonal jump
      j++;
      break;
    }
  }
  k=0;
  if(i>0){
    if(itype[i-1]==UJUMP||itype[i-1]==CJUMP||itype[i-1]==SJUMP)
      k--;
  }
  for(;k<j;k++)
  {
    assert(r < 64);
    if((unneeded_reg[i+k]>>r)&1) return hr;
    if(i+k>=0&&(itype[i+k]==UJUMP||itype[i+k]==CJUMP||itype[i+k]==SJUMP))
    {
      if(ba[i+k]>=start && ba[i+k]<(start+i*4))
      {
        int t=(ba[i+k]-start)>>2;
        int reg=get_reg(regs[t].regmap_entry,r);
        if(reg>=0) return reg;
        //reg=get_reg(regs[t+1].regmap_entry,r);
        //if(reg>=0) return reg;
      }
    }
  }
  return hr;
}


// Allocate every register, preserving source/target regs
void alloc_all(struct regstat *cur,int i)
{
  int hr;

  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG) {
      if(((cur->regmap[hr]&63)!=rs1[i])&&((cur->regmap[hr]&63)!=rs2[i])&&
         ((cur->regmap[hr]&63)!=rt1[i])&&((cur->regmap[hr]&63)!=rt2[i]))
      {
        cur->regmap[hr]=-1;
        cur->dirty&=~(1<<hr);
      }
      // Don't need zeros
      if((cur->regmap[hr]&63)==0)
      {
        cur->regmap[hr]=-1;
        cur->dirty&=~(1<<hr);
      }
    }
  }
}

#ifndef NDEBUG
static int host_tempreg_in_use;

static void host_tempreg_acquire(void)
{
  assert(!host_tempreg_in_use);
  host_tempreg_in_use = 1;
}

static void host_tempreg_release(void)
{
  host_tempreg_in_use = 0;
}
#else
static void host_tempreg_acquire(void) {}
static void host_tempreg_release(void) {}
#endif

#ifdef DRC_DBG
extern void gen_interupt();
extern void do_insn_cmp();
#define FUNCNAME(f) { f, " " #f }
static const struct {
  void *addr;
  const char *name;
} function_names[] = {
  FUNCNAME(cc_interrupt),
  FUNCNAME(gen_interupt),
  FUNCNAME(get_addr_ht),
  FUNCNAME(get_addr),
  FUNCNAME(jump_handler_read8),
  FUNCNAME(jump_handler_read16),
  FUNCNAME(jump_handler_read32),
  FUNCNAME(jump_handler_write8),
  FUNCNAME(jump_handler_write16),
  FUNCNAME(jump_handler_write32),
  FUNCNAME(invalidate_addr),
  FUNCNAME(jump_to_new_pc),
  FUNCNAME(call_gteStall),
  FUNCNAME(new_dyna_leave),
  FUNCNAME(pcsx_mtc0),
  FUNCNAME(pcsx_mtc0_ds),
  FUNCNAME(do_insn_cmp),
#ifdef __arm__
  FUNCNAME(verify_code),
#endif
};

static const char *func_name(const void *a)
{
  int i;
  for (i = 0; i < sizeof(function_names)/sizeof(function_names[0]); i++)
    if (function_names[i].addr == a)
      return function_names[i].name;
  return "";
}
#else
#define func_name(x) ""
#endif

#ifdef __i386__
#include "assem_x86.c"
#endif
#ifdef __x86_64__
#include "assem_x64.c"
#endif
#ifdef __arm__
#include "assem_arm.c"
#endif
#ifdef __aarch64__
#include "assem_arm64.c"
#endif

static void *get_trampoline(const void *f)
{
  size_t i;

  for (i = 0; i < ARRAY_SIZE(ndrc->tramp.f); i++) {
    if (ndrc->tramp.f[i] == f || ndrc->tramp.f[i] == NULL)
      break;
  }
  if (i == ARRAY_SIZE(ndrc->tramp.f)) {
    SysPrintf("trampoline table is full, last func %p\n", f);
    abort();
  }
  if (ndrc->tramp.f[i] == NULL) {
    start_tcache_write(&ndrc->tramp.f[i], &ndrc->tramp.f[i + 1]);
    ndrc->tramp.f[i] = f;
    end_tcache_write(&ndrc->tramp.f[i], &ndrc->tramp.f[i + 1]);
  }
  return &ndrc->tramp.ops[i];
}

static void emit_far_jump(const void *f)
{
  if (can_jump_or_call(f)) {
    emit_jmp(f);
    return;
  }

  f = get_trampoline(f);
  emit_jmp(f);
}

static void emit_far_call(const void *f)
{
  if (can_jump_or_call(f)) {
    emit_call(f);
    return;
  }

  f = get_trampoline(f);
  emit_call(f);
}

// Add virtual address mapping to linked list
void ll_add(struct ll_entry **head,int vaddr,void *addr)
{
  struct ll_entry *new_entry;
  new_entry=malloc(sizeof(struct ll_entry));
  assert(new_entry!=NULL);
  new_entry->vaddr=vaddr;
  new_entry->reg_sv_flags=0;
  new_entry->addr=addr;
  new_entry->next=*head;
  *head=new_entry;
}

void ll_add_flags(struct ll_entry **head,int vaddr,u_int reg_sv_flags,void *addr)
{
  ll_add(head,vaddr,addr);
  (*head)->reg_sv_flags=reg_sv_flags;
}

// Check if an address is already compiled
// but don't return addresses which are about to expire from the cache
void *check_addr(u_int vaddr)
{
  struct ht_entry *ht_bin = hash_table_get(vaddr);
  size_t i;
  for (i = 0; i < ARRAY_SIZE(ht_bin->vaddr); i++) {
    if (ht_bin->vaddr[i] == vaddr)
      if (doesnt_expire_soon((u_char *)ht_bin->tcaddr[i] - MAX_OUTPUT_BLOCK_SIZE))
        if (isclean(ht_bin->tcaddr[i]))
          return ht_bin->tcaddr[i];
  }
  u_int page=get_page(vaddr);
  struct ll_entry *head;
  head=jump_in[page];
  while (head != NULL) {
    if (head->vaddr == vaddr) {
      if (doesnt_expire_soon(head->addr)) {
        // Update existing entry with current address
        if (ht_bin->vaddr[0] == vaddr) {
          ht_bin->tcaddr[0] = head->addr;
          return head->addr;
        }
        if (ht_bin->vaddr[1] == vaddr) {
          ht_bin->tcaddr[1] = head->addr;
          return head->addr;
        }
        // Insert into hash table with low priority.
        // Don't evict existing entries, as they are probably
        // addresses that are being accessed frequently.
        if (ht_bin->vaddr[0] == -1) {
          ht_bin->vaddr[0] = vaddr;
          ht_bin->tcaddr[0] = head->addr;
        }
        else if (ht_bin->vaddr[1] == -1) {
          ht_bin->vaddr[1] = vaddr;
          ht_bin->tcaddr[1] = head->addr;
        }
        return head->addr;
      }
    }
    head=head->next;
  }
  return 0;
}

void remove_hash(int vaddr)
{
  //printf("remove hash: %x\n",vaddr);
  struct ht_entry *ht_bin = hash_table_get(vaddr);
  if (ht_bin->vaddr[1] == vaddr) {
    ht_bin->vaddr[1] = -1;
    ht_bin->tcaddr[1] = NULL;
  }
  if (ht_bin->vaddr[0] == vaddr) {
    ht_bin->vaddr[0] = ht_bin->vaddr[1];
    ht_bin->tcaddr[0] = ht_bin->tcaddr[1];
    ht_bin->vaddr[1] = -1;
    ht_bin->tcaddr[1] = NULL;
  }
}

void ll_remove_matching_addrs(struct ll_entry **head,uintptr_t addr,int shift)
{
  struct ll_entry *next;
  while(*head) {
    if(((uintptr_t)((*head)->addr)>>shift)==(addr>>shift) ||
       ((uintptr_t)((*head)->addr-MAX_OUTPUT_BLOCK_SIZE)>>shift)==(addr>>shift))
    {
      inv_debug("EXP: Remove pointer to %p (%x)\n",(*head)->addr,(*head)->vaddr);
      remove_hash((*head)->vaddr);
      next=(*head)->next;
      free(*head);
      *head=next;
    }
    else
    {
      head=&((*head)->next);
    }
  }
}

// Remove all entries from linked list
void ll_clear(struct ll_entry **head)
{
  struct ll_entry *cur;
  struct ll_entry *next;
  if((cur=*head)) {
    *head=0;
    while(cur) {
      next=cur->next;
      free(cur);
      cur=next;
    }
  }
}

// Dereference the pointers and remove if it matches
static void ll_kill_pointers(struct ll_entry *head,uintptr_t addr,int shift)
{
  while(head) {
    uintptr_t ptr = (uintptr_t)get_pointer(head->addr);
    inv_debug("EXP: Lookup pointer to %lx at %p (%x)\n",(long)ptr,head->addr,head->vaddr);
    if(((ptr>>shift)==(addr>>shift)) ||
       (((ptr-MAX_OUTPUT_BLOCK_SIZE)>>shift)==(addr>>shift)))
    {
      inv_debug("EXP: Kill pointer at %p (%x)\n",head->addr,head->vaddr);
      void *host_addr=find_extjump_insn(head->addr);
      mark_clear_cache(host_addr);
      set_jump_target(host_addr, head->addr);
    }
    head=head->next;
  }
}

// This is called when we write to a compiled block (see do_invstub)
static void invalidate_page(u_int page)
{
  struct ll_entry *head;
  struct ll_entry *next;
  head=jump_in[page];
  jump_in[page]=0;
  while(head!=NULL) {
    inv_debug("INVALIDATE: %x\n",head->vaddr);
    remove_hash(head->vaddr);
    next=head->next;
    free(head);
    head=next;
  }
  head=jump_out[page];
  jump_out[page]=0;
  while(head!=NULL) {
    inv_debug("INVALIDATE: kill pointer to %x (%p)\n",head->vaddr,head->addr);
    void *host_addr=find_extjump_insn(head->addr);
    mark_clear_cache(host_addr);
    set_jump_target(host_addr, head->addr);
    next=head->next;
    free(head);
    head=next;
  }
}

static void invalidate_block_range(u_int block, u_int first, u_int last)
{
  u_int page=get_page(block<<12);
  //printf("first=%d last=%d\n",first,last);
  invalidate_page(page);
  assert(first+5>page); // NB: this assumes MAXBLOCK<=4096 (4 pages)
  assert(last<page+5);
  // Invalidate the adjacent pages if a block crosses a 4K boundary
  while(first<page) {
    invalidate_page(first);
    first++;
  }
  for(first=page+1;first<last;first++) {
    invalidate_page(first);
  }
  do_clear_cache();

  // Don't trap writes
  invalid_code[block]=1;

  #ifdef USE_MINI_HT
  memset(mini_ht,-1,sizeof(mini_ht));
  #endif
}

void invalidate_block(u_int block)
{
  u_int page=get_page(block<<12);
  u_int vpage=get_vpage(block<<12);
  inv_debug("INVALIDATE: %x (%d)\n",block<<12,page);
  //inv_debug("invalid_code[block]=%d\n",invalid_code[block]);
  u_int first,last;
  first=last=page;
  struct ll_entry *head;
  head=jump_dirty[vpage];
  //printf("page=%d vpage=%d\n",page,vpage);
  while(head!=NULL) {
    if(vpage>2047||(head->vaddr>>12)==block) { // Ignore vaddr hash collision
      u_char *start, *end;
      get_bounds(head->addr, &start, &end);
      //printf("start: %p end: %p\n", start, end);
      if (page < 2048 && start >= rdram && end < rdram+RAM_SIZE) {
        if (((start-rdram)>>12) <= page && ((end-1-rdram)>>12) >= page) {
          if ((((start-rdram)>>12)&2047) < first) first = ((start-rdram)>>12)&2047;
          if ((((end-1-rdram)>>12)&2047) > last)  last = ((end-1-rdram)>>12)&2047;
        }
      }
    }
    head=head->next;
  }
  invalidate_block_range(block,first,last);
}

void invalidate_addr(u_int addr)
{
  //static int rhits;
  // this check is done by the caller
  //if (inv_code_start<=addr&&addr<=inv_code_end) { rhits++; return; }
  u_int page=get_vpage(addr);
  if(page<2048) { // RAM
    struct ll_entry *head;
    u_int addr_min=~0, addr_max=0;
    u_int mask=RAM_SIZE-1;
    u_int addr_main=0x80000000|(addr&mask);
    int pg1;
    inv_code_start=addr_main&~0xfff;
    inv_code_end=addr_main|0xfff;
    pg1=page;
    if (pg1>0) {
      // must check previous page too because of spans..
      pg1--;
      inv_code_start-=0x1000;
    }
    for(;pg1<=page;pg1++) {
      for(head=jump_dirty[pg1];head!=NULL;head=head->next) {
        u_char *start_h, *end_h;
        u_int start, end;
        get_bounds(head->addr, &start_h, &end_h);
        start = (uintptr_t)start_h - ram_offset;
        end = (uintptr_t)end_h - ram_offset;
        if(start<=addr_main&&addr_main<end) {
          if(start<addr_min) addr_min=start;
          if(end>addr_max) addr_max=end;
        }
        else if(addr_main<start) {
          if(start<inv_code_end)
            inv_code_end=start-1;
        }
        else {
          if(end>inv_code_start)
            inv_code_start=end;
        }
      }
    }
    if (addr_min!=~0) {
      inv_debug("INV ADDR: %08x hit %08x-%08x\n", addr, addr_min, addr_max);
      inv_code_start=inv_code_end=~0;
      invalidate_block_range(addr>>12,(addr_min&mask)>>12,(addr_max&mask)>>12);
      return;
    }
    else {
      inv_code_start=(addr&~mask)|(inv_code_start&mask);
      inv_code_end=(addr&~mask)|(inv_code_end&mask);
      inv_debug("INV ADDR: %08x miss, inv %08x-%08x, sk %d\n", addr, inv_code_start, inv_code_end, 0);
      return;
    }
  }
  invalidate_block(addr>>12);
}

// This is called when loading a save state.
// Anything could have changed, so invalidate everything.
void invalidate_all_pages(void)
{
  u_int page;
  for(page=0;page<4096;page++)
    invalidate_page(page);
  for(page=0;page<1048576;page++)
    if(!invalid_code[page]) {
      restore_candidate[(page&2047)>>3]|=1<<(page&7);
      restore_candidate[((page&2047)>>3)+256]|=1<<(page&7);
    }
  #ifdef USE_MINI_HT
  memset(mini_ht,-1,sizeof(mini_ht));
  #endif
  do_clear_cache();
}

static void do_invstub(int n)
{
  literal_pool(20);
  u_int reglist=stubs[n].a;
  set_jump_target(stubs[n].addr, out);
  save_regs(reglist);
  if(stubs[n].b!=0) emit_mov(stubs[n].b,0);
  emit_far_call(invalidate_addr);
  restore_regs(reglist);
  emit_jmp(stubs[n].retaddr); // return address
}

// Add an entry to jump_out after making a link
// src should point to code by emit_extjump2()
void add_link(u_int vaddr,void *src)
{
  u_int page=get_page(vaddr);
  inv_debug("add_link: %p -> %x (%d)\n",src,vaddr,page);
  check_extjump2(src);
  ll_add(jump_out+page,vaddr,src);
  //void *ptr=get_pointer(src);
  //inv_debug("add_link: Pointer is to %p\n",ptr);
}

// If a code block was found to be unmodified (bit was set in
// restore_candidate) and it remains unmodified (bit is clear
// in invalid_code) then move the entries for that 4K page from
// the dirty list to the clean list.
void clean_blocks(u_int page)
{
  struct ll_entry *head;
  inv_debug("INV: clean_blocks page=%d\n",page);
  head=jump_dirty[page];
  while(head!=NULL) {
    if(!invalid_code[head->vaddr>>12]) {
      // Don't restore blocks which are about to expire from the cache
      if (doesnt_expire_soon(head->addr)) {
        if(verify_dirty(head->addr)) {
          u_char *start, *end;
          //printf("Possibly Restore %x (%p)\n",head->vaddr, head->addr);
          u_int i;
          u_int inv=0;
          get_bounds(head->addr, &start, &end);
          if (start - rdram < RAM_SIZE) {
            for (i = (start-rdram+0x80000000)>>12; i <= (end-1-rdram+0x80000000)>>12; i++) {
              inv|=invalid_code[i];
            }
          }
          else if((signed int)head->vaddr>=(signed int)0x80000000+RAM_SIZE) {
            inv=1;
          }
          if(!inv) {
            void *clean_addr = get_clean_addr(head->addr);
            if (doesnt_expire_soon(clean_addr)) {
              u_int ppage=page;
              inv_debug("INV: Restored %x (%p/%p)\n",head->vaddr, head->addr, clean_addr);
              //printf("page=%x, addr=%x\n",page,head->vaddr);
              //assert(head->vaddr>>12==(page|0x80000));
              ll_add_flags(jump_in+ppage,head->vaddr,head->reg_sv_flags,clean_addr);
              struct ht_entry *ht_bin = hash_table_get(head->vaddr);
              if (ht_bin->vaddr[0] == head->vaddr)
                ht_bin->tcaddr[0] = clean_addr; // Replace existing entry
              if (ht_bin->vaddr[1] == head->vaddr)
                ht_bin->tcaddr[1] = clean_addr; // Replace existing entry
            }
          }
        }
      }
    }
    head=head->next;
  }
}

/* Register allocation */

// Note: registers are allocated clean (unmodified state)
// if you intend to modify the register, you must call dirty_reg().
static void alloc_reg(struct regstat *cur,int i,signed char reg)
{
  int r,hr;
  int preferred_reg = (reg&7);
  if(reg==CCREG) preferred_reg=HOST_CCREG;
  if(reg==PTEMP||reg==FTEMP) preferred_reg=12;

  // Don't allocate unused registers
  if((cur->u>>reg)&1) return;

  // see if it's already allocated
  for(hr=0;hr<HOST_REGS;hr++)
  {
    if(cur->regmap[hr]==reg) return;
  }

  // Keep the same mapping if the register was already allocated in a loop
  preferred_reg = loop_reg(i,reg,preferred_reg);

  // Try to allocate the preferred register
  if(cur->regmap[preferred_reg]==-1) {
    cur->regmap[preferred_reg]=reg;
    cur->dirty&=~(1<<preferred_reg);
    cur->isconst&=~(1<<preferred_reg);
    return;
  }
  r=cur->regmap[preferred_reg];
  assert(r < 64);
  if((cur->u>>r)&1) {
    cur->regmap[preferred_reg]=reg;
    cur->dirty&=~(1<<preferred_reg);
    cur->isconst&=~(1<<preferred_reg);
    return;
  }

  // Clear any unneeded registers
  // We try to keep the mapping consistent, if possible, because it
  // makes branches easier (especially loops).  So we try to allocate
  // first (see above) before removing old mappings.  If this is not
  // possible then go ahead and clear out the registers that are no
  // longer needed.
  for(hr=0;hr<HOST_REGS;hr++)
  {
    r=cur->regmap[hr];
    if(r>=0) {
      assert(r < 64);
      if((cur->u>>r)&1) {cur->regmap[hr]=-1;break;}
    }
  }
  // Try to allocate any available register, but prefer
  // registers that have not been used recently.
  if(i>0) {
    for(hr=0;hr<HOST_REGS;hr++) {
      if(hr!=EXCLUDE_REG&&cur->regmap[hr]==-1) {
        if(regs[i-1].regmap[hr]!=rs1[i-1]&&regs[i-1].regmap[hr]!=rs2[i-1]&&regs[i-1].regmap[hr]!=rt1[i-1]&&regs[i-1].regmap[hr]!=rt2[i-1]) {
          cur->regmap[hr]=reg;
          cur->dirty&=~(1<<hr);
          cur->isconst&=~(1<<hr);
          return;
        }
      }
    }
  }
  // Try to allocate any available register
  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG&&cur->regmap[hr]==-1) {
      cur->regmap[hr]=reg;
      cur->dirty&=~(1<<hr);
      cur->isconst&=~(1<<hr);
      return;
    }
  }

  // Ok, now we have to evict someone
  // Pick a register we hopefully won't need soon
  u_char hsn[MAXREG+1];
  memset(hsn,10,sizeof(hsn));
  int j;
  lsn(hsn,i,&preferred_reg);
  //printf("eax=%d ecx=%d edx=%d ebx=%d ebp=%d esi=%d edi=%d\n",cur->regmap[0],cur->regmap[1],cur->regmap[2],cur->regmap[3],cur->regmap[5],cur->regmap[6],cur->regmap[7]);
  //printf("hsn(%x): %d %d %d %d %d %d %d\n",start+i*4,hsn[cur->regmap[0]&63],hsn[cur->regmap[1]&63],hsn[cur->regmap[2]&63],hsn[cur->regmap[3]&63],hsn[cur->regmap[5]&63],hsn[cur->regmap[6]&63],hsn[cur->regmap[7]&63]);
  if(i>0) {
    // Don't evict the cycle count at entry points, otherwise the entry
    // stub will have to write it.
    if(bt[i]&&hsn[CCREG]>2) hsn[CCREG]=2;
    if(i>1&&hsn[CCREG]>2&&(itype[i-2]==RJUMP||itype[i-2]==UJUMP||itype[i-2]==CJUMP||itype[i-2]==SJUMP)) hsn[CCREG]=2;
    for(j=10;j>=3;j--)
    {
      // Alloc preferred register if available
      if(hsn[r=cur->regmap[preferred_reg]&63]==j) {
        for(hr=0;hr<HOST_REGS;hr++) {
          // Evict both parts of a 64-bit register
          if((cur->regmap[hr]&63)==r) {
            cur->regmap[hr]=-1;
            cur->dirty&=~(1<<hr);
            cur->isconst&=~(1<<hr);
          }
        }
        cur->regmap[preferred_reg]=reg;
        return;
      }
      for(r=1;r<=MAXREG;r++)
      {
        if(hsn[r]==j&&r!=rs1[i-1]&&r!=rs2[i-1]&&r!=rt1[i-1]&&r!=rt2[i-1]) {
          for(hr=0;hr<HOST_REGS;hr++) {
            if(hr!=HOST_CCREG||j<hsn[CCREG]) {
              if(cur->regmap[hr]==r) {
                cur->regmap[hr]=reg;
                cur->dirty&=~(1<<hr);
                cur->isconst&=~(1<<hr);
                return;
              }
            }
          }
        }
      }
    }
  }
  for(j=10;j>=0;j--)
  {
    for(r=1;r<=MAXREG;r++)
    {
      if(hsn[r]==j) {
        for(hr=0;hr<HOST_REGS;hr++) {
          if(cur->regmap[hr]==r) {
            cur->regmap[hr]=reg;
            cur->dirty&=~(1<<hr);
            cur->isconst&=~(1<<hr);
            return;
          }
        }
      }
    }
  }
  SysPrintf("This shouldn't happen (alloc_reg)");abort();
}

// Allocate a temporary register.  This is done without regard to
// dirty status or whether the register we request is on the unneeded list
// Note: This will only allocate one register, even if called multiple times
static void alloc_reg_temp(struct regstat *cur,int i,signed char reg)
{
  int r,hr;
  int preferred_reg = -1;

  // see if it's already allocated
  for(hr=0;hr<HOST_REGS;hr++)
  {
    if(hr!=EXCLUDE_REG&&cur->regmap[hr]==reg) return;
  }

  // Try to allocate any available register
  for(hr=HOST_REGS-1;hr>=0;hr--) {
    if(hr!=EXCLUDE_REG&&cur->regmap[hr]==-1) {
      cur->regmap[hr]=reg;
      cur->dirty&=~(1<<hr);
      cur->isconst&=~(1<<hr);
      return;
    }
  }

  // Find an unneeded register
  for(hr=HOST_REGS-1;hr>=0;hr--)
  {
    r=cur->regmap[hr];
    if(r>=0) {
      assert(r < 64);
      if((cur->u>>r)&1) {
        if(i==0||((unneeded_reg[i-1]>>r)&1)) {
          cur->regmap[hr]=reg;
          cur->dirty&=~(1<<hr);
          cur->isconst&=~(1<<hr);
          return;
        }
      }
    }
  }

  // Ok, now we have to evict someone
  // Pick a register we hopefully won't need soon
  // TODO: we might want to follow unconditional jumps here
  // TODO: get rid of dupe code and make this into a function
  u_char hsn[MAXREG+1];
  memset(hsn,10,sizeof(hsn));
  int j;
  lsn(hsn,i,&preferred_reg);
  //printf("hsn: %d %d %d %d %d %d %d\n",hsn[cur->regmap[0]&63],hsn[cur->regmap[1]&63],hsn[cur->regmap[2]&63],hsn[cur->regmap[3]&63],hsn[cur->regmap[5]&63],hsn[cur->regmap[6]&63],hsn[cur->regmap[7]&63]);
  if(i>0) {
    // Don't evict the cycle count at entry points, otherwise the entry
    // stub will have to write it.
    if(bt[i]&&hsn[CCREG]>2) hsn[CCREG]=2;
    if(i>1&&hsn[CCREG]>2&&(itype[i-2]==RJUMP||itype[i-2]==UJUMP||itype[i-2]==CJUMP||itype[i-2]==SJUMP)) hsn[CCREG]=2;
    for(j=10;j>=3;j--)
    {
      for(r=1;r<=MAXREG;r++)
      {
        if(hsn[r]==j&&r!=rs1[i-1]&&r!=rs2[i-1]&&r!=rt1[i-1]&&r!=rt2[i-1]) {
          for(hr=0;hr<HOST_REGS;hr++) {
            if(hr!=HOST_CCREG||hsn[CCREG]>2) {
              if(cur->regmap[hr]==r) {
                cur->regmap[hr]=reg;
                cur->dirty&=~(1<<hr);
                cur->isconst&=~(1<<hr);
                return;
              }
            }
          }
        }
      }
    }
  }
  for(j=10;j>=0;j--)
  {
    for(r=1;r<=MAXREG;r++)
    {
      if(hsn[r]==j) {
        for(hr=0;hr<HOST_REGS;hr++) {
          if(cur->regmap[hr]==r) {
            cur->regmap[hr]=reg;
            cur->dirty&=~(1<<hr);
            cur->isconst&=~(1<<hr);
            return;
          }
        }
      }
    }
  }
  SysPrintf("This shouldn't happen");abort();
}

static void mov_alloc(struct regstat *current,int i)
{
  // Note: Don't need to actually alloc the source registers
  //alloc_reg(current,i,rs1[i]);
  alloc_reg(current,i,rt1[i]);

  clear_const(current,rs1[i]);
  clear_const(current,rt1[i]);
  dirty_reg(current,rt1[i]);
}

static void shiftimm_alloc(struct regstat *current,int i)
{
  if(opcode2[i]<=0x3) // SLL/SRL/SRA
  {
    if(rt1[i]) {
      if(rs1[i]&&needed_again(rs1[i],i)) alloc_reg(current,i,rs1[i]);
      else lt1[i]=rs1[i];
      alloc_reg(current,i,rt1[i]);
      dirty_reg(current,rt1[i]);
      if(is_const(current,rs1[i])) {
        int v=get_const(current,rs1[i]);
        if(opcode2[i]==0x00) set_const(current,rt1[i],v<<imm[i]);
        if(opcode2[i]==0x02) set_const(current,rt1[i],(u_int)v>>imm[i]);
        if(opcode2[i]==0x03) set_const(current,rt1[i],v>>imm[i]);
      }
      else clear_const(current,rt1[i]);
    }
  }
  else
  {
    clear_const(current,rs1[i]);
    clear_const(current,rt1[i]);
  }

  if(opcode2[i]>=0x38&&opcode2[i]<=0x3b) // DSLL/DSRL/DSRA
  {
    assert(0);
  }
  if(opcode2[i]==0x3c) // DSLL32
  {
    assert(0);
  }
  if(opcode2[i]==0x3e) // DSRL32
  {
    assert(0);
  }
  if(opcode2[i]==0x3f) // DSRA32
  {
    assert(0);
  }
}

static void shift_alloc(struct regstat *current,int i)
{
  if(rt1[i]) {
    if(opcode2[i]<=0x07) // SLLV/SRLV/SRAV
    {
      if(rs1[i]) alloc_reg(current,i,rs1[i]);
      if(rs2[i]) alloc_reg(current,i,rs2[i]);
      alloc_reg(current,i,rt1[i]);
      if(rt1[i]==rs2[i]) {
        alloc_reg_temp(current,i,-1);
        minimum_free_regs[i]=1;
      }
    } else { // DSLLV/DSRLV/DSRAV
      assert(0);
    }
    clear_const(current,rs1[i]);
    clear_const(current,rs2[i]);
    clear_const(current,rt1[i]);
    dirty_reg(current,rt1[i]);
  }
}

static void alu_alloc(struct regstat *current,int i)
{
  if(opcode2[i]>=0x20&&opcode2[i]<=0x23) { // ADD/ADDU/SUB/SUBU
    if(rt1[i]) {
      if(rs1[i]&&rs2[i]) {
        alloc_reg(current,i,rs1[i]);
        alloc_reg(current,i,rs2[i]);
      }
      else {
        if(rs1[i]&&needed_again(rs1[i],i)) alloc_reg(current,i,rs1[i]);
        if(rs2[i]&&needed_again(rs2[i],i)) alloc_reg(current,i,rs2[i]);
      }
      alloc_reg(current,i,rt1[i]);
    }
  }
  if(opcode2[i]==0x2a||opcode2[i]==0x2b) { // SLT/SLTU
    if(rt1[i]) {
      alloc_reg(current,i,rs1[i]);
      alloc_reg(current,i,rs2[i]);
      alloc_reg(current,i,rt1[i]);
    }
  }
  if(opcode2[i]>=0x24&&opcode2[i]<=0x27) { // AND/OR/XOR/NOR
    if(rt1[i]) {
      if(rs1[i]&&rs2[i]) {
        alloc_reg(current,i,rs1[i]);
        alloc_reg(current,i,rs2[i]);
      }
      else
      {
        if(rs1[i]&&needed_again(rs1[i],i)) alloc_reg(current,i,rs1[i]);
        if(rs2[i]&&needed_again(rs2[i],i)) alloc_reg(current,i,rs2[i]);
      }
      alloc_reg(current,i,rt1[i]);
    }
  }
  if(opcode2[i]>=0x2c&&opcode2[i]<=0x2f) { // DADD/DADDU/DSUB/DSUBU
    assert(0);
  }
  clear_const(current,rs1[i]);
  clear_const(current,rs2[i]);
  clear_const(current,rt1[i]);
  dirty_reg(current,rt1[i]);
}

static void imm16_alloc(struct regstat *current,int i)
{
  if(rs1[i]&&needed_again(rs1[i],i)) alloc_reg(current,i,rs1[i]);
  else lt1[i]=rs1[i];
  if(rt1[i]) alloc_reg(current,i,rt1[i]);
  if(opcode[i]==0x18||opcode[i]==0x19) { // DADDI/DADDIU
    assert(0);
  }
  else if(opcode[i]==0x0a||opcode[i]==0x0b) { // SLTI/SLTIU
    clear_const(current,rs1[i]);
    clear_const(current,rt1[i]);
  }
  else if(opcode[i]>=0x0c&&opcode[i]<=0x0e) { // ANDI/ORI/XORI
    if(is_const(current,rs1[i])) {
      int v=get_const(current,rs1[i]);
      if(opcode[i]==0x0c) set_const(current,rt1[i],v&imm[i]);
      if(opcode[i]==0x0d) set_const(current,rt1[i],v|imm[i]);
      if(opcode[i]==0x0e) set_const(current,rt1[i],v^imm[i]);
    }
    else clear_const(current,rt1[i]);
  }
  else if(opcode[i]==0x08||opcode[i]==0x09) { // ADDI/ADDIU
    if(is_const(current,rs1[i])) {
      int v=get_const(current,rs1[i]);
      set_const(current,rt1[i],v+imm[i]);
    }
    else clear_const(current,rt1[i]);
  }
  else {
    set_const(current,rt1[i],imm[i]<<16); // LUI
  }
  dirty_reg(current,rt1[i]);
}

static void load_alloc(struct regstat *current,int i)
{
  clear_const(current,rt1[i]);
  //if(rs1[i]!=rt1[i]&&needed_again(rs1[i],i)) clear_const(current,rs1[i]); // Does this help or hurt?
  if(!rs1[i]) current->u&=~1LL; // Allow allocating r0 if it's the source register
  if(needed_again(rs1[i],i)) alloc_reg(current,i,rs1[i]);
  if(rt1[i]&&!((current->u>>rt1[i])&1)) {
    alloc_reg(current,i,rt1[i]);
    assert(get_reg(current->regmap,rt1[i])>=0);
    if(opcode[i]==0x27||opcode[i]==0x37) // LWU/LD
    {
      assert(0);
    }
    else if(opcode[i]==0x1A||opcode[i]==0x1B) // LDL/LDR
    {
      assert(0);
    }
    dirty_reg(current,rt1[i]);
    // LWL/LWR need a temporary register for the old value
    if(opcode[i]==0x22||opcode[i]==0x26)
    {
      alloc_reg(current,i,FTEMP);
      alloc_reg_temp(current,i,-1);
      minimum_free_regs[i]=1;
    }
  }
  else
  {
    // Load to r0 or unneeded register (dummy load)
    // but we still need a register to calculate the address
    if(opcode[i]==0x22||opcode[i]==0x26)
    {
      alloc_reg(current,i,FTEMP); // LWL/LWR need another temporary
    }
    alloc_reg_temp(current,i,-1);
    minimum_free_regs[i]=1;
    if(opcode[i]==0x1A||opcode[i]==0x1B) // LDL/LDR
    {
      assert(0);
    }
  }
}

void store_alloc(struct regstat *current,int i)
{
  clear_const(current,rs2[i]);
  if(!(rs2[i])) current->u&=~1LL; // Allow allocating r0 if necessary
  if(needed_again(rs1[i],i)) alloc_reg(current,i,rs1[i]);
  alloc_reg(current,i,rs2[i]);
  if(opcode[i]==0x2c||opcode[i]==0x2d||opcode[i]==0x3f) { // 64-bit SDL/SDR/SD
    assert(0);
  }
  #if defined(HOST_IMM8)
  // On CPUs without 32-bit immediates we need a pointer to invalid_code
  else alloc_reg(current,i,INVCP);
  #endif
  if(opcode[i]==0x2a||opcode[i]==0x2e||opcode[i]==0x2c||opcode[i]==0x2d) { // SWL/SWL/SDL/SDR
    alloc_reg(current,i,FTEMP);
  }
  // We need a temporary register for address generation
  alloc_reg_temp(current,i,-1);
  minimum_free_regs[i]=1;
}

void c1ls_alloc(struct regstat *current,int i)
{
  //clear_const(current,rs1[i]); // FIXME
  clear_const(current,rt1[i]);
  if(needed_again(rs1[i],i)) alloc_reg(current,i,rs1[i]);
  alloc_reg(current,i,CSREG); // Status
  alloc_reg(current,i,FTEMP);
  if(opcode[i]==0x35||opcode[i]==0x3d) { // 64-bit LDC1/SDC1
    assert(0);
  }
  #if defined(HOST_IMM8)
  // On CPUs without 32-bit immediates we need a pointer to invalid_code
  else if((opcode[i]&0x3b)==0x39) // SWC1/SDC1
    alloc_reg(current,i,INVCP);
  #endif
  // We need a temporary register for address generation
  alloc_reg_temp(current,i,-1);
}

void c2ls_alloc(struct regstat *current,int i)
{
  clear_const(current,rt1[i]);
  if(needed_again(rs1[i],i)) alloc_reg(current,i,rs1[i]);
  alloc_reg(current,i,FTEMP);
  #if defined(HOST_IMM8)
  // On CPUs without 32-bit immediates we need a pointer to invalid_code
  if((opcode[i]&0x3b)==0x3a) // SWC2/SDC2
    alloc_reg(current,i,INVCP);
  #endif
  // We need a temporary register for address generation
  alloc_reg_temp(current,i,-1);
  minimum_free_regs[i]=1;
}

#ifndef multdiv_alloc
void multdiv_alloc(struct regstat *current,int i)
{
  //  case 0x18: MULT
  //  case 0x19: MULTU
  //  case 0x1A: DIV
  //  case 0x1B: DIVU
  //  case 0x1C: DMULT
  //  case 0x1D: DMULTU
  //  case 0x1E: DDIV
  //  case 0x1F: DDIVU
  clear_const(current,rs1[i]);
  clear_const(current,rs2[i]);
  if(rs1[i]&&rs2[i])
  {
    if((opcode2[i]&4)==0) // 32-bit
    {
      current->u&=~(1LL<<HIREG);
      current->u&=~(1LL<<LOREG);
      alloc_reg(current,i,HIREG);
      alloc_reg(current,i,LOREG);
      alloc_reg(current,i,rs1[i]);
      alloc_reg(current,i,rs2[i]);
      dirty_reg(current,HIREG);
      dirty_reg(current,LOREG);
    }
    else // 64-bit
    {
      assert(0);
    }
  }
  else
  {
    // Multiply by zero is zero.
    // MIPS does not have a divide by zero exception.
    // The result is undefined, we return zero.
    alloc_reg(current,i,HIREG);
    alloc_reg(current,i,LOREG);
    dirty_reg(current,HIREG);
    dirty_reg(current,LOREG);
  }
}
#endif

void cop0_alloc(struct regstat *current,int i)
{
  if(opcode2[i]==0) // MFC0
  {
    if(rt1[i]) {
      clear_const(current,rt1[i]);
      alloc_all(current,i);
      alloc_reg(current,i,rt1[i]);
      dirty_reg(current,rt1[i]);
    }
  }
  else if(opcode2[i]==4) // MTC0
  {
    if(rs1[i]){
      clear_const(current,rs1[i]);
      alloc_reg(current,i,rs1[i]);
      alloc_all(current,i);
    }
    else {
      alloc_all(current,i); // FIXME: Keep r0
      current->u&=~1LL;
      alloc_reg(current,i,0);
    }
  }
  else
  {
    // TLBR/TLBWI/TLBWR/TLBP/ERET
    assert(opcode2[i]==0x10);
    alloc_all(current,i);
  }
  minimum_free_regs[i]=HOST_REGS;
}

static void cop2_alloc(struct regstat *current,int i)
{
  if (opcode2[i] < 3) // MFC2/CFC2
  {
    alloc_cc(current,i); // for stalls
    dirty_reg(current,CCREG);
    if(rt1[i]){
      clear_const(current,rt1[i]);
      alloc_reg(current,i,rt1[i]);
      dirty_reg(current,rt1[i]);
    }
  }
  else if (opcode2[i] > 3) // MTC2/CTC2
  {
    if(rs1[i]){
      clear_const(current,rs1[i]);
      alloc_reg(current,i,rs1[i]);
    }
    else {
      current->u&=~1LL;
      alloc_reg(current,i,0);
    }
  }
  alloc_reg_temp(current,i,-1);
  minimum_free_regs[i]=1;
}

void c2op_alloc(struct regstat *current,int i)
{
  alloc_cc(current,i); // for stalls
  dirty_reg(current,CCREG);
  alloc_reg_temp(current,i,-1);
}

void syscall_alloc(struct regstat *current,int i)
{
  alloc_cc(current,i);
  dirty_reg(current,CCREG);
  alloc_all(current,i);
  minimum_free_regs[i]=HOST_REGS;
  current->isconst=0;
}

void delayslot_alloc(struct regstat *current,int i)
{
  switch(itype[i]) {
    case UJUMP:
    case CJUMP:
    case SJUMP:
    case RJUMP:
    case SYSCALL:
    case HLECALL:
    case SPAN:
      assem_debug("jump in the delay slot.  this shouldn't happen.\n");//abort();
      SysPrintf("Disabled speculative precompilation\n");
      stop_after_jal=1;
      break;
    case IMM16:
      imm16_alloc(current,i);
      break;
    case LOAD:
    case LOADLR:
      load_alloc(current,i);
      break;
    case STORE:
    case STORELR:
      store_alloc(current,i);
      break;
    case ALU:
      alu_alloc(current,i);
      break;
    case SHIFT:
      shift_alloc(current,i);
      break;
    case MULTDIV:
      multdiv_alloc(current,i);
      break;
    case SHIFTIMM:
      shiftimm_alloc(current,i);
      break;
    case MOV:
      mov_alloc(current,i);
      break;
    case COP0:
      cop0_alloc(current,i);
      break;
    case COP1:
      break;
    case COP2:
      cop2_alloc(current,i);
      break;
    case C1LS:
      c1ls_alloc(current,i);
      break;
    case C2LS:
      c2ls_alloc(current,i);
      break;
    case C2OP:
      c2op_alloc(current,i);
      break;
  }
}

// Special case where a branch and delay slot span two pages in virtual memory
static void pagespan_alloc(struct regstat *current,int i)
{
  current->isconst=0;
  current->wasconst=0;
  regs[i].wasconst=0;
  minimum_free_regs[i]=HOST_REGS;
  alloc_all(current,i);
  alloc_cc(current,i);
  dirty_reg(current,CCREG);
  if(opcode[i]==3) // JAL
  {
    alloc_reg(current,i,31);
    dirty_reg(current,31);
  }
  if(opcode[i]==0&&(opcode2[i]&0x3E)==8) // JR/JALR
  {
    alloc_reg(current,i,rs1[i]);
    if (rt1[i]!=0) {
      alloc_reg(current,i,rt1[i]);
      dirty_reg(current,rt1[i]);
    }
  }
  if((opcode[i]&0x2E)==4) // BEQ/BNE/BEQL/BNEL
  {
    if(rs1[i]) alloc_reg(current,i,rs1[i]);
    if(rs2[i]) alloc_reg(current,i,rs2[i]);
  }
  else
  if((opcode[i]&0x2E)==6) // BLEZ/BGTZ/BLEZL/BGTZL
  {
    if(rs1[i]) alloc_reg(current,i,rs1[i]);
  }
  //else ...
}

static void add_stub(enum stub_type type, void *addr, void *retaddr,
  u_int a, uintptr_t b, uintptr_t c, u_int d, u_int e)
{
  assert(stubcount < ARRAY_SIZE(stubs));
  stubs[stubcount].type = type;
  stubs[stubcount].addr = addr;
  stubs[stubcount].retaddr = retaddr;
  stubs[stubcount].a = a;
  stubs[stubcount].b = b;
  stubs[stubcount].c = c;
  stubs[stubcount].d = d;
  stubs[stubcount].e = e;
  stubcount++;
}

static void add_stub_r(enum stub_type type, void *addr, void *retaddr,
  int i, int addr_reg, const struct regstat *i_regs, int ccadj, u_int reglist)
{
  add_stub(type, addr, retaddr, i, addr_reg, (uintptr_t)i_regs, ccadj, reglist);
}

// Write out a single register
static void wb_register(signed char r,signed char regmap[],uint64_t dirty)
{
  int hr;
  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG) {
      if((regmap[hr]&63)==r) {
        if((dirty>>hr)&1) {
          assert(regmap[hr]<64);
          emit_storereg(r,hr);
        }
      }
    }
  }
}

static void wb_valid(signed char pre[],signed char entry[],u_int dirty_pre,u_int dirty,uint64_t u)
{
  //if(dirty_pre==dirty) return;
  int hr,reg;
  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG) {
      reg=pre[hr];
      if(((~u)>>(reg&63))&1) {
        if(reg>0) {
          if(((dirty_pre&~dirty)>>hr)&1) {
            if(reg>0&&reg<34) {
              emit_storereg(reg,hr);
            }
            else if(reg>=64) {
              assert(0);
            }
          }
        }
      }
    }
  }
}

// trashes r2
static void pass_args(int a0, int a1)
{
  if(a0==1&&a1==0) {
    // must swap
    emit_mov(a0,2); emit_mov(a1,1); emit_mov(2,0);
  }
  else if(a0!=0&&a1==0) {
    emit_mov(a1,1);
    if (a0>=0) emit_mov(a0,0);
  }
  else {
    if(a0>=0&&a0!=0) emit_mov(a0,0);
    if(a1>=0&&a1!=1) emit_mov(a1,1);
  }
}

static void alu_assemble(int i,struct regstat *i_regs)
{
  if(opcode2[i]>=0x20&&opcode2[i]<=0x23) { // ADD/ADDU/SUB/SUBU
    if(rt1[i]) {
      signed char s1,s2,t;
      t=get_reg(i_regs->regmap,rt1[i]);
      if(t>=0) {
        s1=get_reg(i_regs->regmap,rs1[i]);
        s2=get_reg(i_regs->regmap,rs2[i]);
        if(rs1[i]&&rs2[i]) {
          assert(s1>=0);
          assert(s2>=0);
          if(opcode2[i]&2) emit_sub(s1,s2,t);
          else emit_add(s1,s2,t);
        }
        else if(rs1[i]) {
          if(s1>=0) emit_mov(s1,t);
          else emit_loadreg(rs1[i],t);
        }
        else if(rs2[i]) {
          if(s2>=0) {
            if(opcode2[i]&2) emit_neg(s2,t);
            else emit_mov(s2,t);
          }
          else {
            emit_loadreg(rs2[i],t);
            if(opcode2[i]&2) emit_neg(t,t);
          }
        }
        else emit_zeroreg(t);
      }
    }
  }
  if(opcode2[i]>=0x2c&&opcode2[i]<=0x2f) { // DADD/DADDU/DSUB/DSUBU
    assert(0);
  }
  if(opcode2[i]==0x2a||opcode2[i]==0x2b) { // SLT/SLTU
    if(rt1[i]) {
      signed char s1l,s2l,t;
      {
        t=get_reg(i_regs->regmap,rt1[i]);
        //assert(t>=0);
        if(t>=0) {
          s1l=get_reg(i_regs->regmap,rs1[i]);
          s2l=get_reg(i_regs->regmap,rs2[i]);
          if(rs2[i]==0) // rx<r0
          {
            if(opcode2[i]==0x2a&&rs1[i]!=0) { // SLT
              assert(s1l>=0);
              emit_shrimm(s1l,31,t);
            }
            else // SLTU (unsigned can not be less than zero, 0<0)
              emit_zeroreg(t);
          }
          else if(rs1[i]==0) // r0<rx
          {
            assert(s2l>=0);
            if(opcode2[i]==0x2a) // SLT
              emit_set_gz32(s2l,t);
            else // SLTU (set if not zero)
              emit_set_nz32(s2l,t);
          }
          else{
            assert(s1l>=0);assert(s2l>=0);
            if(opcode2[i]==0x2a) // SLT
              emit_set_if_less32(s1l,s2l,t);
            else // SLTU
              emit_set_if_carry32(s1l,s2l,t);
          }
        }
      }
    }
  }
  if(opcode2[i]>=0x24&&opcode2[i]<=0x27) { // AND/OR/XOR/NOR
    if(rt1[i]) {
      signed char s1l,s2l,tl;
      tl=get_reg(i_regs->regmap,rt1[i]);
      {
        if(tl>=0) {
          s1l=get_reg(i_regs->regmap,rs1[i]);
          s2l=get_reg(i_regs->regmap,rs2[i]);
          if(rs1[i]&&rs2[i]) {
            assert(s1l>=0);
            assert(s2l>=0);
            if(opcode2[i]==0x24) { // AND
              emit_and(s1l,s2l,tl);
            } else
            if(opcode2[i]==0x25) { // OR
              emit_or(s1l,s2l,tl);
            } else
            if(opcode2[i]==0x26) { // XOR
              emit_xor(s1l,s2l,tl);
            } else
            if(opcode2[i]==0x27) { // NOR
              emit_or(s1l,s2l,tl);
              emit_not(tl,tl);
            }
          }
          else
          {
            if(opcode2[i]==0x24) { // AND
              emit_zeroreg(tl);
            } else
            if(opcode2[i]==0x25||opcode2[i]==0x26) { // OR/XOR
              if(rs1[i]){
                if(s1l>=0) emit_mov(s1l,tl);
                else emit_loadreg(rs1[i],tl); // CHECK: regmap_entry?
              }
              else
              if(rs2[i]){
                if(s2l>=0) emit_mov(s2l,tl);
                else emit_loadreg(rs2[i],tl); // CHECK: regmap_entry?
              }
              else emit_zeroreg(tl);
            } else
            if(opcode2[i]==0x27) { // NOR
              if(rs1[i]){
                if(s1l>=0) emit_not(s1l,tl);
                else {
                  emit_loadreg(rs1[i],tl);
                  emit_not(tl,tl);
                }
              }
              else
              if(rs2[i]){
                if(s2l>=0) emit_not(s2l,tl);
                else {
                  emit_loadreg(rs2[i],tl);
                  emit_not(tl,tl);
                }
              }
              else emit_movimm(-1,tl);
            }
          }
        }
      }
    }
  }
}

void imm16_assemble(int i,struct regstat *i_regs)
{
  if (opcode[i]==0x0f) { // LUI
    if(rt1[i]) {
      signed char t;
      t=get_reg(i_regs->regmap,rt1[i]);
      //assert(t>=0);
      if(t>=0) {
        if(!((i_regs->isconst>>t)&1))
          emit_movimm(imm[i]<<16,t);
      }
    }
  }
  if(opcode[i]==0x08||opcode[i]==0x09) { // ADDI/ADDIU
    if(rt1[i]) {
      signed char s,t;
      t=get_reg(i_regs->regmap,rt1[i]);
      s=get_reg(i_regs->regmap,rs1[i]);
      if(rs1[i]) {
        //assert(t>=0);
        //assert(s>=0);
        if(t>=0) {
          if(!((i_regs->isconst>>t)&1)) {
            if(s<0) {
              if(i_regs->regmap_entry[t]!=rs1[i]) emit_loadreg(rs1[i],t);
              emit_addimm(t,imm[i],t);
            }else{
              if(!((i_regs->wasconst>>s)&1))
                emit_addimm(s,imm[i],t);
              else
                emit_movimm(constmap[i][s]+imm[i],t);
            }
          }
        }
      } else {
        if(t>=0) {
          if(!((i_regs->isconst>>t)&1))
            emit_movimm(imm[i],t);
        }
      }
    }
  }
  if(opcode[i]==0x18||opcode[i]==0x19) { // DADDI/DADDIU
    if(rt1[i]) {
      signed char sl,tl;
      tl=get_reg(i_regs->regmap,rt1[i]);
      sl=get_reg(i_regs->regmap,rs1[i]);
      if(tl>=0) {
        if(rs1[i]) {
          assert(sl>=0);
          emit_addimm(sl,imm[i],tl);
        } else {
          emit_movimm(imm[i],tl);
        }
      }
    }
  }
  else if(opcode[i]==0x0a||opcode[i]==0x0b) { // SLTI/SLTIU
    if(rt1[i]) {
      //assert(rs1[i]!=0); // r0 might be valid, but it's probably a bug
      signed char sl,t;
      t=get_reg(i_regs->regmap,rt1[i]);
      sl=get_reg(i_regs->regmap,rs1[i]);
      //assert(t>=0);
      if(t>=0) {
        if(rs1[i]>0) {
            if(opcode[i]==0x0a) { // SLTI
              if(sl<0) {
                if(i_regs->regmap_entry[t]!=rs1[i]) emit_loadreg(rs1[i],t);
                emit_slti32(t,imm[i],t);
              }else{
                emit_slti32(sl,imm[i],t);
              }
            }
            else { // SLTIU
              if(sl<0) {
                if(i_regs->regmap_entry[t]!=rs1[i]) emit_loadreg(rs1[i],t);
                emit_sltiu32(t,imm[i],t);
              }else{
                emit_sltiu32(sl,imm[i],t);
              }
            }
        }else{
          // SLTI(U) with r0 is just stupid,
          // nonetheless examples can be found
          if(opcode[i]==0x0a) // SLTI
            if(0<imm[i]) emit_movimm(1,t);
            else emit_zeroreg(t);
          else // SLTIU
          {
            if(imm[i]) emit_movimm(1,t);
            else emit_zeroreg(t);
          }
        }
      }
    }
  }
  else if(opcode[i]>=0x0c&&opcode[i]<=0x0e) { // ANDI/ORI/XORI
    if(rt1[i]) {
      signed char sl,tl;
      tl=get_reg(i_regs->regmap,rt1[i]);
      sl=get_reg(i_regs->regmap,rs1[i]);
      if(tl>=0 && !((i_regs->isconst>>tl)&1)) {
        if(opcode[i]==0x0c) //ANDI
        {
          if(rs1[i]) {
            if(sl<0) {
              if(i_regs->regmap_entry[tl]!=rs1[i]) emit_loadreg(rs1[i],tl);
              emit_andimm(tl,imm[i],tl);
            }else{
              if(!((i_regs->wasconst>>sl)&1))
                emit_andimm(sl,imm[i],tl);
              else
                emit_movimm(constmap[i][sl]&imm[i],tl);
            }
          }
          else
            emit_zeroreg(tl);
        }
        else
        {
          if(rs1[i]) {
            if(sl<0) {
              if(i_regs->regmap_entry[tl]!=rs1[i]) emit_loadreg(rs1[i],tl);
            }
            if(opcode[i]==0x0d) { // ORI
              if(sl<0) {
                emit_orimm(tl,imm[i],tl);
              }else{
                if(!((i_regs->wasconst>>sl)&1))
                  emit_orimm(sl,imm[i],tl);
                else
                  emit_movimm(constmap[i][sl]|imm[i],tl);
              }
            }
            if(opcode[i]==0x0e) { // XORI
              if(sl<0) {
                emit_xorimm(tl,imm[i],tl);
              }else{
                if(!((i_regs->wasconst>>sl)&1))
                  emit_xorimm(sl,imm[i],tl);
                else
                  emit_movimm(constmap[i][sl]^imm[i],tl);
              }
            }
          }
          else {
            emit_movimm(imm[i],tl);
          }
        }
      }
    }
  }
}

void shiftimm_assemble(int i,struct regstat *i_regs)
{
  if(opcode2[i]<=0x3) // SLL/SRL/SRA
  {
    if(rt1[i]) {
      signed char s,t;
      t=get_reg(i_regs->regmap,rt1[i]);
      s=get_reg(i_regs->regmap,rs1[i]);
      //assert(t>=0);
      if(t>=0&&!((i_regs->isconst>>t)&1)){
        if(rs1[i]==0)
        {
          emit_zeroreg(t);
        }
        else
        {
          if(s<0&&i_regs->regmap_entry[t]!=rs1[i]) emit_loadreg(rs1[i],t);
          if(imm[i]) {
            if(opcode2[i]==0) // SLL
            {
              emit_shlimm(s<0?t:s,imm[i],t);
            }
            if(opcode2[i]==2) // SRL
            {
              emit_shrimm(s<0?t:s,imm[i],t);
            }
            if(opcode2[i]==3) // SRA
            {
              emit_sarimm(s<0?t:s,imm[i],t);
            }
          }else{
            // Shift by zero
            if(s>=0 && s!=t) emit_mov(s,t);
          }
        }
      }
      //emit_storereg(rt1[i],t); //DEBUG
    }
  }
  if(opcode2[i]>=0x38&&opcode2[i]<=0x3b) // DSLL/DSRL/DSRA
  {
    assert(0);
  }
  if(opcode2[i]==0x3c) // DSLL32
  {
    assert(0);
  }
  if(opcode2[i]==0x3e) // DSRL32
  {
    assert(0);
  }
  if(opcode2[i]==0x3f) // DSRA32
  {
    assert(0);
  }
}

#ifndef shift_assemble
static void shift_assemble(int i,struct regstat *i_regs)
{
  signed char s,t,shift;
  if (rt1[i] == 0)
    return;
  assert(opcode2[i]<=0x07); // SLLV/SRLV/SRAV
  t = get_reg(i_regs->regmap, rt1[i]);
  s = get_reg(i_regs->regmap, rs1[i]);
  shift = get_reg(i_regs->regmap, rs2[i]);
  if (t < 0)
    return;

  if(rs1[i]==0)
    emit_zeroreg(t);
  else if(rs2[i]==0) {
    assert(s>=0);
    if(s!=t) emit_mov(s,t);
  }
  else {
    host_tempreg_acquire();
    emit_andimm(shift,31,HOST_TEMPREG);
    switch(opcode2[i]) {
    case 4: // SLLV
      emit_shl(s,HOST_TEMPREG,t);
      break;
    case 6: // SRLV
      emit_shr(s,HOST_TEMPREG,t);
      break;
    case 7: // SRAV
      emit_sar(s,HOST_TEMPREG,t);
      break;
    default:
      assert(0);
    }
    host_tempreg_release();
  }
}

#endif

enum {
  MTYPE_8000 = 0,
  MTYPE_8020,
  MTYPE_0000,
  MTYPE_A000,
  MTYPE_1F80,
};

static int get_ptr_mem_type(u_int a)
{
  if(a < 0x00200000) {
    if(a<0x1000&&((start>>20)==0xbfc||(start>>24)==0xa0))
      // return wrong, must use memhandler for BIOS self-test to pass
      // 007 does similar stuff from a00 mirror, weird stuff
      return MTYPE_8000;
    return MTYPE_0000;
  }
  if(0x1f800000 <= a && a < 0x1f801000)
    return MTYPE_1F80;
  if(0x80200000 <= a && a < 0x80800000)
    return MTYPE_8020;
  if(0xa0000000 <= a && a < 0xa0200000)
    return MTYPE_A000;
  return MTYPE_8000;
}

static void *emit_fastpath_cmp_jump(int i,int addr,int *addr_reg_override)
{
  void *jaddr = NULL;
  int type=0;
  int mr=rs1[i];
  if(((smrv_strong|smrv_weak)>>mr)&1) {
    type=get_ptr_mem_type(smrv[mr]);
    //printf("set %08x @%08x r%d %d\n", smrv[mr], start+i*4, mr, type);
  }
  else {
    // use the mirror we are running on
    type=get_ptr_mem_type(start);
    //printf("set nospec   @%08x r%d %d\n", start+i*4, mr, type);
  }

  if(type==MTYPE_8020) { // RAM 80200000+ mirror
    host_tempreg_acquire();
    emit_andimm(addr,~0x00e00000,HOST_TEMPREG);
    addr=*addr_reg_override=HOST_TEMPREG;
    type=0;
  }
  else if(type==MTYPE_0000) { // RAM 0 mirror
    host_tempreg_acquire();
    emit_orimm(addr,0x80000000,HOST_TEMPREG);
    addr=*addr_reg_override=HOST_TEMPREG;
    type=0;
  }
  else if(type==MTYPE_A000) { // RAM A mirror
    host_tempreg_acquire();
    emit_andimm(addr,~0x20000000,HOST_TEMPREG);
    addr=*addr_reg_override=HOST_TEMPREG;
    type=0;
  }
  else if(type==MTYPE_1F80) { // scratchpad
    if (psxH == (void *)0x1f800000) {
      host_tempreg_acquire();
      emit_xorimm(addr,0x1f800000,HOST_TEMPREG);
      emit_cmpimm(HOST_TEMPREG,0x1000);
      host_tempreg_release();
      jaddr=out;
      emit_jc(0);
    }
    else {
      // do the usual RAM check, jump will go to the right handler
      type=0;
    }
  }

  if(type==0)
  {
    emit_cmpimm(addr,RAM_SIZE);
    jaddr=out;
    #ifdef CORTEX_A8_BRANCH_PREDICTION_HACK
    // Hint to branch predictor that the branch is unlikely to be taken
    if(rs1[i]>=28)
      emit_jno_unlikely(0);
    else
    #endif
      emit_jno(0);
    if(ram_offset!=0) {
      host_tempreg_acquire();
      emit_addimm(addr,ram_offset,HOST_TEMPREG);
      addr=*addr_reg_override=HOST_TEMPREG;
    }
  }

  return jaddr;
}

// return memhandler, or get directly accessable address and return 0
static void *get_direct_memhandler(void *table, u_int addr,
  enum stub_type type, uintptr_t *addr_host)
{
  uintptr_t l1, l2 = 0;
  l1 = ((uintptr_t *)table)[addr>>12];
  if ((l1 & (1ul << (sizeof(l1)*8-1))) == 0) {
    uintptr_t v = l1 << 1;
    *addr_host = v + addr;
    return NULL;
  }
  else {
    l1 <<= 1;
    if (type == LOADB_STUB || type == LOADBU_STUB || type == STOREB_STUB)
      l2 = ((uintptr_t *)l1)[0x1000/4 + 0x1000/2 + (addr&0xfff)];
    else if (type == LOADH_STUB || type == LOADHU_STUB || type == STOREH_STUB)
      l2=((uintptr_t *)l1)[0x1000/4 + (addr&0xfff)/2];
    else
      l2=((uintptr_t *)l1)[(addr&0xfff)/4];
    if ((l2 & (1<<31)) == 0) {
      uintptr_t v = l2 << 1;
      *addr_host = v + (addr&0xfff);
      return NULL;
    }
    return (void *)(l2 << 1);
  }
}

static u_int get_host_reglist(const signed char *regmap)
{
  u_int reglist = 0, hr;
  for (hr = 0; hr < HOST_REGS; hr++) {
    if (hr != EXCLUDE_REG && regmap[hr] >= 0)
      reglist |= 1 << hr;
  }
  return reglist;
}

static u_int reglist_exclude(u_int reglist, int r1, int r2)
{
  if (r1 >= 0)
    reglist &= ~(1u << r1);
  if (r2 >= 0)
    reglist &= ~(1u << r2);
  return reglist;
}

// find a temp caller-saved register not in reglist (so assumed to be free)
static int reglist_find_free(u_int reglist)
{
  u_int free_regs = ~reglist & CALLER_SAVE_REGS;
  if (free_regs == 0)
    return -1;
  return __builtin_ctz(free_regs);
}

static void load_assemble(int i, const struct regstat *i_regs)
{
  int s,tl,addr;
  int offset;
  void *jaddr=0;
  int memtarget=0,c=0;
  int fastio_reg_override=-1;
  u_int reglist=get_host_reglist(i_regs->regmap);
  tl=get_reg(i_regs->regmap,rt1[i]);
  s=get_reg(i_regs->regmap,rs1[i]);
  offset=imm[i];
  if(i_regs->regmap[HOST_CCREG]==CCREG) reglist&=~(1<<HOST_CCREG);
  if(s>=0) {
    c=(i_regs->wasconst>>s)&1;
    if (c) {
      memtarget=((signed int)(constmap[i][s]+offset))<(signed int)0x80000000+RAM_SIZE;
    }
  }
  //printf("load_assemble: c=%d\n",c);
  //if(c) printf("load_assemble: const=%lx\n",(long)constmap[i][s]+offset);
  // FIXME: Even if the load is a NOP, we should check for pagefaults...
  if((tl<0&&(!c||(((u_int)constmap[i][s]+offset)>>16)==0x1f80))
    ||rt1[i]==0) {
      // could be FIFO, must perform the read
      // ||dummy read
      assem_debug("(forced read)\n");
      tl=get_reg(i_regs->regmap,-1);
      assert(tl>=0);
  }
  if(offset||s<0||c) addr=tl;
  else addr=s;
  //if(tl<0) tl=get_reg(i_regs->regmap,-1);
 if(tl>=0) {
  //printf("load_assemble: c=%d\n",c);
  //if(c) printf("load_assemble: const=%lx\n",(long)constmap[i][s]+offset);
  assert(tl>=0); // Even if the load is a NOP, we must check for pagefaults and I/O
  reglist&=~(1<<tl);
  if(!c) {
    #ifdef R29_HACK
    // Strmnnrmn's speed hack
    if(rs1[i]!=29||start<0x80001000||start>=0x80000000+RAM_SIZE)
    #endif
    {
      jaddr=emit_fastpath_cmp_jump(i,addr,&fastio_reg_override);
    }
  }
  else if(ram_offset&&memtarget) {
    host_tempreg_acquire();
    emit_addimm(addr,ram_offset,HOST_TEMPREG);
    fastio_reg_override=HOST_TEMPREG;
  }
  int dummy=(rt1[i]==0)||(tl!=get_reg(i_regs->regmap,rt1[i])); // ignore loads to r0 and unneeded reg
  if (opcode[i]==0x20) { // LB
    if(!c||memtarget) {
      if(!dummy) {
        {
          int x=0,a=tl;
          if(!c) a=addr;
          if(fastio_reg_override>=0) a=fastio_reg_override;

          emit_movsbl_indexed(x,a,tl);
        }
      }
      if(jaddr)
        add_stub_r(LOADB_STUB,jaddr,out,i,addr,i_regs,ccadj[i],reglist);
    }
    else
      inline_readstub(LOADB_STUB,i,constmap[i][s]+offset,i_regs->regmap,rt1[i],ccadj[i],reglist);
  }
  if (opcode[i]==0x21) { // LH
    if(!c||memtarget) {
      if(!dummy) {
        int x=0,a=tl;
        if(!c) a=addr;
        if(fastio_reg_override>=0) a=fastio_reg_override;
        emit_movswl_indexed(x,a,tl);
      }
      if(jaddr)
        add_stub_r(LOADH_STUB,jaddr,out,i,addr,i_regs,ccadj[i],reglist);
    }
    else
      inline_readstub(LOADH_STUB,i,constmap[i][s]+offset,i_regs->regmap,rt1[i],ccadj[i],reglist);
  }
  if (opcode[i]==0x23) { // LW
    if(!c||memtarget) {
      if(!dummy) {
        int a=addr;
        if(fastio_reg_override>=0) a=fastio_reg_override;
        emit_readword_indexed(0,a,tl);
      }
      if(jaddr)
        add_stub_r(LOADW_STUB,jaddr,out,i,addr,i_regs,ccadj[i],reglist);
    }
    else
      inline_readstub(LOADW_STUB,i,constmap[i][s]+offset,i_regs->regmap,rt1[i],ccadj[i],reglist);
  }
  if (opcode[i]==0x24) { // LBU
    if(!c||memtarget) {
      if(!dummy) {
        int x=0,a=tl;
        if(!c) a=addr;
        if(fastio_reg_override>=0) a=fastio_reg_override;

        emit_movzbl_indexed(x,a,tl);
      }
      if(jaddr)
        add_stub_r(LOADBU_STUB,jaddr,out,i,addr,i_regs,ccadj[i],reglist);
    }
    else
      inline_readstub(LOADBU_STUB,i,constmap[i][s]+offset,i_regs->regmap,rt1[i],ccadj[i],reglist);
  }
  if (opcode[i]==0x25) { // LHU
    if(!c||memtarget) {
      if(!dummy) {
        int x=0,a=tl;
        if(!c) a=addr;
        if(fastio_reg_override>=0) a=fastio_reg_override;
        emit_movzwl_indexed(x,a,tl);
      }
      if(jaddr)
        add_stub_r(LOADHU_STUB,jaddr,out,i,addr,i_regs,ccadj[i],reglist);
    }
    else
      inline_readstub(LOADHU_STUB,i,constmap[i][s]+offset,i_regs->regmap,rt1[i],ccadj[i],reglist);
  }
  if (opcode[i]==0x27) { // LWU
    assert(0);
  }
  if (opcode[i]==0x37) { // LD
    assert(0);
  }
 }
 if (fastio_reg_override == HOST_TEMPREG)
   host_tempreg_release();
}

#ifndef loadlr_assemble
static void loadlr_assemble(int i, const struct regstat *i_regs)
{
  int s,tl,temp,temp2,addr;
  int offset;
  void *jaddr=0;
  int memtarget=0,c=0;
  int fastio_reg_override=-1;
  u_int reglist=get_host_reglist(i_regs->regmap);
  tl=get_reg(i_regs->regmap,rt1[i]);
  s=get_reg(i_regs->regmap,rs1[i]);
  temp=get_reg(i_regs->regmap,-1);
  temp2=get_reg(i_regs->regmap,FTEMP);
  addr=get_reg(i_regs->regmap,AGEN1+(i&1));
  assert(addr<0);
  offset=imm[i];
  reglist|=1<<temp;
  if(offset||s<0||c) addr=temp2;
  else addr=s;
  if(s>=0) {
    c=(i_regs->wasconst>>s)&1;
    if(c) {
      memtarget=((signed int)(constmap[i][s]+offset))<(signed int)0x80000000+RAM_SIZE;
    }
  }
  if(!c) {
    emit_shlimm(addr,3,temp);
    if (opcode[i]==0x22||opcode[i]==0x26) {
      emit_andimm(addr,0xFFFFFFFC,temp2); // LWL/LWR
    }else{
      emit_andimm(addr,0xFFFFFFF8,temp2); // LDL/LDR
    }
    jaddr=emit_fastpath_cmp_jump(i,temp2,&fastio_reg_override);
  }
  else {
    if(ram_offset&&memtarget) {
      host_tempreg_acquire();
      emit_addimm(temp2,ram_offset,HOST_TEMPREG);
      fastio_reg_override=HOST_TEMPREG;
    }
    if (opcode[i]==0x22||opcode[i]==0x26) {
      emit_movimm(((constmap[i][s]+offset)<<3)&24,temp); // LWL/LWR
    }else{
      emit_movimm(((constmap[i][s]+offset)<<3)&56,temp); // LDL/LDR
    }
  }
  if (opcode[i]==0x22||opcode[i]==0x26) { // LWL/LWR
    if(!c||memtarget) {
      int a=temp2;
      if(fastio_reg_override>=0) a=fastio_reg_override;
      emit_readword_indexed(0,a,temp2);
      if(fastio_reg_override==HOST_TEMPREG) host_tempreg_release();
      if(jaddr) add_stub_r(LOADW_STUB,jaddr,out,i,temp2,i_regs,ccadj[i],reglist);
    }
    else
      inline_readstub(LOADW_STUB,i,(constmap[i][s]+offset)&0xFFFFFFFC,i_regs->regmap,FTEMP,ccadj[i],reglist);
    if(rt1[i]) {
      assert(tl>=0);
      emit_andimm(temp,24,temp);
      if (opcode[i]==0x22) // LWL
        emit_xorimm(temp,24,temp);
      host_tempreg_acquire();
      emit_movimm(-1,HOST_TEMPREG);
      if (opcode[i]==0x26) {
        emit_shr(temp2,temp,temp2);
        emit_bic_lsr(tl,HOST_TEMPREG,temp,tl);
      }else{
        emit_shl(temp2,temp,temp2);
        emit_bic_lsl(tl,HOST_TEMPREG,temp,tl);
      }
      host_tempreg_release();
      emit_or(temp2,tl,tl);
    }
    //emit_storereg(rt1[i],tl); // DEBUG
  }
  if (opcode[i]==0x1A||opcode[i]==0x1B) { // LDL/LDR
    assert(0);
  }
}
#endif

void store_assemble(int i, const struct regstat *i_regs)
{
  int s,tl;
  int addr,temp;
  int offset;
  void *jaddr=0;
  enum stub_type type;
  int memtarget=0,c=0;
  int agr=AGEN1+(i&1);
  int fastio_reg_override=-1;
  u_int reglist=get_host_reglist(i_regs->regmap);
  tl=get_reg(i_regs->regmap,rs2[i]);
  s=get_reg(i_regs->regmap,rs1[i]);
  temp=get_reg(i_regs->regmap,agr);
  if(temp<0) temp=get_reg(i_regs->regmap,-1);
  offset=imm[i];
  if(s>=0) {
    c=(i_regs->wasconst>>s)&1;
    if(c) {
      memtarget=((signed int)(constmap[i][s]+offset))<(signed int)0x80000000+RAM_SIZE;
    }
  }
  assert(tl>=0);
  assert(temp>=0);
  if(i_regs->regmap[HOST_CCREG]==CCREG) reglist&=~(1<<HOST_CCREG);
  if(offset||s<0||c) addr=temp;
  else addr=s;
  if(!c) {
    jaddr=emit_fastpath_cmp_jump(i,addr,&fastio_reg_override);
  }
  else if(ram_offset&&memtarget) {
    host_tempreg_acquire();
    emit_addimm(addr,ram_offset,HOST_TEMPREG);
    fastio_reg_override=HOST_TEMPREG;
  }

  if (opcode[i]==0x28) { // SB
    if(!c||memtarget) {
      int x=0,a=temp;
      if(!c) a=addr;
      if(fastio_reg_override>=0) a=fastio_reg_override;
      emit_writebyte_indexed(tl,x,a);
    }
    type=STOREB_STUB;
  }
  if (opcode[i]==0x29) { // SH
    if(!c||memtarget) {
      int x=0,a=temp;
      if(!c) a=addr;
      if(fastio_reg_override>=0) a=fastio_reg_override;
      emit_writehword_indexed(tl,x,a);
    }
    type=STOREH_STUB;
  }
  if (opcode[i]==0x2B) { // SW
    if(!c||memtarget) {
      int a=addr;
      if(fastio_reg_override>=0) a=fastio_reg_override;
      emit_writeword_indexed(tl,0,a);
    }
    type=STOREW_STUB;
  }
  if (opcode[i]==0x3F) { // SD
    assert(0);
    type=STORED_STUB;
  }
  if(fastio_reg_override==HOST_TEMPREG)
    host_tempreg_release();
  if(jaddr) {
    // PCSX store handlers don't check invcode again
    reglist|=1<<addr;
    add_stub_r(type,jaddr,out,i,addr,i_regs,ccadj[i],reglist);
    jaddr=0;
  }
  if(!(i_regs->waswritten&(1<<rs1[i])) && !HACK_ENABLED(NDHACK_NO_SMC_CHECK)) {
    if(!c||memtarget) {
      #ifdef DESTRUCTIVE_SHIFT
      // The x86 shift operation is 'destructive'; it overwrites the
      // source register, so we need to make a copy first and use that.
      addr=temp;
      #endif
      #if defined(HOST_IMM8)
      int ir=get_reg(i_regs->regmap,INVCP);
      assert(ir>=0);
      emit_cmpmem_indexedsr12_reg(ir,addr,1);
      #else
      emit_cmpmem_indexedsr12_imm(invalid_code,addr,1);
      #endif
      #if defined(HAVE_CONDITIONAL_CALL) && !defined(DESTRUCTIVE_SHIFT)
      emit_callne(invalidate_addr_reg[addr]);
      #else
      void *jaddr2 = out;
      emit_jne(0);
      add_stub(INVCODE_STUB,jaddr2,out,reglist|(1<<HOST_CCREG),addr,0,0,0);
      #endif
    }
  }
  u_int addr_val=constmap[i][s]+offset;
  if(jaddr) {
    add_stub_r(type,jaddr,out,i,addr,i_regs,ccadj[i],reglist);
  } else if(c&&!memtarget) {
    inline_writestub(type,i,addr_val,i_regs->regmap,rs2[i],ccadj[i],reglist);
  }
  // basic current block modification detection..
  // not looking back as that should be in mips cache already
  // (see Spyro2 title->attract mode)
  if(c&&start+i*4<addr_val&&addr_val<start+slen*4) {
    SysPrintf("write to %08x hits block %08x, pc=%08x\n",addr_val,start,start+i*4);
    assert(i_regs->regmap==regs[i].regmap); // not delay slot
    if(i_regs->regmap==regs[i].regmap) {
      load_all_consts(regs[i].regmap_entry,regs[i].wasdirty,i);
      wb_dirtys(regs[i].regmap_entry,regs[i].wasdirty);
      emit_movimm(start+i*4+4,0);
      emit_writeword(0,&pcaddr);
      emit_addimm(HOST_CCREG,2,HOST_CCREG);
      emit_far_call(get_addr_ht);
      emit_jmpreg(0);
    }
  }
}

static void storelr_assemble(int i, const struct regstat *i_regs)
{
  int s,tl;
  int temp;
  int offset;
  void *jaddr=0;
  void *case1, *case2, *case3;
  void *done0, *done1, *done2;
  int memtarget=0,c=0;
  int agr=AGEN1+(i&1);
  u_int reglist=get_host_reglist(i_regs->regmap);
  tl=get_reg(i_regs->regmap,rs2[i]);
  s=get_reg(i_regs->regmap,rs1[i]);
  temp=get_reg(i_regs->regmap,agr);
  if(temp<0) temp=get_reg(i_regs->regmap,-1);
  offset=imm[i];
  if(s>=0) {
    c=(i_regs->isconst>>s)&1;
    if(c) {
      memtarget=((signed int)(constmap[i][s]+offset))<(signed int)0x80000000+RAM_SIZE;
    }
  }
  assert(tl>=0);
  assert(temp>=0);
  if(!c) {
    emit_cmpimm(s<0||offset?temp:s,RAM_SIZE);
    if(!offset&&s!=temp) emit_mov(s,temp);
    jaddr=out;
    emit_jno(0);
  }
  else
  {
    if(!memtarget||!rs1[i]) {
      jaddr=out;
      emit_jmp(0);
    }
  }
  if(ram_offset)
    emit_addimm_no_flags(ram_offset,temp);

  if (opcode[i]==0x2C||opcode[i]==0x2D) { // SDL/SDR
    assert(0);
  }

  emit_xorimm(temp,3,temp);
  emit_testimm(temp,2);
  case2=out;
  emit_jne(0);
  emit_testimm(temp,1);
  case1=out;
  emit_jne(0);
  // 0
  if (opcode[i]==0x2A) { // SWL
    emit_writeword_indexed(tl,0,temp);
  }
  else if (opcode[i]==0x2E) { // SWR
    emit_writebyte_indexed(tl,3,temp);
  }
  else
    assert(0);
  done0=out;
  emit_jmp(0);
  // 1
  set_jump_target(case1, out);
  if (opcode[i]==0x2A) { // SWL
    // Write 3 msb into three least significant bytes
    if(rs2[i]) emit_rorimm(tl,8,tl);
    emit_writehword_indexed(tl,-1,temp);
    if(rs2[i]) emit_rorimm(tl,16,tl);
    emit_writebyte_indexed(tl,1,temp);
    if(rs2[i]) emit_rorimm(tl,8,tl);
  }
  else if (opcode[i]==0x2E) { // SWR
    // Write two lsb into two most significant bytes
    emit_writehword_indexed(tl,1,temp);
  }
  done1=out;
  emit_jmp(0);
  // 2
  set_jump_target(case2, out);
  emit_testimm(temp,1);
  case3=out;
  emit_jne(0);
  if (opcode[i]==0x2A) { // SWL
    // Write two msb into two least significant bytes
    if(rs2[i]) emit_rorimm(tl,16,tl);
    emit_writehword_indexed(tl,-2,temp);
    if(rs2[i]) emit_rorimm(tl,16,tl);
  }
  else if (opcode[i]==0x2E) { // SWR
    // Write 3 lsb into three most significant bytes
    emit_writebyte_indexed(tl,-1,temp);
    if(rs2[i]) emit_rorimm(tl,8,tl);
    emit_writehword_indexed(tl,0,temp);
    if(rs2[i]) emit_rorimm(tl,24,tl);
  }
  done2=out;
  emit_jmp(0);
  // 3
  set_jump_target(case3, out);
  if (opcode[i]==0x2A) { // SWL
    // Write msb into least significant byte
    if(rs2[i]) emit_rorimm(tl,24,tl);
    emit_writebyte_indexed(tl,-3,temp);
    if(rs2[i]) emit_rorimm(tl,8,tl);
  }
  else if (opcode[i]==0x2E) { // SWR
    // Write entire word
    emit_writeword_indexed(tl,-3,temp);
  }
  set_jump_target(done0, out);
  set_jump_target(done1, out);
  set_jump_target(done2, out);
  if(!c||!memtarget)
    add_stub_r(STORELR_STUB,jaddr,out,i,temp,i_regs,ccadj[i],reglist);
  if(!(i_regs->waswritten&(1<<rs1[i])) && !HACK_ENABLED(NDHACK_NO_SMC_CHECK)) {
    emit_addimm_no_flags(-ram_offset,temp);
    #if defined(HOST_IMM8)
    int ir=get_reg(i_regs->regmap,INVCP);
    assert(ir>=0);
    emit_cmpmem_indexedsr12_reg(ir,temp,1);
    #else
    emit_cmpmem_indexedsr12_imm(invalid_code,temp,1);
    #endif
    #if defined(HAVE_CONDITIONAL_CALL) && !defined(DESTRUCTIVE_SHIFT)
    emit_callne(invalidate_addr_reg[temp]);
    #else
    void *jaddr2 = out;
    emit_jne(0);
    add_stub(INVCODE_STUB,jaddr2,out,reglist|(1<<HOST_CCREG),temp,0,0,0);
    #endif
  }
}

static void cop0_assemble(int i,struct regstat *i_regs)
{
  if(opcode2[i]==0) // MFC0
  {
    signed char t=get_reg(i_regs->regmap,rt1[i]);
    u_int copr=(source[i]>>11)&0x1f;
    //assert(t>=0); // Why does this happen?  OOT is weird
    if(t>=0&&rt1[i]!=0) {
      emit_readword(&reg_cop0[copr],t);
    }
  }
  else if(opcode2[i]==4) // MTC0
  {
    signed char s=get_reg(i_regs->regmap,rs1[i]);
    char copr=(source[i]>>11)&0x1f;
    assert(s>=0);
    wb_register(rs1[i],i_regs->regmap,i_regs->dirty);
    if(copr==9||copr==11||copr==12||copr==13) {
      emit_readword(&last_count,HOST_TEMPREG);
      emit_loadreg(CCREG,HOST_CCREG); // TODO: do proper reg alloc
      emit_add(HOST_CCREG,HOST_TEMPREG,HOST_CCREG);
      emit_addimm(HOST_CCREG,CLOCK_ADJUST(ccadj[i]),HOST_CCREG);
      emit_writeword(HOST_CCREG,&Count);
    }
    // What a mess.  The status register (12) can enable interrupts,
    // so needs a special case to handle a pending interrupt.
    // The interrupt must be taken immediately, because a subsequent
    // instruction might disable interrupts again.
    if(copr==12||copr==13) {
      if (is_delayslot) {
        // burn cycles to cause cc_interrupt, which will
        // reschedule next_interupt. Relies on CCREG from above.
        assem_debug("MTC0 DS %d\n", copr);
        emit_writeword(HOST_CCREG,&last_count);
        emit_movimm(0,HOST_CCREG);
        emit_storereg(CCREG,HOST_CCREG);
        emit_loadreg(rs1[i],1);
        emit_movimm(copr,0);
        emit_far_call(pcsx_mtc0_ds);
        emit_loadreg(rs1[i],s);
        return;
      }
      emit_movimm(start+i*4+4,HOST_TEMPREG);
      emit_writeword(HOST_TEMPREG,&pcaddr);
      emit_movimm(0,HOST_TEMPREG);
      emit_writeword(HOST_TEMPREG,&pending_exception);
    }
    if(s==HOST_CCREG)
      emit_loadreg(rs1[i],1);
    else if(s!=1)
      emit_mov(s,1);
    emit_movimm(copr,0);
    emit_far_call(pcsx_mtc0);
    if(copr==9||copr==11||copr==12||copr==13) {
      emit_readword(&Count,HOST_CCREG);
      emit_readword(&next_interupt,HOST_TEMPREG);
      emit_addimm(HOST_CCREG,-CLOCK_ADJUST(ccadj[i]),HOST_CCREG);
      emit_sub(HOST_CCREG,HOST_TEMPREG,HOST_CCREG);
      emit_writeword(HOST_TEMPREG,&last_count);
      emit_storereg(CCREG,HOST_CCREG);
    }
    if(copr==12||copr==13) {
      assert(!is_delayslot);
      emit_readword(&pending_exception,14);
      emit_test(14,14);
      void *jaddr = out;
      emit_jeq(0);
      emit_readword(&pcaddr, 0);
      emit_addimm(HOST_CCREG,2,HOST_CCREG);
      emit_far_call(get_addr_ht);
      emit_jmpreg(0);
      set_jump_target(jaddr, out);
    }
    emit_loadreg(rs1[i],s);
  }
  else
  {
    assert(opcode2[i]==0x10);
    //if((source[i]&0x3f)==0x10) // RFE
    {
      emit_readword(&Status,0);
      emit_andimm(0,0x3c,1);
      emit_andimm(0,~0xf,0);
      emit_orrshr_imm(1,2,0);
      emit_writeword(0,&Status);
    }
  }
}

static void cop1_unusable(int i,struct regstat *i_regs)
{
  // XXX: should just just do the exception instead
  //if(!cop1_usable)
  {
    void *jaddr=out;
    emit_jmp(0);
    add_stub_r(FP_STUB,jaddr,out,i,0,i_regs,is_delayslot,0);
  }
}

static void cop1_assemble(int i,struct regstat *i_regs)
{
  cop1_unusable(i, i_regs);
}

static void c1ls_assemble(int i,struct regstat *i_regs)
{
  cop1_unusable(i, i_regs);
}

// FP_STUB
static void do_cop1stub(int n)
{
  literal_pool(256);
  assem_debug("do_cop1stub %x\n",start+stubs[n].a*4);
  set_jump_target(stubs[n].addr, out);
  int i=stubs[n].a;
//  int rs=stubs[n].b;
  struct regstat *i_regs=(struct regstat *)stubs[n].c;
  int ds=stubs[n].d;
  if(!ds) {
    load_all_consts(regs[i].regmap_entry,regs[i].wasdirty,i);
    //if(i_regs!=&regs[i]) printf("oops: regs[i]=%x i_regs=%x",(int)&regs[i],(int)i_regs);
  }
  //else {printf("fp exception in delay slot\n");}
  wb_dirtys(i_regs->regmap_entry,i_regs->wasdirty);
  if(regs[i].regmap_entry[HOST_CCREG]!=CCREG) emit_loadreg(CCREG,HOST_CCREG);
  emit_movimm(start+(i-ds)*4,EAX); // Get PC
  emit_addimm(HOST_CCREG,CLOCK_ADJUST(ccadj[i]),HOST_CCREG); // CHECK: is this right?  There should probably be an extra cycle...
  emit_far_jump(ds?fp_exception_ds:fp_exception);
}

static int cop2_is_stalling_op(int i, int *cycles)
{
  if (opcode[i] == 0x3a) { // SWC2
    *cycles = 0;
    return 1;
  }
  if (itype[i] == COP2 && (opcode2[i] == 0 || opcode2[i] == 2)) { // MFC2/CFC2
    *cycles = 0;
    return 1;
  }
  if (itype[i] == C2OP) {
    *cycles = gte_cycletab[source[i] & 0x3f];
    return 1;
  }
  // ... what about MTC2/CTC2/LWC2?
  return 0;
}

#if 0
static void log_gte_stall(int stall, u_int cycle)
{
  if ((u_int)stall <= 44)
    printf("x    stall %2d %u\n", stall, cycle + last_count);
 if (cycle + last_count > 1215348544) exit(1);
}

static void emit_log_gte_stall(int i, int stall, u_int reglist)
{
  save_regs(reglist);
  if (stall > 0)
    emit_movimm(stall, 0);
  else
    emit_mov(HOST_TEMPREG, 0);
  emit_addimm(HOST_CCREG, CLOCK_ADJUST(ccadj[i]), 1);
  emit_far_call(log_gte_stall);
  restore_regs(reglist);
}
#endif

static void cop2_call_stall_check(u_int op, int i, const struct regstat *i_regs, u_int reglist)
{
  int j = i, other_gte_op_cycles = -1, stall = -MAXBLOCK, cycles_passed;
  int rtmp = reglist_find_free(reglist);

  if (HACK_ENABLED(NDHACK_GTE_NO_STALL))
    return;
  //assert(get_reg(i_regs->regmap, CCREG) == HOST_CCREG);
  if (get_reg(i_regs->regmap, CCREG) != HOST_CCREG) {
    // happens occasionally... cc evicted? Don't bother then
    //printf("no cc %08x\n", start + i*4);
    return;
  }
  if (!bt[i]) {
    for (j = i - 1; j >= 0; j--) {
      //if (is_ds[j]) break;
      if (cop2_is_stalling_op(j, &other_gte_op_cycles) || bt[j])
        break;
    }
  }
  cycles_passed = CLOCK_ADJUST(ccadj[i] - ccadj[j]);
  if (other_gte_op_cycles >= 0)
    stall = other_gte_op_cycles - cycles_passed;
  else if (cycles_passed >= 44)
    stall = 0; // can't stall
  if (stall == -MAXBLOCK && rtmp >= 0) {
    // unknown stall, do the expensive runtime check
    assem_debug("; cop2_call_stall_check\n");
#if 0 // too slow
    save_regs(reglist);
    emit_movimm(gte_cycletab[op], 0);
    emit_addimm(HOST_CCREG, CLOCK_ADJUST(ccadj[i]), 1);
    emit_far_call(call_gteStall);
    restore_regs(reglist);
#else
    host_tempreg_acquire();
    emit_readword(&psxRegs.gteBusyCycle, rtmp);
    emit_addimm(rtmp, -CLOCK_ADJUST(ccadj[i]), rtmp);
    emit_sub(rtmp, HOST_CCREG, HOST_TEMPREG);
    emit_cmpimm(HOST_TEMPREG, 44);
    emit_cmovb_reg(rtmp, HOST_CCREG);
    //emit_log_gte_stall(i, 0, reglist);
    host_tempreg_release();
#endif
  }
  else if (stall > 0) {
    //emit_log_gte_stall(i, stall, reglist);
    emit_addimm(HOST_CCREG, stall, HOST_CCREG);
  }

  // save gteBusyCycle, if needed
  if (gte_cycletab[op] == 0)
    return;
  other_gte_op_cycles = -1;
  for (j = i + 1; j < slen; j++) {
    if (cop2_is_stalling_op(j, &other_gte_op_cycles))
      break;
    if (is_jump(j)) {
      // check ds
      if (j + 1 < slen && cop2_is_stalling_op(j + 1, &other_gte_op_cycles))
        j++;
      break;
    }
  }
  if (other_gte_op_cycles >= 0)
    // will handle stall when assembling that op
    return;
  cycles_passed = CLOCK_ADJUST(ccadj[min(j, slen -1)] - ccadj[i]);
  if (cycles_passed >= 44)
    return;
  assem_debug("; save gteBusyCycle\n");
  host_tempreg_acquire();
#if 0
  emit_readword(&last_count, HOST_TEMPREG);
  emit_add(HOST_TEMPREG, HOST_CCREG, HOST_TEMPREG);
  emit_addimm(HOST_TEMPREG, CLOCK_ADJUST(ccadj[i]), HOST_TEMPREG);
  emit_addimm(HOST_TEMPREG, gte_cycletab[op]), HOST_TEMPREG);
  emit_writeword(HOST_TEMPREG, &psxRegs.gteBusyCycle);
#else
  emit_addimm(HOST_CCREG, CLOCK_ADJUST(ccadj[i]) + gte_cycletab[op], HOST_TEMPREG);
  emit_writeword(HOST_TEMPREG, &psxRegs.gteBusyCycle);
#endif
  host_tempreg_release();
}

static void cop2_get_dreg(u_int copr,signed char tl,signed char temp)
{
  switch (copr) {
    case 1:
    case 3:
    case 5:
    case 8:
    case 9:
    case 10:
    case 11:
      emit_readword(&reg_cop2d[copr],tl);
      emit_signextend16(tl,tl);
      emit_writeword(tl,&reg_cop2d[copr]); // hmh
      break;
    case 7:
    case 16:
    case 17:
    case 18:
    case 19:
      emit_readword(&reg_cop2d[copr],tl);
      emit_andimm(tl,0xffff,tl);
      emit_writeword(tl,&reg_cop2d[copr]);
      break;
    case 15:
      emit_readword(&reg_cop2d[14],tl); // SXY2
      emit_writeword(tl,&reg_cop2d[copr]);
      break;
    case 28:
    case 29:
      c2op_mfc2_29_assemble(tl,temp);
      break;
    default:
      emit_readword(&reg_cop2d[copr],tl);
      break;
  }
}

static void cop2_put_dreg(u_int copr,signed char sl,signed char temp)
{
  switch (copr) {
    case 15:
      emit_readword(&reg_cop2d[13],temp);  // SXY1
      emit_writeword(sl,&reg_cop2d[copr]);
      emit_writeword(temp,&reg_cop2d[12]); // SXY0
      emit_readword(&reg_cop2d[14],temp);  // SXY2
      emit_writeword(sl,&reg_cop2d[14]);
      emit_writeword(temp,&reg_cop2d[13]); // SXY1
      break;
    case 28:
      emit_andimm(sl,0x001f,temp);
      emit_shlimm(temp,7,temp);
      emit_writeword(temp,&reg_cop2d[9]);
      emit_andimm(sl,0x03e0,temp);
      emit_shlimm(temp,2,temp);
      emit_writeword(temp,&reg_cop2d[10]);
      emit_andimm(sl,0x7c00,temp);
      emit_shrimm(temp,3,temp);
      emit_writeword(temp,&reg_cop2d[11]);
      emit_writeword(sl,&reg_cop2d[28]);
      break;
    case 30:
      emit_xorsar_imm(sl,sl,31,temp);
#if defined(HAVE_ARMV5) || defined(__aarch64__)
      emit_clz(temp,temp);
#else
      emit_movs(temp,HOST_TEMPREG);
      emit_movimm(0,temp);
      emit_jeq((int)out+4*4);
      emit_addpl_imm(temp,1,temp);
      emit_lslpls_imm(HOST_TEMPREG,1,HOST_TEMPREG);
      emit_jns((int)out-2*4);
#endif
      emit_writeword(sl,&reg_cop2d[30]);
      emit_writeword(temp,&reg_cop2d[31]);
      break;
    case 31:
      break;
    default:
      emit_writeword(sl,&reg_cop2d[copr]);
      break;
  }
}

static void c2ls_assemble(int i, const struct regstat *i_regs)
{
  int s,tl;
  int ar;
  int offset;
  int memtarget=0,c=0;
  void *jaddr2=NULL;
  enum stub_type type;
  int agr=AGEN1+(i&1);
  int fastio_reg_override=-1;
  u_int reglist=get_host_reglist(i_regs->regmap);
  u_int copr=(source[i]>>16)&0x1f;
  s=get_reg(i_regs->regmap,rs1[i]);
  tl=get_reg(i_regs->regmap,FTEMP);
  offset=imm[i];
  assert(rs1[i]>0);
  assert(tl>=0);

  if(i_regs->regmap[HOST_CCREG]==CCREG)
    reglist&=~(1<<HOST_CCREG);

  // get the address
  if (opcode[i]==0x3a) { // SWC2
    ar=get_reg(i_regs->regmap,agr);
    if(ar<0) ar=get_reg(i_regs->regmap,-1);
    reglist|=1<<ar;
  } else { // LWC2
    ar=tl;
  }
  if(s>=0) c=(i_regs->wasconst>>s)&1;
  memtarget=c&&(((signed int)(constmap[i][s]+offset))<(signed int)0x80000000+RAM_SIZE);
  if (!offset&&!c&&s>=0) ar=s;
  assert(ar>=0);

  if (opcode[i]==0x3a) { // SWC2
    cop2_call_stall_check(0, i, i_regs, reglist_exclude(reglist, tl, -1));
    cop2_get_dreg(copr,tl,-1);
    type=STOREW_STUB;
  }
  else
    type=LOADW_STUB;

  if(c&&!memtarget) {
    jaddr2=out;
    emit_jmp(0); // inline_readstub/inline_writestub?
  }
  else {
    if(!c) {
      jaddr2=emit_fastpath_cmp_jump(i,ar,&fastio_reg_override);
    }
    else if(ram_offset&&memtarget) {
      host_tempreg_acquire();
      emit_addimm(ar,ram_offset,HOST_TEMPREG);
      fastio_reg_override=HOST_TEMPREG;
    }
    if (opcode[i]==0x32) { // LWC2
      int a=ar;
      if(fastio_reg_override>=0) a=fastio_reg_override;
      emit_readword_indexed(0,a,tl);
    }
    if (opcode[i]==0x3a) { // SWC2
      #ifdef DESTRUCTIVE_SHIFT
      if(!offset&&!c&&s>=0) emit_mov(s,ar);
      #endif
      int a=ar;
      if(fastio_reg_override>=0) a=fastio_reg_override;
      emit_writeword_indexed(tl,0,a);
    }
  }
  if(fastio_reg_override==HOST_TEMPREG)
    host_tempreg_release();
  if(jaddr2)
    add_stub_r(type,jaddr2,out,i,ar,i_regs,ccadj[i],reglist);
  if(opcode[i]==0x3a) // SWC2
  if(!(i_regs->waswritten&(1<<rs1[i])) && !HACK_ENABLED(NDHACK_NO_SMC_CHECK)) {
#if defined(HOST_IMM8)
    int ir=get_reg(i_regs->regmap,INVCP);
    assert(ir>=0);
    emit_cmpmem_indexedsr12_reg(ir,ar,1);
#else
    emit_cmpmem_indexedsr12_imm(invalid_code,ar,1);
#endif
    #if defined(HAVE_CONDITIONAL_CALL) && !defined(DESTRUCTIVE_SHIFT)
    emit_callne(invalidate_addr_reg[ar]);
    #else
    void *jaddr3 = out;
    emit_jne(0);
    add_stub(INVCODE_STUB,jaddr3,out,reglist|(1<<HOST_CCREG),ar,0,0,0);
    #endif
  }
  if (opcode[i]==0x32) { // LWC2
    host_tempreg_acquire();
    cop2_put_dreg(copr,tl,HOST_TEMPREG);
    host_tempreg_release();
  }
}

static void cop2_assemble(int i, const struct regstat *i_regs)
{
  u_int copr = (source[i]>>11) & 0x1f;
  signed char temp = get_reg(i_regs->regmap, -1);

  if (opcode2[i] == 0 || opcode2[i] == 2) { // MFC2/CFC2
    if (!HACK_ENABLED(NDHACK_GTE_NO_STALL)) {
      signed char tl = get_reg(i_regs->regmap, rt1[i]);
      u_int reglist = reglist_exclude(get_host_reglist(i_regs->regmap), tl, temp);
      cop2_call_stall_check(0, i, i_regs, reglist);
    }
  }
  if (opcode2[i]==0) { // MFC2
    signed char tl=get_reg(i_regs->regmap,rt1[i]);
    if(tl>=0&&rt1[i]!=0)
      cop2_get_dreg(copr,tl,temp);
  }
  else if (opcode2[i]==4) { // MTC2
    signed char sl=get_reg(i_regs->regmap,rs1[i]);
    cop2_put_dreg(copr,sl,temp);
  }
  else if (opcode2[i]==2) // CFC2
  {
    signed char tl=get_reg(i_regs->regmap,rt1[i]);
    if(tl>=0&&rt1[i]!=0)
      emit_readword(&reg_cop2c[copr],tl);
  }
  else if (opcode2[i]==6) // CTC2
  {
    signed char sl=get_reg(i_regs->regmap,rs1[i]);
    switch(copr) {
      case 4:
      case 12:
      case 20:
      case 26:
      case 27:
      case 29:
      case 30:
        emit_signextend16(sl,temp);
        break;
      case 31:
        c2op_ctc2_31_assemble(sl,temp);
        break;
      default:
        temp=sl;
        break;
    }
    emit_writeword(temp,&reg_cop2c[copr]);
    assert(sl>=0);
  }
}

static void do_unalignedwritestub(int n)
{
  assem_debug("do_unalignedwritestub %x\n",start+stubs[n].a*4);
  literal_pool(256);
  set_jump_target(stubs[n].addr, out);

  int i=stubs[n].a;
  struct regstat *i_regs=(struct regstat *)stubs[n].c;
  int addr=stubs[n].b;
  u_int reglist=stubs[n].e;
  signed char *i_regmap=i_regs->regmap;
  int temp2=get_reg(i_regmap,FTEMP);
  int rt;
  rt=get_reg(i_regmap,rs2[i]);
  assert(rt>=0);
  assert(addr>=0);
  assert(opcode[i]==0x2a||opcode[i]==0x2e); // SWL/SWR only implemented
  reglist|=(1<<addr);
  reglist&=~(1<<temp2);

#if 1
  // don't bother with it and call write handler
  save_regs(reglist);
  pass_args(addr,rt);
  int cc=get_reg(i_regmap,CCREG);
  if(cc<0)
    emit_loadreg(CCREG,2);
  emit_addimm(cc<0?2:cc,CLOCK_ADJUST((int)stubs[n].d+1),2);
  emit_far_call((opcode[i]==0x2a?jump_handle_swl:jump_handle_swr));
  emit_addimm(0,-CLOCK_ADJUST((int)stubs[n].d+1),cc<0?2:cc);
  if(cc<0)
    emit_storereg(CCREG,2);
  restore_regs(reglist);
  emit_jmp(stubs[n].retaddr); // return address
#else
  emit_andimm(addr,0xfffffffc,temp2);
  emit_writeword(temp2,&address);

  save_regs(reglist);
  emit_shrimm(addr,16,1);
  int cc=get_reg(i_regmap,CCREG);
  if(cc<0) {
    emit_loadreg(CCREG,2);
  }
  emit_movimm((u_int)readmem,0);
  emit_addimm(cc<0?2:cc,2*stubs[n].d+2,2);
  emit_call((int)&indirect_jump_indexed);
  restore_regs(reglist);

  emit_readword(&readmem_dword,temp2);
  int temp=addr; //hmh
  emit_shlimm(addr,3,temp);
  emit_andimm(temp,24,temp);
  if (opcode[i]==0x2a) // SWL
    emit_xorimm(temp,24,temp);
  emit_movimm(-1,HOST_TEMPREG);
  if (opcode[i]==0x2a) { // SWL
    emit_bic_lsr(temp2,HOST_TEMPREG,temp,temp2);
    emit_orrshr(rt,temp,temp2);
  }else{
    emit_bic_lsl(temp2,HOST_TEMPREG,temp,temp2);
    emit_orrshl(rt,temp,temp2);
  }
  emit_readword(&address,addr);
  emit_writeword(temp2,&word);
  //save_regs(reglist); // don't need to, no state changes
  emit_shrimm(addr,16,1);
  emit_movimm((u_int)writemem,0);
  //emit_call((int)&indirect_jump_indexed);
  emit_mov(15,14);
  emit_readword_dualindexedx4(0,1,15);
  emit_readword(&Count,HOST_TEMPREG);
  emit_readword(&next_interupt,2);
  emit_addimm(HOST_TEMPREG,-2*stubs[n].d-2,HOST_TEMPREG);
  emit_writeword(2,&last_count);
  emit_sub(HOST_TEMPREG,2,cc<0?HOST_TEMPREG:cc);
  if(cc<0) {
    emit_storereg(CCREG,HOST_TEMPREG);
  }
  restore_regs(reglist);
  emit_jmp(stubs[n].retaddr); // return address
#endif
}

#ifndef multdiv_assemble
void multdiv_assemble(int i,struct regstat *i_regs)
{
  printf("Need multdiv_assemble for this architecture.\n");
  abort();
}
#endif

static void mov_assemble(int i,struct regstat *i_regs)
{
  //if(opcode2[i]==0x10||opcode2[i]==0x12) { // MFHI/MFLO
  //if(opcode2[i]==0x11||opcode2[i]==0x13) { // MTHI/MTLO
  if(rt1[i]) {
    signed char sl,tl;
    tl=get_reg(i_regs->regmap,rt1[i]);
    //assert(tl>=0);
    if(tl>=0) {
      sl=get_reg(i_regs->regmap,rs1[i]);
      if(sl>=0) emit_mov(sl,tl);
      else emit_loadreg(rs1[i],tl);
    }
  }
}

// call interpreter, exception handler, things that change pc/regs/cycles ...
static void call_c_cpu_handler(int i, const struct regstat *i_regs, u_int pc, void *func)
{
  signed char ccreg=get_reg(i_regs->regmap,CCREG);
  assert(ccreg==HOST_CCREG);
  assert(!is_delayslot);
  (void)ccreg;

  emit_movimm(pc,3); // Get PC
  emit_readword(&last_count,2);
  emit_writeword(3,&psxRegs.pc);
  emit_addimm(HOST_CCREG,CLOCK_ADJUST(ccadj[i]),HOST_CCREG); // XXX
  emit_add(2,HOST_CCREG,2);
  emit_writeword(2,&psxRegs.cycle);
  emit_far_call(func);
  emit_far_jump(jump_to_new_pc);
}

static void syscall_assemble(int i,struct regstat *i_regs)
{
  emit_movimm(0x20,0); // cause code
  emit_movimm(0,1);    // not in delay slot
  call_c_cpu_handler(i,i_regs,start+i*4,psxException);
}

static void hlecall_assemble(int i,struct regstat *i_regs)
{
  void *hlefunc = psxNULL;
  uint32_t hleCode = source[i] & 0x03ffffff;
  if (hleCode < ARRAY_SIZE(psxHLEt))
    hlefunc = psxHLEt[hleCode];

  call_c_cpu_handler(i,i_regs,start+i*4+4,hlefunc);
}

static void intcall_assemble(int i,struct regstat *i_regs)
{
  call_c_cpu_handler(i,i_regs,start+i*4,execI);
}

static void speculate_mov(int rs,int rt)
{
  if(rt!=0) {
    smrv_strong_next|=1<<rt;
    smrv[rt]=smrv[rs];
  }
}

static void speculate_mov_weak(int rs,int rt)
{
  if(rt!=0) {
    smrv_weak_next|=1<<rt;
    smrv[rt]=smrv[rs];
  }
}

static void speculate_register_values(int i)
{
  if(i==0) {
    memcpy(smrv,psxRegs.GPR.r,sizeof(smrv));
    // gp,sp are likely to stay the same throughout the block
    smrv_strong_next=(1<<28)|(1<<29)|(1<<30);
    smrv_weak_next=~smrv_strong_next;
    //printf(" llr %08x\n", smrv[4]);
  }
  smrv_strong=smrv_strong_next;
  smrv_weak=smrv_weak_next;
  switch(itype[i]) {
    case ALU:
      if     ((smrv_strong>>rs1[i])&1) speculate_mov(rs1[i],rt1[i]);
      else if((smrv_strong>>rs2[i])&1) speculate_mov(rs2[i],rt1[i]);
      else if((smrv_weak>>rs1[i])&1) speculate_mov_weak(rs1[i],rt1[i]);
      else if((smrv_weak>>rs2[i])&1) speculate_mov_weak(rs2[i],rt1[i]);
      else {
        smrv_strong_next&=~(1<<rt1[i]);
        smrv_weak_next&=~(1<<rt1[i]);
      }
      break;
    case SHIFTIMM:
      smrv_strong_next&=~(1<<rt1[i]);
      smrv_weak_next&=~(1<<rt1[i]);
      // fallthrough
    case IMM16:
      if(rt1[i]&&is_const(&regs[i],rt1[i])) {
        int value,hr=get_reg(regs[i].regmap,rt1[i]);
        if(hr>=0) {
          if(get_final_value(hr,i,&value))
               smrv[rt1[i]]=value;
          else smrv[rt1[i]]=constmap[i][hr];
          smrv_strong_next|=1<<rt1[i];
        }
      }
      else {
        if     ((smrv_strong>>rs1[i])&1) speculate_mov(rs1[i],rt1[i]);
        else if((smrv_weak>>rs1[i])&1) speculate_mov_weak(rs1[i],rt1[i]);
      }
      break;
    case LOAD:
      if(start<0x2000&&(rt1[i]==26||(smrv[rt1[i]]>>24)==0xa0)) {
        // special case for BIOS
        smrv[rt1[i]]=0xa0000000;
        smrv_strong_next|=1<<rt1[i];
        break;
      }
      // fallthrough
    case SHIFT:
    case LOADLR:
    case MOV:
      smrv_strong_next&=~(1<<rt1[i]);
      smrv_weak_next&=~(1<<rt1[i]);
      break;
    case COP0:
    case COP2:
      if(opcode2[i]==0||opcode2[i]==2) { // MFC/CFC
        smrv_strong_next&=~(1<<rt1[i]);
        smrv_weak_next&=~(1<<rt1[i]);
      }
      break;
    case C2LS:
      if (opcode[i]==0x32) { // LWC2
        smrv_strong_next&=~(1<<rt1[i]);
        smrv_weak_next&=~(1<<rt1[i]);
      }
      break;
  }
#if 0
  int r=4;
  printf("x %08x %08x %d %d c %08x %08x\n",smrv[r],start+i*4,
    ((smrv_strong>>r)&1),(smrv_weak>>r)&1,regs[i].isconst,regs[i].wasconst);
#endif
}

static void ds_assemble(int i,struct regstat *i_regs)
{
  speculate_register_values(i);
  is_delayslot=1;
  switch(itype[i]) {
    case ALU:
      alu_assemble(i,i_regs);break;
    case IMM16:
      imm16_assemble(i,i_regs);break;
    case SHIFT:
      shift_assemble(i,i_regs);break;
    case SHIFTIMM:
      shiftimm_assemble(i,i_regs);break;
    case LOAD:
      load_assemble(i,i_regs);break;
    case LOADLR:
      loadlr_assemble(i,i_regs);break;
    case STORE:
      store_assemble(i,i_regs);break;
    case STORELR:
      storelr_assemble(i,i_regs);break;
    case COP0:
      cop0_assemble(i,i_regs);break;
    case COP1:
      cop1_assemble(i,i_regs);break;
    case C1LS:
      c1ls_assemble(i,i_regs);break;
    case COP2:
      cop2_assemble(i,i_regs);break;
    case C2LS:
      c2ls_assemble(i,i_regs);break;
    case C2OP:
      c2op_assemble(i,i_regs);break;
    case MULTDIV:
      multdiv_assemble(i,i_regs);break;
    case MOV:
      mov_assemble(i,i_regs);break;
    case SYSCALL:
    case HLECALL:
    case INTCALL:
    case SPAN:
    case UJUMP:
    case RJUMP:
    case CJUMP:
    case SJUMP:
      SysPrintf("Jump in the delay slot.  This is probably a bug.\n");
  }
  is_delayslot=0;
}

// Is the branch target a valid internal jump?
static int internal_branch(int addr)
{
  if(addr&1) return 0; // Indirect (register) jump
  if(addr>=start && addr<start+slen*4-4)
  {
    return 1;
  }
  return 0;
}

static void wb_invalidate(signed char pre[],signed char entry[],uint64_t dirty,uint64_t u)
{
  int hr;
  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG) {
      if(pre[hr]!=entry[hr]) {
        if(pre[hr]>=0) {
          if((dirty>>hr)&1) {
            if(get_reg(entry,pre[hr])<0) {
              assert(pre[hr]<64);
              if(!((u>>pre[hr])&1))
                emit_storereg(pre[hr],hr);
            }
          }
        }
      }
    }
  }
  // Move from one register to another (no writeback)
  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG) {
      if(pre[hr]!=entry[hr]) {
        if(pre[hr]>=0&&(pre[hr]&63)<TEMPREG) {
          int nr;
          if((nr=get_reg(entry,pre[hr]))>=0) {
            emit_mov(hr,nr);
          }
        }
      }
    }
  }
}

// Load the specified registers
// This only loads the registers given as arguments because
// we don't want to load things that will be overwritten
static void load_regs(signed char entry[],signed char regmap[],int rs1,int rs2)
{
  int hr;
  // Load 32-bit regs
  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG&&regmap[hr]>=0) {
      if(entry[hr]!=regmap[hr]) {
        if(regmap[hr]==rs1||regmap[hr]==rs2)
        {
          if(regmap[hr]==0) {
            emit_zeroreg(hr);
          }
          else
          {
            emit_loadreg(regmap[hr],hr);
          }
        }
      }
    }
  }
}

// Load registers prior to the start of a loop
// so that they are not loaded within the loop
static void loop_preload(signed char pre[],signed char entry[])
{
  int hr;
  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG) {
      if(pre[hr]!=entry[hr]) {
        if(entry[hr]>=0) {
          if(get_reg(pre,entry[hr])<0) {
            assem_debug("loop preload:\n");
            //printf("loop preload: %d\n",hr);
            if(entry[hr]==0) {
              emit_zeroreg(hr);
            }
            else if(entry[hr]<TEMPREG)
            {
              emit_loadreg(entry[hr],hr);
            }
            else if(entry[hr]-64<TEMPREG)
            {
              emit_loadreg(entry[hr],hr);
            }
          }
        }
      }
    }
  }
}

// Generate address for load/store instruction
// goes to AGEN for writes, FTEMP for LOADLR and cop1/2 loads
void address_generation(int i,struct regstat *i_regs,signed char entry[])
{
  if(itype[i]==LOAD||itype[i]==LOADLR||itype[i]==STORE||itype[i]==STORELR||itype[i]==C1LS||itype[i]==C2LS) {
    int ra=-1;
    int agr=AGEN1+(i&1);
    if(itype[i]==LOAD) {
      ra=get_reg(i_regs->regmap,rt1[i]);
      if(ra<0) ra=get_reg(i_regs->regmap,-1);
      assert(ra>=0);
    }
    if(itype[i]==LOADLR) {
      ra=get_reg(i_regs->regmap,FTEMP);
    }
    if(itype[i]==STORE||itype[i]==STORELR) {
      ra=get_reg(i_regs->regmap,agr);
      if(ra<0) ra=get_reg(i_regs->regmap,-1);
    }
    if(itype[i]==C1LS||itype[i]==C2LS) {
      if ((opcode[i]&0x3b)==0x31||(opcode[i]&0x3b)==0x32) // LWC1/LDC1/LWC2/LDC2
        ra=get_reg(i_regs->regmap,FTEMP);
      else { // SWC1/SDC1/SWC2/SDC2
        ra=get_reg(i_regs->regmap,agr);
        if(ra<0) ra=get_reg(i_regs->regmap,-1);
      }
    }
    int rs=get_reg(i_regs->regmap,rs1[i]);
    if(ra>=0) {
      int offset=imm[i];
      int c=(i_regs->wasconst>>rs)&1;
      if(rs1[i]==0) {
        // Using r0 as a base address
        if(!entry||entry[ra]!=agr) {
          if (opcode[i]==0x22||opcode[i]==0x26) {
            emit_movimm(offset&0xFFFFFFFC,ra); // LWL/LWR
          }else if (opcode[i]==0x1a||opcode[i]==0x1b) {
            emit_movimm(offset&0xFFFFFFF8,ra); // LDL/LDR
          }else{
            emit_movimm(offset,ra);
          }
        } // else did it in the previous cycle
      }
      else if(rs<0) {
        if(!entry||entry[ra]!=rs1[i])
          emit_loadreg(rs1[i],ra);
        //if(!entry||entry[ra]!=rs1[i])
        //  printf("poor load scheduling!\n");
      }
      else if(c) {
        if(rs1[i]!=rt1[i]||itype[i]!=LOAD) {
          if(!entry||entry[ra]!=agr) {
            if (opcode[i]==0x22||opcode[i]==0x26) {
              emit_movimm((constmap[i][rs]+offset)&0xFFFFFFFC,ra); // LWL/LWR
            }else if (opcode[i]==0x1a||opcode[i]==0x1b) {
              emit_movimm((constmap[i][rs]+offset)&0xFFFFFFF8,ra); // LDL/LDR
            }else{
              emit_movimm(constmap[i][rs]+offset,ra);
              regs[i].loadedconst|=1<<ra;
            }
          } // else did it in the previous cycle
        } // else load_consts already did it
      }
      if(offset&&!c&&rs1[i]) {
        if(rs>=0) {
          emit_addimm(rs,offset,ra);
        }else{
          emit_addimm(ra,offset,ra);
        }
      }
    }
  }
  // Preload constants for next instruction
  if(itype[i+1]==LOAD||itype[i+1]==LOADLR||itype[i+1]==STORE||itype[i+1]==STORELR||itype[i+1]==C1LS||itype[i+1]==C2LS) {
    int agr,ra;
    // Actual address
    agr=AGEN1+((i+1)&1);
    ra=get_reg(i_regs->regmap,agr);
    if(ra>=0) {
      int rs=get_reg(regs[i+1].regmap,rs1[i+1]);
      int offset=imm[i+1];
      int c=(regs[i+1].wasconst>>rs)&1;
      if(c&&(rs1[i+1]!=rt1[i+1]||itype[i+1]!=LOAD)) {
        if (opcode[i+1]==0x22||opcode[i+1]==0x26) {
          emit_movimm((constmap[i+1][rs]+offset)&0xFFFFFFFC,ra); // LWL/LWR
        }else if (opcode[i+1]==0x1a||opcode[i+1]==0x1b) {
          emit_movimm((constmap[i+1][rs]+offset)&0xFFFFFFF8,ra); // LDL/LDR
        }else{
          emit_movimm(constmap[i+1][rs]+offset,ra);
          regs[i+1].loadedconst|=1<<ra;
        }
      }
      else if(rs1[i+1]==0) {
        // Using r0 as a base address
        if (opcode[i+1]==0x22||opcode[i+1]==0x26) {
          emit_movimm(offset&0xFFFFFFFC,ra); // LWL/LWR
        }else if (opcode[i+1]==0x1a||opcode[i+1]==0x1b) {
          emit_movimm(offset&0xFFFFFFF8,ra); // LDL/LDR
        }else{
          emit_movimm(offset,ra);
        }
      }
    }
  }
}

static int get_final_value(int hr, int i, int *value)
{
  int reg=regs[i].regmap[hr];
  while(i<slen-1) {
    if(regs[i+1].regmap[hr]!=reg) break;
    if(!((regs[i+1].isconst>>hr)&1)) break;
    if(bt[i+1]) break;
    i++;
  }
  if(i<slen-1) {
    if(itype[i]==UJUMP||itype[i]==RJUMP||itype[i]==CJUMP||itype[i]==SJUMP) {
      *value=constmap[i][hr];
      return 1;
    }
    if(!bt[i+1]) {
      if(itype[i+1]==UJUMP||itype[i+1]==RJUMP||itype[i+1]==CJUMP||itype[i+1]==SJUMP) {
        // Load in delay slot, out-of-order execution
        if(itype[i+2]==LOAD&&rs1[i+2]==reg&&rt1[i+2]==reg&&((regs[i+1].wasconst>>hr)&1))
        {
          // Precompute load address
          *value=constmap[i][hr]+imm[i+2];
          return 1;
        }
      }
      if(itype[i+1]==LOAD&&rs1[i+1]==reg&&rt1[i+1]==reg)
      {
        // Precompute load address
        *value=constmap[i][hr]+imm[i+1];
        //printf("c=%x imm=%lx\n",(long)constmap[i][hr],imm[i+1]);
        return 1;
      }
    }
  }
  *value=constmap[i][hr];
  //printf("c=%lx\n",(long)constmap[i][hr]);
  if(i==slen-1) return 1;
  assert(reg < 64);
  return !((unneeded_reg[i+1]>>reg)&1);
}

// Load registers with known constants
static void load_consts(signed char pre[],signed char regmap[],int i)
{
  int hr,hr2;
  // propagate loaded constant flags
  if(i==0||bt[i])
    regs[i].loadedconst=0;
  else {
    for(hr=0;hr<HOST_REGS;hr++) {
      if(hr!=EXCLUDE_REG&&regmap[hr]>=0&&((regs[i-1].isconst>>hr)&1)&&pre[hr]==regmap[hr]
         &&regmap[hr]==regs[i-1].regmap[hr]&&((regs[i-1].loadedconst>>hr)&1))
      {
        regs[i].loadedconst|=1<<hr;
      }
    }
  }
  // Load 32-bit regs
  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG&&regmap[hr]>=0) {
      //if(entry[hr]!=regmap[hr]) {
      if(!((regs[i].loadedconst>>hr)&1)) {
        assert(regmap[hr]<64);
        if(((regs[i].isconst>>hr)&1)&&regmap[hr]>0) {
          int value,similar=0;
          if(get_final_value(hr,i,&value)) {
            // see if some other register has similar value
            for(hr2=0;hr2<HOST_REGS;hr2++) {
              if(hr2!=EXCLUDE_REG&&((regs[i].loadedconst>>hr2)&1)) {
                if(is_similar_value(value,constmap[i][hr2])) {
                  similar=1;
                  break;
                }
              }
            }
            if(similar) {
              int value2;
              if(get_final_value(hr2,i,&value2)) // is this needed?
                emit_movimm_from(value2,hr2,value,hr);
              else
                emit_movimm(value,hr);
            }
            else if(value==0) {
              emit_zeroreg(hr);
            }
            else {
              emit_movimm(value,hr);
            }
          }
          regs[i].loadedconst|=1<<hr;
        }
      }
    }
  }
}

void load_all_consts(signed char regmap[], u_int dirty, int i)
{
  int hr;
  // Load 32-bit regs
  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG&&regmap[hr]>=0&&((dirty>>hr)&1)) {
      assert(regmap[hr] < 64);
      if(((regs[i].isconst>>hr)&1)&&regmap[hr]>0) {
        int value=constmap[i][hr];
        if(value==0) {
          emit_zeroreg(hr);
        }
        else {
          emit_movimm(value,hr);
        }
      }
    }
  }
}

// Write out all dirty registers (except cycle count)
static void wb_dirtys(signed char i_regmap[],uint64_t i_dirty)
{
  int hr;
  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG) {
      if(i_regmap[hr]>0) {
        if(i_regmap[hr]!=CCREG) {
          if((i_dirty>>hr)&1) {
            assert(i_regmap[hr]<64);
            emit_storereg(i_regmap[hr],hr);
          }
        }
      }
    }
  }
}

// Write out dirty registers that we need to reload (pair with load_needed_regs)
// This writes the registers not written by store_regs_bt
void wb_needed_dirtys(signed char i_regmap[],uint64_t i_dirty,int addr)
{
  int hr;
  int t=(addr-start)>>2;
  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG) {
      if(i_regmap[hr]>0) {
        if(i_regmap[hr]!=CCREG) {
          if(i_regmap[hr]==regs[t].regmap_entry[hr] && ((regs[t].dirty>>hr)&1)) {
            if((i_dirty>>hr)&1) {
              assert(i_regmap[hr]<64);
              emit_storereg(i_regmap[hr],hr);
            }
          }
        }
      }
    }
  }
}

// Load all registers (except cycle count)
void load_all_regs(signed char i_regmap[])
{
  int hr;
  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG) {
      if(i_regmap[hr]==0) {
        emit_zeroreg(hr);
      }
      else
      if(i_regmap[hr]>0 && (i_regmap[hr]&63)<TEMPREG && i_regmap[hr]!=CCREG)
      {
        emit_loadreg(i_regmap[hr],hr);
      }
    }
  }
}

// Load all current registers also needed by next instruction
void load_needed_regs(signed char i_regmap[],signed char next_regmap[])
{
  int hr;
  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG) {
      if(get_reg(next_regmap,i_regmap[hr])>=0) {
        if(i_regmap[hr]==0) {
          emit_zeroreg(hr);
        }
        else
        if(i_regmap[hr]>0 && (i_regmap[hr]&63)<TEMPREG && i_regmap[hr]!=CCREG)
        {
          emit_loadreg(i_regmap[hr],hr);
        }
      }
    }
  }
}

// Load all regs, storing cycle count if necessary
void load_regs_entry(int t)
{
  int hr;
  if(is_ds[t]) emit_addimm(HOST_CCREG,CLOCK_ADJUST(1),HOST_CCREG);
  else if(ccadj[t]) emit_addimm(HOST_CCREG,-CLOCK_ADJUST(ccadj[t]),HOST_CCREG);
  if(regs[t].regmap_entry[HOST_CCREG]!=CCREG) {
    emit_storereg(CCREG,HOST_CCREG);
  }
  // Load 32-bit regs
  for(hr=0;hr<HOST_REGS;hr++) {
    if(regs[t].regmap_entry[hr]>=0&&regs[t].regmap_entry[hr]<TEMPREG) {
      if(regs[t].regmap_entry[hr]==0) {
        emit_zeroreg(hr);
      }
      else if(regs[t].regmap_entry[hr]!=CCREG)
      {
        emit_loadreg(regs[t].regmap_entry[hr],hr);
      }
    }
  }
}

// Store dirty registers prior to branch
void store_regs_bt(signed char i_regmap[],uint64_t i_dirty,int addr)
{
  if(internal_branch(addr))
  {
    int t=(addr-start)>>2;
    int hr;
    for(hr=0;hr<HOST_REGS;hr++) {
      if(hr!=EXCLUDE_REG) {
        if(i_regmap[hr]>0 && i_regmap[hr]!=CCREG) {
          if(i_regmap[hr]!=regs[t].regmap_entry[hr] || !((regs[t].dirty>>hr)&1)) {
            if((i_dirty>>hr)&1) {
              assert(i_regmap[hr]<64);
              if(!((unneeded_reg[t]>>i_regmap[hr])&1))
                emit_storereg(i_regmap[hr],hr);
            }
          }
        }
      }
    }
  }
  else
  {
    // Branch out of this block, write out all dirty regs
    wb_dirtys(i_regmap,i_dirty);
  }
}

// Load all needed registers for branch target
static void load_regs_bt(signed char i_regmap[],uint64_t i_dirty,int addr)
{
  //if(addr>=start && addr<(start+slen*4))
  if(internal_branch(addr))
  {
    int t=(addr-start)>>2;
    int hr;
    // Store the cycle count before loading something else
    if(i_regmap[HOST_CCREG]!=CCREG) {
      assert(i_regmap[HOST_CCREG]==-1);
    }
    if(regs[t].regmap_entry[HOST_CCREG]!=CCREG) {
      emit_storereg(CCREG,HOST_CCREG);
    }
    // Load 32-bit regs
    for(hr=0;hr<HOST_REGS;hr++) {
      if(hr!=EXCLUDE_REG&&regs[t].regmap_entry[hr]>=0&&regs[t].regmap_entry[hr]<TEMPREG) {
        if(i_regmap[hr]!=regs[t].regmap_entry[hr]) {
          if(regs[t].regmap_entry[hr]==0) {
            emit_zeroreg(hr);
          }
          else if(regs[t].regmap_entry[hr]!=CCREG)
          {
            emit_loadreg(regs[t].regmap_entry[hr],hr);
          }
        }
      }
    }
  }
}

static int match_bt(signed char i_regmap[],uint64_t i_dirty,int addr)
{
  if(addr>=start && addr<start+slen*4-4)
  {
    int t=(addr-start)>>2;
    int hr;
    if(regs[t].regmap_entry[HOST_CCREG]!=CCREG) return 0;
    for(hr=0;hr<HOST_REGS;hr++)
    {
      if(hr!=EXCLUDE_REG)
      {
        if(i_regmap[hr]!=regs[t].regmap_entry[hr])
        {
          if(regs[t].regmap_entry[hr]>=0&&(regs[t].regmap_entry[hr]|64)<TEMPREG+64)
          {
            return 0;
          }
          else
          if((i_dirty>>hr)&1)
          {
            if(i_regmap[hr]<TEMPREG)
            {
              if(!((unneeded_reg[t]>>i_regmap[hr])&1))
                return 0;
            }
            else if(i_regmap[hr]>=64&&i_regmap[hr]<TEMPREG+64)
            {
              assert(0);
            }
          }
        }
        else // Same register but is it 32-bit or dirty?
        if(i_regmap[hr]>=0)
        {
          if(!((regs[t].dirty>>hr)&1))
          {
            if((i_dirty>>hr)&1)
            {
              if(!((unneeded_reg[t]>>i_regmap[hr])&1))
              {
                //printf("%x: dirty no match\n",addr);
                return 0;
              }
            }
          }
        }
      }
    }
    // Delay slots are not valid branch targets
    //if(t>0&&(itype[t-1]==RJUMP||itype[t-1]==UJUMP||itype[t-1]==CJUMP||itype[t-1]==SJUMP)) return 0;
    // Delay slots require additional processing, so do not match
    if(is_ds[t]) return 0;
  }
  else
  {
    int hr;
    for(hr=0;hr<HOST_REGS;hr++)
    {
      if(hr!=EXCLUDE_REG)
      {
        if(i_regmap[hr]>=0)
        {
          if(hr!=HOST_CCREG||i_regmap[hr]!=CCREG)
          {
            if((i_dirty>>hr)&1)
            {
              return 0;
            }
          }
        }
      }
    }
  }
  return 1;
}

#ifdef DRC_DBG
static void drc_dbg_emit_do_cmp(int i)
{
  extern void do_insn_cmp();
  //extern int cycle;
  u_int hr, reglist = get_host_reglist(regs[i].regmap);

  assem_debug("//do_insn_cmp %08x\n", start+i*4);
  save_regs(reglist);
  // write out changed consts to match the interpreter
  if (i > 0 && !bt[i]) {
    for (hr = 0; hr < HOST_REGS; hr++) {
      int reg = regs[i-1].regmap[hr];
      if (hr == EXCLUDE_REG || reg < 0)
        continue;
      if (!((regs[i-1].isconst >> hr) & 1))
        continue;
      if (i > 1 && reg == regs[i-2].regmap[hr] && constmap[i-1][hr] == constmap[i-2][hr])
        continue;
      emit_movimm(constmap[i-1][hr],0);
      emit_storereg(reg, 0);
    }
  }
  emit_movimm(start+i*4,0);
  emit_writeword(0,&pcaddr);
  emit_far_call(do_insn_cmp);
  //emit_readword(&cycle,0);
  //emit_addimm(0,2,0);
  //emit_writeword(0,&cycle);
  (void)get_reg2;
  restore_regs(reglist);
  assem_debug("\\\\do_insn_cmp\n");
}
#else
#define drc_dbg_emit_do_cmp(x)
#endif

// Used when a branch jumps into the delay slot of another branch
static void ds_assemble_entry(int i)
{
  int t=(ba[i]-start)>>2;
  if (!instr_addr[t])
    instr_addr[t] = out;
  assem_debug("Assemble delay slot at %x\n",ba[i]);
  assem_debug("<->\n");
  drc_dbg_emit_do_cmp(t);
  if(regs[t].regmap_entry[HOST_CCREG]==CCREG&&regs[t].regmap[HOST_CCREG]!=CCREG)
    wb_register(CCREG,regs[t].regmap_entry,regs[t].wasdirty);
  load_regs(regs[t].regmap_entry,regs[t].regmap,rs1[t],rs2[t]);
  address_generation(t,&regs[t],regs[t].regmap_entry);
  if(itype[t]==STORE||itype[t]==STORELR||(opcode[t]&0x3b)==0x39||(opcode[t]&0x3b)==0x3a)
    load_regs(regs[t].regmap_entry,regs[t].regmap,INVCP,INVCP);
  is_delayslot=0;
  switch(itype[t]) {
    case ALU:
      alu_assemble(t,&regs[t]);break;
    case IMM16:
      imm16_assemble(t,&regs[t]);break;
    case SHIFT:
      shift_assemble(t,&regs[t]);break;
    case SHIFTIMM:
      shiftimm_assemble(t,&regs[t]);break;
    case LOAD:
      load_assemble(t,&regs[t]);break;
    case LOADLR:
      loadlr_assemble(t,&regs[t]);break;
    case STORE:
      store_assemble(t,&regs[t]);break;
    case STORELR:
      storelr_assemble(t,&regs[t]);break;
    case COP0:
      cop0_assemble(t,&regs[t]);break;
    case COP1:
      cop1_assemble(t,&regs[t]);break;
    case C1LS:
      c1ls_assemble(t,&regs[t]);break;
    case COP2:
      cop2_assemble(t,&regs[t]);break;
    case C2LS:
      c2ls_assemble(t,&regs[t]);break;
    case C2OP:
      c2op_assemble(t,&regs[t]);break;
    case MULTDIV:
      multdiv_assemble(t,&regs[t]);break;
    case MOV:
      mov_assemble(t,&regs[t]);break;
    case SYSCALL:
    case HLECALL:
    case INTCALL:
    case SPAN:
    case UJUMP:
    case RJUMP:
    case CJUMP:
    case SJUMP:
      SysPrintf("Jump in the delay slot.  This is probably a bug.\n");
  }
  store_regs_bt(regs[t].regmap,regs[t].dirty,ba[i]+4);
  load_regs_bt(regs[t].regmap,regs[t].dirty,ba[i]+4);
  if(internal_branch(ba[i]+4))
    assem_debug("branch: internal\n");
  else
    assem_debug("branch: external\n");
  assert(internal_branch(ba[i]+4));
  add_to_linker(out,ba[i]+4,internal_branch(ba[i]+4));
  emit_jmp(0);
}

static void emit_extjump(void *addr, u_int target)
{
  emit_extjump2(addr, target, dyna_linker);
}

static void emit_extjump_ds(void *addr, u_int target)
{
  emit_extjump2(addr, target, dyna_linker_ds);
}

// Load 2 immediates optimizing for small code size
static void emit_mov2imm_compact(int imm1,u_int rt1,int imm2,u_int rt2)
{
  emit_movimm(imm1,rt1);
  emit_movimm_from(imm1,rt1,imm2,rt2);
}

void do_cc(int i,signed char i_regmap[],int *adj,int addr,int taken,int invert)
{
  int count;
  void *jaddr;
  void *idle=NULL;
  int t=0;
  if(itype[i]==RJUMP)
  {
    *adj=0;
  }
  //if(ba[i]>=start && ba[i]<(start+slen*4))
  if(internal_branch(ba[i]))
  {
    t=(ba[i]-start)>>2;
    if(is_ds[t]) *adj=-1; // Branch into delay slot adds an extra cycle
    else *adj=ccadj[t];
  }
  else
  {
    *adj=0;
  }
  count=ccadj[i];
  if(taken==TAKEN && i==(ba[i]-start)>>2 && source[i+1]==0) {
    // Idle loop
    if(count&1) emit_addimm_and_set_flags(2*(count+2),HOST_CCREG);
    idle=out;
    //emit_subfrommem(&idlecount,HOST_CCREG); // Count idle cycles
    emit_andimm(HOST_CCREG,3,HOST_CCREG);
    jaddr=out;
    emit_jmp(0);
  }
  else if(*adj==0||invert) {
    int cycles=CLOCK_ADJUST(count+2);
    // faster loop HACK
#if 0
    if (t&&*adj) {
      int rel=t-i;
      if(-NO_CYCLE_PENALTY_THR<rel&&rel<0)
        cycles=CLOCK_ADJUST(*adj)+count+2-*adj;
    }
#endif
    emit_addimm_and_set_flags(cycles,HOST_CCREG);
    jaddr=out;
    emit_jns(0);
  }
  else
  {
    emit_cmpimm(HOST_CCREG,-CLOCK_ADJUST(count+2));
    jaddr=out;
    emit_jns(0);
  }
  add_stub(CC_STUB,jaddr,idle?idle:out,(*adj==0||invert||idle)?0:(count+2),i,addr,taken,0);
}

static void do_ccstub(int n)
{
  literal_pool(256);
  assem_debug("do_ccstub %x\n",start+(u_int)stubs[n].b*4);
  set_jump_target(stubs[n].addr, out);
  int i=stubs[n].b;
  if(stubs[n].d==NULLDS) {
    // Delay slot instruction is nullified ("likely" branch)
    wb_dirtys(regs[i].regmap,regs[i].dirty);
  }
  else if(stubs[n].d!=TAKEN) {
    wb_dirtys(branch_regs[i].regmap,branch_regs[i].dirty);
  }
  else {
    if(internal_branch(ba[i]))
      wb_needed_dirtys(branch_regs[i].regmap,branch_regs[i].dirty,ba[i]);
  }
  if(stubs[n].c!=-1)
  {
    // Save PC as return address
    emit_movimm(stubs[n].c,EAX);
    emit_writeword(EAX,&pcaddr);
  }
  else
  {
    // Return address depends on which way the branch goes
    if(itype[i]==CJUMP||itype[i]==SJUMP)
    {
      int s1l=get_reg(branch_regs[i].regmap,rs1[i]);
      int s2l=get_reg(branch_regs[i].regmap,rs2[i]);
      if(rs1[i]==0)
      {
        s1l=s2l;
        s2l=-1;
      }
      else if(rs2[i]==0)
      {
        s2l=-1;
      }
      assert(s1l>=0);
      #ifdef DESTRUCTIVE_WRITEBACK
      if(rs1[i]) {
        if((branch_regs[i].dirty>>s1l)&&1)
          emit_loadreg(rs1[i],s1l);
      }
      else {
        if((branch_regs[i].dirty>>s1l)&1)
          emit_loadreg(rs2[i],s1l);
      }
      if(s2l>=0)
        if((branch_regs[i].dirty>>s2l)&1)
          emit_loadreg(rs2[i],s2l);
      #endif
      int hr=0;
      int addr=-1,alt=-1,ntaddr=-1;
      while(hr<HOST_REGS)
      {
        if(hr!=EXCLUDE_REG && hr!=HOST_CCREG &&
           (branch_regs[i].regmap[hr]&63)!=rs1[i] &&
           (branch_regs[i].regmap[hr]&63)!=rs2[i] )
        {
          addr=hr++;break;
        }
        hr++;
      }
      while(hr<HOST_REGS)
      {
        if(hr!=EXCLUDE_REG && hr!=HOST_CCREG &&
           (branch_regs[i].regmap[hr]&63)!=rs1[i] &&
           (branch_regs[i].regmap[hr]&63)!=rs2[i] )
        {
          alt=hr++;break;
        }
        hr++;
      }
      if((opcode[i]&0x2E)==6) // BLEZ/BGTZ needs another register
      {
        while(hr<HOST_REGS)
        {
          if(hr!=EXCLUDE_REG && hr!=HOST_CCREG &&
             (branch_regs[i].regmap[hr]&63)!=rs1[i] &&
             (branch_regs[i].regmap[hr]&63)!=rs2[i] )
          {
            ntaddr=hr;break;
          }
          hr++;
        }
        assert(hr<HOST_REGS);
      }
      if((opcode[i]&0x2f)==4) // BEQ
      {
        #ifdef HAVE_CMOV_IMM
        if(s2l>=0) emit_cmp(s1l,s2l);
        else emit_test(s1l,s1l);
        emit_cmov2imm_e_ne_compact(ba[i],start+i*4+8,addr);
        #else
        emit_mov2imm_compact(ba[i],addr,start+i*4+8,alt);
        if(s2l>=0) emit_cmp(s1l,s2l);
        else emit_test(s1l,s1l);
        emit_cmovne_reg(alt,addr);
        #endif
      }
      if((opcode[i]&0x2f)==5) // BNE
      {
        #ifdef HAVE_CMOV_IMM
        if(s2l>=0) emit_cmp(s1l,s2l);
        else emit_test(s1l,s1l);
        emit_cmov2imm_e_ne_compact(start+i*4+8,ba[i],addr);
        #else
        emit_mov2imm_compact(start+i*4+8,addr,ba[i],alt);
        if(s2l>=0) emit_cmp(s1l,s2l);
        else emit_test(s1l,s1l);
        emit_cmovne_reg(alt,addr);
        #endif
      }
      if((opcode[i]&0x2f)==6) // BLEZ
      {
        //emit_movimm(ba[i],alt);
        //emit_movimm(start+i*4+8,addr);
        emit_mov2imm_compact(ba[i],alt,start+i*4+8,addr);
        emit_cmpimm(s1l,1);
        emit_cmovl_reg(alt,addr);
      }
      if((opcode[i]&0x2f)==7) // BGTZ
      {
        //emit_movimm(ba[i],addr);
        //emit_movimm(start+i*4+8,ntaddr);
        emit_mov2imm_compact(ba[i],addr,start+i*4+8,ntaddr);
        emit_cmpimm(s1l,1);
        emit_cmovl_reg(ntaddr,addr);
      }
      if((opcode[i]==1)&&(opcode2[i]&0x2D)==0) // BLTZ
      {
        //emit_movimm(ba[i],alt);
        //emit_movimm(start+i*4+8,addr);
        emit_mov2imm_compact(ba[i],alt,start+i*4+8,addr);
        emit_test(s1l,s1l);
        emit_cmovs_reg(alt,addr);
      }
      if((opcode[i]==1)&&(opcode2[i]&0x2D)==1) // BGEZ
      {
        //emit_movimm(ba[i],addr);
        //emit_movimm(start+i*4+8,alt);
        emit_mov2imm_compact(ba[i],addr,start+i*4+8,alt);
        emit_test(s1l,s1l);
        emit_cmovs_reg(alt,addr);
      }
      if(opcode[i]==0x11 && opcode2[i]==0x08 ) {
        if(source[i]&0x10000) // BC1T
        {
          //emit_movimm(ba[i],alt);
          //emit_movimm(start+i*4+8,addr);
          emit_mov2imm_compact(ba[i],alt,start+i*4+8,addr);
          emit_testimm(s1l,0x800000);
          emit_cmovne_reg(alt,addr);
        }
        else // BC1F
        {
          //emit_movimm(ba[i],addr);
          //emit_movimm(start+i*4+8,alt);
          emit_mov2imm_compact(ba[i],addr,start+i*4+8,alt);
          emit_testimm(s1l,0x800000);
          emit_cmovne_reg(alt,addr);
        }
      }
      emit_writeword(addr,&pcaddr);
    }
    else
    if(itype[i]==RJUMP)
    {
      int r=get_reg(branch_regs[i].regmap,rs1[i]);
      if(rs1[i]==rt1[i+1]||rs1[i]==rt2[i+1]) {
        r=get_reg(branch_regs[i].regmap,RTEMP);
      }
      emit_writeword(r,&pcaddr);
    }
    else {SysPrintf("Unknown branch type in do_ccstub\n");abort();}
  }
  // Update cycle count
  assert(branch_regs[i].regmap[HOST_CCREG]==CCREG||branch_regs[i].regmap[HOST_CCREG]==-1);
  if(stubs[n].a) emit_addimm(HOST_CCREG,CLOCK_ADJUST((signed int)stubs[n].a),HOST_CCREG);
  emit_far_call(cc_interrupt);
  if(stubs[n].a) emit_addimm(HOST_CCREG,-CLOCK_ADJUST((signed int)stubs[n].a),HOST_CCREG);
  if(stubs[n].d==TAKEN) {
    if(internal_branch(ba[i]))
      load_needed_regs(branch_regs[i].regmap,regs[(ba[i]-start)>>2].regmap_entry);
    else if(itype[i]==RJUMP) {
      if(get_reg(branch_regs[i].regmap,RTEMP)>=0)
        emit_readword(&pcaddr,get_reg(branch_regs[i].regmap,RTEMP));
      else
        emit_loadreg(rs1[i],get_reg(branch_regs[i].regmap,rs1[i]));
    }
  }else if(stubs[n].d==NOTTAKEN) {
    if(i<slen-2) load_needed_regs(branch_regs[i].regmap,regmap_pre[i+2]);
    else load_all_regs(branch_regs[i].regmap);
  }else if(stubs[n].d==NULLDS) {
    // Delay slot instruction is nullified ("likely" branch)
    if(i<slen-2) load_needed_regs(regs[i].regmap,regmap_pre[i+2]);
    else load_all_regs(regs[i].regmap);
  }else{
    load_all_regs(branch_regs[i].regmap);
  }
  if (stubs[n].retaddr)
    emit_jmp(stubs[n].retaddr);
  else
    do_jump_vaddr(stubs[n].e);
}

static void add_to_linker(void *addr, u_int target, int ext)
{
  assert(linkcount < ARRAY_SIZE(link_addr));
  link_addr[linkcount].addr = addr;
  link_addr[linkcount].target = target;
  link_addr[linkcount].ext = ext;
  linkcount++;
}

static void ujump_assemble_write_ra(int i)
{
  int rt;
  unsigned int return_address;
  rt=get_reg(branch_regs[i].regmap,31);
  assem_debug("branch(%d): eax=%d ecx=%d edx=%d ebx=%d ebp=%d esi=%d edi=%d\n",i,branch_regs[i].regmap[0],branch_regs[i].regmap[1],branch_regs[i].regmap[2],branch_regs[i].regmap[3],branch_regs[i].regmap[5],branch_regs[i].regmap[6],branch_regs[i].regmap[7]);
  //assert(rt>=0);
  return_address=start+i*4+8;
  if(rt>=0) {
    #ifdef USE_MINI_HT
    if(internal_branch(return_address)&&rt1[i+1]!=31) {
      int temp=-1; // note: must be ds-safe
      #ifdef HOST_TEMPREG
      temp=HOST_TEMPREG;
      #endif
      if(temp>=0) do_miniht_insert(return_address,rt,temp);
      else emit_movimm(return_address,rt);
    }
    else
    #endif
    {
      #ifdef REG_PREFETCH
      if(temp>=0)
      {
        if(i_regmap[temp]!=PTEMP) emit_movimm((uintptr_t)hash_table_get(return_address),temp);
      }
      #endif
      emit_movimm(return_address,rt); // PC into link register
      #ifdef IMM_PREFETCH
      emit_prefetch(hash_table_get(return_address));
      #endif
    }
  }
}

static void ujump_assemble(int i,struct regstat *i_regs)
{
  int ra_done=0;
  if(i==(ba[i]-start)>>2) assem_debug("idle loop\n");
  address_generation(i+1,i_regs,regs[i].regmap_entry);
  #ifdef REG_PREFETCH
  int temp=get_reg(branch_regs[i].regmap,PTEMP);
  if(rt1[i]==31&&temp>=0)
  {
    signed char *i_regmap=i_regs->regmap;
    int return_address=start+i*4+8;
    if(get_reg(branch_regs[i].regmap,31)>0)
    if(i_regmap[temp]==PTEMP) emit_movimm((uintptr_t)hash_table_get(return_address),temp);
  }
  #endif
  if(rt1[i]==31&&(rt1[i]==rs1[i+1]||rt1[i]==rs2[i+1])) {
    ujump_assemble_write_ra(i); // writeback ra for DS
    ra_done=1;
  }
  ds_assemble(i+1,i_regs);
  uint64_t bc_unneeded=branch_regs[i].u;
  bc_unneeded|=1|(1LL<<rt1[i]);
  wb_invalidate(regs[i].regmap,branch_regs[i].regmap,regs[i].dirty,bc_unneeded);
  load_regs(regs[i].regmap,branch_regs[i].regmap,CCREG,CCREG);
  if(!ra_done&&rt1[i]==31)
    ujump_assemble_write_ra(i);
  int cc,adj;
  cc=get_reg(branch_regs[i].regmap,CCREG);
  assert(cc==HOST_CCREG);
  store_regs_bt(branch_regs[i].regmap,branch_regs[i].dirty,ba[i]);
  #ifdef REG_PREFETCH
  if(rt1[i]==31&&temp>=0) emit_prefetchreg(temp);
  #endif
  do_cc(i,branch_regs[i].regmap,&adj,ba[i],TAKEN,0);
  if(adj) emit_addimm(cc,CLOCK_ADJUST(ccadj[i]+2-adj),cc);
  load_regs_bt(branch_regs[i].regmap,branch_regs[i].dirty,ba[i]);
  if(internal_branch(ba[i]))
    assem_debug("branch: internal\n");
  else
    assem_debug("branch: external\n");
  if(internal_branch(ba[i])&&is_ds[(ba[i]-start)>>2]) {
    ds_assemble_entry(i);
  }
  else {
    add_to_linker(out,ba[i],internal_branch(ba[i]));
    emit_jmp(0);
  }
}

static void rjump_assemble_write_ra(int i)
{
  int rt,return_address;
  assert(rt1[i+1]!=rt1[i]);
  assert(rt2[i+1]!=rt1[i]);
  rt=get_reg(branch_regs[i].regmap,rt1[i]);
  assem_debug("branch(%d): eax=%d ecx=%d edx=%d ebx=%d ebp=%d esi=%d edi=%d\n",i,branch_regs[i].regmap[0],branch_regs[i].regmap[1],branch_regs[i].regmap[2],branch_regs[i].regmap[3],branch_regs[i].regmap[5],branch_regs[i].regmap[6],branch_regs[i].regmap[7]);
  assert(rt>=0);
  return_address=start+i*4+8;
  #ifdef REG_PREFETCH
  if(temp>=0)
  {
    if(i_regmap[temp]!=PTEMP) emit_movimm((uintptr_t)hash_table_get(return_address),temp);
  }
  #endif
  emit_movimm(return_address,rt); // PC into link register
  #ifdef IMM_PREFETCH
  emit_prefetch(hash_table_get(return_address));
  #endif
}

static void rjump_assemble(int i,struct regstat *i_regs)
{
  int temp;
  int rs,cc;
  int ra_done=0;
  rs=get_reg(branch_regs[i].regmap,rs1[i]);
  assert(rs>=0);
  if(rs1[i]==rt1[i+1]||rs1[i]==rt2[i+1]) {
    // Delay slot abuse, make a copy of the branch address register
    temp=get_reg(branch_regs[i].regmap,RTEMP);
    assert(temp>=0);
    assert(regs[i].regmap[temp]==RTEMP);
    emit_mov(rs,temp);
    rs=temp;
  }
  address_generation(i+1,i_regs,regs[i].regmap_entry);
  #ifdef REG_PREFETCH
  if(rt1[i]==31)
  {
    if((temp=get_reg(branch_regs[i].regmap,PTEMP))>=0) {
      signed char *i_regmap=i_regs->regmap;
      int return_address=start+i*4+8;
      if(i_regmap[temp]==PTEMP) emit_movimm((uintptr_t)hash_table_get(return_address),temp);
    }
  }
  #endif
  #ifdef USE_MINI_HT
  if(rs1[i]==31) {
    int rh=get_reg(regs[i].regmap,RHASH);
    if(rh>=0) do_preload_rhash(rh);
  }
  #endif
  if(rt1[i]!=0&&(rt1[i]==rs1[i+1]||rt1[i]==rs2[i+1])) {
    rjump_assemble_write_ra(i);
    ra_done=1;
  }
  ds_assemble(i+1,i_regs);
  uint64_t bc_unneeded=branch_regs[i].u;
  bc_unneeded|=1|(1LL<<rt1[i]);
  bc_unneeded&=~(1LL<<rs1[i]);
  wb_invalidate(regs[i].regmap,branch_regs[i].regmap,regs[i].dirty,bc_unneeded);
  load_regs(regs[i].regmap,branch_regs[i].regmap,rs1[i],CCREG);
  if(!ra_done&&rt1[i]!=0)
    rjump_assemble_write_ra(i);
  cc=get_reg(branch_regs[i].regmap,CCREG);
  assert(cc==HOST_CCREG);
  (void)cc;
  #ifdef USE_MINI_HT
  int rh=get_reg(branch_regs[i].regmap,RHASH);
  int ht=get_reg(branch_regs[i].regmap,RHTBL);
  if(rs1[i]==31) {
    if(regs[i].regmap[rh]!=RHASH) do_preload_rhash(rh);
    do_preload_rhtbl(ht);
    do_rhash(rs,rh);
  }
  #endif
  store_regs_bt(branch_regs[i].regmap,branch_regs[i].dirty,-1);
  #ifdef DESTRUCTIVE_WRITEBACK
  if((branch_regs[i].dirty>>rs)&1) {
    if(rs1[i]!=rt1[i+1]&&rs1[i]!=rt2[i+1]) {
      emit_loadreg(rs1[i],rs);
    }
  }
  #endif
  #ifdef REG_PREFETCH
  if(rt1[i]==31&&temp>=0) emit_prefetchreg(temp);
  #endif
  #ifdef USE_MINI_HT
  if(rs1[i]==31) {
    do_miniht_load(ht,rh);
  }
  #endif
  //do_cc(i,branch_regs[i].regmap,&adj,-1,TAKEN);
  //if(adj) emit_addimm(cc,2*(ccadj[i]+2-adj),cc); // ??? - Shouldn't happen
  //assert(adj==0);
  emit_addimm_and_set_flags(CLOCK_ADJUST(ccadj[i]+2),HOST_CCREG);
  add_stub(CC_STUB,out,NULL,0,i,-1,TAKEN,rs);
  if(itype[i+1]==COP0&&(source[i+1]&0x3f)==0x10)
    // special case for RFE
    emit_jmp(0);
  else
    emit_jns(0);
  //load_regs_bt(branch_regs[i].regmap,branch_regs[i].dirty,-1);
  #ifdef USE_MINI_HT
  if(rs1[i]==31) {
    do_miniht_jump(rs,rh,ht);
  }
  else
  #endif
  {
    do_jump_vaddr(rs);
  }
  #ifdef CORTEX_A8_BRANCH_PREDICTION_HACK
  if(rt1[i]!=31&&i<slen-2&&(((u_int)out)&7)) emit_mov(13,13);
  #endif
}

static void cjump_assemble(int i,struct regstat *i_regs)
{
  signed char *i_regmap=i_regs->regmap;
  int cc;
  int match;
  match=match_bt(branch_regs[i].regmap,branch_regs[i].dirty,ba[i]);
  assem_debug("match=%d\n",match);
  int s1l,s2l;
  int unconditional=0,nop=0;
  int invert=0;
  int internal=internal_branch(ba[i]);
  if(i==(ba[i]-start)>>2) assem_debug("idle loop\n");
  if(!match) invert=1;
  #ifdef CORTEX_A8_BRANCH_PREDICTION_HACK
  if(i>(ba[i]-start)>>2) invert=1;
  #endif
  #ifdef __aarch64__
  invert=1; // because of near cond. branches
  #endif

  if(ooo[i]) {
    s1l=get_reg(branch_regs[i].regmap,rs1[i]);
    s2l=get_reg(branch_regs[i].regmap,rs2[i]);
  }
  else {
    s1l=get_reg(i_regmap,rs1[i]);
    s2l=get_reg(i_regmap,rs2[i]);
  }
  if(rs1[i]==0&&rs2[i]==0)
  {
    if(opcode[i]&1) nop=1;
    else unconditional=1;
    //assert(opcode[i]!=5);
    //assert(opcode[i]!=7);
    //assert(opcode[i]!=0x15);
    //assert(opcode[i]!=0x17);
  }
  else if(rs1[i]==0)
  {
    s1l=s2l;
    s2l=-1;
  }
  else if(rs2[i]==0)
  {
    s2l=-1;
  }

  if(ooo[i]) {
    // Out of order execution (delay slot first)
    //printf("OOOE\n");
    address_generation(i+1,i_regs,regs[i].regmap_entry);
    ds_assemble(i+1,i_regs);
    int adj;
    uint64_t bc_unneeded=branch_regs[i].u;
    bc_unneeded&=~((1LL<<rs1[i])|(1LL<<rs2[i]));
    bc_unneeded|=1;
    wb_invalidate(regs[i].regmap,branch_regs[i].regmap,regs[i].dirty,bc_unneeded);
    load_regs(regs[i].regmap,branch_regs[i].regmap,rs1[i],rs2[i]);
    load_regs(regs[i].regmap,branch_regs[i].regmap,CCREG,CCREG);
    cc=get_reg(branch_regs[i].regmap,CCREG);
    assert(cc==HOST_CCREG);
    if(unconditional)
      store_regs_bt(branch_regs[i].regmap,branch_regs[i].dirty,ba[i]);
    //do_cc(i,branch_regs[i].regmap,&adj,unconditional?ba[i]:-1,unconditional);
    //assem_debug("cycle count (adj)\n");
    if(unconditional) {
      do_cc(i,branch_regs[i].regmap,&adj,ba[i],TAKEN,0);
      if(i!=(ba[i]-start)>>2 || source[i+1]!=0) {
        if(adj) emit_addimm(cc,CLOCK_ADJUST(ccadj[i]+2-adj),cc);
        load_regs_bt(branch_regs[i].regmap,branch_regs[i].dirty,ba[i]);
        if(internal)
          assem_debug("branch: internal\n");
        else
          assem_debug("branch: external\n");
        if(internal&&is_ds[(ba[i]-start)>>2]) {
          ds_assemble_entry(i);
        }
        else {
          add_to_linker(out,ba[i],internal);
          emit_jmp(0);
        }
        #ifdef CORTEX_A8_BRANCH_PREDICTION_HACK
        if(((u_int)out)&7) emit_addnop(0);
        #endif
      }
    }
    else if(nop) {
      emit_addimm_and_set_flags(CLOCK_ADJUST(ccadj[i]+2),cc);
      void *jaddr=out;
      emit_jns(0);
      add_stub(CC_STUB,jaddr,out,0,i,start+i*4+8,NOTTAKEN,0);
    }
    else {
      void *taken = NULL, *nottaken = NULL, *nottaken1 = NULL;
      do_cc(i,branch_regs[i].regmap,&adj,-1,0,invert);
      if(adj&&!invert) emit_addimm(cc,CLOCK_ADJUST(ccadj[i]+2-adj),cc);

      //printf("branch(%d): eax=%d ecx=%d edx=%d ebx=%d ebp=%d esi=%d edi=%d\n",i,branch_regs[i].regmap[0],branch_regs[i].regmap[1],branch_regs[i].regmap[2],branch_regs[i].regmap[3],branch_regs[i].regmap[5],branch_regs[i].regmap[6],branch_regs[i].regmap[7]);
      assert(s1l>=0);
      if(opcode[i]==4) // BEQ
      {
        if(s2l>=0) emit_cmp(s1l,s2l);
        else emit_test(s1l,s1l);
        if(invert){
          nottaken=out;
          emit_jne(DJT_1);
        }else{
          add_to_linker(out,ba[i],internal);
          emit_jeq(0);
        }
      }
      if(opcode[i]==5) // BNE
      {
        if(s2l>=0) emit_cmp(s1l,s2l);
        else emit_test(s1l,s1l);
        if(invert){
          nottaken=out;
          emit_jeq(DJT_1);
        }else{
          add_to_linker(out,ba[i],internal);
          emit_jne(0);
        }
      }
      if(opcode[i]==6) // BLEZ
      {
        emit_cmpimm(s1l,1);
        if(invert){
          nottaken=out;
          emit_jge(DJT_1);
        }else{
          add_to_linker(out,ba[i],internal);
          emit_jl(0);
        }
      }
      if(opcode[i]==7) // BGTZ
      {
        emit_cmpimm(s1l,1);
        if(invert){
          nottaken=out;
          emit_jl(DJT_1);
        }else{
          add_to_linker(out,ba[i],internal);
          emit_jge(0);
        }
      }
      if(invert) {
        if(taken) set_jump_target(taken, out);
        #ifdef CORTEX_A8_BRANCH_PREDICTION_HACK
        if(match&&(!internal||!is_ds[(ba[i]-start)>>2])) {
          if(adj) {
            emit_addimm(cc,-CLOCK_ADJUST(adj),cc);
            add_to_linker(out,ba[i],internal);
          }else{
            emit_addnop(13);
            add_to_linker(out,ba[i],internal*2);
          }
          emit_jmp(0);
        }else
        #endif
        {
          if(adj) emit_addimm(cc,-CLOCK_ADJUST(adj),cc);
          store_regs_bt(branch_regs[i].regmap,branch_regs[i].dirty,ba[i]);
          load_regs_bt(branch_regs[i].regmap,branch_regs[i].dirty,ba[i]);
          if(internal)
            assem_debug("branch: internal\n");
          else
            assem_debug("branch: external\n");
          if(internal&&is_ds[(ba[i]-start)>>2]) {
            ds_assemble_entry(i);
          }
          else {
            add_to_linker(out,ba[i],internal);
            emit_jmp(0);
          }
        }
        set_jump_target(nottaken, out);
      }

      if(nottaken1) set_jump_target(nottaken1, out);
      if(adj) {
        if(!invert) emit_addimm(cc,CLOCK_ADJUST(adj),cc);
      }
    } // (!unconditional)
  } // if(ooo)
  else
  {
    // In-order execution (branch first)
    //if(likely[i]) printf("IOL\n");
    //else
    //printf("IOE\n");
    void *taken = NULL, *nottaken = NULL, *nottaken1 = NULL;
    if(!unconditional&&!nop) {
      //printf("branch(%d): eax=%d ecx=%d edx=%d ebx=%d ebp=%d esi=%d edi=%d\n",i,branch_regs[i].regmap[0],branch_regs[i].regmap[1],branch_regs[i].regmap[2],branch_regs[i].regmap[3],branch_regs[i].regmap[5],branch_regs[i].regmap[6],branch_regs[i].regmap[7]);
      assert(s1l>=0);
      if((opcode[i]&0x2f)==4) // BEQ
      {
        if(s2l>=0) emit_cmp(s1l,s2l);
        else emit_test(s1l,s1l);
        nottaken=out;
        emit_jne(DJT_2);
      }
      if((opcode[i]&0x2f)==5) // BNE
      {
        if(s2l>=0) emit_cmp(s1l,s2l);
        else emit_test(s1l,s1l);
        nottaken=out;
        emit_jeq(DJT_2);
      }
      if((opcode[i]&0x2f)==6) // BLEZ
      {
        emit_cmpimm(s1l,1);
        nottaken=out;
        emit_jge(DJT_2);
      }
      if((opcode[i]&0x2f)==7) // BGTZ
      {
        emit_cmpimm(s1l,1);
        nottaken=out;
        emit_jl(DJT_2);
      }
    } // if(!unconditional)
    int adj;
    uint64_t ds_unneeded=branch_regs[i].u;
    ds_unneeded&=~((1LL<<rs1[i+1])|(1LL<<rs2[i+1]));
    ds_unneeded|=1;
    // branch taken
    if(!nop) {
      if(taken) set_jump_target(taken, out);
      assem_debug("1:\n");
      wb_invalidate(regs[i].regmap,branch_regs[i].regmap,regs[i].dirty,ds_unneeded);
      // load regs
      load_regs(regs[i].regmap,branch_regs[i].regmap,rs1[i+1],rs2[i+1]);
      address_generation(i+1,&branch_regs[i],0);
      load_regs(regs[i].regmap,branch_regs[i].regmap,CCREG,INVCP);
      ds_assemble(i+1,&branch_regs[i]);
      cc=get_reg(branch_regs[i].regmap,CCREG);
      if(cc==-1) {
        emit_loadreg(CCREG,cc=HOST_CCREG);
        // CHECK: Is the following instruction (fall thru) allocated ok?
      }
      assert(cc==HOST_CCREG);
      store_regs_bt(branch_regs[i].regmap,branch_regs[i].dirty,ba[i]);
      do_cc(i,i_regmap,&adj,ba[i],TAKEN,0);
      assem_debug("cycle count (adj)\n");
      if(adj) emit_addimm(cc,CLOCK_ADJUST(ccadj[i]+2-adj),cc);
      load_regs_bt(branch_regs[i].regmap,branch_regs[i].dirty,ba[i]);
      if(internal)
        assem_debug("branch: internal\n");
      else
        assem_debug("branch: external\n");
      if(internal&&is_ds[(ba[i]-start)>>2]) {
        ds_assemble_entry(i);
      }
      else {
        add_to_linker(out,ba[i],internal);
        emit_jmp(0);
      }
    }
    // branch not taken
    if(!unconditional) {
      if(nottaken1) set_jump_target(nottaken1, out);
      set_jump_target(nottaken, out);
      assem_debug("2:\n");
      if(!likely[i]) {
        wb_invalidate(regs[i].regmap,branch_regs[i].regmap,regs[i].dirty,ds_unneeded);
        load_regs(regs[i].regmap,branch_regs[i].regmap,rs1[i+1],rs2[i+1]);
        address_generation(i+1,&branch_regs[i],0);
        load_regs(regs[i].regmap,branch_regs[i].regmap,CCREG,CCREG);
        ds_assemble(i+1,&branch_regs[i]);
      }
      cc=get_reg(branch_regs[i].regmap,CCREG);
      if(cc==-1&&!likely[i]) {
        // Cycle count isn't in a register, temporarily load it then write it out
        emit_loadreg(CCREG,HOST_CCREG);
        emit_addimm_and_set_flags(CLOCK_ADJUST(ccadj[i]+2),HOST_CCREG);
        void *jaddr=out;
        emit_jns(0);
        add_stub(CC_STUB,jaddr,out,0,i,start+i*4+8,NOTTAKEN,0);
        emit_storereg(CCREG,HOST_CCREG);
      }
      else{
        cc=get_reg(i_regmap,CCREG);
        assert(cc==HOST_CCREG);
        emit_addimm_and_set_flags(CLOCK_ADJUST(ccadj[i]+2),cc);
        void *jaddr=out;
        emit_jns(0);
        add_stub(CC_STUB,jaddr,out,0,i,start+i*4+8,likely[i]?NULLDS:NOTTAKEN,0);
      }
    }
  }
}

static void sjump_assemble(int i,struct regstat *i_regs)
{
  signed char *i_regmap=i_regs->regmap;
  int cc;
  int match;
  match=match_bt(branch_regs[i].regmap,branch_regs[i].dirty,ba[i]);
  assem_debug("smatch=%d\n",match);
  int s1l;
  int unconditional=0,nevertaken=0;
  int invert=0;
  int internal=internal_branch(ba[i]);
  if(i==(ba[i]-start)>>2) assem_debug("idle loop\n");
  if(!match) invert=1;
  #ifdef CORTEX_A8_BRANCH_PREDICTION_HACK
  if(i>(ba[i]-start)>>2) invert=1;
  #endif
  #ifdef __aarch64__
  invert=1; // because of near cond. branches
  #endif

  //if(opcode2[i]>=0x10) return; // FIXME (BxxZAL)
  //assert(opcode2[i]<0x10||rs1[i]==0); // FIXME (BxxZAL)

  if(ooo[i]) {
    s1l=get_reg(branch_regs[i].regmap,rs1[i]);
  }
  else {
    s1l=get_reg(i_regmap,rs1[i]);
  }
  if(rs1[i]==0)
  {
    if(opcode2[i]&1) unconditional=1;
    else nevertaken=1;
    // These are never taken (r0 is never less than zero)
    //assert(opcode2[i]!=0);
    //assert(opcode2[i]!=2);
    //assert(opcode2[i]!=0x10);
    //assert(opcode2[i]!=0x12);
  }

  if(ooo[i]) {
    // Out of order execution (delay slot first)
    //printf("OOOE\n");
    address_generation(i+1,i_regs,regs[i].regmap_entry);
    ds_assemble(i+1,i_regs);
    int adj;
    uint64_t bc_unneeded=branch_regs[i].u;
    bc_unneeded&=~((1LL<<rs1[i])|(1LL<<rs2[i]));
    bc_unneeded|=1;
    wb_invalidate(regs[i].regmap,branch_regs[i].regmap,regs[i].dirty,bc_unneeded);
    load_regs(regs[i].regmap,branch_regs[i].regmap,rs1[i],rs1[i]);
    load_regs(regs[i].regmap,branch_regs[i].regmap,CCREG,CCREG);
    if(rt1[i]==31) {
      int rt,return_address;
      rt=get_reg(branch_regs[i].regmap,31);
      assem_debug("branch(%d): eax=%d ecx=%d edx=%d ebx=%d ebp=%d esi=%d edi=%d\n",i,branch_regs[i].regmap[0],branch_regs[i].regmap[1],branch_regs[i].regmap[2],branch_regs[i].regmap[3],branch_regs[i].regmap[5],branch_regs[i].regmap[6],branch_regs[i].regmap[7]);
      if(rt>=0) {
        // Save the PC even if the branch is not taken
        return_address=start+i*4+8;
        emit_movimm(return_address,rt); // PC into link register
        #ifdef IMM_PREFETCH
        if(!nevertaken) emit_prefetch(hash_table_get(return_address));
        #endif
      }
    }
    cc=get_reg(branch_regs[i].regmap,CCREG);
    assert(cc==HOST_CCREG);
    if(unconditional)
      store_regs_bt(branch_regs[i].regmap,branch_regs[i].dirty,ba[i]);
    //do_cc(i,branch_regs[i].regmap,&adj,unconditional?ba[i]:-1,unconditional);
    assem_debug("cycle count (adj)\n");
    if(unconditional) {
      do_cc(i,branch_regs[i].regmap,&adj,ba[i],TAKEN,0);
      if(i!=(ba[i]-start)>>2 || source[i+1]!=0) {
        if(adj) emit_addimm(cc,CLOCK_ADJUST(ccadj[i]+2-adj),cc);
        load_regs_bt(branch_regs[i].regmap,branch_regs[i].dirty,ba[i]);
        if(internal)
          assem_debug("branch: internal\n");
        else
          assem_debug("branch: external\n");
        if(internal&&is_ds[(ba[i]-start)>>2]) {
          ds_assemble_entry(i);
        }
        else {
          add_to_linker(out,ba[i],internal);
          emit_jmp(0);
        }
        #ifdef CORTEX_A8_BRANCH_PREDICTION_HACK
        if(((u_int)out)&7) emit_addnop(0);
        #endif
      }
    }
    else if(nevertaken) {
      emit_addimm_and_set_flags(CLOCK_ADJUST(ccadj[i]+2),cc);
      void *jaddr=out;
      emit_jns(0);
      add_stub(CC_STUB,jaddr,out,0,i,start+i*4+8,NOTTAKEN,0);
    }
    else {
      void *nottaken = NULL;
      do_cc(i,branch_regs[i].regmap,&adj,-1,0,invert);
      if(adj&&!invert) emit_addimm(cc,CLOCK_ADJUST(ccadj[i]+2-adj),cc);
      {
        assert(s1l>=0);
        if((opcode2[i]&0xf)==0) // BLTZ/BLTZAL
        {
          emit_test(s1l,s1l);
          if(invert){
            nottaken=out;
            emit_jns(DJT_1);
          }else{
            add_to_linker(out,ba[i],internal);
            emit_js(0);
          }
        }
        if((opcode2[i]&0xf)==1) // BGEZ/BLTZAL
        {
          emit_test(s1l,s1l);
          if(invert){
            nottaken=out;
            emit_js(DJT_1);
          }else{
            add_to_linker(out,ba[i],internal);
            emit_jns(0);
          }
        }
      }

      if(invert) {
        #ifdef CORTEX_A8_BRANCH_PREDICTION_HACK
        if(match&&(!internal||!is_ds[(ba[i]-start)>>2])) {
          if(adj) {
            emit_addimm(cc,-CLOCK_ADJUST(adj),cc);
            add_to_linker(out,ba[i],internal);
          }else{
            emit_addnop(13);
            add_to_linker(out,ba[i],internal*2);
          }
          emit_jmp(0);
        }else
        #endif
        {
          if(adj) emit_addimm(cc,-CLOCK_ADJUST(adj),cc);
          store_regs_bt(branch_regs[i].regmap,branch_regs[i].dirty,ba[i]);
          load_regs_bt(branch_regs[i].regmap,branch_regs[i].dirty,ba[i]);
          if(internal)
            assem_debug("branch: internal\n");
          else
            assem_debug("branch: external\n");
          if(internal&&is_ds[(ba[i]-start)>>2]) {
            ds_assemble_entry(i);
          }
          else {
            add_to_linker(out,ba[i],internal);
            emit_jmp(0);
          }
        }
        set_jump_target(nottaken, out);
      }

      if(adj) {
        if(!invert) emit_addimm(cc,CLOCK_ADJUST(adj),cc);
      }
    } // (!unconditional)
  } // if(ooo)
  else
  {
    // In-order execution (branch first)
    //printf("IOE\n");
    void *nottaken = NULL;
    if(rt1[i]==31) {
      int rt,return_address;
      rt=get_reg(branch_regs[i].regmap,31);
      if(rt>=0) {
        // Save the PC even if the branch is not taken
        return_address=start+i*4+8;
        emit_movimm(return_address,rt); // PC into link register
        #ifdef IMM_PREFETCH
        emit_prefetch(hash_table_get(return_address));
        #endif
      }
    }
    if(!unconditional) {
      //printf("branch(%d): eax=%d ecx=%d edx=%d ebx=%d ebp=%d esi=%d edi=%d\n",i,branch_regs[i].regmap[0],branch_regs[i].regmap[1],branch_regs[i].regmap[2],branch_regs[i].regmap[3],branch_regs[i].regmap[5],branch_regs[i].regmap[6],branch_regs[i].regmap[7]);
        assert(s1l>=0);
        if((opcode2[i]&0x0d)==0) // BLTZ/BLTZL/BLTZAL/BLTZALL
        {
          emit_test(s1l,s1l);
          nottaken=out;
          emit_jns(DJT_1);
        }
        if((opcode2[i]&0x0d)==1) // BGEZ/BGEZL/BGEZAL/BGEZALL
        {
          emit_test(s1l,s1l);
          nottaken=out;
          emit_js(DJT_1);
        }
    } // if(!unconditional)
    int adj;
    uint64_t ds_unneeded=branch_regs[i].u;
    ds_unneeded&=~((1LL<<rs1[i+1])|(1LL<<rs2[i+1]));
    ds_unneeded|=1;
    // branch taken
    if(!nevertaken) {
      //assem_debug("1:\n");
      wb_invalidate(regs[i].regmap,branch_regs[i].regmap,regs[i].dirty,ds_unneeded);
      // load regs
      load_regs(regs[i].regmap,branch_regs[i].regmap,rs1[i+1],rs2[i+1]);
      address_generation(i+1,&branch_regs[i],0);
      load_regs(regs[i].regmap,branch_regs[i].regmap,CCREG,INVCP);
      ds_assemble(i+1,&branch_regs[i]);
      cc=get_reg(branch_regs[i].regmap,CCREG);
      if(cc==-1) {
        emit_loadreg(CCREG,cc=HOST_CCREG);
        // CHECK: Is the following instruction (fall thru) allocated ok?
      }
      assert(cc==HOST_CCREG);
      store_regs_bt(branch_regs[i].regmap,branch_regs[i].dirty,ba[i]);
      do_cc(i,i_regmap,&adj,ba[i],TAKEN,0);
      assem_debug("cycle count (adj)\n");
      if(adj) emit_addimm(cc,CLOCK_ADJUST(ccadj[i]+2-adj),cc);
      load_regs_bt(branch_regs[i].regmap,branch_regs[i].dirty,ba[i]);
      if(internal)
        assem_debug("branch: internal\n");
      else
        assem_debug("branch: external\n");
      if(internal&&is_ds[(ba[i]-start)>>2]) {
        ds_assemble_entry(i);
      }
      else {
        add_to_linker(out,ba[i],internal);
        emit_jmp(0);
      }
    }
    // branch not taken
    if(!unconditional) {
      set_jump_target(nottaken, out);
      assem_debug("1:\n");
      if(!likely[i]) {
        wb_invalidate(regs[i].regmap,branch_regs[i].regmap,regs[i].dirty,ds_unneeded);
        load_regs(regs[i].regmap,branch_regs[i].regmap,rs1[i+1],rs2[i+1]);
        address_generation(i+1,&branch_regs[i],0);
        load_regs(regs[i].regmap,branch_regs[i].regmap,CCREG,CCREG);
        ds_assemble(i+1,&branch_regs[i]);
      }
      cc=get_reg(branch_regs[i].regmap,CCREG);
      if(cc==-1&&!likely[i]) {
        // Cycle count isn't in a register, temporarily load it then write it out
        emit_loadreg(CCREG,HOST_CCREG);
        emit_addimm_and_set_flags(CLOCK_ADJUST(ccadj[i]+2),HOST_CCREG);
        void *jaddr=out;
        emit_jns(0);
        add_stub(CC_STUB,jaddr,out,0,i,start+i*4+8,NOTTAKEN,0);
        emit_storereg(CCREG,HOST_CCREG);
      }
      else{
        cc=get_reg(i_regmap,CCREG);
        assert(cc==HOST_CCREG);
        emit_addimm_and_set_flags(CLOCK_ADJUST(ccadj[i]+2),cc);
        void *jaddr=out;
        emit_jns(0);
        add_stub(CC_STUB,jaddr,out,0,i,start+i*4+8,likely[i]?NULLDS:NOTTAKEN,0);
      }
    }
  }
}

static void pagespan_assemble(int i,struct regstat *i_regs)
{
  int s1l=get_reg(i_regs->regmap,rs1[i]);
  int s2l=get_reg(i_regs->regmap,rs2[i]);
  void *taken = NULL;
  void *nottaken = NULL;
  int unconditional=0;
  if(rs1[i]==0)
  {
    s1l=s2l;
    s2l=-1;
  }
  else if(rs2[i]==0)
  {
    s2l=-1;
  }
  int hr=0;
  int addr=-1,alt=-1,ntaddr=-1;
  if(i_regs->regmap[HOST_BTREG]<0) {addr=HOST_BTREG;}
  else {
    while(hr<HOST_REGS)
    {
      if(hr!=EXCLUDE_REG && hr!=HOST_CCREG &&
         (i_regs->regmap[hr]&63)!=rs1[i] &&
         (i_regs->regmap[hr]&63)!=rs2[i] )
      {
        addr=hr++;break;
      }
      hr++;
    }
  }
  while(hr<HOST_REGS)
  {
    if(hr!=EXCLUDE_REG && hr!=HOST_CCREG && hr!=HOST_BTREG &&
       (i_regs->regmap[hr]&63)!=rs1[i] &&
       (i_regs->regmap[hr]&63)!=rs2[i] )
    {
      alt=hr++;break;
    }
    hr++;
  }
  if((opcode[i]&0x2E)==6) // BLEZ/BGTZ needs another register
  {
    while(hr<HOST_REGS)
    {
      if(hr!=EXCLUDE_REG && hr!=HOST_CCREG && hr!=HOST_BTREG &&
         (i_regs->regmap[hr]&63)!=rs1[i] &&
         (i_regs->regmap[hr]&63)!=rs2[i] )
      {
        ntaddr=hr;break;
      }
      hr++;
    }
  }
  assert(hr<HOST_REGS);
  if((opcode[i]&0x2e)==4||opcode[i]==0x11) { // BEQ/BNE/BEQL/BNEL/BC1
    load_regs(regs[i].regmap_entry,regs[i].regmap,CCREG,CCREG);
  }
  emit_addimm(HOST_CCREG,CLOCK_ADJUST(ccadj[i]+2),HOST_CCREG);
  if(opcode[i]==2) // J
  {
    unconditional=1;
  }
  if(opcode[i]==3) // JAL
  {
    // TODO: mini_ht
    int rt=get_reg(i_regs->regmap,31);
    emit_movimm(start+i*4+8,rt);
    unconditional=1;
  }
  if(opcode[i]==0&&(opcode2[i]&0x3E)==8) // JR/JALR
  {
    emit_mov(s1l,addr);
    if(opcode2[i]==9) // JALR
    {
      int rt=get_reg(i_regs->regmap,rt1[i]);
      emit_movimm(start+i*4+8,rt);
    }
  }
  if((opcode[i]&0x3f)==4) // BEQ
  {
    if(rs1[i]==rs2[i])
    {
      unconditional=1;
    }
    else
    #ifdef HAVE_CMOV_IMM
    if(1) {
      if(s2l>=0) emit_cmp(s1l,s2l);
      else emit_test(s1l,s1l);
      emit_cmov2imm_e_ne_compact(ba[i],start+i*4+8,addr);
    }
    else
    #endif
    {
      assert(s1l>=0);
      emit_mov2imm_compact(ba[i],addr,start+i*4+8,alt);
      if(s2l>=0) emit_cmp(s1l,s2l);
      else emit_test(s1l,s1l);
      emit_cmovne_reg(alt,addr);
    }
  }
  if((opcode[i]&0x3f)==5) // BNE
  {
    #ifdef HAVE_CMOV_IMM
    if(s2l>=0) emit_cmp(s1l,s2l);
    else emit_test(s1l,s1l);
    emit_cmov2imm_e_ne_compact(start+i*4+8,ba[i],addr);
    #else
    assert(s1l>=0);
    emit_mov2imm_compact(start+i*4+8,addr,ba[i],alt);
    if(s2l>=0) emit_cmp(s1l,s2l);
    else emit_test(s1l,s1l);
    emit_cmovne_reg(alt,addr);
    #endif
  }
  if((opcode[i]&0x3f)==0x14) // BEQL
  {
    if(s2l>=0) emit_cmp(s1l,s2l);
    else emit_test(s1l,s1l);
    if(nottaken) set_jump_target(nottaken, out);
    nottaken=out;
    emit_jne(0);
  }
  if((opcode[i]&0x3f)==0x15) // BNEL
  {
    if(s2l>=0) emit_cmp(s1l,s2l);
    else emit_test(s1l,s1l);
    nottaken=out;
    emit_jeq(0);
    if(taken) set_jump_target(taken, out);
  }
  if((opcode[i]&0x3f)==6) // BLEZ
  {
    emit_mov2imm_compact(ba[i],alt,start+i*4+8,addr);
    emit_cmpimm(s1l,1);
    emit_cmovl_reg(alt,addr);
  }
  if((opcode[i]&0x3f)==7) // BGTZ
  {
    emit_mov2imm_compact(ba[i],addr,start+i*4+8,ntaddr);
    emit_cmpimm(s1l,1);
    emit_cmovl_reg(ntaddr,addr);
  }
  if((opcode[i]&0x3f)==0x16) // BLEZL
  {
    assert((opcode[i]&0x3f)!=0x16);
  }
  if((opcode[i]&0x3f)==0x17) // BGTZL
  {
    assert((opcode[i]&0x3f)!=0x17);
  }
  assert(opcode[i]!=1); // BLTZ/BGEZ

  //FIXME: Check CSREG
  if(opcode[i]==0x11 && opcode2[i]==0x08 ) {
    if((source[i]&0x30000)==0) // BC1F
    {
      emit_mov2imm_compact(ba[i],addr,start+i*4+8,alt);
      emit_testimm(s1l,0x800000);
      emit_cmovne_reg(alt,addr);
    }
    if((source[i]&0x30000)==0x10000) // BC1T
    {
      emit_mov2imm_compact(ba[i],alt,start+i*4+8,addr);
      emit_testimm(s1l,0x800000);
      emit_cmovne_reg(alt,addr);
    }
    if((source[i]&0x30000)==0x20000) // BC1FL
    {
      emit_testimm(s1l,0x800000);
      nottaken=out;
      emit_jne(0);
    }
    if((source[i]&0x30000)==0x30000) // BC1TL
    {
      emit_testimm(s1l,0x800000);
      nottaken=out;
      emit_jeq(0);
    }
  }

  assert(i_regs->regmap[HOST_CCREG]==CCREG);
  wb_dirtys(regs[i].regmap,regs[i].dirty);
  if(likely[i]||unconditional)
  {
    emit_movimm(ba[i],HOST_BTREG);
  }
  else if(addr!=HOST_BTREG)
  {
    emit_mov(addr,HOST_BTREG);
  }
  void *branch_addr=out;
  emit_jmp(0);
  int target_addr=start+i*4+5;
  void *stub=out;
  void *compiled_target_addr=check_addr(target_addr);
  emit_extjump_ds(branch_addr, target_addr);
  if(compiled_target_addr) {
    set_jump_target(branch_addr, compiled_target_addr);
    add_link(target_addr,stub);
  }
  else set_jump_target(branch_addr, stub);
  if(likely[i]) {
    // Not-taken path
    set_jump_target(nottaken, out);
    wb_dirtys(regs[i].regmap,regs[i].dirty);
    void *branch_addr=out;
    emit_jmp(0);
    int target_addr=start+i*4+8;
    void *stub=out;
    void *compiled_target_addr=check_addr(target_addr);
    emit_extjump_ds(branch_addr, target_addr);
    if(compiled_target_addr) {
      set_jump_target(branch_addr, compiled_target_addr);
      add_link(target_addr,stub);
    }
    else set_jump_target(branch_addr, stub);
  }
}

// Assemble the delay slot for the above
static void pagespan_ds()
{
  assem_debug("initial delay slot:\n");
  u_int vaddr=start+1;
  u_int page=get_page(vaddr);
  u_int vpage=get_vpage(vaddr);
  ll_add(jump_dirty+vpage,vaddr,(void *)out);
  do_dirty_stub_ds();
  ll_add(jump_in+page,vaddr,(void *)out);
  assert(regs[0].regmap_entry[HOST_CCREG]==CCREG);
  if(regs[0].regmap[HOST_CCREG]!=CCREG)
    wb_register(CCREG,regs[0].regmap_entry,regs[0].wasdirty);
  if(regs[0].regmap[HOST_BTREG]!=BTREG)
    emit_writeword(HOST_BTREG,&branch_target);
  load_regs(regs[0].regmap_entry,regs[0].regmap,rs1[0],rs2[0]);
  address_generation(0,&regs[0],regs[0].regmap_entry);
  if(itype[0]==STORE||itype[0]==STORELR||(opcode[0]&0x3b)==0x39||(opcode[0]&0x3b)==0x3a)
    load_regs(regs[0].regmap_entry,regs[0].regmap,INVCP,INVCP);
  is_delayslot=0;
  switch(itype[0]) {
    case ALU:
      alu_assemble(0,&regs[0]);break;
    case IMM16:
      imm16_assemble(0,&regs[0]);break;
    case SHIFT:
      shift_assemble(0,&regs[0]);break;
    case SHIFTIMM:
      shiftimm_assemble(0,&regs[0]);break;
    case LOAD:
      load_assemble(0,&regs[0]);break;
    case LOADLR:
      loadlr_assemble(0,&regs[0]);break;
    case STORE:
      store_assemble(0,&regs[0]);break;
    case STORELR:
      storelr_assemble(0,&regs[0]);break;
    case COP0:
      cop0_assemble(0,&regs[0]);break;
    case COP1:
      cop1_assemble(0,&regs[0]);break;
    case C1LS:
      c1ls_assemble(0,&regs[0]);break;
    case COP2:
      cop2_assemble(0,&regs[0]);break;
    case C2LS:
      c2ls_assemble(0,&regs[0]);break;
    case C2OP:
      c2op_assemble(0,&regs[0]);break;
    case MULTDIV:
      multdiv_assemble(0,&regs[0]);break;
    case MOV:
      mov_assemble(0,&regs[0]);break;
    case SYSCALL:
    case HLECALL:
    case INTCALL:
    case SPAN:
    case UJUMP:
    case RJUMP:
    case CJUMP:
    case SJUMP:
      SysPrintf("Jump in the delay slot.  This is probably a bug.\n");
  }
  int btaddr=get_reg(regs[0].regmap,BTREG);
  if(btaddr<0) {
    btaddr=get_reg(regs[0].regmap,-1);
    emit_readword(&branch_target,btaddr);
  }
  assert(btaddr!=HOST_CCREG);
  if(regs[0].regmap[HOST_CCREG]!=CCREG) emit_loadreg(CCREG,HOST_CCREG);
#ifdef HOST_IMM8
  host_tempreg_acquire();
  emit_movimm(start+4,HOST_TEMPREG);
  emit_cmp(btaddr,HOST_TEMPREG);
  host_tempreg_release();
#else
  emit_cmpimm(btaddr,start+4);
#endif
  void *branch = out;
  emit_jeq(0);
  store_regs_bt(regs[0].regmap,regs[0].dirty,-1);
  do_jump_vaddr(btaddr);
  set_jump_target(branch, out);
  store_regs_bt(regs[0].regmap,regs[0].dirty,start+4);
  load_regs_bt(regs[0].regmap,regs[0].dirty,start+4);
}

// Basic liveness analysis for MIPS registers
void unneeded_registers(int istart,int iend,int r)
{
  int i;
  uint64_t u,gte_u,b,gte_b;
  uint64_t temp_u,temp_gte_u=0;
  uint64_t gte_u_unknown=0;
  if (HACK_ENABLED(NDHACK_GTE_UNNEEDED))
    gte_u_unknown=~0ll;
  if(iend==slen-1) {
    u=1;
    gte_u=gte_u_unknown;
  }else{
    //u=unneeded_reg[iend+1];
    u=1;
    gte_u=gte_unneeded[iend+1];
  }

  for (i=iend;i>=istart;i--)
  {
    //printf("unneeded registers i=%d (%d,%d) r=%d\n",i,istart,iend,r);
    if(itype[i]==RJUMP||itype[i]==UJUMP||itype[i]==CJUMP||itype[i]==SJUMP)
    {
      // If subroutine call, flag return address as a possible branch target
      if(rt1[i]==31 && i<slen-2) bt[i+2]=1;

      if(ba[i]<start || ba[i]>=(start+slen*4))
      {
        // Branch out of this block, flush all regs
        u=1;
        gte_u=gte_u_unknown;
        branch_unneeded_reg[i]=u;
        // Merge in delay slot
        u|=(1LL<<rt1[i+1])|(1LL<<rt2[i+1]);
        u&=~((1LL<<rs1[i+1])|(1LL<<rs2[i+1]));
        u|=1;
        gte_u|=gte_rt[i+1];
        gte_u&=~gte_rs[i+1];
        // If branch is "likely" (and conditional)
        // then we skip the delay slot on the fall-thru path
        if(likely[i]) {
          if(i<slen-1) {
            u&=unneeded_reg[i+2];
            gte_u&=gte_unneeded[i+2];
          }
          else
          {
            u=1;
            gte_u=gte_u_unknown;
          }
        }
      }
      else
      {
        // Internal branch, flag target
        bt[(ba[i]-start)>>2]=1;
        if(ba[i]<=start+i*4) {
          // Backward branch
          if(is_ujump(i))
          {
            // Unconditional branch
            temp_u=1;
            temp_gte_u=0;
          } else {
            // Conditional branch (not taken case)
            temp_u=unneeded_reg[i+2];
            temp_gte_u&=gte_unneeded[i+2];
          }
          // Merge in delay slot
          temp_u|=(1LL<<rt1[i+1])|(1LL<<rt2[i+1]);
          temp_u&=~((1LL<<rs1[i+1])|(1LL<<rs2[i+1]));
          temp_u|=1;
          temp_gte_u|=gte_rt[i+1];
          temp_gte_u&=~gte_rs[i+1];
          // If branch is "likely" (and conditional)
          // then we skip the delay slot on the fall-thru path
          if(likely[i]) {
            if(i<slen-1) {
              temp_u&=unneeded_reg[i+2];
              temp_gte_u&=gte_unneeded[i+2];
            }
            else
            {
              temp_u=1;
              temp_gte_u=gte_u_unknown;
            }
          }
          temp_u|=(1LL<<rt1[i])|(1LL<<rt2[i]);
          temp_u&=~((1LL<<rs1[i])|(1LL<<rs2[i]));
          temp_u|=1;
          temp_gte_u|=gte_rt[i];
          temp_gte_u&=~gte_rs[i];
          unneeded_reg[i]=temp_u;
          gte_unneeded[i]=temp_gte_u;
          // Only go three levels deep.  This recursion can take an
          // excessive amount of time if there are a lot of nested loops.
          if(r<2) {
            unneeded_registers((ba[i]-start)>>2,i-1,r+1);
          }else{
            unneeded_reg[(ba[i]-start)>>2]=1;
            gte_unneeded[(ba[i]-start)>>2]=gte_u_unknown;
          }
        } /*else*/ if(1) {
          if (is_ujump(i))
          {
            // Unconditional branch
            u=unneeded_reg[(ba[i]-start)>>2];
            gte_u=gte_unneeded[(ba[i]-start)>>2];
            branch_unneeded_reg[i]=u;
            // Merge in delay slot
            u|=(1LL<<rt1[i+1])|(1LL<<rt2[i+1]);
            u&=~((1LL<<rs1[i+1])|(1LL<<rs2[i+1]));
            u|=1;
            gte_u|=gte_rt[i+1];
            gte_u&=~gte_rs[i+1];
          } else {
            // Conditional branch
            b=unneeded_reg[(ba[i]-start)>>2];
            gte_b=gte_unneeded[(ba[i]-start)>>2];
            branch_unneeded_reg[i]=b;
            // Branch delay slot
            b|=(1LL<<rt1[i+1])|(1LL<<rt2[i+1]);
            b&=~((1LL<<rs1[i+1])|(1LL<<rs2[i+1]));
            b|=1;
            gte_b|=gte_rt[i+1];
            gte_b&=~gte_rs[i+1];
            // If branch is "likely" then we skip the
            // delay slot on the fall-thru path
            if(likely[i]) {
              u=b;
              gte_u=gte_b;
              if(i<slen-1) {
                u&=unneeded_reg[i+2];
                gte_u&=gte_unneeded[i+2];
              }
            } else {
              u&=b;
              gte_u&=gte_b;
            }
            if(i<slen-1) {
              branch_unneeded_reg[i]&=unneeded_reg[i+2];
            } else {
              branch_unneeded_reg[i]=1;
            }
          }
        }
      }
    }
    else if(itype[i]==SYSCALL||itype[i]==HLECALL||itype[i]==INTCALL)
    {
      // SYSCALL instruction (software interrupt)
      u=1;
    }
    else if(itype[i]==COP0 && (source[i]&0x3f)==0x18)
    {
      // ERET instruction (return from interrupt)
      u=1;
    }
    //u=1; // DEBUG
    // Written registers are unneeded
    u|=1LL<<rt1[i];
    u|=1LL<<rt2[i];
    gte_u|=gte_rt[i];
    // Accessed registers are needed
    u&=~(1LL<<rs1[i]);
    u&=~(1LL<<rs2[i]);
    gte_u&=~gte_rs[i];
    if(gte_rs[i]&&rt1[i]&&(unneeded_reg[i+1]&(1ll<<rt1[i])))
      gte_u|=gte_rs[i]&gte_unneeded[i+1]; // MFC2/CFC2 to dead register, unneeded
    // Source-target dependencies
    // R0 is always unneeded
    u|=1;
    // Save it
    unneeded_reg[i]=u;
    gte_unneeded[i]=gte_u;
    /*
    printf("ur (%d,%d) %x: ",istart,iend,start+i*4);
    printf("U:");
    int r;
    for(r=1;r<=CCREG;r++) {
      if((unneeded_reg[i]>>r)&1) {
        if(r==HIREG) printf(" HI");
        else if(r==LOREG) printf(" LO");
        else printf(" r%d",r);
      }
    }
    printf("\n");
    */
  }
}

// Write back dirty registers as soon as we will no longer modify them,
// so that we don't end up with lots of writes at the branches.
void clean_registers(int istart,int iend,int wr)
{
  int i;
  int r;
  u_int will_dirty_i,will_dirty_next,temp_will_dirty;
  u_int wont_dirty_i,wont_dirty_next,temp_wont_dirty;
  if(iend==slen-1) {
    will_dirty_i=will_dirty_next=0;
    wont_dirty_i=wont_dirty_next=0;
  }else{
    will_dirty_i=will_dirty_next=will_dirty[iend+1];
    wont_dirty_i=wont_dirty_next=wont_dirty[iend+1];
  }
  for (i=iend;i>=istart;i--)
  {
    if(itype[i]==RJUMP||itype[i]==UJUMP||itype[i]==CJUMP||itype[i]==SJUMP)
    {
      if(ba[i]<start || ba[i]>=(start+slen*4))
      {
        // Branch out of this block, flush all regs
        if (is_ujump(i))
        {
          // Unconditional branch
          will_dirty_i=0;
          wont_dirty_i=0;
          // Merge in delay slot (will dirty)
          for(r=0;r<HOST_REGS;r++) {
            if(r!=EXCLUDE_REG) {
              if((branch_regs[i].regmap[r]&63)==rt1[i]) will_dirty_i|=1<<r;
              if((branch_regs[i].regmap[r]&63)==rt2[i]) will_dirty_i|=1<<r;
              if((branch_regs[i].regmap[r]&63)==rt1[i+1]) will_dirty_i|=1<<r;
              if((branch_regs[i].regmap[r]&63)==rt2[i+1]) will_dirty_i|=1<<r;
              if((branch_regs[i].regmap[r]&63)>33) will_dirty_i&=~(1<<r);
              if(branch_regs[i].regmap[r]<=0) will_dirty_i&=~(1<<r);
              if(branch_regs[i].regmap[r]==CCREG) will_dirty_i|=1<<r;
              if((regs[i].regmap[r]&63)==rt1[i]) will_dirty_i|=1<<r;
              if((regs[i].regmap[r]&63)==rt2[i]) will_dirty_i|=1<<r;
              if((regs[i].regmap[r]&63)==rt1[i+1]) will_dirty_i|=1<<r;
              if((regs[i].regmap[r]&63)==rt2[i+1]) will_dirty_i|=1<<r;
              if((regs[i].regmap[r]&63)>33) will_dirty_i&=~(1<<r);
              if(regs[i].regmap[r]<=0) will_dirty_i&=~(1<<r);
              if(regs[i].regmap[r]==CCREG) will_dirty_i|=1<<r;
            }
          }
        }
        else
        {
          // Conditional branch
          will_dirty_i=0;
          wont_dirty_i=wont_dirty_next;
          // Merge in delay slot (will dirty)
          for(r=0;r<HOST_REGS;r++) {
            if(r!=EXCLUDE_REG) {
              if(!likely[i]) {
                // Might not dirty if likely branch is not taken
                if((branch_regs[i].regmap[r]&63)==rt1[i]) will_dirty_i|=1<<r;
                if((branch_regs[i].regmap[r]&63)==rt2[i]) will_dirty_i|=1<<r;
                if((branch_regs[i].regmap[r]&63)==rt1[i+1]) will_dirty_i|=1<<r;
                if((branch_regs[i].regmap[r]&63)==rt2[i+1]) will_dirty_i|=1<<r;
                if((branch_regs[i].regmap[r]&63)>33) will_dirty_i&=~(1<<r);
                if(branch_regs[i].regmap[r]==0) will_dirty_i&=~(1<<r);
                if(branch_regs[i].regmap[r]==CCREG) will_dirty_i|=1<<r;
                //if((regs[i].regmap[r]&63)==rt1[i]) will_dirty_i|=1<<r;
                //if((regs[i].regmap[r]&63)==rt2[i]) will_dirty_i|=1<<r;
                if((regs[i].regmap[r]&63)==rt1[i+1]) will_dirty_i|=1<<r;
                if((regs[i].regmap[r]&63)==rt2[i+1]) will_dirty_i|=1<<r;
                if((regs[i].regmap[r]&63)>33) will_dirty_i&=~(1<<r);
                if(regs[i].regmap[r]<=0) will_dirty_i&=~(1<<r);
                if(regs[i].regmap[r]==CCREG) will_dirty_i|=1<<r;
              }
            }
          }
        }
        // Merge in delay slot (wont dirty)
        for(r=0;r<HOST_REGS;r++) {
          if(r!=EXCLUDE_REG) {
            if((regs[i].regmap[r]&63)==rt1[i]) wont_dirty_i|=1<<r;
            if((regs[i].regmap[r]&63)==rt2[i]) wont_dirty_i|=1<<r;
            if((regs[i].regmap[r]&63)==rt1[i+1]) wont_dirty_i|=1<<r;
            if((regs[i].regmap[r]&63)==rt2[i+1]) wont_dirty_i|=1<<r;
            if(regs[i].regmap[r]==CCREG) wont_dirty_i|=1<<r;
            if((branch_regs[i].regmap[r]&63)==rt1[i]) wont_dirty_i|=1<<r;
            if((branch_regs[i].regmap[r]&63)==rt2[i]) wont_dirty_i|=1<<r;
            if((branch_regs[i].regmap[r]&63)==rt1[i+1]) wont_dirty_i|=1<<r;
            if((branch_regs[i].regmap[r]&63)==rt2[i+1]) wont_dirty_i|=1<<r;
            if(branch_regs[i].regmap[r]==CCREG) wont_dirty_i|=1<<r;
          }
        }
        if(wr) {
          #ifndef DESTRUCTIVE_WRITEBACK
          branch_regs[i].dirty&=wont_dirty_i;
          #endif
          branch_regs[i].dirty|=will_dirty_i;
        }
      }
      else
      {
        // Internal branch
        if(ba[i]<=start+i*4) {
          // Backward branch
          if (is_ujump(i))
          {
            // Unconditional branch
            temp_will_dirty=0;
            temp_wont_dirty=0;
            // Merge in delay slot (will dirty)
            for(r=0;r<HOST_REGS;r++) {
              if(r!=EXCLUDE_REG) {
                if((branch_regs[i].regmap[r]&63)==rt1[i]) temp_will_dirty|=1<<r;
                if((branch_regs[i].regmap[r]&63)==rt2[i]) temp_will_dirty|=1<<r;
                if((branch_regs[i].regmap[r]&63)==rt1[i+1]) temp_will_dirty|=1<<r;
                if((branch_regs[i].regmap[r]&63)==rt2[i+1]) temp_will_dirty|=1<<r;
                if((branch_regs[i].regmap[r]&63)>33) temp_will_dirty&=~(1<<r);
                if(branch_regs[i].regmap[r]<=0) temp_will_dirty&=~(1<<r);
                if(branch_regs[i].regmap[r]==CCREG) temp_will_dirty|=1<<r;
                if((regs[i].regmap[r]&63)==rt1[i]) temp_will_dirty|=1<<r;
                if((regs[i].regmap[r]&63)==rt2[i]) temp_will_dirty|=1<<r;
                if((regs[i].regmap[r]&63)==rt1[i+1]) temp_will_dirty|=1<<r;
                if((regs[i].regmap[r]&63)==rt2[i+1]) temp_will_dirty|=1<<r;
                if((regs[i].regmap[r]&63)>33) temp_will_dirty&=~(1<<r);
                if(regs[i].regmap[r]<=0) temp_will_dirty&=~(1<<r);
                if(regs[i].regmap[r]==CCREG) temp_will_dirty|=1<<r;
              }
            }
          } else {
            // Conditional branch (not taken case)
            temp_will_dirty=will_dirty_next;
            temp_wont_dirty=wont_dirty_next;
            // Merge in delay slot (will dirty)
            for(r=0;r<HOST_REGS;r++) {
              if(r!=EXCLUDE_REG) {
                if(!likely[i]) {
                  // Will not dirty if likely branch is not taken
                  if((branch_regs[i].regmap[r]&63)==rt1[i]) temp_will_dirty|=1<<r;
                  if((branch_regs[i].regmap[r]&63)==rt2[i]) temp_will_dirty|=1<<r;
                  if((branch_regs[i].regmap[r]&63)==rt1[i+1]) temp_will_dirty|=1<<r;
                  if((branch_regs[i].regmap[r]&63)==rt2[i+1]) temp_will_dirty|=1<<r;
                  if((branch_regs[i].regmap[r]&63)>33) temp_will_dirty&=~(1<<r);
                  if(branch_regs[i].regmap[r]==0) temp_will_dirty&=~(1<<r);
                  if(branch_regs[i].regmap[r]==CCREG) temp_will_dirty|=1<<r;
                  //if((regs[i].regmap[r]&63)==rt1[i]) temp_will_dirty|=1<<r;
                  //if((regs[i].regmap[r]&63)==rt2[i]) temp_will_dirty|=1<<r;
                  if((regs[i].regmap[r]&63)==rt1[i+1]) temp_will_dirty|=1<<r;
                  if((regs[i].regmap[r]&63)==rt2[i+1]) temp_will_dirty|=1<<r;
                  if((regs[i].regmap[r]&63)>33) temp_will_dirty&=~(1<<r);
                  if(regs[i].regmap[r]<=0) temp_will_dirty&=~(1<<r);
                  if(regs[i].regmap[r]==CCREG) temp_will_dirty|=1<<r;
                }
              }
            }
          }
          // Merge in delay slot (wont dirty)
          for(r=0;r<HOST_REGS;r++) {
            if(r!=EXCLUDE_REG) {
              if((regs[i].regmap[r]&63)==rt1[i]) temp_wont_dirty|=1<<r;
              if((regs[i].regmap[r]&63)==rt2[i]) temp_wont_dirty|=1<<r;
              if((regs[i].regmap[r]&63)==rt1[i+1]) temp_wont_dirty|=1<<r;
              if((regs[i].regmap[r]&63)==rt2[i+1]) temp_wont_dirty|=1<<r;
              if(regs[i].regmap[r]==CCREG) temp_wont_dirty|=1<<r;
              if((branch_regs[i].regmap[r]&63)==rt1[i]) temp_wont_dirty|=1<<r;
              if((branch_regs[i].regmap[r]&63)==rt2[i]) temp_wont_dirty|=1<<r;
              if((branch_regs[i].regmap[r]&63)==rt1[i+1]) temp_wont_dirty|=1<<r;
              if((branch_regs[i].regmap[r]&63)==rt2[i+1]) temp_wont_dirty|=1<<r;
              if(branch_regs[i].regmap[r]==CCREG) temp_wont_dirty|=1<<r;
            }
          }
          // Deal with changed mappings
          if(i<iend) {
            for(r=0;r<HOST_REGS;r++) {
              if(r!=EXCLUDE_REG) {
                if(regs[i].regmap[r]!=regmap_pre[i][r]) {
                  temp_will_dirty&=~(1<<r);
                  temp_wont_dirty&=~(1<<r);
                  if((regmap_pre[i][r]&63)>0 && (regmap_pre[i][r]&63)<34) {
                    temp_will_dirty|=((unneeded_reg[i]>>(regmap_pre[i][r]&63))&1)<<r;
                    temp_wont_dirty|=((unneeded_reg[i]>>(regmap_pre[i][r]&63))&1)<<r;
                  } else {
                    temp_will_dirty|=1<<r;
                    temp_wont_dirty|=1<<r;
                  }
                }
              }
            }
          }
          if(wr) {
            will_dirty[i]=temp_will_dirty;
            wont_dirty[i]=temp_wont_dirty;
            clean_registers((ba[i]-start)>>2,i-1,0);
          }else{
            // Limit recursion.  It can take an excessive amount
            // of time if there are a lot of nested loops.
            will_dirty[(ba[i]-start)>>2]=0;
            wont_dirty[(ba[i]-start)>>2]=-1;
          }
        }
        /*else*/ if(1)
        {
          if (is_ujump(i))
          {
            // Unconditional branch
            will_dirty_i=0;
            wont_dirty_i=0;
          //if(ba[i]>start+i*4) { // Disable recursion (for debugging)
            for(r=0;r<HOST_REGS;r++) {
              if(r!=EXCLUDE_REG) {
                if(branch_regs[i].regmap[r]==regs[(ba[i]-start)>>2].regmap_entry[r]) {
                  will_dirty_i|=will_dirty[(ba[i]-start)>>2]&(1<<r);
                  wont_dirty_i|=wont_dirty[(ba[i]-start)>>2]&(1<<r);
                }
                if(branch_regs[i].regmap[r]>=0) {
                  will_dirty_i|=((unneeded_reg[(ba[i]-start)>>2]>>(branch_regs[i].regmap[r]&63))&1)<<r;
                  wont_dirty_i|=((unneeded_reg[(ba[i]-start)>>2]>>(branch_regs[i].regmap[r]&63))&1)<<r;
                }
              }
            }
          //}
            // Merge in delay slot
            for(r=0;r<HOST_REGS;r++) {
              if(r!=EXCLUDE_REG) {
                if((branch_regs[i].regmap[r]&63)==rt1[i]) will_dirty_i|=1<<r;
                if((branch_regs[i].regmap[r]&63)==rt2[i]) will_dirty_i|=1<<r;
                if((branch_regs[i].regmap[r]&63)==rt1[i+1]) will_dirty_i|=1<<r;
                if((branch_regs[i].regmap[r]&63)==rt2[i+1]) will_dirty_i|=1<<r;
                if((branch_regs[i].regmap[r]&63)>33) will_dirty_i&=~(1<<r);
                if(branch_regs[i].regmap[r]<=0) will_dirty_i&=~(1<<r);
                if(branch_regs[i].regmap[r]==CCREG) will_dirty_i|=1<<r;
                if((regs[i].regmap[r]&63)==rt1[i]) will_dirty_i|=1<<r;
                if((regs[i].regmap[r]&63)==rt2[i]) will_dirty_i|=1<<r;
                if((regs[i].regmap[r]&63)==rt1[i+1]) will_dirty_i|=1<<r;
                if((regs[i].regmap[r]&63)==rt2[i+1]) will_dirty_i|=1<<r;
                if((regs[i].regmap[r]&63)>33) will_dirty_i&=~(1<<r);
                if(regs[i].regmap[r]<=0) will_dirty_i&=~(1<<r);
                if(regs[i].regmap[r]==CCREG) will_dirty_i|=1<<r;
              }
            }
          } else {
            // Conditional branch
            will_dirty_i=will_dirty_next;
            wont_dirty_i=wont_dirty_next;
          //if(ba[i]>start+i*4) { // Disable recursion (for debugging)
            for(r=0;r<HOST_REGS;r++) {
              if(r!=EXCLUDE_REG) {
                signed char target_reg=branch_regs[i].regmap[r];
                if(target_reg==regs[(ba[i]-start)>>2].regmap_entry[r]) {
                  will_dirty_i&=will_dirty[(ba[i]-start)>>2]&(1<<r);
                  wont_dirty_i|=wont_dirty[(ba[i]-start)>>2]&(1<<r);
                }
                else if(target_reg>=0) {
                  will_dirty_i&=((unneeded_reg[(ba[i]-start)>>2]>>(target_reg&63))&1)<<r;
                  wont_dirty_i|=((unneeded_reg[(ba[i]-start)>>2]>>(target_reg&63))&1)<<r;
                }
                // Treat delay slot as part of branch too
                /*if(regs[i+1].regmap[r]==regs[(ba[i]-start)>>2].regmap_entry[r]) {
                  will_dirty[i+1]&=will_dirty[(ba[i]-start)>>2]&(1<<r);
                  wont_dirty[i+1]|=wont_dirty[(ba[i]-start)>>2]&(1<<r);
                }
                else
                {
                  will_dirty[i+1]&=~(1<<r);
                }*/
              }
            }
          //}
            // Merge in delay slot
            for(r=0;r<HOST_REGS;r++) {
              if(r!=EXCLUDE_REG) {
                if(!likely[i]) {
                  // Might not dirty if likely branch is not taken
                  if((branch_regs[i].regmap[r]&63)==rt1[i]) will_dirty_i|=1<<r;
                  if((branch_regs[i].regmap[r]&63)==rt2[i]) will_dirty_i|=1<<r;
                  if((branch_regs[i].regmap[r]&63)==rt1[i+1]) will_dirty_i|=1<<r;
                  if((branch_regs[i].regmap[r]&63)==rt2[i+1]) will_dirty_i|=1<<r;
                  if((branch_regs[i].regmap[r]&63)>33) will_dirty_i&=~(1<<r);
                  if(branch_regs[i].regmap[r]<=0) will_dirty_i&=~(1<<r);
                  if(branch_regs[i].regmap[r]==CCREG) will_dirty_i|=1<<r;
                  //if((regs[i].regmap[r]&63)==rt1[i]) will_dirty_i|=1<<r;
                  //if((regs[i].regmap[r]&63)==rt2[i]) will_dirty_i|=1<<r;
                  if((regs[i].regmap[r]&63)==rt1[i+1]) will_dirty_i|=1<<r;
                  if((regs[i].regmap[r]&63)==rt2[i+1]) will_dirty_i|=1<<r;
                  if((regs[i].regmap[r]&63)>33) will_dirty_i&=~(1<<r);
                  if(regs[i].regmap[r]<=0) will_dirty_i&=~(1<<r);
                  if(regs[i].regmap[r]==CCREG) will_dirty_i|=1<<r;
                }
              }
            }
          }
          // Merge in delay slot (won't dirty)
          for(r=0;r<HOST_REGS;r++) {
            if(r!=EXCLUDE_REG) {
              if((regs[i].regmap[r]&63)==rt1[i]) wont_dirty_i|=1<<r;
              if((regs[i].regmap[r]&63)==rt2[i]) wont_dirty_i|=1<<r;
              if((regs[i].regmap[r]&63)==rt1[i+1]) wont_dirty_i|=1<<r;
              if((regs[i].regmap[r]&63)==rt2[i+1]) wont_dirty_i|=1<<r;
              if(regs[i].regmap[r]==CCREG) wont_dirty_i|=1<<r;
              if((branch_regs[i].regmap[r]&63)==rt1[i]) wont_dirty_i|=1<<r;
              if((branch_regs[i].regmap[r]&63)==rt2[i]) wont_dirty_i|=1<<r;
              if((branch_regs[i].regmap[r]&63)==rt1[i+1]) wont_dirty_i|=1<<r;
              if((branch_regs[i].regmap[r]&63)==rt2[i+1]) wont_dirty_i|=1<<r;
              if(branch_regs[i].regmap[r]==CCREG) wont_dirty_i|=1<<r;
            }
          }
          if(wr) {
            #ifndef DESTRUCTIVE_WRITEBACK
            branch_regs[i].dirty&=wont_dirty_i;
            #endif
            branch_regs[i].dirty|=will_dirty_i;
          }
        }
      }
    }
    else if(itype[i]==SYSCALL||itype[i]==HLECALL||itype[i]==INTCALL)
    {
      // SYSCALL instruction (software interrupt)
      will_dirty_i=0;
      wont_dirty_i=0;
    }
    else if(itype[i]==COP0 && (source[i]&0x3f)==0x18)
    {
      // ERET instruction (return from interrupt)
      will_dirty_i=0;
      wont_dirty_i=0;
    }
    will_dirty_next=will_dirty_i;
    wont_dirty_next=wont_dirty_i;
    for(r=0;r<HOST_REGS;r++) {
      if(r!=EXCLUDE_REG) {
        if((regs[i].regmap[r]&63)==rt1[i]) will_dirty_i|=1<<r;
        if((regs[i].regmap[r]&63)==rt2[i]) will_dirty_i|=1<<r;
        if((regs[i].regmap[r]&63)>33) will_dirty_i&=~(1<<r);
        if(regs[i].regmap[r]<=0) will_dirty_i&=~(1<<r);
        if(regs[i].regmap[r]==CCREG) will_dirty_i|=1<<r;
        if((regs[i].regmap[r]&63)==rt1[i]) wont_dirty_i|=1<<r;
        if((regs[i].regmap[r]&63)==rt2[i]) wont_dirty_i|=1<<r;
        if(regs[i].regmap[r]==CCREG) wont_dirty_i|=1<<r;
        if(i>istart) {
          if(itype[i]!=RJUMP&&itype[i]!=UJUMP&&itype[i]!=CJUMP&&itype[i]!=SJUMP)
          {
            // Don't store a register immediately after writing it,
            // may prevent dual-issue.
            if((regs[i].regmap[r]&63)==rt1[i-1]) wont_dirty_i|=1<<r;
            if((regs[i].regmap[r]&63)==rt2[i-1]) wont_dirty_i|=1<<r;
          }
        }
      }
    }
    // Save it
    will_dirty[i]=will_dirty_i;
    wont_dirty[i]=wont_dirty_i;
    // Mark registers that won't be dirtied as not dirty
    if(wr) {
      /*printf("wr (%d,%d) %x will:",istart,iend,start+i*4);
      for(r=0;r<HOST_REGS;r++) {
        if((will_dirty_i>>r)&1) {
          printf(" r%d",r);
        }
      }
      printf("\n");*/

      //if(i==istart||(itype[i-1]!=RJUMP&&itype[i-1]!=UJUMP&&itype[i-1]!=CJUMP&&itype[i-1]!=SJUMP)) {
        regs[i].dirty|=will_dirty_i;
        #ifndef DESTRUCTIVE_WRITEBACK
        regs[i].dirty&=wont_dirty_i;
        if(itype[i]==RJUMP||itype[i]==UJUMP||itype[i]==CJUMP||itype[i]==SJUMP)
        {
          if (i < iend-1 && !is_ujump(i)) {
            for(r=0;r<HOST_REGS;r++) {
              if(r!=EXCLUDE_REG) {
                if(regs[i].regmap[r]==regmap_pre[i+2][r]) {
                  regs[i+2].wasdirty&=wont_dirty_i|~(1<<r);
                }else {/*printf("i: %x (%d) mismatch(+2): %d\n",start+i*4,i,r);assert(!((wont_dirty_i>>r)&1));*/}
              }
            }
          }
        }
        else
        {
          if(i<iend) {
            for(r=0;r<HOST_REGS;r++) {
              if(r!=EXCLUDE_REG) {
                if(regs[i].regmap[r]==regmap_pre[i+1][r]) {
                  regs[i+1].wasdirty&=wont_dirty_i|~(1<<r);
                }else {/*printf("i: %x (%d) mismatch(+1): %d\n",start+i*4,i,r);assert(!((wont_dirty_i>>r)&1));*/}
              }
            }
          }
        }
        #endif
      //}
    }
    // Deal with changed mappings
    temp_will_dirty=will_dirty_i;
    temp_wont_dirty=wont_dirty_i;
    for(r=0;r<HOST_REGS;r++) {
      if(r!=EXCLUDE_REG) {
        int nr;
        if(regs[i].regmap[r]==regmap_pre[i][r]) {
          if(wr) {
            #ifndef DESTRUCTIVE_WRITEBACK
            regs[i].wasdirty&=wont_dirty_i|~(1<<r);
            #endif
            regs[i].wasdirty|=will_dirty_i&(1<<r);
          }
        }
        else if(regmap_pre[i][r]>=0&&(nr=get_reg(regs[i].regmap,regmap_pre[i][r]))>=0) {
          // Register moved to a different register
          will_dirty_i&=~(1<<r);
          wont_dirty_i&=~(1<<r);
          will_dirty_i|=((temp_will_dirty>>nr)&1)<<r;
          wont_dirty_i|=((temp_wont_dirty>>nr)&1)<<r;
          if(wr) {
            #ifndef DESTRUCTIVE_WRITEBACK
            regs[i].wasdirty&=wont_dirty_i|~(1<<r);
            #endif
            regs[i].wasdirty|=will_dirty_i&(1<<r);
          }
        }
        else {
          will_dirty_i&=~(1<<r);
          wont_dirty_i&=~(1<<r);
          if((regmap_pre[i][r]&63)>0 && (regmap_pre[i][r]&63)<34) {
            will_dirty_i|=((unneeded_reg[i]>>(regmap_pre[i][r]&63))&1)<<r;
            wont_dirty_i|=((unneeded_reg[i]>>(regmap_pre[i][r]&63))&1)<<r;
          } else {
            wont_dirty_i|=1<<r;
            /*printf("i: %x (%d) mismatch: %d\n",start+i*4,i,r);assert(!((will_dirty>>r)&1));*/
          }
        }
      }
    }
  }
}

#ifdef DISASM
  /* disassembly */
void disassemble_inst(int i)
{
    if (bt[i]) printf("*"); else printf(" ");
    switch(itype[i]) {
      case UJUMP:
        printf (" %x: %s %8x\n",start+i*4,insn[i],ba[i]);break;
      case CJUMP:
        printf (" %x: %s r%d,r%d,%8x\n",start+i*4,insn[i],rs1[i],rs2[i],i?start+i*4+4+((signed int)((unsigned int)source[i]<<16)>>14):*ba);break;
      case SJUMP:
        printf (" %x: %s r%d,%8x\n",start+i*4,insn[i],rs1[i],start+i*4+4+((signed int)((unsigned int)source[i]<<16)>>14));break;
      case RJUMP:
        if (opcode[i]==0x9&&rt1[i]!=31)
          printf (" %x: %s r%d,r%d\n",start+i*4,insn[i],rt1[i],rs1[i]);
        else
          printf (" %x: %s r%d\n",start+i*4,insn[i],rs1[i]);
        break;
      case SPAN:
        printf (" %x: %s (pagespan) r%d,r%d,%8x\n",start+i*4,insn[i],rs1[i],rs2[i],ba[i]);break;
      case IMM16:
        if(opcode[i]==0xf) //LUI
          printf (" %x: %s r%d,%4x0000\n",start+i*4,insn[i],rt1[i],imm[i]&0xffff);
        else
          printf (" %x: %s r%d,r%d,%d\n",start+i*4,insn[i],rt1[i],rs1[i],imm[i]);
        break;
      case LOAD:
      case LOADLR:
        printf (" %x: %s r%d,r%d+%x\n",start+i*4,insn[i],rt1[i],rs1[i],imm[i]);
        break;
      case STORE:
      case STORELR:
        printf (" %x: %s r%d,r%d+%x\n",start+i*4,insn[i],rs2[i],rs1[i],imm[i]);
        break;
      case ALU:
      case SHIFT:
        printf (" %x: %s r%d,r%d,r%d\n",start+i*4,insn[i],rt1[i],rs1[i],rs2[i]);
        break;
      case MULTDIV:
        printf (" %x: %s r%d,r%d\n",start+i*4,insn[i],rs1[i],rs2[i]);
        break;
      case SHIFTIMM:
        printf (" %x: %s r%d,r%d,%d\n",start+i*4,insn[i],rt1[i],rs1[i],imm[i]);
        break;
      case MOV:
        if((opcode2[i]&0x1d)==0x10)
          printf (" %x: %s r%d\n",start+i*4,insn[i],rt1[i]);
        else if((opcode2[i]&0x1d)==0x11)
          printf (" %x: %s r%d\n",start+i*4,insn[i],rs1[i]);
        else
          printf (" %x: %s\n",start+i*4,insn[i]);
        break;
      case COP0:
        if(opcode2[i]==0)
          printf (" %x: %s r%d,cpr0[%d]\n",start+i*4,insn[i],rt1[i],(source[i]>>11)&0x1f); // MFC0
        else if(opcode2[i]==4)
          printf (" %x: %s r%d,cpr0[%d]\n",start+i*4,insn[i],rs1[i],(source[i]>>11)&0x1f); // MTC0
        else printf (" %x: %s\n",start+i*4,insn[i]);
        break;
      case COP1:
        if(opcode2[i]<3)
          printf (" %x: %s r%d,cpr1[%d]\n",start+i*4,insn[i],rt1[i],(source[i]>>11)&0x1f); // MFC1
        else if(opcode2[i]>3)
          printf (" %x: %s r%d,cpr1[%d]\n",start+i*4,insn[i],rs1[i],(source[i]>>11)&0x1f); // MTC1
        else printf (" %x: %s\n",start+i*4,insn[i]);
        break;
      case COP2:
        if(opcode2[i]<3)
          printf (" %x: %s r%d,cpr2[%d]\n",start+i*4,insn[i],rt1[i],(source[i]>>11)&0x1f); // MFC2
        else if(opcode2[i]>3)
          printf (" %x: %s r%d,cpr2[%d]\n",start+i*4,insn[i],rs1[i],(source[i]>>11)&0x1f); // MTC2
        else printf (" %x: %s\n",start+i*4,insn[i]);
        break;
      case C1LS:
        printf (" %x: %s cpr1[%d],r%d+%x\n",start+i*4,insn[i],(source[i]>>16)&0x1f,rs1[i],imm[i]);
        break;
      case C2LS:
        printf (" %x: %s cpr2[%d],r%d+%x\n",start+i*4,insn[i],(source[i]>>16)&0x1f,rs1[i],imm[i]);
        break;
      case INTCALL:
        printf (" %x: %s (INTCALL)\n",start+i*4,insn[i]);
        break;
      default:
        //printf (" %s %8x\n",insn[i],source[i]);
        printf (" %x: %s\n",start+i*4,insn[i]);
    }
}
#else
static void disassemble_inst(int i) {}
#endif // DISASM

#define DRC_TEST_VAL 0x74657374

static void new_dynarec_test(void)
{
  int (*testfunc)(void);
  void *beginning;
  int ret[2];
  size_t i;

  // check structure linkage
  if ((u_char *)rcnts - (u_char *)&psxRegs != sizeof(psxRegs))
  {
    SysPrintf("linkage_arm* miscompilation/breakage detected.\n");
  }

  SysPrintf("testing if we can run recompiled code...\n");
  ((volatile u_int *)out)[0]++; // make cache dirty

  for (i = 0; i < ARRAY_SIZE(ret); i++) {
    out = ndrc->translation_cache;
    beginning = start_block();
    emit_movimm(DRC_TEST_VAL + i, 0); // test
    emit_ret();
    literal_pool(0);
    end_block(beginning);
    testfunc = beginning;
    ret[i] = testfunc();
  }

  if (ret[0] == DRC_TEST_VAL && ret[1] == DRC_TEST_VAL + 1)
    SysPrintf("test passed.\n");
  else
    SysPrintf("test failed, will likely crash soon (r=%08x %08x)\n", ret[0], ret[1]);
  out = ndrc->translation_cache;
}

// clear the state completely, instead of just marking
// things invalid like invalidate_all_pages() does
void new_dynarec_clear_full(void)
{
  int n;
  out = ndrc->translation_cache;
  memset(invalid_code,1,sizeof(invalid_code));
  memset(hash_table,0xff,sizeof(hash_table));
  memset(mini_ht,-1,sizeof(mini_ht));
  memset(restore_candidate,0,sizeof(restore_candidate));
  memset(shadow,0,sizeof(shadow));
  copy=shadow;
  expirep=16384; // Expiry pointer, +2 blocks
  pending_exception=0;
  literalcount=0;
  stop_after_jal=0;
  inv_code_start=inv_code_end=~0;
  // TLB
  for(n=0;n<4096;n++) ll_clear(jump_in+n);
  for(n=0;n<4096;n++) ll_clear(jump_out+n);
  for(n=0;n<4096;n++) ll_clear(jump_dirty+n);
}

void new_dynarec_init(void)
{
  SysPrintf("Init new dynarec\n");

#ifdef BASE_ADDR_DYNAMIC
  #ifdef VITA
  sceBlock = sceKernelAllocMemBlockForVM("code", 1 << TARGET_SIZE_2);
  if (sceBlock < 0)
    SysPrintf("sceKernelAllocMemBlockForVM failed\n");
  int ret = sceKernelGetMemBlockBase(sceBlock, (void **)&ndrc);
  if (ret < 0)
    SysPrintf("sceKernelGetMemBlockBase failed\n");
  #else
  uintptr_t desired_addr = 0;
  #ifdef __ELF__
  extern char _end;
  desired_addr = ((uintptr_t)&_end + 0xffffff) & ~0xffffffl;
  #endif
  ndrc = mmap((void *)desired_addr, sizeof(*ndrc),
            PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ndrc == MAP_FAILED) {
    SysPrintf("mmap() failed: %s\n", strerror(errno));
    abort();
  }
  #endif
#else
  #ifndef NO_WRITE_EXEC
  // not all systems allow execute in data segment by default
  if (mprotect(ndrc, sizeof(ndrc->translation_cache) + sizeof(ndrc->tramp.ops),
               PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
    SysPrintf("mprotect() failed: %s\n", strerror(errno));
  #endif
#endif
  out = ndrc->translation_cache;
  cycle_multiplier=200;
  new_dynarec_clear_full();
#ifdef HOST_IMM8
  // Copy this into local area so we don't have to put it in every literal pool
  invc_ptr=invalid_code;
#endif
  arch_init();
  new_dynarec_test();
#ifndef RAM_FIXED
  ram_offset=(uintptr_t)rdram-0x80000000;
#endif
  if (ram_offset!=0)
    SysPrintf("warning: RAM is not directly mapped, performance will suffer\n");
}

void new_dynarec_cleanup(void)
{
  int n;
#ifdef BASE_ADDR_DYNAMIC
  #ifdef VITA
  sceKernelFreeMemBlock(sceBlock);
  sceBlock = -1;
  #else
  if (munmap(ndrc, sizeof(*ndrc)) < 0)
    SysPrintf("munmap() failed\n");
  #endif
#endif
  for(n=0;n<4096;n++) ll_clear(jump_in+n);
  for(n=0;n<4096;n++) ll_clear(jump_out+n);
  for(n=0;n<4096;n++) ll_clear(jump_dirty+n);
  #ifdef ROM_COPY
  if (munmap (ROM_COPY, 67108864) < 0) {SysPrintf("munmap() failed\n");}
  #endif
}

static u_int *get_source_start(u_int addr, u_int *limit)
{
  if (!HACK_ENABLED(NDHACK_OVERRIDE_CYCLE_M))
    cycle_multiplier_override = 0;

  if (addr < 0x00200000 ||
    (0xa0000000 <= addr && addr < 0xa0200000))
  {
    // used for BIOS calls mostly?
    *limit = (addr&0xa0000000)|0x00200000;
    return (u_int *)(rdram + (addr&0x1fffff));
  }
  else if (!Config.HLE && (
    /* (0x9fc00000 <= addr && addr < 0x9fc80000) ||*/
    (0xbfc00000 <= addr && addr < 0xbfc80000)))
  {
    // BIOS. The multiplier should be much higher as it's uncached 8bit mem,
    // but timings in PCSX are too tied to the interpreter's BIAS
    if (!HACK_ENABLED(NDHACK_OVERRIDE_CYCLE_M))
      cycle_multiplier_override = 200;

    *limit = (addr & 0xfff00000) | 0x80000;
    return (u_int *)((u_char *)psxR + (addr&0x7ffff));
  }
  else if (addr >= 0x80000000 && addr < 0x80000000+RAM_SIZE) {
    *limit = (addr & 0x80600000) + 0x00200000;
    return (u_int *)(rdram + (addr&0x1fffff));
  }
  return NULL;
}

static u_int scan_for_ret(u_int addr)
{
  u_int limit = 0;
  u_int *mem;

  mem = get_source_start(addr, &limit);
  if (mem == NULL)
    return addr;

  if (limit > addr + 0x1000)
    limit = addr + 0x1000;
  for (; addr < limit; addr += 4, mem++) {
    if (*mem == 0x03e00008) // jr $ra
      return addr + 8;
  }
  return addr;
}

struct savestate_block {
  uint32_t addr;
  uint32_t regflags;
};

static int addr_cmp(const void *p1_, const void *p2_)
{
  const struct savestate_block *p1 = p1_, *p2 = p2_;
  return p1->addr - p2->addr;
}

int new_dynarec_save_blocks(void *save, int size)
{
  struct savestate_block *blocks = save;
  int maxcount = size / sizeof(blocks[0]);
  struct savestate_block tmp_blocks[1024];
  struct ll_entry *head;
  int p, s, d, o, bcnt;
  u_int addr;

  o = 0;
  for (p = 0; p < ARRAY_SIZE(jump_in); p++) {
    bcnt = 0;
    for (head = jump_in[p]; head != NULL; head = head->next) {
      tmp_blocks[bcnt].addr = head->vaddr;
      tmp_blocks[bcnt].regflags = head->reg_sv_flags;
      bcnt++;
    }
    if (bcnt < 1)
      continue;
    qsort(tmp_blocks, bcnt, sizeof(tmp_blocks[0]), addr_cmp);

    addr = tmp_blocks[0].addr;
    for (s = d = 0; s < bcnt; s++) {
      if (tmp_blocks[s].addr < addr)
        continue;
      if (d == 0 || tmp_blocks[d-1].addr != tmp_blocks[s].addr)
        tmp_blocks[d++] = tmp_blocks[s];
      addr = scan_for_ret(tmp_blocks[s].addr);
    }

    if (o + d > maxcount)
      d = maxcount - o;
    memcpy(&blocks[o], tmp_blocks, d * sizeof(blocks[0]));
    o += d;
  }

  return o * sizeof(blocks[0]);
}

void new_dynarec_load_blocks(const void *save, int size)
{
  const struct savestate_block *blocks = save;
  int count = size / sizeof(blocks[0]);
  u_int regs_save[32];
  uint32_t f;
  int i, b;

  get_addr(psxRegs.pc);

  // change GPRs for speculation to at least partially work..
  memcpy(regs_save, &psxRegs.GPR, sizeof(regs_save));
  for (i = 1; i < 32; i++)
    psxRegs.GPR.r[i] = 0x80000000;

  for (b = 0; b < count; b++) {
    for (f = blocks[b].regflags, i = 0; f; f >>= 1, i++) {
      if (f & 1)
        psxRegs.GPR.r[i] = 0x1f800000;
    }

    get_addr(blocks[b].addr);

    for (f = blocks[b].regflags, i = 0; f; f >>= 1, i++) {
      if (f & 1)
        psxRegs.GPR.r[i] = 0x80000000;
    }
  }

  memcpy(&psxRegs.GPR, regs_save, sizeof(regs_save));
}

int new_recompile_block(u_int addr)
{
  u_int pagelimit = 0;
  u_int state_rflags = 0;
  int i;

  assem_debug("NOTCOMPILED: addr = %x -> %p\n", addr, out);
  //printf("TRACE: count=%d next=%d (compile %x)\n",Count,next_interupt,addr);
  //if(debug)
  //printf("fpu mapping=%x enabled=%x\n",(Status & 0x04000000)>>26,(Status & 0x20000000)>>29);

  // this is just for speculation
  for (i = 1; i < 32; i++) {
    if ((psxRegs.GPR.r[i] & 0xffff0000) == 0x1f800000)
      state_rflags |= 1 << i;
  }

  start = (u_int)addr&~3;
  //assert(((u_int)addr&1)==0); // start-in-delay-slot flag
  new_dynarec_did_compile=1;
  if (Config.HLE && start == 0x80001000) // hlecall
  {
    // XXX: is this enough? Maybe check hleSoftCall?
    void *beginning=start_block();
    u_int page=get_page(start);

    invalid_code[start>>12]=0;
    emit_movimm(start,0);
    emit_writeword(0,&pcaddr);
    emit_far_jump(new_dyna_leave);
    literal_pool(0);
    end_block(beginning);
    ll_add_flags(jump_in+page,start,state_rflags,(void *)beginning);
    return 0;
  }

  source = get_source_start(start, &pagelimit);
  if (source == NULL) {
    SysPrintf("Compile at bogus memory address: %08x\n", addr);
    abort();
  }

  /* Pass 1: disassemble */
  /* Pass 2: register dependencies, branch targets */
  /* Pass 3: register allocation */
  /* Pass 4: branch dependencies */
  /* Pass 5: pre-alloc */
  /* Pass 6: optimize clean/dirty state */
  /* Pass 7: flag 32-bit registers */
  /* Pass 8: assembly */
  /* Pass 9: linker */
  /* Pass 10: garbage collection / free memory */

  int j;
  int done=0;
  unsigned int type,op,op2;

  //printf("addr = %x source = %x %x\n", addr,source,source[0]);

  /* Pass 1 disassembly */

  for(i=0;!done;i++) {
    bt[i]=0;likely[i]=0;ooo[i]=0;op2=0;
    minimum_free_regs[i]=0;
    opcode[i]=op=source[i]>>26;
    switch(op)
    {
      case 0x00: strcpy(insn[i],"special"); type=NI;
        op2=source[i]&0x3f;
        switch(op2)
        {
          case 0x00: strcpy(insn[i],"SLL"); type=SHIFTIMM; break;
          case 0x02: strcpy(insn[i],"SRL"); type=SHIFTIMM; break;
          case 0x03: strcpy(insn[i],"SRA"); type=SHIFTIMM; break;
          case 0x04: strcpy(insn[i],"SLLV"); type=SHIFT; break;
          case 0x06: strcpy(insn[i],"SRLV"); type=SHIFT; break;
          case 0x07: strcpy(insn[i],"SRAV"); type=SHIFT; break;
          case 0x08: strcpy(insn[i],"JR"); type=RJUMP; break;
          case 0x09: strcpy(insn[i],"JALR"); type=RJUMP; break;
          case 0x0C: strcpy(insn[i],"SYSCALL"); type=SYSCALL; break;
          case 0x0D: strcpy(insn[i],"BREAK"); type=OTHER; break;
          case 0x0F: strcpy(insn[i],"SYNC"); type=OTHER; break;
          case 0x10: strcpy(insn[i],"MFHI"); type=MOV; break;
          case 0x11: strcpy(insn[i],"MTHI"); type=MOV; break;
          case 0x12: strcpy(insn[i],"MFLO"); type=MOV; break;
          case 0x13: strcpy(insn[i],"MTLO"); type=MOV; break;
          case 0x18: strcpy(insn[i],"MULT"); type=MULTDIV; break;
          case 0x19: strcpy(insn[i],"MULTU"); type=MULTDIV; break;
          case 0x1A: strcpy(insn[i],"DIV"); type=MULTDIV; break;
          case 0x1B: strcpy(insn[i],"DIVU"); type=MULTDIV; break;
          case 0x20: strcpy(insn[i],"ADD"); type=ALU; break;
          case 0x21: strcpy(insn[i],"ADDU"); type=ALU; break;
          case 0x22: strcpy(insn[i],"SUB"); type=ALU; break;
          case 0x23: strcpy(insn[i],"SUBU"); type=ALU; break;
          case 0x24: strcpy(insn[i],"AND"); type=ALU; break;
          case 0x25: strcpy(insn[i],"OR"); type=ALU; break;
          case 0x26: strcpy(insn[i],"XOR"); type=ALU; break;
          case 0x27: strcpy(insn[i],"NOR"); type=ALU; break;
          case 0x2A: strcpy(insn[i],"SLT"); type=ALU; break;
          case 0x2B: strcpy(insn[i],"SLTU"); type=ALU; break;
          case 0x30: strcpy(insn[i],"TGE"); type=NI; break;
          case 0x31: strcpy(insn[i],"TGEU"); type=NI; break;
          case 0x32: strcpy(insn[i],"TLT"); type=NI; break;
          case 0x33: strcpy(insn[i],"TLTU"); type=NI; break;
          case 0x34: strcpy(insn[i],"TEQ"); type=NI; break;
          case 0x36: strcpy(insn[i],"TNE"); type=NI; break;
#if 0
          case 0x14: strcpy(insn[i],"DSLLV"); type=SHIFT; break;
          case 0x16: strcpy(insn[i],"DSRLV"); type=SHIFT; break;
          case 0x17: strcpy(insn[i],"DSRAV"); type=SHIFT; break;
          case 0x1C: strcpy(insn[i],"DMULT"); type=MULTDIV; break;
          case 0x1D: strcpy(insn[i],"DMULTU"); type=MULTDIV; break;
          case 0x1E: strcpy(insn[i],"DDIV"); type=MULTDIV; break;
          case 0x1F: strcpy(insn[i],"DDIVU"); type=MULTDIV; break;
          case 0x2C: strcpy(insn[i],"DADD"); type=ALU; break;
          case 0x2D: strcpy(insn[i],"DADDU"); type=ALU; break;
          case 0x2E: strcpy(insn[i],"DSUB"); type=ALU; break;
          case 0x2F: strcpy(insn[i],"DSUBU"); type=ALU; break;
          case 0x38: strcpy(insn[i],"DSLL"); type=SHIFTIMM; break;
          case 0x3A: strcpy(insn[i],"DSRL"); type=SHIFTIMM; break;
          case 0x3B: strcpy(insn[i],"DSRA"); type=SHIFTIMM; break;
          case 0x3C: strcpy(insn[i],"DSLL32"); type=SHIFTIMM; break;
          case 0x3E: strcpy(insn[i],"DSRL32"); type=SHIFTIMM; break;
          case 0x3F: strcpy(insn[i],"DSRA32"); type=SHIFTIMM; break;
#endif
        }
        break;
      case 0x01: strcpy(insn[i],"regimm"); type=NI;
        op2=(source[i]>>16)&0x1f;
        switch(op2)
        {
          case 0x00: strcpy(insn[i],"BLTZ"); type=SJUMP; break;
          case 0x01: strcpy(insn[i],"BGEZ"); type=SJUMP; break;
          case 0x02: strcpy(insn[i],"BLTZL"); type=SJUMP; break;
          case 0x03: strcpy(insn[i],"BGEZL"); type=SJUMP; break;
          case 0x08: strcpy(insn[i],"TGEI"); type=NI; break;
          case 0x09: strcpy(insn[i],"TGEIU"); type=NI; break;
          case 0x0A: strcpy(insn[i],"TLTI"); type=NI; break;
          case 0x0B: strcpy(insn[i],"TLTIU"); type=NI; break;
          case 0x0C: strcpy(insn[i],"TEQI"); type=NI; break;
          case 0x0E: strcpy(insn[i],"TNEI"); type=NI; break;
          case 0x10: strcpy(insn[i],"BLTZAL"); type=SJUMP; break;
          case 0x11: strcpy(insn[i],"BGEZAL"); type=SJUMP; break;
          case 0x12: strcpy(insn[i],"BLTZALL"); type=SJUMP; break;
          case 0x13: strcpy(insn[i],"BGEZALL"); type=SJUMP; break;
        }
        break;
      case 0x02: strcpy(insn[i],"J"); type=UJUMP; break;
      case 0x03: strcpy(insn[i],"JAL"); type=UJUMP; break;
      case 0x04: strcpy(insn[i],"BEQ"); type=CJUMP; break;
      case 0x05: strcpy(insn[i],"BNE"); type=CJUMP; break;
      case 0x06: strcpy(insn[i],"BLEZ"); type=CJUMP; break;
      case 0x07: strcpy(insn[i],"BGTZ"); type=CJUMP; break;
      case 0x08: strcpy(insn[i],"ADDI"); type=IMM16; break;
      case 0x09: strcpy(insn[i],"ADDIU"); type=IMM16; break;
      case 0x0A: strcpy(insn[i],"SLTI"); type=IMM16; break;
      case 0x0B: strcpy(insn[i],"SLTIU"); type=IMM16; break;
      case 0x0C: strcpy(insn[i],"ANDI"); type=IMM16; break;
      case 0x0D: strcpy(insn[i],"ORI"); type=IMM16; break;
      case 0x0E: strcpy(insn[i],"XORI"); type=IMM16; break;
      case 0x0F: strcpy(insn[i],"LUI"); type=IMM16; break;
      case 0x10: strcpy(insn[i],"cop0"); type=NI;
        op2=(source[i]>>21)&0x1f;
        switch(op2)
        {
          case 0x00: strcpy(insn[i],"MFC0"); type=COP0; break;
          case 0x02: strcpy(insn[i],"CFC0"); type=COP0; break;
          case 0x04: strcpy(insn[i],"MTC0"); type=COP0; break;
          case 0x06: strcpy(insn[i],"CTC0"); type=COP0; break;
          case 0x10: strcpy(insn[i],"RFE"); type=COP0; break;
        }
        break;
      case 0x11: strcpy(insn[i],"cop1"); type=COP1;
        op2=(source[i]>>21)&0x1f;
        break;
#if 0
      case 0x14: strcpy(insn[i],"BEQL"); type=CJUMP; break;
      case 0x15: strcpy(insn[i],"BNEL"); type=CJUMP; break;
      case 0x16: strcpy(insn[i],"BLEZL"); type=CJUMP; break;
      case 0x17: strcpy(insn[i],"BGTZL"); type=CJUMP; break;
      case 0x18: strcpy(insn[i],"DADDI"); type=IMM16; break;
      case 0x19: strcpy(insn[i],"DADDIU"); type=IMM16; break;
      case 0x1A: strcpy(insn[i],"LDL"); type=LOADLR; break;
      case 0x1B: strcpy(insn[i],"LDR"); type=LOADLR; break;
#endif
      case 0x20: strcpy(insn[i],"LB"); type=LOAD; break;
      case 0x21: strcpy(insn[i],"LH"); type=LOAD; break;
      case 0x22: strcpy(insn[i],"LWL"); type=LOADLR; break;
      case 0x23: strcpy(insn[i],"LW"); type=LOAD; break;
      case 0x24: strcpy(insn[i],"LBU"); type=LOAD; break;
      case 0x25: strcpy(insn[i],"LHU"); type=LOAD; break;
      case 0x26: strcpy(insn[i],"LWR"); type=LOADLR; break;
#if 0
      case 0x27: strcpy(insn[i],"LWU"); type=LOAD; break;
#endif
      case 0x28: strcpy(insn[i],"SB"); type=STORE; break;
      case 0x29: strcpy(insn[i],"SH"); type=STORE; break;
      case 0x2A: strcpy(insn[i],"SWL"); type=STORELR; break;
      case 0x2B: strcpy(insn[i],"SW"); type=STORE; break;
#if 0
      case 0x2C: strcpy(insn[i],"SDL"); type=STORELR; break;
      case 0x2D: strcpy(insn[i],"SDR"); type=STORELR; break;
#endif
      case 0x2E: strcpy(insn[i],"SWR"); type=STORELR; break;
      case 0x2F: strcpy(insn[i],"CACHE"); type=NOP; break;
      case 0x30: strcpy(insn[i],"LL"); type=NI; break;
      case 0x31: strcpy(insn[i],"LWC1"); type=C1LS; break;
#if 0
      case 0x34: strcpy(insn[i],"LLD"); type=NI; break;
      case 0x35: strcpy(insn[i],"LDC1"); type=C1LS; break;
      case 0x37: strcpy(insn[i],"LD"); type=LOAD; break;
#endif
      case 0x38: strcpy(insn[i],"SC"); type=NI; break;
      case 0x39: strcpy(insn[i],"SWC1"); type=C1LS; break;
#if 0
      case 0x3C: strcpy(insn[i],"SCD"); type=NI; break;
      case 0x3D: strcpy(insn[i],"SDC1"); type=C1LS; break;
      case 0x3F: strcpy(insn[i],"SD"); type=STORE; break;
#endif
      case 0x12: strcpy(insn[i],"COP2"); type=NI;
        op2=(source[i]>>21)&0x1f;
        //if (op2 & 0x10)
        if (source[i]&0x3f) { // use this hack to support old savestates with patched gte insns
          if (gte_handlers[source[i]&0x3f]!=NULL) {
            if (gte_regnames[source[i]&0x3f]!=NULL)
              strcpy(insn[i],gte_regnames[source[i]&0x3f]);
            else
              snprintf(insn[i], sizeof(insn[i]), "COP2 %x", source[i]&0x3f);
            type=C2OP;
          }
        }
        else switch(op2)
        {
          case 0x00: strcpy(insn[i],"MFC2"); type=COP2; break;
          case 0x02: strcpy(insn[i],"CFC2"); type=COP2; break;
          case 0x04: strcpy(insn[i],"MTC2"); type=COP2; break;
          case 0x06: strcpy(insn[i],"CTC2"); type=COP2; break;
        }
        break;
      case 0x32: strcpy(insn[i],"LWC2"); type=C2LS; break;
      case 0x3A: strcpy(insn[i],"SWC2"); type=C2LS; break;
      case 0x3B: strcpy(insn[i],"HLECALL"); type=HLECALL; break;
      default: strcpy(insn[i],"???"); type=NI;
        SysPrintf("NI %08x @%08x (%08x)\n", source[i], addr + i*4, addr);
        break;
    }
    itype[i]=type;
    opcode2[i]=op2;
    /* Get registers/immediates */
    lt1[i]=0;
    dep1[i]=0;
    dep2[i]=0;
    gte_rs[i]=gte_rt[i]=0;
    switch(type) {
      case LOAD:
        rs1[i]=(source[i]>>21)&0x1f;
        rs2[i]=0;
        rt1[i]=(source[i]>>16)&0x1f;
        rt2[i]=0;
        imm[i]=(short)source[i];
        break;
      case STORE:
      case STORELR:
        rs1[i]=(source[i]>>21)&0x1f;
        rs2[i]=(source[i]>>16)&0x1f;
        rt1[i]=0;
        rt2[i]=0;
        imm[i]=(short)source[i];
        break;
      case LOADLR:
        // LWL/LWR only load part of the register,
        // therefore the target register must be treated as a source too
        rs1[i]=(source[i]>>21)&0x1f;
        rs2[i]=(source[i]>>16)&0x1f;
        rt1[i]=(source[i]>>16)&0x1f;
        rt2[i]=0;
        imm[i]=(short)source[i];
        if(op==0x26) dep1[i]=rt1[i]; // LWR
        break;
      case IMM16:
        if (op==0x0f) rs1[i]=0; // LUI instruction has no source register
        else rs1[i]=(source[i]>>21)&0x1f;
        rs2[i]=0;
        rt1[i]=(source[i]>>16)&0x1f;
        rt2[i]=0;
        if(op>=0x0c&&op<=0x0e) { // ANDI/ORI/XORI
          imm[i]=(unsigned short)source[i];
        }else{
          imm[i]=(short)source[i];
        }
        if(op==0x0d||op==0x0e) dep1[i]=rs1[i]; // ORI/XORI
        break;
      case UJUMP:
        rs1[i]=0;
        rs2[i]=0;
        rt1[i]=0;
        rt2[i]=0;
        // The JAL instruction writes to r31.
        if (op&1) {
          rt1[i]=31;
        }
        rs2[i]=CCREG;
        break;
      case RJUMP:
        rs1[i]=(source[i]>>21)&0x1f;
        rs2[i]=0;
        rt1[i]=0;
        rt2[i]=0;
        // The JALR instruction writes to rd.
        if (op2&1) {
          rt1[i]=(source[i]>>11)&0x1f;
        }
        rs2[i]=CCREG;
        break;
      case CJUMP:
        rs1[i]=(source[i]>>21)&0x1f;
        rs2[i]=(source[i]>>16)&0x1f;
        rt1[i]=0;
        rt2[i]=0;
        if(op&2) { // BGTZ/BLEZ
          rs2[i]=0;
        }
        likely[i]=op>>4;
        break;
      case SJUMP:
        rs1[i]=(source[i]>>21)&0x1f;
        rs2[i]=CCREG;
        rt1[i]=0;
        rt2[i]=0;
        if(op2&0x10) { // BxxAL
          rt1[i]=31;
          // NOTE: If the branch is not taken, r31 is still overwritten
        }
        likely[i]=(op2&2)>>1;
        break;
      case ALU:
        rs1[i]=(source[i]>>21)&0x1f; // source
        rs2[i]=(source[i]>>16)&0x1f; // subtract amount
        rt1[i]=(source[i]>>11)&0x1f; // destination
        rt2[i]=0;
        if(op2>=0x24&&op2<=0x27) { // AND/OR/XOR/NOR
          dep1[i]=rs1[i];dep2[i]=rs2[i];
        }
        else if(op2>=0x2c&&op2<=0x2f) { // DADD/DSUB
          dep1[i]=rs1[i];dep2[i]=rs2[i];
        }
        break;
      case MULTDIV:
        rs1[i]=(source[i]>>21)&0x1f; // source
        rs2[i]=(source[i]>>16)&0x1f; // divisor
        rt1[i]=HIREG;
        rt2[i]=LOREG;
        break;
      case MOV:
        rs1[i]=0;
        rs2[i]=0;
        rt1[i]=0;
        rt2[i]=0;
        if(op2==0x10) rs1[i]=HIREG; // MFHI
        if(op2==0x11) rt1[i]=HIREG; // MTHI
        if(op2==0x12) rs1[i]=LOREG; // MFLO
        if(op2==0x13) rt1[i]=LOREG; // MTLO
        if((op2&0x1d)==0x10) rt1[i]=(source[i]>>11)&0x1f; // MFxx
        if((op2&0x1d)==0x11) rs1[i]=(source[i]>>21)&0x1f; // MTxx
        dep1[i]=rs1[i];
        break;
      case SHIFT:
        rs1[i]=(source[i]>>16)&0x1f; // target of shift
        rs2[i]=(source[i]>>21)&0x1f; // shift amount
        rt1[i]=(source[i]>>11)&0x1f; // destination
        rt2[i]=0;
        break;
      case SHIFTIMM:
        rs1[i]=(source[i]>>16)&0x1f;
        rs2[i]=0;
        rt1[i]=(source[i]>>11)&0x1f;
        rt2[i]=0;
        imm[i]=(source[i]>>6)&0x1f;
        // DSxx32 instructions
        if(op2>=0x3c) imm[i]|=0x20;
        break;
      case COP0:
        rs1[i]=0;
        rs2[i]=0;
        rt1[i]=0;
        rt2[i]=0;
        if(op2==0||op2==2) rt1[i]=(source[i]>>16)&0x1F; // MFC0/CFC0
        if(op2==4||op2==6) rs1[i]=(source[i]>>16)&0x1F; // MTC0/CTC0
        if(op2==4&&((source[i]>>11)&0x1f)==12) rt2[i]=CSREG; // Status
        if(op2==16) if((source[i]&0x3f)==0x18) rs2[i]=CCREG; // ERET
        break;
      case COP1:
        rs1[i]=0;
        rs2[i]=0;
        rt1[i]=0;
        rt2[i]=0;
        if(op2<3) rt1[i]=(source[i]>>16)&0x1F; // MFC1/DMFC1/CFC1
        if(op2>3) rs1[i]=(source[i]>>16)&0x1F; // MTC1/DMTC1/CTC1
        rs2[i]=CSREG;
        break;
      case COP2:
        rs1[i]=0;
        rs2[i]=0;
        rt1[i]=0;
        rt2[i]=0;
        if(op2<3) rt1[i]=(source[i]>>16)&0x1F; // MFC2/CFC2
        if(op2>3) rs1[i]=(source[i]>>16)&0x1F; // MTC2/CTC2
        rs2[i]=CSREG;
        int gr=(source[i]>>11)&0x1F;
        switch(op2)
        {
          case 0x00: gte_rs[i]=1ll<<gr; break; // MFC2
          case 0x04: gte_rt[i]=1ll<<gr; break; // MTC2
          case 0x02: gte_rs[i]=1ll<<(gr+32); break; // CFC2
          case 0x06: gte_rt[i]=1ll<<(gr+32); break; // CTC2
        }
        break;
      case C1LS:
        rs1[i]=(source[i]>>21)&0x1F;
        rs2[i]=CSREG;
        rt1[i]=0;
        rt2[i]=0;
        imm[i]=(short)source[i];
        break;
      case C2LS:
        rs1[i]=(source[i]>>21)&0x1F;
        rs2[i]=0;
        rt1[i]=0;
        rt2[i]=0;
        imm[i]=(short)source[i];
        if(op==0x32) gte_rt[i]=1ll<<((source[i]>>16)&0x1F); // LWC2
        else gte_rs[i]=1ll<<((source[i]>>16)&0x1F); // SWC2
        break;
      case C2OP:
        rs1[i]=0;
        rs2[i]=0;
        rt1[i]=0;
        rt2[i]=0;
        gte_rs[i]=gte_reg_reads[source[i]&0x3f];
        gte_rt[i]=gte_reg_writes[source[i]&0x3f];
        gte_rt[i]|=1ll<<63; // every op changes flags
        if((source[i]&0x3f)==GTE_MVMVA) {
          int v = (source[i] >> 15) & 3;
          gte_rs[i]&=~0xe3fll;
          if(v==3) gte_rs[i]|=0xe00ll;
          else gte_rs[i]|=3ll<<(v*2);
        }
        break;
      case SYSCALL:
      case HLECALL:
      case INTCALL:
        rs1[i]=CCREG;
        rs2[i]=0;
        rt1[i]=0;
        rt2[i]=0;
        break;
      default:
        rs1[i]=0;
        rs2[i]=0;
        rt1[i]=0;
        rt2[i]=0;
    }
    /* Calculate branch target addresses */
    if(type==UJUMP)
      ba[i]=((start+i*4+4)&0xF0000000)|(((unsigned int)source[i]<<6)>>4);
    else if(type==CJUMP&&rs1[i]==rs2[i]&&(op&1))
      ba[i]=start+i*4+8; // Ignore never taken branch
    else if(type==SJUMP&&rs1[i]==0&&!(op2&1))
      ba[i]=start+i*4+8; // Ignore never taken branch
    else if(type==CJUMP||type==SJUMP)
      ba[i]=start+i*4+4+((signed int)((unsigned int)source[i]<<16)>>14);
    else ba[i]=-1;
    if (i > 0 && is_jump(i-1)) {
      int do_in_intrp=0;
      // branch in delay slot?
      if(type==RJUMP||type==UJUMP||type==CJUMP||type==SJUMP) {
        // don't handle first branch and call interpreter if it's hit
        SysPrintf("branch in delay slot @%08x (%08x)\n", addr + i*4, addr);
        do_in_intrp=1;
      }
      // basic load delay detection
      else if((type==LOAD||type==LOADLR||type==COP0||type==COP2||type==C2LS)&&rt1[i]!=0) {
        int t=(ba[i-1]-start)/4;
        if(0 <= t && t < i &&(rt1[i]==rs1[t]||rt1[i]==rs2[t])&&itype[t]!=CJUMP&&itype[t]!=SJUMP) {
          // jump target wants DS result - potential load delay effect
          SysPrintf("load delay @%08x (%08x)\n", addr + i*4, addr);
          do_in_intrp=1;
          bt[t+1]=1; // expected return from interpreter
        }
        else if(i>=2&&rt1[i-2]==2&&rt1[i]==2&&rs1[i]!=2&&rs2[i]!=2&&rs1[i-1]!=2&&rs2[i-1]!=2&&
              !(i>=3&&is_jump(i-3))) {
          // v0 overwrite like this is a sign of trouble, bail out
          SysPrintf("v0 overwrite @%08x (%08x)\n", addr + i*4, addr);
          do_in_intrp=1;
        }
      }
      if(do_in_intrp) {
        rs1[i-1]=CCREG;
        rs2[i-1]=rt1[i-1]=rt2[i-1]=0;
        ba[i-1]=-1;
        itype[i-1]=INTCALL;
        done=2;
        i--; // don't compile the DS
      }
    }
    /* Is this the end of the block? */
    if (i > 0 && is_ujump(i-1)) {
      if(rt1[i-1]==0) { // Continue past subroutine call (JAL)
        done=2;
      }
      else {
        if(stop_after_jal) done=1;
        // Stop on BREAK
        if((source[i+1]&0xfc00003f)==0x0d) done=1;
      }
      // Don't recompile stuff that's already compiled
      if(check_addr(start+i*4+4)) done=1;
      // Don't get too close to the limit
      if(i>MAXBLOCK/2) done=1;
    }
    if(itype[i]==SYSCALL&&stop_after_jal) done=1;
    if(itype[i]==HLECALL||itype[i]==INTCALL) done=2;
    if(done==2) {
      // Does the block continue due to a branch?
      for(j=i-1;j>=0;j--)
      {
        if(ba[j]==start+i*4) done=j=0; // Branch into delay slot
        if(ba[j]==start+i*4+4) done=j=0;
        if(ba[j]==start+i*4+8) done=j=0;
      }
    }
    //assert(i<MAXBLOCK-1);
    if(start+i*4==pagelimit-4) done=1;
    assert(start+i*4<pagelimit);
    if (i==MAXBLOCK-1) done=1;
    // Stop if we're compiling junk
    if(itype[i]==NI&&opcode[i]==0x11) {
      done=stop_after_jal=1;
      SysPrintf("Disabled speculative precompilation\n");
    }
  }
  slen=i;
  if(itype[i-1]==UJUMP||itype[i-1]==CJUMP||itype[i-1]==SJUMP||itype[i-1]==RJUMP) {
    if(start+i*4==pagelimit) {
      itype[i-1]=SPAN;
    }
  }
  assert(slen>0);

  /* Pass 2 - Register dependencies and branch targets */

  unneeded_registers(0,slen-1,0);

  /* Pass 3 - Register allocation */

  struct regstat current; // Current register allocations/status
  current.dirty=0;
  current.u=unneeded_reg[0];
  clear_all_regs(current.regmap);
  alloc_reg(&current,0,CCREG);
  dirty_reg(&current,CCREG);
  current.isconst=0;
  current.wasconst=0;
  current.waswritten=0;
  int ds=0;
  int cc=0;
  int hr=-1;

  if((u_int)addr&1) {
    // First instruction is delay slot
    cc=-1;
    bt[1]=1;
    ds=1;
    unneeded_reg[0]=1;
    current.regmap[HOST_BTREG]=BTREG;
  }

  for(i=0;i<slen;i++)
  {
    if(bt[i])
    {
      int hr;
      for(hr=0;hr<HOST_REGS;hr++)
      {
        // Is this really necessary?
        if(current.regmap[hr]==0) current.regmap[hr]=-1;
      }
      current.isconst=0;
      current.waswritten=0;
    }

    memcpy(regmap_pre[i],current.regmap,sizeof(current.regmap));
    regs[i].wasconst=current.isconst;
    regs[i].wasdirty=current.dirty;
    regs[i].loadedconst=0;
    if(itype[i]!=UJUMP&&itype[i]!=CJUMP&&itype[i]!=SJUMP&&itype[i]!=RJUMP) {
      if(i+1<slen) {
        current.u=unneeded_reg[i+1]&~((1LL<<rs1[i])|(1LL<<rs2[i]));
        current.u|=1;
      } else {
        current.u=1;
      }
    } else {
      if(i+1<slen) {
        current.u=branch_unneeded_reg[i]&~((1LL<<rs1[i+1])|(1LL<<rs2[i+1]));
        current.u&=~((1LL<<rs1[i])|(1LL<<rs2[i]));
        current.u|=1;
      } else { SysPrintf("oops, branch at end of block with no delay slot\n");abort(); }
    }
    is_ds[i]=ds;
    if(ds) {
      ds=0; // Skip delay slot, already allocated as part of branch
      // ...but we need to alloc it in case something jumps here
      if(i+1<slen) {
        current.u=branch_unneeded_reg[i-1]&unneeded_reg[i+1];
      }else{
        current.u=branch_unneeded_reg[i-1];
      }
      current.u&=~((1LL<<rs1[i])|(1LL<<rs2[i]));
      current.u|=1;
      struct regstat temp;
      memcpy(&temp,&current,sizeof(current));
      temp.wasdirty=temp.dirty;
      // TODO: Take into account unconditional branches, as below
      delayslot_alloc(&temp,i);
      memcpy(regs[i].regmap,temp.regmap,sizeof(temp.regmap));
      regs[i].wasdirty=temp.wasdirty;
      regs[i].dirty=temp.dirty;
      regs[i].isconst=0;
      regs[i].wasconst=0;
      current.isconst=0;
      // Create entry (branch target) regmap
      for(hr=0;hr<HOST_REGS;hr++)
      {
        int r=temp.regmap[hr];
        if(r>=0) {
          if(r!=regmap_pre[i][hr]) {
            regs[i].regmap_entry[hr]=-1;
          }
          else
          {
              assert(r < 64);
              if((current.u>>r)&1) {
                regs[i].regmap_entry[hr]=-1;
                regs[i].regmap[hr]=-1;
                //Don't clear regs in the delay slot as the branch might need them
                //current.regmap[hr]=-1;
              }else
                regs[i].regmap_entry[hr]=r;
          }
        } else {
          // First instruction expects CCREG to be allocated
          if(i==0&&hr==HOST_CCREG)
            regs[i].regmap_entry[hr]=CCREG;
          else
            regs[i].regmap_entry[hr]=-1;
        }
      }
    }
    else { // Not delay slot
      switch(itype[i]) {
        case UJUMP:
          //current.isconst=0; // DEBUG
          //current.wasconst=0; // DEBUG
          //regs[i].wasconst=0; // DEBUG
          clear_const(&current,rt1[i]);
          alloc_cc(&current,i);
          dirty_reg(&current,CCREG);
          if (rt1[i]==31) {
            alloc_reg(&current,i,31);
            dirty_reg(&current,31);
            //assert(rs1[i+1]!=31&&rs2[i+1]!=31);
            //assert(rt1[i+1]!=rt1[i]);
            #ifdef REG_PREFETCH
            alloc_reg(&current,i,PTEMP);
            #endif
          }
          ooo[i]=1;
          delayslot_alloc(&current,i+1);
          //current.isconst=0; // DEBUG
          ds=1;
          //printf("i=%d, isconst=%x\n",i,current.isconst);
          break;
        case RJUMP:
          //current.isconst=0;
          //current.wasconst=0;
          //regs[i].wasconst=0;
          clear_const(&current,rs1[i]);
          clear_const(&current,rt1[i]);
          alloc_cc(&current,i);
          dirty_reg(&current,CCREG);
          if(rs1[i]!=rt1[i+1]&&rs1[i]!=rt2[i+1]) {
            alloc_reg(&current,i,rs1[i]);
            if (rt1[i]!=0) {
              alloc_reg(&current,i,rt1[i]);
              dirty_reg(&current,rt1[i]);
              assert(rs1[i+1]!=rt1[i]&&rs2[i+1]!=rt1[i]);
              assert(rt1[i+1]!=rt1[i]);
              #ifdef REG_PREFETCH
              alloc_reg(&current,i,PTEMP);
              #endif
            }
            #ifdef USE_MINI_HT
            if(rs1[i]==31) { // JALR
              alloc_reg(&current,i,RHASH);
              alloc_reg(&current,i,RHTBL);
            }
            #endif
            delayslot_alloc(&current,i+1);
          } else {
            // The delay slot overwrites our source register,
            // allocate a temporary register to hold the old value.
            current.isconst=0;
            current.wasconst=0;
            regs[i].wasconst=0;
            delayslot_alloc(&current,i+1);
            current.isconst=0;
            alloc_reg(&current,i,RTEMP);
          }
          //current.isconst=0; // DEBUG
          ooo[i]=1;
          ds=1;
          break;
        case CJUMP:
          //current.isconst=0;
          //current.wasconst=0;
          //regs[i].wasconst=0;
          clear_const(&current,rs1[i]);
          clear_const(&current,rs2[i]);
          if((opcode[i]&0x3E)==4) // BEQ/BNE
          {
            alloc_cc(&current,i);
            dirty_reg(&current,CCREG);
            if(rs1[i]) alloc_reg(&current,i,rs1[i]);
            if(rs2[i]) alloc_reg(&current,i,rs2[i]);
            if((rs1[i]&&(rs1[i]==rt1[i+1]||rs1[i]==rt2[i+1]))||
               (rs2[i]&&(rs2[i]==rt1[i+1]||rs2[i]==rt2[i+1]))) {
              // The delay slot overwrites one of our conditions.
              // Allocate the branch condition registers instead.
              current.isconst=0;
              current.wasconst=0;
              regs[i].wasconst=0;
              if(rs1[i]) alloc_reg(&current,i,rs1[i]);
              if(rs2[i]) alloc_reg(&current,i,rs2[i]);
            }
            else
            {
              ooo[i]=1;
              delayslot_alloc(&current,i+1);
            }
          }
          else
          if((opcode[i]&0x3E)==6) // BLEZ/BGTZ
          {
            alloc_cc(&current,i);
            dirty_reg(&current,CCREG);
            alloc_reg(&current,i,rs1[i]);
            if(rs1[i]&&(rs1[i]==rt1[i+1]||rs1[i]==rt2[i+1])) {
              // The delay slot overwrites one of our conditions.
              // Allocate the branch condition registers instead.
              current.isconst=0;
              current.wasconst=0;
              regs[i].wasconst=0;
              if(rs1[i]) alloc_reg(&current,i,rs1[i]);
            }
            else
            {
              ooo[i]=1;
              delayslot_alloc(&current,i+1);
            }
          }
          else
          // Don't alloc the delay slot yet because we might not execute it
          if((opcode[i]&0x3E)==0x14) // BEQL/BNEL
          {
            current.isconst=0;
            current.wasconst=0;
            regs[i].wasconst=0;
            alloc_cc(&current,i);
            dirty_reg(&current,CCREG);
            alloc_reg(&current,i,rs1[i]);
            alloc_reg(&current,i,rs2[i]);
          }
          else
          if((opcode[i]&0x3E)==0x16) // BLEZL/BGTZL
          {
            current.isconst=0;
            current.wasconst=0;
            regs[i].wasconst=0;
            alloc_cc(&current,i);
            dirty_reg(&current,CCREG);
            alloc_reg(&current,i,rs1[i]);
          }
          ds=1;
          //current.isconst=0;
          break;
        case SJUMP:
          //current.isconst=0;
          //current.wasconst=0;
          //regs[i].wasconst=0;
          clear_const(&current,rs1[i]);
          clear_const(&current,rt1[i]);
          //if((opcode2[i]&0x1E)==0x0) // BLTZ/BGEZ
          if((opcode2[i]&0x0E)==0x0) // BLTZ/BGEZ
          {
            alloc_cc(&current,i);
            dirty_reg(&current,CCREG);
            alloc_reg(&current,i,rs1[i]);
            if (rt1[i]==31) { // BLTZAL/BGEZAL
              alloc_reg(&current,i,31);
              dirty_reg(&current,31);
              //#ifdef REG_PREFETCH
              //alloc_reg(&current,i,PTEMP);
              //#endif
            }
            if((rs1[i]&&(rs1[i]==rt1[i+1]||rs1[i]==rt2[i+1])) // The delay slot overwrites the branch condition.
               ||(rt1[i]==31&&(rs1[i+1]==31||rs2[i+1]==31||rt1[i+1]==31||rt2[i+1]==31))) { // DS touches $ra
              // Allocate the branch condition registers instead.
              current.isconst=0;
              current.wasconst=0;
              regs[i].wasconst=0;
              if(rs1[i]) alloc_reg(&current,i,rs1[i]);
            }
            else
            {
              ooo[i]=1;
              delayslot_alloc(&current,i+1);
            }
          }
          else
          // Don't alloc the delay slot yet because we might not execute it
          if((opcode2[i]&0x1E)==0x2) // BLTZL/BGEZL
          {
            current.isconst=0;
            current.wasconst=0;
            regs[i].wasconst=0;
            alloc_cc(&current,i);
            dirty_reg(&current,CCREG);
            alloc_reg(&current,i,rs1[i]);
          }
          ds=1;
          //current.isconst=0;
          break;
        case IMM16:
          imm16_alloc(&current,i);
          break;
        case LOAD:
        case LOADLR:
          load_alloc(&current,i);
          break;
        case STORE:
        case STORELR:
          store_alloc(&current,i);
          break;
        case ALU:
          alu_alloc(&current,i);
          break;
        case SHIFT:
          shift_alloc(&current,i);
          break;
        case MULTDIV:
          multdiv_alloc(&current,i);
          break;
        case SHIFTIMM:
          shiftimm_alloc(&current,i);
          break;
        case MOV:
          mov_alloc(&current,i);
          break;
        case COP0:
          cop0_alloc(&current,i);
          break;
        case COP1:
          break;
        case COP2:
          cop2_alloc(&current,i);
          break;
        case C1LS:
          c1ls_alloc(&current,i);
          break;
        case C2LS:
          c2ls_alloc(&current,i);
          break;
        case C2OP:
          c2op_alloc(&current,i);
          break;
        case SYSCALL:
        case HLECALL:
        case INTCALL:
          syscall_alloc(&current,i);
          break;
        case SPAN:
          pagespan_alloc(&current,i);
          break;
      }

      // Create entry (branch target) regmap
      for(hr=0;hr<HOST_REGS;hr++)
      {
        int r,or;
        r=current.regmap[hr];
        if(r>=0) {
          if(r!=regmap_pre[i][hr]) {
            // TODO: delay slot (?)
            or=get_reg(regmap_pre[i],r); // Get old mapping for this register
            if(or<0||(r&63)>=TEMPREG){
              regs[i].regmap_entry[hr]=-1;
            }
            else
            {
              // Just move it to a different register
              regs[i].regmap_entry[hr]=r;
              // If it was dirty before, it's still dirty
              if((regs[i].wasdirty>>or)&1) dirty_reg(&current,r&63);
            }
          }
          else
          {
            // Unneeded
            if(r==0){
              regs[i].regmap_entry[hr]=0;
            }
            else
            {
              assert(r<64);
              if((current.u>>r)&1) {
                regs[i].regmap_entry[hr]=-1;
                //regs[i].regmap[hr]=-1;
                current.regmap[hr]=-1;
              }else
                regs[i].regmap_entry[hr]=r;
            }
          }
        } else {
          // Branches expect CCREG to be allocated at the target
          if(regmap_pre[i][hr]==CCREG)
            regs[i].regmap_entry[hr]=CCREG;
          else
            regs[i].regmap_entry[hr]=-1;
        }
      }
      memcpy(regs[i].regmap,current.regmap,sizeof(current.regmap));
    }

    if(i>0&&(itype[i-1]==STORE||itype[i-1]==STORELR||(itype[i-1]==C2LS&&opcode[i-1]==0x3a))&&(u_int)imm[i-1]<0x800)
      current.waswritten|=1<<rs1[i-1];
    current.waswritten&=~(1<<rt1[i]);
    current.waswritten&=~(1<<rt2[i]);
    if((itype[i]==STORE||itype[i]==STORELR||(itype[i]==C2LS&&opcode[i]==0x3a))&&(u_int)imm[i]>=0x800)
      current.waswritten&=~(1<<rs1[i]);

    /* Branch post-alloc */
    if(i>0)
    {
      current.wasdirty=current.dirty;
      switch(itype[i-1]) {
        case UJUMP:
          memcpy(&branch_regs[i-1],&current,sizeof(current));
          branch_regs[i-1].isconst=0;
          branch_regs[i-1].wasconst=0;
          branch_regs[i-1].u=branch_unneeded_reg[i-1]&~((1LL<<rs1[i-1])|(1LL<<rs2[i-1]));
          alloc_cc(&branch_regs[i-1],i-1);
          dirty_reg(&branch_regs[i-1],CCREG);
          if(rt1[i-1]==31) { // JAL
            alloc_reg(&branch_regs[i-1],i-1,31);
            dirty_reg(&branch_regs[i-1],31);
          }
          memcpy(&branch_regs[i-1].regmap_entry,&branch_regs[i-1].regmap,sizeof(current.regmap));
          memcpy(constmap[i],constmap[i-1],sizeof(constmap[i]));
          break;
        case RJUMP:
          memcpy(&branch_regs[i-1],&current,sizeof(current));
          branch_regs[i-1].isconst=0;
          branch_regs[i-1].wasconst=0;
          branch_regs[i-1].u=branch_unneeded_reg[i-1]&~((1LL<<rs1[i-1])|(1LL<<rs2[i-1]));
          alloc_cc(&branch_regs[i-1],i-1);
          dirty_reg(&branch_regs[i-1],CCREG);
          alloc_reg(&branch_regs[i-1],i-1,rs1[i-1]);
          if(rt1[i-1]!=0) { // JALR
            alloc_reg(&branch_regs[i-1],i-1,rt1[i-1]);
            dirty_reg(&branch_regs[i-1],rt1[i-1]);
          }
          #ifdef USE_MINI_HT
          if(rs1[i-1]==31) { // JALR
            alloc_reg(&branch_regs[i-1],i-1,RHASH);
            alloc_reg(&branch_regs[i-1],i-1,RHTBL);
          }
          #endif
          memcpy(&branch_regs[i-1].regmap_entry,&branch_regs[i-1].regmap,sizeof(current.regmap));
          memcpy(constmap[i],constmap[i-1],sizeof(constmap[i]));
          break;
        case CJUMP:
          if((opcode[i-1]&0x3E)==4) // BEQ/BNE
          {
            alloc_cc(&current,i-1);
            dirty_reg(&current,CCREG);
            if((rs1[i-1]&&(rs1[i-1]==rt1[i]||rs1[i-1]==rt2[i]))||
               (rs2[i-1]&&(rs2[i-1]==rt1[i]||rs2[i-1]==rt2[i]))) {
              // The delay slot overwrote one of our conditions
              // Delay slot goes after the test (in order)
              current.u=branch_unneeded_reg[i-1]&~((1LL<<rs1[i])|(1LL<<rs2[i]));
              current.u|=1;
              delayslot_alloc(&current,i);
              current.isconst=0;
            }
            else
            {
              current.u=branch_unneeded_reg[i-1]&~((1LL<<rs1[i-1])|(1LL<<rs2[i-1]));
              // Alloc the branch condition registers
              if(rs1[i-1]) alloc_reg(&current,i-1,rs1[i-1]);
              if(rs2[i-1]) alloc_reg(&current,i-1,rs2[i-1]);
            }
            memcpy(&branch_regs[i-1],&current,sizeof(current));
            branch_regs[i-1].isconst=0;
            branch_regs[i-1].wasconst=0;
            memcpy(&branch_regs[i-1].regmap_entry,&current.regmap,sizeof(current.regmap));
            memcpy(constmap[i],constmap[i-1],sizeof(constmap[i]));
          }
          else
          if((opcode[i-1]&0x3E)==6) // BLEZ/BGTZ
          {
            alloc_cc(&current,i-1);
            dirty_reg(&current,CCREG);
            if(rs1[i-1]==rt1[i]||rs1[i-1]==rt2[i]) {
              // The delay slot overwrote the branch condition
              // Delay slot goes after the test (in order)
              current.u=branch_unneeded_reg[i-1]&~((1LL<<rs1[i])|(1LL<<rs2[i]));
              current.u|=1;
              delayslot_alloc(&current,i);
              current.isconst=0;
            }
            else
            {
              current.u=branch_unneeded_reg[i-1]&~(1LL<<rs1[i-1]);
              // Alloc the branch condition register
              alloc_reg(&current,i-1,rs1[i-1]);
            }
            memcpy(&branch_regs[i-1],&current,sizeof(current));
            branch_regs[i-1].isconst=0;
            branch_regs[i-1].wasconst=0;
            memcpy(&branch_regs[i-1].regmap_entry,&current.regmap,sizeof(current.regmap));
            memcpy(constmap[i],constmap[i-1],sizeof(constmap[i]));
          }
          else
          // Alloc the delay slot in case the branch is taken
          if((opcode[i-1]&0x3E)==0x14) // BEQL/BNEL
          {
            memcpy(&branch_regs[i-1],&current,sizeof(current));
            branch_regs[i-1].u=(branch_unneeded_reg[i-1]&~((1LL<<rs1[i])|(1LL<<rs2[i])|(1LL<<rt1[i])|(1LL<<rt2[i])))|1;
            alloc_cc(&branch_regs[i-1],i);
            dirty_reg(&branch_regs[i-1],CCREG);
            delayslot_alloc(&branch_regs[i-1],i);
            branch_regs[i-1].isconst=0;
            alloc_reg(&current,i,CCREG); // Not taken path
            dirty_reg(&current,CCREG);
            memcpy(&branch_regs[i-1].regmap_entry,&branch_regs[i-1].regmap,sizeof(current.regmap));
          }
          else
          if((opcode[i-1]&0x3E)==0x16) // BLEZL/BGTZL
          {
            memcpy(&branch_regs[i-1],&current,sizeof(current));
            branch_regs[i-1].u=(branch_unneeded_reg[i-1]&~((1LL<<rs1[i])|(1LL<<rs2[i])|(1LL<<rt1[i])|(1LL<<rt2[i])))|1;
            alloc_cc(&branch_regs[i-1],i);
            dirty_reg(&branch_regs[i-1],CCREG);
            delayslot_alloc(&branch_regs[i-1],i);
            branch_regs[i-1].isconst=0;
            alloc_reg(&current,i,CCREG); // Not taken path
            dirty_reg(&current,CCREG);
            memcpy(&branch_regs[i-1].regmap_entry,&branch_regs[i-1].regmap,sizeof(current.regmap));
          }
          break;
        case SJUMP:
          //if((opcode2[i-1]&0x1E)==0) // BLTZ/BGEZ
          if((opcode2[i-1]&0x0E)==0) // BLTZ/BGEZ
          {
            alloc_cc(&current,i-1);
            dirty_reg(&current,CCREG);
            if(rs1[i-1]==rt1[i]||rs1[i-1]==rt2[i]) {
              // The delay slot overwrote the branch condition
              // Delay slot goes after the test (in order)
              current.u=branch_unneeded_reg[i-1]&~((1LL<<rs1[i])|(1LL<<rs2[i]));
              current.u|=1;
              delayslot_alloc(&current,i);
              current.isconst=0;
            }
            else
            {
              current.u=branch_unneeded_reg[i-1]&~(1LL<<rs1[i-1]);
              // Alloc the branch condition register
              alloc_reg(&current,i-1,rs1[i-1]);
            }
            memcpy(&branch_regs[i-1],&current,sizeof(current));
            branch_regs[i-1].isconst=0;
            branch_regs[i-1].wasconst=0;
            memcpy(&branch_regs[i-1].regmap_entry,&current.regmap,sizeof(current.regmap));
            memcpy(constmap[i],constmap[i-1],sizeof(constmap[i]));
          }
          else
          // Alloc the delay slot in case the branch is taken
          if((opcode2[i-1]&0x1E)==2) // BLTZL/BGEZL
          {
            memcpy(&branch_regs[i-1],&current,sizeof(current));
            branch_regs[i-1].u=(branch_unneeded_reg[i-1]&~((1LL<<rs1[i])|(1LL<<rs2[i])|(1LL<<rt1[i])|(1LL<<rt2[i])))|1;
            alloc_cc(&branch_regs[i-1],i);
            dirty_reg(&branch_regs[i-1],CCREG);
            delayslot_alloc(&branch_regs[i-1],i);
            branch_regs[i-1].isconst=0;
            alloc_reg(&current,i,CCREG); // Not taken path
            dirty_reg(&current,CCREG);
            memcpy(&branch_regs[i-1].regmap_entry,&branch_regs[i-1].regmap,sizeof(current.regmap));
          }
          // FIXME: BLTZAL/BGEZAL
          if(opcode2[i-1]&0x10) { // BxxZAL
            alloc_reg(&branch_regs[i-1],i-1,31);
            dirty_reg(&branch_regs[i-1],31);
          }
          break;
      }

      if (is_ujump(i-1))
      {
        if(rt1[i-1]==31) // JAL/JALR
        {
          // Subroutine call will return here, don't alloc any registers
          current.dirty=0;
          clear_all_regs(current.regmap);
          alloc_reg(&current,i,CCREG);
          dirty_reg(&current,CCREG);
        }
        else if(i+1<slen)
        {
          // Internal branch will jump here, match registers to caller
          current.dirty=0;
          clear_all_regs(current.regmap);
          alloc_reg(&current,i,CCREG);
          dirty_reg(&current,CCREG);
          for(j=i-1;j>=0;j--)
          {
            if(ba[j]==start+i*4+4) {
              memcpy(current.regmap,branch_regs[j].regmap,sizeof(current.regmap));
              current.dirty=branch_regs[j].dirty;
              break;
            }
          }
          while(j>=0) {
            if(ba[j]==start+i*4+4) {
              for(hr=0;hr<HOST_REGS;hr++) {
                if(current.regmap[hr]!=branch_regs[j].regmap[hr]) {
                  current.regmap[hr]=-1;
                }
                current.dirty&=branch_regs[j].dirty;
              }
            }
            j--;
          }
        }
      }
    }

    // Count cycles in between branches
    ccadj[i]=cc;
    if(i>0&&(itype[i-1]==RJUMP||itype[i-1]==UJUMP||itype[i-1]==CJUMP||itype[i-1]==SJUMP||itype[i]==SYSCALL||itype[i]==HLECALL))
    {
      cc=0;
    }
#if !defined(DRC_DBG)
    else if(itype[i]==C2OP&&gte_cycletab[source[i]&0x3f]>2)
    {
      // this should really be removed since the real stalls have been implemented,
      // but doing so causes sizeable perf regression against the older version
      u_int gtec = gte_cycletab[source[i] & 0x3f];
      cc += HACK_ENABLED(NDHACK_GTE_NO_STALL) ? gtec/2 : 2;
    }
    else if(i>1&&itype[i]==STORE&&itype[i-1]==STORE&&itype[i-2]==STORE&&!bt[i])
    {
      cc+=4;
    }
    else if(itype[i]==C2LS)
    {
      // same as with C2OP
      cc += HACK_ENABLED(NDHACK_GTE_NO_STALL) ? 4 : 2;
    }
#endif
    else
    {
      cc++;
    }

    if(!is_ds[i]) {
      regs[i].dirty=current.dirty;
      regs[i].isconst=current.isconst;
      memcpy(constmap[i],current_constmap,sizeof(constmap[i]));
    }
    for(hr=0;hr<HOST_REGS;hr++) {
      if(hr!=EXCLUDE_REG&&regs[i].regmap[hr]>=0) {
        if(regmap_pre[i][hr]!=regs[i].regmap[hr]) {
          regs[i].wasconst&=~(1<<hr);
        }
      }
    }
    if(current.regmap[HOST_BTREG]==BTREG) current.regmap[HOST_BTREG]=-1;
    regs[i].waswritten=current.waswritten;
  }

  /* Pass 4 - Cull unused host registers */

  uint64_t nr=0;

  for (i=slen-1;i>=0;i--)
  {
    int hr;
    if(itype[i]==RJUMP||itype[i]==UJUMP||itype[i]==CJUMP||itype[i]==SJUMP)
    {
      if(ba[i]<start || ba[i]>=(start+slen*4))
      {
        // Branch out of this block, don't need anything
        nr=0;
      }
      else
      {
        // Internal branch
        // Need whatever matches the target
        nr=0;
        int t=(ba[i]-start)>>2;
        for(hr=0;hr<HOST_REGS;hr++)
        {
          if(regs[i].regmap_entry[hr]>=0) {
            if(regs[i].regmap_entry[hr]==regs[t].regmap_entry[hr]) nr|=1<<hr;
          }
        }
      }
      // Conditional branch may need registers for following instructions
      if (!is_ujump(i))
      {
        if(i<slen-2) {
          nr|=needed_reg[i+2];
          for(hr=0;hr<HOST_REGS;hr++)
          {
            if(regmap_pre[i+2][hr]>=0&&get_reg(regs[i+2].regmap_entry,regmap_pre[i+2][hr])<0) nr&=~(1<<hr);
            //if((regmap_entry[i+2][hr])>=0) if(!((nr>>hr)&1)) printf("%x-bogus(%d=%d)\n",start+i*4,hr,regmap_entry[i+2][hr]);
          }
        }
      }
      // Don't need stuff which is overwritten
      //if(regs[i].regmap[hr]!=regmap_pre[i][hr]) nr&=~(1<<hr);
      //if(regs[i].regmap[hr]<0) nr&=~(1<<hr);
      // Merge in delay slot
      for(hr=0;hr<HOST_REGS;hr++)
      {
        if(!likely[i]) {
          // These are overwritten unless the branch is "likely"
          // and the delay slot is nullified if not taken
          if(rt1[i+1]&&rt1[i+1]==(regs[i].regmap[hr]&63)) nr&=~(1<<hr);
          if(rt2[i+1]&&rt2[i+1]==(regs[i].regmap[hr]&63)) nr&=~(1<<hr);
        }
        if(rs1[i+1]==regmap_pre[i][hr]) nr|=1<<hr;
        if(rs2[i+1]==regmap_pre[i][hr]) nr|=1<<hr;
        if(rs1[i+1]==regs[i].regmap_entry[hr]) nr|=1<<hr;
        if(rs2[i+1]==regs[i].regmap_entry[hr]) nr|=1<<hr;
        if(itype[i+1]==STORE || itype[i+1]==STORELR || (opcode[i+1]&0x3b)==0x39 || (opcode[i+1]&0x3b)==0x3a) {
          if(regmap_pre[i][hr]==INVCP) nr|=1<<hr;
          if(regs[i].regmap_entry[hr]==INVCP) nr|=1<<hr;
        }
      }
    }
    else if(itype[i]==SYSCALL||itype[i]==HLECALL||itype[i]==INTCALL)
    {
      // SYSCALL instruction (software interrupt)
      nr=0;
    }
    else if(itype[i]==COP0 && (source[i]&0x3f)==0x18)
    {
      // ERET instruction (return from interrupt)
      nr=0;
    }
    else // Non-branch
    {
      if(i<slen-1) {
        for(hr=0;hr<HOST_REGS;hr++) {
          if(regmap_pre[i+1][hr]>=0&&get_reg(regs[i+1].regmap_entry,regmap_pre[i+1][hr])<0) nr&=~(1<<hr);
          if(regs[i].regmap[hr]!=regmap_pre[i+1][hr]) nr&=~(1<<hr);
          if(regs[i].regmap[hr]!=regmap_pre[i][hr]) nr&=~(1<<hr);
          if(regs[i].regmap[hr]<0) nr&=~(1<<hr);
        }
      }
    }
    for(hr=0;hr<HOST_REGS;hr++)
    {
      // Overwritten registers are not needed
      if(rt1[i]&&rt1[i]==(regs[i].regmap[hr]&63)) nr&=~(1<<hr);
      if(rt2[i]&&rt2[i]==(regs[i].regmap[hr]&63)) nr&=~(1<<hr);
      if(FTEMP==(regs[i].regmap[hr]&63)) nr&=~(1<<hr);
      // Source registers are needed
      if(rs1[i]==regmap_pre[i][hr]) nr|=1<<hr;
      if(rs2[i]==regmap_pre[i][hr]) nr|=1<<hr;
      if(rs1[i]==regs[i].regmap_entry[hr]) nr|=1<<hr;
      if(rs2[i]==regs[i].regmap_entry[hr]) nr|=1<<hr;
      if(itype[i]==STORE || itype[i]==STORELR || (opcode[i]&0x3b)==0x39 || (opcode[i]&0x3b)==0x3a) {
        if(regmap_pre[i][hr]==INVCP) nr|=1<<hr;
        if(regs[i].regmap_entry[hr]==INVCP) nr|=1<<hr;
      }
      // Don't store a register immediately after writing it,
      // may prevent dual-issue.
      // But do so if this is a branch target, otherwise we
      // might have to load the register before the branch.
      if(i>0&&!bt[i]&&((regs[i].wasdirty>>hr)&1)) {
        if((regmap_pre[i][hr]>0&&!((unneeded_reg[i]>>regmap_pre[i][hr])&1))) {
          if(rt1[i-1]==(regmap_pre[i][hr]&63)) nr|=1<<hr;
          if(rt2[i-1]==(regmap_pre[i][hr]&63)) nr|=1<<hr;
        }
        if((regs[i].regmap_entry[hr]>0&&!((unneeded_reg[i]>>regs[i].regmap_entry[hr])&1))) {
          if(rt1[i-1]==(regs[i].regmap_entry[hr]&63)) nr|=1<<hr;
          if(rt2[i-1]==(regs[i].regmap_entry[hr]&63)) nr|=1<<hr;
        }
      }
    }
    // Cycle count is needed at branches.  Assume it is needed at the target too.
    if(i==0||bt[i]||itype[i]==CJUMP||itype[i]==SPAN) {
      if(regmap_pre[i][HOST_CCREG]==CCREG) nr|=1<<HOST_CCREG;
      if(regs[i].regmap_entry[HOST_CCREG]==CCREG) nr|=1<<HOST_CCREG;
    }
    // Save it
    needed_reg[i]=nr;

    // Deallocate unneeded registers
    for(hr=0;hr<HOST_REGS;hr++)
    {
      if(!((nr>>hr)&1)) {
        if(regs[i].regmap_entry[hr]!=CCREG) regs[i].regmap_entry[hr]=-1;
        if((regs[i].regmap[hr]&63)!=rs1[i] && (regs[i].regmap[hr]&63)!=rs2[i] &&
           (regs[i].regmap[hr]&63)!=rt1[i] && (regs[i].regmap[hr]&63)!=rt2[i] &&
           (regs[i].regmap[hr]&63)!=PTEMP && (regs[i].regmap[hr]&63)!=CCREG)
        {
          if (!is_ujump(i))
          {
            if(likely[i]) {
              regs[i].regmap[hr]=-1;
              regs[i].isconst&=~(1<<hr);
              if(i<slen-2) {
                regmap_pre[i+2][hr]=-1;
                regs[i+2].wasconst&=~(1<<hr);
              }
            }
          }
        }
        if(itype[i]==RJUMP||itype[i]==UJUMP||itype[i]==CJUMP||itype[i]==SJUMP)
        {
          int map=0,temp=0;
          if(itype[i+1]==STORE || itype[i+1]==STORELR ||
             (opcode[i+1]&0x3b)==0x39 || (opcode[i+1]&0x3b)==0x3a) { // SWC1/SDC1 || SWC2/SDC2
            map=INVCP;
          }
          if(itype[i+1]==LOADLR || itype[i+1]==STORELR ||
             itype[i+1]==C1LS || itype[i+1]==C2LS)
            temp=FTEMP;
          if((regs[i].regmap[hr]&63)!=rs1[i] && (regs[i].regmap[hr]&63)!=rs2[i] &&
             (regs[i].regmap[hr]&63)!=rt1[i] && (regs[i].regmap[hr]&63)!=rt2[i] &&
             (regs[i].regmap[hr]&63)!=rt1[i+1] && (regs[i].regmap[hr]&63)!=rt2[i+1] &&
             regs[i].regmap[hr]!=rs1[i+1] && regs[i].regmap[hr]!=rs2[i+1] &&
             (regs[i].regmap[hr]&63)!=temp && regs[i].regmap[hr]!=PTEMP &&
             regs[i].regmap[hr]!=RHASH && regs[i].regmap[hr]!=RHTBL &&
             regs[i].regmap[hr]!=RTEMP && regs[i].regmap[hr]!=CCREG &&
             regs[i].regmap[hr]!=map )
          {
            regs[i].regmap[hr]=-1;
            regs[i].isconst&=~(1<<hr);
            if((branch_regs[i].regmap[hr]&63)!=rs1[i] && (branch_regs[i].regmap[hr]&63)!=rs2[i] &&
               (branch_regs[i].regmap[hr]&63)!=rt1[i] && (branch_regs[i].regmap[hr]&63)!=rt2[i] &&
               (branch_regs[i].regmap[hr]&63)!=rt1[i+1] && (branch_regs[i].regmap[hr]&63)!=rt2[i+1] &&
               branch_regs[i].regmap[hr]!=rs1[i+1] && branch_regs[i].regmap[hr]!=rs2[i+1] &&
               (branch_regs[i].regmap[hr]&63)!=temp && branch_regs[i].regmap[hr]!=PTEMP &&
               branch_regs[i].regmap[hr]!=RHASH && branch_regs[i].regmap[hr]!=RHTBL &&
               branch_regs[i].regmap[hr]!=RTEMP && branch_regs[i].regmap[hr]!=CCREG &&
               branch_regs[i].regmap[hr]!=map)
            {
              branch_regs[i].regmap[hr]=-1;
              branch_regs[i].regmap_entry[hr]=-1;
              if (!is_ujump(i))
              {
                if(!likely[i]&&i<slen-2) {
                  regmap_pre[i+2][hr]=-1;
                  regs[i+2].wasconst&=~(1<<hr);
                }
              }
            }
          }
        }
        else
        {
          // Non-branch
          if(i>0)
          {
            int map=-1,temp=-1;
            if(itype[i]==STORE || itype[i]==STORELR ||
                      (opcode[i]&0x3b)==0x39 || (opcode[i]&0x3b)==0x3a) { // SWC1/SDC1 || SWC2/SDC2
              map=INVCP;
            }
            if(itype[i]==LOADLR || itype[i]==STORELR ||
               itype[i]==C1LS || itype[i]==C2LS)
              temp=FTEMP;
            if((regs[i].regmap[hr]&63)!=rt1[i] && (regs[i].regmap[hr]&63)!=rt2[i] &&
               regs[i].regmap[hr]!=rs1[i] && regs[i].regmap[hr]!=rs2[i] &&
               (regs[i].regmap[hr]&63)!=temp && regs[i].regmap[hr]!=map &&
               (itype[i]!=SPAN||regs[i].regmap[hr]!=CCREG))
            {
              if(i<slen-1&&!is_ds[i]) {
                assert(regs[i].regmap[hr]<64);
                if(regmap_pre[i+1][hr]!=-1 || regs[i].regmap[hr]>0)
                if(regmap_pre[i+1][hr]!=regs[i].regmap[hr])
                {
                  SysPrintf("fail: %x (%d %d!=%d)\n",start+i*4,hr,regmap_pre[i+1][hr],regs[i].regmap[hr]);
                  assert(regmap_pre[i+1][hr]==regs[i].regmap[hr]);
                }
                regmap_pre[i+1][hr]=-1;
                if(regs[i+1].regmap_entry[hr]==CCREG) regs[i+1].regmap_entry[hr]=-1;
                regs[i+1].wasconst&=~(1<<hr);
              }
              regs[i].regmap[hr]=-1;
              regs[i].isconst&=~(1<<hr);
            }
          }
        }
      } // if needed
    } // for hr
  }

  /* Pass 5 - Pre-allocate registers */

  // If a register is allocated during a loop, try to allocate it for the
  // entire loop, if possible.  This avoids loading/storing registers
  // inside of the loop.

  signed char f_regmap[HOST_REGS];
  clear_all_regs(f_regmap);
  for(i=0;i<slen-1;i++)
  {
    if(itype[i]==UJUMP||itype[i]==CJUMP||itype[i]==SJUMP)
    {
      if(ba[i]>=start && ba[i]<(start+i*4))
      if(itype[i+1]==NOP||itype[i+1]==MOV||itype[i+1]==ALU
      ||itype[i+1]==SHIFTIMM||itype[i+1]==IMM16||itype[i+1]==LOAD
      ||itype[i+1]==STORE||itype[i+1]==STORELR||itype[i+1]==C1LS
      ||itype[i+1]==SHIFT||itype[i+1]==COP1
      ||itype[i+1]==COP2||itype[i+1]==C2LS||itype[i+1]==C2OP)
      {
        int t=(ba[i]-start)>>2;
        if(t>0&&(itype[t-1]!=UJUMP&&itype[t-1]!=RJUMP&&itype[t-1]!=CJUMP&&itype[t-1]!=SJUMP)) // loop_preload can't handle jumps into delay slots
        if(t<2||(itype[t-2]!=UJUMP&&itype[t-2]!=RJUMP)||rt1[t-2]!=31) // call/ret assumes no registers allocated
        for(hr=0;hr<HOST_REGS;hr++)
        {
          if(regs[i].regmap[hr]>=0) {
            if(f_regmap[hr]!=regs[i].regmap[hr]) {
              // dealloc old register
              int n;
              for(n=0;n<HOST_REGS;n++)
              {
                if(f_regmap[n]==regs[i].regmap[hr]) {f_regmap[n]=-1;}
              }
              // and alloc new one
              f_regmap[hr]=regs[i].regmap[hr];
            }
          }
          if(branch_regs[i].regmap[hr]>=0) {
            if(f_regmap[hr]!=branch_regs[i].regmap[hr]) {
              // dealloc old register
              int n;
              for(n=0;n<HOST_REGS;n++)
              {
                if(f_regmap[n]==branch_regs[i].regmap[hr]) {f_regmap[n]=-1;}
              }
              // and alloc new one
              f_regmap[hr]=branch_regs[i].regmap[hr];
            }
          }
          if(ooo[i]) {
            if(count_free_regs(regs[i].regmap)<=minimum_free_regs[i+1])
              f_regmap[hr]=branch_regs[i].regmap[hr];
          }else{
            if(count_free_regs(branch_regs[i].regmap)<=minimum_free_regs[i+1])
              f_regmap[hr]=branch_regs[i].regmap[hr];
          }
          // Avoid dirty->clean transition
          #ifdef DESTRUCTIVE_WRITEBACK
          if(t>0) if(get_reg(regmap_pre[t],f_regmap[hr])>=0) if((regs[t].wasdirty>>get_reg(regmap_pre[t],f_regmap[hr]))&1) f_regmap[hr]=-1;
          #endif
          // This check is only strictly required in the DESTRUCTIVE_WRITEBACK
          // case above, however it's always a good idea.  We can't hoist the
          // load if the register was already allocated, so there's no point
          // wasting time analyzing most of these cases.  It only "succeeds"
          // when the mapping was different and the load can be replaced with
          // a mov, which is of negligible benefit.  So such cases are
          // skipped below.
          if(f_regmap[hr]>0) {
            if(regs[t].regmap[hr]==f_regmap[hr]||(regs[t].regmap_entry[hr]<0&&get_reg(regmap_pre[t],f_regmap[hr])<0)) {
              int r=f_regmap[hr];
              for(j=t;j<=i;j++)
              {
                //printf("Test %x -> %x, %x %d/%d\n",start+i*4,ba[i],start+j*4,hr,r);
                if(r<34&&((unneeded_reg[j]>>r)&1)) break;
                assert(r < 64);
                if(regs[j].regmap[hr]==f_regmap[hr]&&(f_regmap[hr]&63)<TEMPREG) {
                  //printf("Hit %x -> %x, %x %d/%d\n",start+i*4,ba[i],start+j*4,hr,r);
                  int k;
                  if(regs[i].regmap[hr]==-1&&branch_regs[i].regmap[hr]==-1) {
                    if(get_reg(regs[i+2].regmap,f_regmap[hr])>=0) break;
                    if(r>63) {
                      if(get_reg(regs[i].regmap,r&63)<0) break;
                      if(get_reg(branch_regs[i].regmap,r&63)<0) break;
                    }
                    k=i;
                    while(k>1&&regs[k-1].regmap[hr]==-1) {
                      if(count_free_regs(regs[k-1].regmap)<=minimum_free_regs[k-1]) {
                        //printf("no free regs for store %x\n",start+(k-1)*4);
                        break;
                      }
                      if(get_reg(regs[k-1].regmap,f_regmap[hr])>=0) {
                        //printf("no-match due to different register\n");
                        break;
                      }
                      if(itype[k-2]==UJUMP||itype[k-2]==RJUMP||itype[k-2]==CJUMP||itype[k-2]==SJUMP) {
                        //printf("no-match due to branch\n");
                        break;
                      }
                      // call/ret fast path assumes no registers allocated
                      if(k>2&&(itype[k-3]==UJUMP||itype[k-3]==RJUMP)&&rt1[k-3]==31) {
                        break;
                      }
                      assert(r < 64);
                      k--;
                    }
                    if(regs[k-1].regmap[hr]==f_regmap[hr]&&regmap_pre[k][hr]==f_regmap[hr]) {
                      //printf("Extend r%d, %x ->\n",hr,start+k*4);
                      while(k<i) {
                        regs[k].regmap_entry[hr]=f_regmap[hr];
                        regs[k].regmap[hr]=f_regmap[hr];
                        regmap_pre[k+1][hr]=f_regmap[hr];
                        regs[k].wasdirty&=~(1<<hr);
                        regs[k].dirty&=~(1<<hr);
                        regs[k].wasdirty|=(1<<hr)&regs[k-1].dirty;
                        regs[k].dirty|=(1<<hr)&regs[k].wasdirty;
                        regs[k].wasconst&=~(1<<hr);
                        regs[k].isconst&=~(1<<hr);
                        k++;
                      }
                    }
                    else {
                      //printf("Fail Extend r%d, %x ->\n",hr,start+k*4);
                      break;
                    }
                    assert(regs[i-1].regmap[hr]==f_regmap[hr]);
                    if(regs[i-1].regmap[hr]==f_regmap[hr]&&regmap_pre[i][hr]==f_regmap[hr]) {
                      //printf("OK fill %x (r%d)\n",start+i*4,hr);
                      regs[i].regmap_entry[hr]=f_regmap[hr];
                      regs[i].regmap[hr]=f_regmap[hr];
                      regs[i].wasdirty&=~(1<<hr);
                      regs[i].dirty&=~(1<<hr);
                      regs[i].wasdirty|=(1<<hr)&regs[i-1].dirty;
                      regs[i].dirty|=(1<<hr)&regs[i-1].dirty;
                      regs[i].wasconst&=~(1<<hr);
                      regs[i].isconst&=~(1<<hr);
                      branch_regs[i].regmap_entry[hr]=f_regmap[hr];
                      branch_regs[i].wasdirty&=~(1<<hr);
                      branch_regs[i].wasdirty|=(1<<hr)&regs[i].dirty;
                      branch_regs[i].regmap[hr]=f_regmap[hr];
                      branch_regs[i].dirty&=~(1<<hr);
                      branch_regs[i].dirty|=(1<<hr)&regs[i].dirty;
                      branch_regs[i].wasconst&=~(1<<hr);
                      branch_regs[i].isconst&=~(1<<hr);
                      if (!is_ujump(i)) {
                        regmap_pre[i+2][hr]=f_regmap[hr];
                        regs[i+2].wasdirty&=~(1<<hr);
                        regs[i+2].wasdirty|=(1<<hr)&regs[i].dirty;
                      }
                    }
                  }
                  for(k=t;k<j;k++) {
                    // Alloc register clean at beginning of loop,
                    // but may dirty it in pass 6
                    regs[k].regmap_entry[hr]=f_regmap[hr];
                    regs[k].regmap[hr]=f_regmap[hr];
                    regs[k].dirty&=~(1<<hr);
                    regs[k].wasconst&=~(1<<hr);
                    regs[k].isconst&=~(1<<hr);
                    if(itype[k]==UJUMP||itype[k]==RJUMP||itype[k]==CJUMP||itype[k]==SJUMP) {
                      branch_regs[k].regmap_entry[hr]=f_regmap[hr];
                      branch_regs[k].regmap[hr]=f_regmap[hr];
                      branch_regs[k].dirty&=~(1<<hr);
                      branch_regs[k].wasconst&=~(1<<hr);
                      branch_regs[k].isconst&=~(1<<hr);
                      if (!is_ujump(k)) {
                        regmap_pre[k+2][hr]=f_regmap[hr];
                        regs[k+2].wasdirty&=~(1<<hr);
                      }
                    }
                    else
                    {
                      regmap_pre[k+1][hr]=f_regmap[hr];
                      regs[k+1].wasdirty&=~(1<<hr);
                    }
                  }
                  if(regs[j].regmap[hr]==f_regmap[hr])
                    regs[j].regmap_entry[hr]=f_regmap[hr];
                  break;
                }
                if(j==i) break;
                if(regs[j].regmap[hr]>=0)
                  break;
                if(get_reg(regs[j].regmap,f_regmap[hr])>=0) {
                  //printf("no-match due to different register\n");
                  break;
                }
                if (is_ujump(j))
                {
                  // Stop on unconditional branch
                  break;
                }
                if(itype[j]==CJUMP||itype[j]==SJUMP)
                {
                  if(ooo[j]) {
                    if(count_free_regs(regs[j].regmap)<=minimum_free_regs[j+1])
                      break;
                  }else{
                    if(count_free_regs(branch_regs[j].regmap)<=minimum_free_regs[j+1])
                      break;
                  }
                  if(get_reg(branch_regs[j].regmap,f_regmap[hr])>=0) {
                    //printf("no-match due to different register (branch)\n");
                    break;
                  }
                }
                if(count_free_regs(regs[j].regmap)<=minimum_free_regs[j]) {
                  //printf("No free regs for store %x\n",start+j*4);
                  break;
                }
                assert(f_regmap[hr]<64);
              }
            }
          }
        }
      }
    }else{
      // Non branch or undetermined branch target
      for(hr=0;hr<HOST_REGS;hr++)
      {
        if(hr!=EXCLUDE_REG) {
          if(regs[i].regmap[hr]>=0) {
            if(f_regmap[hr]!=regs[i].regmap[hr]) {
              // dealloc old register
              int n;
              for(n=0;n<HOST_REGS;n++)
              {
                if(f_regmap[n]==regs[i].regmap[hr]) {f_regmap[n]=-1;}
              }
              // and alloc new one
              f_regmap[hr]=regs[i].regmap[hr];
            }
          }
        }
      }
      // Try to restore cycle count at branch targets
      if(bt[i]) {
        for(j=i;j<slen-1;j++) {
          if(regs[j].regmap[HOST_CCREG]!=-1) break;
          if(count_free_regs(regs[j].regmap)<=minimum_free_regs[j]) {
            //printf("no free regs for store %x\n",start+j*4);
            break;
          }
        }
        if(regs[j].regmap[HOST_CCREG]==CCREG) {
          int k=i;
          //printf("Extend CC, %x -> %x\n",start+k*4,start+j*4);
          while(k<j) {
            regs[k].regmap_entry[HOST_CCREG]=CCREG;
            regs[k].regmap[HOST_CCREG]=CCREG;
            regmap_pre[k+1][HOST_CCREG]=CCREG;
            regs[k+1].wasdirty|=1<<HOST_CCREG;
            regs[k].dirty|=1<<HOST_CCREG;
            regs[k].wasconst&=~(1<<HOST_CCREG);
            regs[k].isconst&=~(1<<HOST_CCREG);
            k++;
          }
          regs[j].regmap_entry[HOST_CCREG]=CCREG;
        }
        // Work backwards from the branch target
        if(j>i&&f_regmap[HOST_CCREG]==CCREG)
        {
          //printf("Extend backwards\n");
          int k;
          k=i;
          while(regs[k-1].regmap[HOST_CCREG]==-1) {
            if(count_free_regs(regs[k-1].regmap)<=minimum_free_regs[k-1]) {
              //printf("no free regs for store %x\n",start+(k-1)*4);
              break;
            }
            k--;
          }
          if(regs[k-1].regmap[HOST_CCREG]==CCREG) {
            //printf("Extend CC, %x ->\n",start+k*4);
            while(k<=i) {
              regs[k].regmap_entry[HOST_CCREG]=CCREG;
              regs[k].regmap[HOST_CCREG]=CCREG;
              regmap_pre[k+1][HOST_CCREG]=CCREG;
              regs[k+1].wasdirty|=1<<HOST_CCREG;
              regs[k].dirty|=1<<HOST_CCREG;
              regs[k].wasconst&=~(1<<HOST_CCREG);
              regs[k].isconst&=~(1<<HOST_CCREG);
              k++;
            }
          }
          else {
            //printf("Fail Extend CC, %x ->\n",start+k*4);
          }
        }
      }
      if(itype[i]!=STORE&&itype[i]!=STORELR&&itype[i]!=C1LS&&itype[i]!=SHIFT&&
         itype[i]!=NOP&&itype[i]!=MOV&&itype[i]!=ALU&&itype[i]!=SHIFTIMM&&
         itype[i]!=IMM16&&itype[i]!=LOAD&&itype[i]!=COP1)
      {
        memcpy(f_regmap,regs[i].regmap,sizeof(f_regmap));
      }
    }
  }

  // This allocates registers (if possible) one instruction prior
  // to use, which can avoid a load-use penalty on certain CPUs.
  for(i=0;i<slen-1;i++)
  {
    if(!i||(itype[i-1]!=UJUMP&&itype[i-1]!=CJUMP&&itype[i-1]!=SJUMP&&itype[i-1]!=RJUMP))
    {
      if(!bt[i+1])
      {
        if(itype[i]==ALU||itype[i]==MOV||itype[i]==LOAD||itype[i]==SHIFTIMM||itype[i]==IMM16
           ||((itype[i]==COP1||itype[i]==COP2)&&opcode2[i]<3))
        {
          if(rs1[i+1]) {
            if((hr=get_reg(regs[i+1].regmap,rs1[i+1]))>=0)
            {
              if(regs[i].regmap[hr]<0&&regs[i+1].regmap_entry[hr]<0)
              {
                regs[i].regmap[hr]=regs[i+1].regmap[hr];
                regmap_pre[i+1][hr]=regs[i+1].regmap[hr];
                regs[i+1].regmap_entry[hr]=regs[i+1].regmap[hr];
                regs[i].isconst&=~(1<<hr);
                regs[i].isconst|=regs[i+1].isconst&(1<<hr);
                constmap[i][hr]=constmap[i+1][hr];
                regs[i+1].wasdirty&=~(1<<hr);
                regs[i].dirty&=~(1<<hr);
              }
            }
          }
          if(rs2[i+1]) {
            if((hr=get_reg(regs[i+1].regmap,rs2[i+1]))>=0)
            {
              if(regs[i].regmap[hr]<0&&regs[i+1].regmap_entry[hr]<0)
              {
                regs[i].regmap[hr]=regs[i+1].regmap[hr];
                regmap_pre[i+1][hr]=regs[i+1].regmap[hr];
                regs[i+1].regmap_entry[hr]=regs[i+1].regmap[hr];
                regs[i].isconst&=~(1<<hr);
                regs[i].isconst|=regs[i+1].isconst&(1<<hr);
                constmap[i][hr]=constmap[i+1][hr];
                regs[i+1].wasdirty&=~(1<<hr);
                regs[i].dirty&=~(1<<hr);
              }
            }
          }
          // Preload target address for load instruction (non-constant)
          if(itype[i+1]==LOAD&&rs1[i+1]&&get_reg(regs[i+1].regmap,rs1[i+1])<0) {
            if((hr=get_reg(regs[i+1].regmap,rt1[i+1]))>=0)
            {
              if(regs[i].regmap[hr]<0&&regs[i+1].regmap_entry[hr]<0)
              {
                regs[i].regmap[hr]=rs1[i+1];
                regmap_pre[i+1][hr]=rs1[i+1];
                regs[i+1].regmap_entry[hr]=rs1[i+1];
                regs[i].isconst&=~(1<<hr);
                regs[i].isconst|=regs[i+1].isconst&(1<<hr);
                constmap[i][hr]=constmap[i+1][hr];
                regs[i+1].wasdirty&=~(1<<hr);
                regs[i].dirty&=~(1<<hr);
              }
            }
          }
          // Load source into target register
          if(lt1[i+1]&&get_reg(regs[i+1].regmap,rs1[i+1])<0) {
            if((hr=get_reg(regs[i+1].regmap,rt1[i+1]))>=0)
            {
              if(regs[i].regmap[hr]<0&&regs[i+1].regmap_entry[hr]<0)
              {
                regs[i].regmap[hr]=rs1[i+1];
                regmap_pre[i+1][hr]=rs1[i+1];
                regs[i+1].regmap_entry[hr]=rs1[i+1];
                regs[i].isconst&=~(1<<hr);
                regs[i].isconst|=regs[i+1].isconst&(1<<hr);
                constmap[i][hr]=constmap[i+1][hr];
                regs[i+1].wasdirty&=~(1<<hr);
                regs[i].dirty&=~(1<<hr);
              }
            }
          }
          // Address for store instruction (non-constant)
          if(itype[i+1]==STORE||itype[i+1]==STORELR
             ||(opcode[i+1]&0x3b)==0x39||(opcode[i+1]&0x3b)==0x3a) { // SB/SH/SW/SD/SWC1/SDC1/SWC2/SDC2
            if(get_reg(regs[i+1].regmap,rs1[i+1])<0) {
              hr=get_reg2(regs[i].regmap,regs[i+1].regmap,-1);
              if(hr<0) hr=get_reg(regs[i+1].regmap,-1);
              else {regs[i+1].regmap[hr]=AGEN1+((i+1)&1);regs[i+1].isconst&=~(1<<hr);}
              assert(hr>=0);
              if(regs[i].regmap[hr]<0&&regs[i+1].regmap_entry[hr]<0)
              {
                regs[i].regmap[hr]=rs1[i+1];
                regmap_pre[i+1][hr]=rs1[i+1];
                regs[i+1].regmap_entry[hr]=rs1[i+1];
                regs[i].isconst&=~(1<<hr);
                regs[i].isconst|=regs[i+1].isconst&(1<<hr);
                constmap[i][hr]=constmap[i+1][hr];
                regs[i+1].wasdirty&=~(1<<hr);
                regs[i].dirty&=~(1<<hr);
              }
            }
          }
          if(itype[i+1]==LOADLR||(opcode[i+1]&0x3b)==0x31||(opcode[i+1]&0x3b)==0x32) { // LWC1/LDC1, LWC2/LDC2
            if(get_reg(regs[i+1].regmap,rs1[i+1])<0) {
              int nr;
              hr=get_reg(regs[i+1].regmap,FTEMP);
              assert(hr>=0);
              if(regs[i].regmap[hr]<0&&regs[i+1].regmap_entry[hr]<0)
              {
                regs[i].regmap[hr]=rs1[i+1];
                regmap_pre[i+1][hr]=rs1[i+1];
                regs[i+1].regmap_entry[hr]=rs1[i+1];
                regs[i].isconst&=~(1<<hr);
                regs[i].isconst|=regs[i+1].isconst&(1<<hr);
                constmap[i][hr]=constmap[i+1][hr];
                regs[i+1].wasdirty&=~(1<<hr);
                regs[i].dirty&=~(1<<hr);
              }
              else if((nr=get_reg2(regs[i].regmap,regs[i+1].regmap,-1))>=0)
              {
                // move it to another register
                regs[i+1].regmap[hr]=-1;
                regmap_pre[i+2][hr]=-1;
                regs[i+1].regmap[nr]=FTEMP;
                regmap_pre[i+2][nr]=FTEMP;
                regs[i].regmap[nr]=rs1[i+1];
                regmap_pre[i+1][nr]=rs1[i+1];
                regs[i+1].regmap_entry[nr]=rs1[i+1];
                regs[i].isconst&=~(1<<nr);
                regs[i+1].isconst&=~(1<<nr);
                regs[i].dirty&=~(1<<nr);
                regs[i+1].wasdirty&=~(1<<nr);
                regs[i+1].dirty&=~(1<<nr);
                regs[i+2].wasdirty&=~(1<<nr);
              }
            }
          }
          if(itype[i+1]==LOAD||itype[i+1]==LOADLR||itype[i+1]==STORE||itype[i+1]==STORELR/*||itype[i+1]==C1LS||||itype[i+1]==C2LS*/) {
            if(itype[i+1]==LOAD)
              hr=get_reg(regs[i+1].regmap,rt1[i+1]);
            if(itype[i+1]==LOADLR||(opcode[i+1]&0x3b)==0x31||(opcode[i+1]&0x3b)==0x32) // LWC1/LDC1, LWC2/LDC2
              hr=get_reg(regs[i+1].regmap,FTEMP);
            if(itype[i+1]==STORE||itype[i+1]==STORELR||(opcode[i+1]&0x3b)==0x39||(opcode[i+1]&0x3b)==0x3a) { // SWC1/SDC1/SWC2/SDC2
              hr=get_reg(regs[i+1].regmap,AGEN1+((i+1)&1));
              if(hr<0) hr=get_reg(regs[i+1].regmap,-1);
            }
            if(hr>=0&&regs[i].regmap[hr]<0) {
              int rs=get_reg(regs[i+1].regmap,rs1[i+1]);
              if(rs>=0&&((regs[i+1].wasconst>>rs)&1)) {
                regs[i].regmap[hr]=AGEN1+((i+1)&1);
                regmap_pre[i+1][hr]=AGEN1+((i+1)&1);
                regs[i+1].regmap_entry[hr]=AGEN1+((i+1)&1);
                regs[i].isconst&=~(1<<hr);
                regs[i+1].wasdirty&=~(1<<hr);
                regs[i].dirty&=~(1<<hr);
              }
            }
          }
        }
      }
    }
  }

  /* Pass 6 - Optimize clean/dirty state */
  clean_registers(0,slen-1,1);

  /* Pass 7 - Identify 32-bit registers */
  for (i=slen-1;i>=0;i--)
  {
    if(itype[i]==CJUMP||itype[i]==SJUMP)
    {
      // Conditional branch
      if((source[i]>>16)!=0x1000&&i<slen-2) {
        // Mark this address as a branch target since it may be called
        // upon return from interrupt
        bt[i+2]=1;
      }
    }
  }

  if(itype[slen-1]==SPAN) {
    bt[slen-1]=1; // Mark as a branch target so instruction can restart after exception
  }

#ifdef DISASM
  /* Debug/disassembly */
  for(i=0;i<slen;i++)
  {
    printf("U:");
    int r;
    for(r=1;r<=CCREG;r++) {
      if((unneeded_reg[i]>>r)&1) {
        if(r==HIREG) printf(" HI");
        else if(r==LOREG) printf(" LO");
        else printf(" r%d",r);
      }
    }
    printf("\n");
    #if defined(__i386__) || defined(__x86_64__)
    printf("pre: eax=%d ecx=%d edx=%d ebx=%d ebp=%d esi=%d edi=%d\n",regmap_pre[i][0],regmap_pre[i][1],regmap_pre[i][2],regmap_pre[i][3],regmap_pre[i][5],regmap_pre[i][6],regmap_pre[i][7]);
    #endif
    #ifdef __arm__
    printf("pre: r0=%d r1=%d r2=%d r3=%d r4=%d r5=%d r6=%d r7=%d r8=%d r9=%d r10=%d r12=%d\n",regmap_pre[i][0],regmap_pre[i][1],regmap_pre[i][2],regmap_pre[i][3],regmap_pre[i][4],regmap_pre[i][5],regmap_pre[i][6],regmap_pre[i][7],regmap_pre[i][8],regmap_pre[i][9],regmap_pre[i][10],regmap_pre[i][12]);
    #endif
    #if defined(__i386__) || defined(__x86_64__)
    printf("needs: ");
    if(needed_reg[i]&1) printf("eax ");
    if((needed_reg[i]>>1)&1) printf("ecx ");
    if((needed_reg[i]>>2)&1) printf("edx ");
    if((needed_reg[i]>>3)&1) printf("ebx ");
    if((needed_reg[i]>>5)&1) printf("ebp ");
    if((needed_reg[i]>>6)&1) printf("esi ");
    if((needed_reg[i]>>7)&1) printf("edi ");
    printf("\n");
    printf("entry: eax=%d ecx=%d edx=%d ebx=%d ebp=%d esi=%d edi=%d\n",regs[i].regmap_entry[0],regs[i].regmap_entry[1],regs[i].regmap_entry[2],regs[i].regmap_entry[3],regs[i].regmap_entry[5],regs[i].regmap_entry[6],regs[i].regmap_entry[7]);
    printf("dirty: ");
    if(regs[i].wasdirty&1) printf("eax ");
    if((regs[i].wasdirty>>1)&1) printf("ecx ");
    if((regs[i].wasdirty>>2)&1) printf("edx ");
    if((regs[i].wasdirty>>3)&1) printf("ebx ");
    if((regs[i].wasdirty>>5)&1) printf("ebp ");
    if((regs[i].wasdirty>>6)&1) printf("esi ");
    if((regs[i].wasdirty>>7)&1) printf("edi ");
    #endif
    #ifdef __arm__
    printf("entry: r0=%d r1=%d r2=%d r3=%d r4=%d r5=%d r6=%d r7=%d r8=%d r9=%d r10=%d r12=%d\n",regs[i].regmap_entry[0],regs[i].regmap_entry[1],regs[i].regmap_entry[2],regs[i].regmap_entry[3],regs[i].regmap_entry[4],regs[i].regmap_entry[5],regs[i].regmap_entry[6],regs[i].regmap_entry[7],regs[i].regmap_entry[8],regs[i].regmap_entry[9],regs[i].regmap_entry[10],regs[i].regmap_entry[12]);
    printf("dirty: ");
    if(regs[i].wasdirty&1) printf("r0 ");
    if((regs[i].wasdirty>>1)&1) printf("r1 ");
    if((regs[i].wasdirty>>2)&1) printf("r2 ");
    if((regs[i].wasdirty>>3)&1) printf("r3 ");
    if((regs[i].wasdirty>>4)&1) printf("r4 ");
    if((regs[i].wasdirty>>5)&1) printf("r5 ");
    if((regs[i].wasdirty>>6)&1) printf("r6 ");
    if((regs[i].wasdirty>>7)&1) printf("r7 ");
    if((regs[i].wasdirty>>8)&1) printf("r8 ");
    if((regs[i].wasdirty>>9)&1) printf("r9 ");
    if((regs[i].wasdirty>>10)&1) printf("r10 ");
    if((regs[i].wasdirty>>12)&1) printf("r12 ");
    #endif
    printf("\n");
    disassemble_inst(i);
    //printf ("ccadj[%d] = %d\n",i,ccadj[i]);
    #if defined(__i386__) || defined(__x86_64__)
    printf("eax=%d ecx=%d edx=%d ebx=%d ebp=%d esi=%d edi=%d dirty: ",regs[i].regmap[0],regs[i].regmap[1],regs[i].regmap[2],regs[i].regmap[3],regs[i].regmap[5],regs[i].regmap[6],regs[i].regmap[7]);
    if(regs[i].dirty&1) printf("eax ");
    if((regs[i].dirty>>1)&1) printf("ecx ");
    if((regs[i].dirty>>2)&1) printf("edx ");
    if((regs[i].dirty>>3)&1) printf("ebx ");
    if((regs[i].dirty>>5)&1) printf("ebp ");
    if((regs[i].dirty>>6)&1) printf("esi ");
    if((regs[i].dirty>>7)&1) printf("edi ");
    #endif
    #ifdef __arm__
    printf("r0=%d r1=%d r2=%d r3=%d r4=%d r5=%d r6=%d r7=%d r8=%d r9=%d r10=%d r12=%d dirty: ",regs[i].regmap[0],regs[i].regmap[1],regs[i].regmap[2],regs[i].regmap[3],regs[i].regmap[4],regs[i].regmap[5],regs[i].regmap[6],regs[i].regmap[7],regs[i].regmap[8],regs[i].regmap[9],regs[i].regmap[10],regs[i].regmap[12]);
    if(regs[i].dirty&1) printf("r0 ");
    if((regs[i].dirty>>1)&1) printf("r1 ");
    if((regs[i].dirty>>2)&1) printf("r2 ");
    if((regs[i].dirty>>3)&1) printf("r3 ");
    if((regs[i].dirty>>4)&1) printf("r4 ");
    if((regs[i].dirty>>5)&1) printf("r5 ");
    if((regs[i].dirty>>6)&1) printf("r6 ");
    if((regs[i].dirty>>7)&1) printf("r7 ");
    if((regs[i].dirty>>8)&1) printf("r8 ");
    if((regs[i].dirty>>9)&1) printf("r9 ");
    if((regs[i].dirty>>10)&1) printf("r10 ");
    if((regs[i].dirty>>12)&1) printf("r12 ");
    #endif
    printf("\n");
    if(regs[i].isconst) {
      printf("constants: ");
      #if defined(__i386__) || defined(__x86_64__)
      if(regs[i].isconst&1) printf("eax=%x ",(u_int)constmap[i][0]);
      if((regs[i].isconst>>1)&1) printf("ecx=%x ",(u_int)constmap[i][1]);
      if((regs[i].isconst>>2)&1) printf("edx=%x ",(u_int)constmap[i][2]);
      if((regs[i].isconst>>3)&1) printf("ebx=%x ",(u_int)constmap[i][3]);
      if((regs[i].isconst>>5)&1) printf("ebp=%x ",(u_int)constmap[i][5]);
      if((regs[i].isconst>>6)&1) printf("esi=%x ",(u_int)constmap[i][6]);
      if((regs[i].isconst>>7)&1) printf("edi=%x ",(u_int)constmap[i][7]);
      #endif
      #if defined(__arm__) || defined(__aarch64__)
      int r;
      for (r = 0; r < ARRAY_SIZE(constmap[i]); r++)
        if ((regs[i].isconst >> r) & 1)
          printf(" r%d=%x", r, (u_int)constmap[i][r]);
      #endif
      printf("\n");
    }
    if(itype[i]==RJUMP||itype[i]==UJUMP||itype[i]==CJUMP||itype[i]==SJUMP) {
      #if defined(__i386__) || defined(__x86_64__)
      printf("branch(%d): eax=%d ecx=%d edx=%d ebx=%d ebp=%d esi=%d edi=%d dirty: ",i,branch_regs[i].regmap[0],branch_regs[i].regmap[1],branch_regs[i].regmap[2],branch_regs[i].regmap[3],branch_regs[i].regmap[5],branch_regs[i].regmap[6],branch_regs[i].regmap[7]);
      if(branch_regs[i].dirty&1) printf("eax ");
      if((branch_regs[i].dirty>>1)&1) printf("ecx ");
      if((branch_regs[i].dirty>>2)&1) printf("edx ");
      if((branch_regs[i].dirty>>3)&1) printf("ebx ");
      if((branch_regs[i].dirty>>5)&1) printf("ebp ");
      if((branch_regs[i].dirty>>6)&1) printf("esi ");
      if((branch_regs[i].dirty>>7)&1) printf("edi ");
      #endif
      #ifdef __arm__
      printf("branch(%d): r0=%d r1=%d r2=%d r3=%d r4=%d r5=%d r6=%d r7=%d r8=%d r9=%d r10=%d r12=%d dirty: ",i,branch_regs[i].regmap[0],branch_regs[i].regmap[1],branch_regs[i].regmap[2],branch_regs[i].regmap[3],branch_regs[i].regmap[4],branch_regs[i].regmap[5],branch_regs[i].regmap[6],branch_regs[i].regmap[7],branch_regs[i].regmap[8],branch_regs[i].regmap[9],branch_regs[i].regmap[10],branch_regs[i].regmap[12]);
      if(branch_regs[i].dirty&1) printf("r0 ");
      if((branch_regs[i].dirty>>1)&1) printf("r1 ");
      if((branch_regs[i].dirty>>2)&1) printf("r2 ");
      if((branch_regs[i].dirty>>3)&1) printf("r3 ");
      if((branch_regs[i].dirty>>4)&1) printf("r4 ");
      if((branch_regs[i].dirty>>5)&1) printf("r5 ");
      if((branch_regs[i].dirty>>6)&1) printf("r6 ");
      if((branch_regs[i].dirty>>7)&1) printf("r7 ");
      if((branch_regs[i].dirty>>8)&1) printf("r8 ");
      if((branch_regs[i].dirty>>9)&1) printf("r9 ");
      if((branch_regs[i].dirty>>10)&1) printf("r10 ");
      if((branch_regs[i].dirty>>12)&1) printf("r12 ");
      #endif
    }
  }
#endif // DISASM

  /* Pass 8 - Assembly */
  linkcount=0;stubcount=0;
  ds=0;is_delayslot=0;
  u_int dirty_pre=0;
  void *beginning=start_block();
  if((u_int)addr&1) {
    ds=1;
    pagespan_ds();
  }
  void *instr_addr0_override = NULL;

  if (start == 0x80030000) {
    // nasty hack for the fastbios thing
    // override block entry to this code
    instr_addr0_override = out;
    emit_movimm(start,0);
    // abuse io address var as a flag that we
    // have already returned here once
    emit_readword(&address,1);
    emit_writeword(0,&pcaddr);
    emit_writeword(0,&address);
    emit_cmp(0,1);
    #ifdef __aarch64__
    emit_jeq(out + 4*2);
    emit_far_jump(new_dyna_leave);
    #else
    emit_jne(new_dyna_leave);
    #endif
  }
  for(i=0;i<slen;i++)
  {
    //if(ds) printf("ds: ");
    disassemble_inst(i);
    if(ds) {
      ds=0; // Skip delay slot
      if(bt[i]) assem_debug("OOPS - branch into delay slot\n");
      instr_addr[i] = NULL;
    } else {
      speculate_register_values(i);
      #ifndef DESTRUCTIVE_WRITEBACK
      if (i < 2 || !is_ujump(i-2))
      {
        wb_valid(regmap_pre[i],regs[i].regmap_entry,dirty_pre,regs[i].wasdirty,unneeded_reg[i]);
      }
      if((itype[i]==CJUMP||itype[i]==SJUMP)&&!likely[i]) {
        dirty_pre=branch_regs[i].dirty;
      }else{
        dirty_pre=regs[i].dirty;
      }
      #endif
      // write back
      if (i < 2 || !is_ujump(i-2))
      {
        wb_invalidate(regmap_pre[i],regs[i].regmap_entry,regs[i].wasdirty,unneeded_reg[i]);
        loop_preload(regmap_pre[i],regs[i].regmap_entry);
      }
      // branch target entry point
      instr_addr[i] = out;
      assem_debug("<->\n");
      drc_dbg_emit_do_cmp(i);

      // load regs
      if(regs[i].regmap_entry[HOST_CCREG]==CCREG&&regs[i].regmap[HOST_CCREG]!=CCREG)
        wb_register(CCREG,regs[i].regmap_entry,regs[i].wasdirty);
      load_regs(regs[i].regmap_entry,regs[i].regmap,rs1[i],rs2[i]);
      address_generation(i,&regs[i],regs[i].regmap_entry);
      load_consts(regmap_pre[i],regs[i].regmap,i);
      if(itype[i]==RJUMP||itype[i]==UJUMP||itype[i]==CJUMP||itype[i]==SJUMP)
      {
        // Load the delay slot registers if necessary
        if(rs1[i+1]!=rs1[i]&&rs1[i+1]!=rs2[i]&&(rs1[i+1]!=rt1[i]||rt1[i]==0))
          load_regs(regs[i].regmap_entry,regs[i].regmap,rs1[i+1],rs1[i+1]);
        if(rs2[i+1]!=rs1[i+1]&&rs2[i+1]!=rs1[i]&&rs2[i+1]!=rs2[i]&&(rs2[i+1]!=rt1[i]||rt1[i]==0))
          load_regs(regs[i].regmap_entry,regs[i].regmap,rs2[i+1],rs2[i+1]);
        if(itype[i+1]==STORE||itype[i+1]==STORELR||(opcode[i+1]&0x3b)==0x39||(opcode[i+1]&0x3b)==0x3a)
          load_regs(regs[i].regmap_entry,regs[i].regmap,INVCP,INVCP);
      }
      else if(i+1<slen)
      {
        // Preload registers for following instruction
        if(rs1[i+1]!=rs1[i]&&rs1[i+1]!=rs2[i])
          if(rs1[i+1]!=rt1[i]&&rs1[i+1]!=rt2[i])
            load_regs(regs[i].regmap_entry,regs[i].regmap,rs1[i+1],rs1[i+1]);
        if(rs2[i+1]!=rs1[i+1]&&rs2[i+1]!=rs1[i]&&rs2[i+1]!=rs2[i])
          if(rs2[i+1]!=rt1[i]&&rs2[i+1]!=rt2[i])
            load_regs(regs[i].regmap_entry,regs[i].regmap,rs2[i+1],rs2[i+1]);
      }
      // TODO: if(is_ooo(i)) address_generation(i+1);
      if(itype[i]==CJUMP)
        load_regs(regs[i].regmap_entry,regs[i].regmap,CCREG,CCREG);
      if(itype[i]==STORE||itype[i]==STORELR||(opcode[i]&0x3b)==0x39||(opcode[i]&0x3b)==0x3a)
        load_regs(regs[i].regmap_entry,regs[i].regmap,INVCP,INVCP);
      // assemble
      switch(itype[i]) {
        case ALU:
          alu_assemble(i,&regs[i]);break;
        case IMM16:
          imm16_assemble(i,&regs[i]);break;
        case SHIFT:
          shift_assemble(i,&regs[i]);break;
        case SHIFTIMM:
          shiftimm_assemble(i,&regs[i]);break;
        case LOAD:
          load_assemble(i,&regs[i]);break;
        case LOADLR:
          loadlr_assemble(i,&regs[i]);break;
        case STORE:
          store_assemble(i,&regs[i]);break;
        case STORELR:
          storelr_assemble(i,&regs[i]);break;
        case COP0:
          cop0_assemble(i,&regs[i]);break;
        case COP1:
          cop1_assemble(i,&regs[i]);break;
        case C1LS:
          c1ls_assemble(i,&regs[i]);break;
        case COP2:
          cop2_assemble(i,&regs[i]);break;
        case C2LS:
          c2ls_assemble(i,&regs[i]);break;
        case C2OP:
          c2op_assemble(i,&regs[i]);break;
        case MULTDIV:
          multdiv_assemble(i,&regs[i]);break;
        case MOV:
          mov_assemble(i,&regs[i]);break;
        case SYSCALL:
          syscall_assemble(i,&regs[i]);break;
        case HLECALL:
          hlecall_assemble(i,&regs[i]);break;
        case INTCALL:
          intcall_assemble(i,&regs[i]);break;
        case UJUMP:
          ujump_assemble(i,&regs[i]);ds=1;break;
        case RJUMP:
          rjump_assemble(i,&regs[i]);ds=1;break;
        case CJUMP:
          cjump_assemble(i,&regs[i]);ds=1;break;
        case SJUMP:
          sjump_assemble(i,&regs[i]);ds=1;break;
        case SPAN:
          pagespan_assemble(i,&regs[i]);break;
      }
      if (is_ujump(i))
        literal_pool(1024);
      else
        literal_pool_jumpover(256);
    }
  }
  //assert(is_ujump(i-2));
  // If the block did not end with an unconditional branch,
  // add a jump to the next instruction.
  if(i>1) {
    if(!is_ujump(i-2)&&itype[i-1]!=SPAN) {
      assert(itype[i-1]!=UJUMP&&itype[i-1]!=CJUMP&&itype[i-1]!=SJUMP&&itype[i-1]!=RJUMP);
      assert(i==slen);
      if(itype[i-2]!=CJUMP&&itype[i-2]!=SJUMP) {
        store_regs_bt(regs[i-1].regmap,regs[i-1].dirty,start+i*4);
        if(regs[i-1].regmap[HOST_CCREG]!=CCREG)
          emit_loadreg(CCREG,HOST_CCREG);
        emit_addimm(HOST_CCREG,CLOCK_ADJUST(ccadj[i-1]+1),HOST_CCREG);
      }
      else if(!likely[i-2])
      {
        store_regs_bt(branch_regs[i-2].regmap,branch_regs[i-2].dirty,start+i*4);
        assert(branch_regs[i-2].regmap[HOST_CCREG]==CCREG);
      }
      else
      {
        store_regs_bt(regs[i-2].regmap,regs[i-2].dirty,start+i*4);
        assert(regs[i-2].regmap[HOST_CCREG]==CCREG);
      }
      add_to_linker(out,start+i*4,0);
      emit_jmp(0);
    }
  }
  else
  {
    assert(i>0);
    assert(itype[i-1]!=UJUMP&&itype[i-1]!=CJUMP&&itype[i-1]!=SJUMP&&itype[i-1]!=RJUMP);
    store_regs_bt(regs[i-1].regmap,regs[i-1].dirty,start+i*4);
    if(regs[i-1].regmap[HOST_CCREG]!=CCREG)
      emit_loadreg(CCREG,HOST_CCREG);
    emit_addimm(HOST_CCREG,CLOCK_ADJUST(ccadj[i-1]+1),HOST_CCREG);
    add_to_linker(out,start+i*4,0);
    emit_jmp(0);
  }

  // TODO: delay slot stubs?
  // Stubs
  for(i=0;i<stubcount;i++)
  {
    switch(stubs[i].type)
    {
      case LOADB_STUB:
      case LOADH_STUB:
      case LOADW_STUB:
      case LOADD_STUB:
      case LOADBU_STUB:
      case LOADHU_STUB:
        do_readstub(i);break;
      case STOREB_STUB:
      case STOREH_STUB:
      case STOREW_STUB:
      case STORED_STUB:
        do_writestub(i);break;
      case CC_STUB:
        do_ccstub(i);break;
      case INVCODE_STUB:
        do_invstub(i);break;
      case FP_STUB:
        do_cop1stub(i);break;
      case STORELR_STUB:
        do_unalignedwritestub(i);break;
    }
  }

  if (instr_addr0_override)
    instr_addr[0] = instr_addr0_override;

  /* Pass 9 - Linker */
  for(i=0;i<linkcount;i++)
  {
    assem_debug("%p -> %8x\n",link_addr[i].addr,link_addr[i].target);
    literal_pool(64);
    if (!link_addr[i].ext)
    {
      void *stub = out;
      void *addr = check_addr(link_addr[i].target);
      emit_extjump(link_addr[i].addr, link_addr[i].target);
      if (addr) {
        set_jump_target(link_addr[i].addr, addr);
        add_link(link_addr[i].target,stub);
      }
      else
        set_jump_target(link_addr[i].addr, stub);
    }
    else
    {
      // Internal branch
      int target=(link_addr[i].target-start)>>2;
      assert(target>=0&&target<slen);
      assert(instr_addr[target]);
      //#ifdef CORTEX_A8_BRANCH_PREDICTION_HACK
      //set_jump_target_fillslot(link_addr[i].addr,instr_addr[target],link_addr[i].ext>>1);
      //#else
      set_jump_target(link_addr[i].addr, instr_addr[target]);
      //#endif
    }
  }
  // External Branch Targets (jump_in)
  if(copy+slen*4>(void *)shadow+sizeof(shadow)) copy=shadow;
  for(i=0;i<slen;i++)
  {
    if(bt[i]||i==0)
    {
      if(instr_addr[i]) // TODO - delay slots (=null)
      {
        u_int vaddr=start+i*4;
        u_int page=get_page(vaddr);
        u_int vpage=get_vpage(vaddr);
        literal_pool(256);
        {
          assem_debug("%p (%d) <- %8x\n",instr_addr[i],i,start+i*4);
          assem_debug("jump_in: %x\n",start+i*4);
          ll_add(jump_dirty+vpage,vaddr,out);
          void *entry_point = do_dirty_stub(i);
          ll_add_flags(jump_in+page,vaddr,state_rflags,entry_point);
          // If there was an existing entry in the hash table,
          // replace it with the new address.
          // Don't add new entries.  We'll insert the
          // ones that actually get used in check_addr().
          struct ht_entry *ht_bin = hash_table_get(vaddr);
          if (ht_bin->vaddr[0] == vaddr)
            ht_bin->tcaddr[0] = entry_point;
          if (ht_bin->vaddr[1] == vaddr)
            ht_bin->tcaddr[1] = entry_point;
        }
      }
    }
  }
  // Write out the literal pool if necessary
  literal_pool(0);
  #ifdef CORTEX_A8_BRANCH_PREDICTION_HACK
  // Align code
  if(((u_int)out)&7) emit_addnop(13);
  #endif
  assert(out - (u_char *)beginning < MAX_OUTPUT_BLOCK_SIZE);
  //printf("shadow buffer: %p-%p\n",copy,(u_char *)copy+slen*4);
  memcpy(copy,source,slen*4);
  copy+=slen*4;

  end_block(beginning);

  // If we're within 256K of the end of the buffer,
  // start over from the beginning. (Is 256K enough?)
  if (out > ndrc->translation_cache + sizeof(ndrc->translation_cache) - MAX_OUTPUT_BLOCK_SIZE)
    out = ndrc->translation_cache;

  // Trap writes to any of the pages we compiled
  for(i=start>>12;i<=(start+slen*4)>>12;i++) {
    invalid_code[i]=0;
  }
  inv_code_start=inv_code_end=~0;

  // for PCSX we need to mark all mirrors too
  if(get_page(start)<(RAM_SIZE>>12))
    for(i=start>>12;i<=(start+slen*4)>>12;i++)
      invalid_code[((u_int)0x00000000>>12)|(i&0x1ff)]=
      invalid_code[((u_int)0x80000000>>12)|(i&0x1ff)]=
      invalid_code[((u_int)0xa0000000>>12)|(i&0x1ff)]=0;

  /* Pass 10 - Free memory by expiring oldest blocks */

  int end=(((out-ndrc->translation_cache)>>(TARGET_SIZE_2-16))+16384)&65535;
  while(expirep!=end)
  {
    int shift=TARGET_SIZE_2-3; // Divide into 8 blocks
    uintptr_t base=(uintptr_t)ndrc->translation_cache+((expirep>>13)<<shift); // Base address of this block
    inv_debug("EXP: Phase %d\n",expirep);
    switch((expirep>>11)&3)
    {
      case 0:
        // Clear jump_in and jump_dirty
        ll_remove_matching_addrs(jump_in+(expirep&2047),base,shift);
        ll_remove_matching_addrs(jump_dirty+(expirep&2047),base,shift);
        ll_remove_matching_addrs(jump_in+2048+(expirep&2047),base,shift);
        ll_remove_matching_addrs(jump_dirty+2048+(expirep&2047),base,shift);
        break;
      case 1:
        // Clear pointers
        ll_kill_pointers(jump_out[expirep&2047],base,shift);
        ll_kill_pointers(jump_out[(expirep&2047)+2048],base,shift);
        break;
      case 2:
        // Clear hash table
        for(i=0;i<32;i++) {
          struct ht_entry *ht_bin = &hash_table[((expirep&2047)<<5)+i];
          if (((uintptr_t)ht_bin->tcaddr[1]>>shift) == (base>>shift) ||
             (((uintptr_t)ht_bin->tcaddr[1]-MAX_OUTPUT_BLOCK_SIZE)>>shift)==(base>>shift)) {
            inv_debug("EXP: Remove hash %x -> %p\n",ht_bin->vaddr[1],ht_bin->tcaddr[1]);
            ht_bin->vaddr[1] = -1;
            ht_bin->tcaddr[1] = NULL;
          }
          if (((uintptr_t)ht_bin->tcaddr[0]>>shift) == (base>>shift) ||
             (((uintptr_t)ht_bin->tcaddr[0]-MAX_OUTPUT_BLOCK_SIZE)>>shift)==(base>>shift)) {
            inv_debug("EXP: Remove hash %x -> %p\n",ht_bin->vaddr[0],ht_bin->tcaddr[0]);
            ht_bin->vaddr[0] = ht_bin->vaddr[1];
            ht_bin->tcaddr[0] = ht_bin->tcaddr[1];
            ht_bin->vaddr[1] = -1;
            ht_bin->tcaddr[1] = NULL;
          }
        }
        break;
      case 3:
        // Clear jump_out
        if((expirep&2047)==0)
          do_clear_cache();
        ll_remove_matching_addrs(jump_out+(expirep&2047),base,shift);
        ll_remove_matching_addrs(jump_out+2048+(expirep&2047),base,shift);
        break;
    }
    expirep=(expirep+1)&65535;
  }
  return 0;
}

// vim:shiftwidth=2:expandtab
