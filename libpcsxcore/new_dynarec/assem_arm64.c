/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus/PCSX - assem_arm64.c                                      *
 *   Copyright (C) 2009-2011 Ari64                                         *
 *   Copyright (C) 2010-2021 notaz                                         *
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

#include "arm_features.h"

#if   defined(BASE_ADDR_FIXED)
#elif defined(BASE_ADDR_DYNAMIC)
u_char *translation_cache;
#else
u_char translation_cache[1 << TARGET_SIZE_2] __attribute__((aligned(4096)));
#endif

#define CALLER_SAVE_REGS 0x0007ffff

#define unused __attribute__((unused))

extern int cycle_count;
extern int last_count;
extern int pcaddr;
extern int pending_exception;
extern int branch_target;
extern u_int mini_ht[32][2];

static u_int needs_clear_cache[1<<(TARGET_SIZE_2-17)];

//void indirect_jump_indexed();
//void indirect_jump();
void do_interrupt();
//void jump_vaddr_r0();

void * const jump_vaddr_reg[32];

/* Linker */

static void set_jump_target(void *addr, void *target_)
{
  assert(0);
}

// from a pointer to external jump stub (which was produced by emit_extjump2)
// find where the jumping insn is
static void *find_extjump_insn(void *stub)
{
  assert(0);
  return NULL;
}

// find where external branch is liked to using addr of it's stub:
// get address that insn one after stub loads (dyna_linker arg1),
// treat it as a pointer to branch insn,
// return addr where that branch jumps to
static void *get_pointer(void *stub)
{
  //printf("get_pointer(%x)\n",(int)stub);
  assert(0);
  return NULL;
}

// Find the "clean" entry point from a "dirty" entry point
// by skipping past the call to verify_code
static void *get_clean_addr(void *addr)
{
  assert(0);
  return NULL;
}

static int verify_dirty(u_int *ptr)
{
  assert(0);
  return 0;
}

// This doesn't necessarily find all clean entry points, just
// guarantees that it's not dirty
static int isclean(void *addr)
{
  assert(0);
  return 0;
}

// get source that block at addr was compiled from (host pointers)
static void get_bounds(void *addr, u_char **start, u_char **end)
{
  assert(0);
}

// Allocate a specific ARM register.
static void alloc_arm_reg(struct regstat *cur,int i,signed char reg,int hr)
{
  int n;
  int dirty=0;

  // see if it's already allocated (and dealloc it)
  for(n=0;n<HOST_REGS;n++)
  {
    if(n!=EXCLUDE_REG&&cur->regmap[n]==reg) {
      dirty=(cur->dirty>>n)&1;
      cur->regmap[n]=-1;
    }
  }

  cur->regmap[hr]=reg;
  cur->dirty&=~(1<<hr);
  cur->dirty|=dirty<<hr;
  cur->isconst&=~(1<<hr);
}

// Alloc cycle count into dedicated register
static void alloc_cc(struct regstat *cur,int i)
{
  alloc_arm_reg(cur,i,CCREG,HOST_CCREG);
}

/* Special alloc */


/* Assembler */

static unused const char *regname[32] = {
  "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6", "r7",
  "r8",  "r9", "r10", "r11", "r12", "r13", "r14", "r15"
 "ip0", "ip1", "r18", "r19", "r20", "r21", "r22", "r23"
 "r24", "r25", "r26", "r27", "r28",  "fp",  "lr",  "sp"
};

#pragma GCC diagnostic ignored "-Wunused-function"
static void output_w32(u_int word)
{
  *((u_int *)out) = word;
  out += 4;
}

static u_int rm_rn_rd(u_int rm, u_int rn, u_int rd)
{
  assert(rm < 31);
  assert(rn < 31);
  assert(rd < 31);
  return (rm << 16) | (rn << 5) | rd;
}

static u_int imm16_rd(u_int imm16, u_int rd)
{
  assert(imm16 < 0x10000);
  assert(rd < 31);
  return (imm16 << 5) | rd;
}

static u_int genjmp(u_char *addr)
{
  intptr_t offset = addr - out;
  if (offset < -134217728 || offset > 134217727) {
    if ((uintptr_t)addr > 2) {
      SysPrintf("%s: out of range: %08x\n", __func__, offset);
      exit(1);
    }
    return 0;
  }
  return ((u_int)offset >> 2) & 0x01ffffff;
}

static u_int genjmpcc(u_char *addr)
{
  intptr_t offset = addr - out;
  if (offset < -1048576 || offset > 1048572) {
    if ((uintptr_t)addr > 2) {
      SysPrintf("%s: out of range: %08x\n", __func__, offset);
      exit(1);
    }
    return 0;
  }
  return ((u_int)offset >> 2) & 0xfffff;
}

static void emit_mov(u_int rs, u_int rt)
{
  assem_debug("mov %s,%s\n",regname[rt],regname[rs]);
  assert(0);
}

static void emit_movs(u_int rs, u_int rt)
{
  assem_debug("mov %s,%s\n",regname[rt],regname[rs]);
  assert(0);
}

static void emit_add(u_int rs1,u_int rs2,u_int rt)
{
  assem_debug("add %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  assert(0);
}

static void emit_sbc(u_int rs1,u_int rs2,u_int rt)
{
  assem_debug("sbc %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  assert(0);
}

static void emit_neg(u_int rs, u_int rt)
{
  assem_debug("rsb %s,%s,#0\n",regname[rt],regname[rs]);
  assert(0);
}

static void emit_negs(u_int rs, u_int rt)
{
  assem_debug("rsbs %s,%s,#0\n",regname[rt],regname[rs]);
  assert(0);
}

static void emit_sub(u_int rs1,u_int rs2,u_int rt)
{
  assem_debug("sub %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  assert(0);
}

static void emit_subs(u_int rs1,u_int rs2,u_int rt)
{
  assem_debug("subs %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  assert(0);
}

static void emit_zeroreg(u_int rt)
{
  assem_debug("mov %s,#0\n",regname[rt]);
  assert(0);
}

static void emit_movw(u_int imm,u_int rt)
{
  assert(imm<65536);
  assem_debug("movw %s,#%d (0x%x)\n",regname[rt],imm,imm);
  assert(0);
}

static void emit_movt(u_int imm,u_int rt)
{
  assem_debug("movt %s,#%d (0x%x)\n",regname[rt],imm&0xffff0000,imm&0xffff0000);
  assert(0);
}

static void emit_movimm(u_int imm, u_int rt)
{
  assem_debug("mov %s,#%x\n", regname[rt], imm);
  if      ((imm & 0xffff0000) == 0)
    output_w32(0x52800000 | imm16_rd(imm, rt));
  else if ((imm & 0xffff0000) == 0xffff0000)
    assert(0);
  else {
    output_w32(0x52800000 | imm16_rd(imm & 0xffff, rt));
    output_w32(0x72a00000 | imm16_rd(imm >> 16, rt));
  }
}

static void emit_loadreg(u_int r, u_int hr)
{
  assert(r < 64);
  if (r == 0)
    emit_zeroreg(hr);
  else {
    void *addr = &reg[r];
    switch (r) {
    case HIREG: addr = &hi; break;
    case LOREG: addr = &lo; break;
    case CCREG: addr = &cycle_count; break;
    case CSREG: addr = &Status; break;
    case INVCP: addr = &invc_ptr; break;
    default: assert(r < 32); break;
    }
    uintptr_t offset = (u_char *)addr - (u_char *)&dynarec_local;
    assert(offset < 4096);
    assem_debug("ldr %s,fp+%lx\n", regname[hr], offset);
    assert(0);
  }
}

static void emit_storereg(u_int r, int hr)
{
  assert(r < 64);
  void *addr = &reg[r];
  switch (r) {
  case HIREG: addr = &hi; break;
  case LOREG: addr = &lo; break;
  case CCREG: addr = &cycle_count; break;
  default: assert(r < 32); break;
  }
  uintptr_t offset = (u_char *)addr - (u_char *)&dynarec_local;
  assert(offset < 4096);
  assem_debug("str %s,fp+%lx\n", regname[hr], offset);
  assert(0);
}

static void emit_test(u_int rs, u_int rt)
{
  assem_debug("tst %s,%s\n",regname[rs],regname[rt]);
  assert(0);
}

static void emit_testimm(u_int rs,int imm)
{
  assem_debug("tst %s,#%d\n",regname[rs],imm);
  assert(0);
}

static void emit_testeqimm(u_int rs,int imm)
{
  assem_debug("tsteq %s,$%d\n",regname[rs],imm);
  assert(0);
}

static void emit_not(u_int rs,u_int rt)
{
  assem_debug("mvn %s,%s\n",regname[rt],regname[rs]);
  assert(0);
}

static void emit_mvnmi(u_int rs,u_int rt)
{
  assem_debug("mvnmi %s,%s\n",regname[rt],regname[rs]);
  assert(0);
}

static void emit_and(u_int rs1,u_int rs2,u_int rt)
{
  assem_debug("and %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  assert(0);
}

static void emit_or(u_int rs1,u_int rs2,u_int rt)
{
  assem_debug("orr %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  assert(0);
}

static void emit_orrshl_imm(u_int rs,u_int imm,u_int rt)
{
  assert(rs < 31);
  assert(rt < 31);
  assert(imm < 32);
  assem_debug("orr %s,%s,%s,lsl #%d\n",regname[rt],regname[rt],regname[rs],imm);
  assert(0);
}

static void emit_orrshr_imm(u_int rs,u_int imm,u_int rt)
{
  assert(rs < 31);
  assert(rt < 31);
  assert(imm < 32);
  assem_debug("orr %s,%s,%s,lsr #%d\n",regname[rt],regname[rt],regname[rs],imm);
  assert(0);
}

static void emit_or_and_set_flags(u_int rs1,u_int rs2,u_int rt)
{
  assem_debug("orrs %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  assert(0);
}

static void emit_xor(u_int rs1,u_int rs2,u_int rt)
{
  assem_debug("eor %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  assert(0);
}

static void emit_addimm(u_int rs, uintptr_t imm, u_int rt)
{
  assert(rs < 31);
  assert(rt < 31);
  assert(0);
}

static void emit_addimm_and_set_flags(int imm, u_int rt)
{
  assert(0);
}

static void emit_addimm_no_flags(u_int imm,u_int rt)
{
  emit_addimm(rt,imm,rt);
}

static void emit_addnop(u_int r)
{
  assert(r<16);
  assem_debug("add %s,%s,#0 (nop)\n",regname[r],regname[r]);
  assert(0);
}

static void emit_adcimm(u_int rs,int imm,u_int rt)
{
  assem_debug("adc %s,%s,#%d\n",regname[rt],regname[rs],imm);
  assert(0);
}

static void emit_rscimm(u_int rs,int imm,u_int rt)
{
  assem_debug("rsc %s,%s,#%d\n",regname[rt],regname[rs],imm);
  assert(0);
}

static void emit_addimm64_32(u_int rsh,u_int rsl,int imm,u_int rth,u_int rtl)
{
  assert(0);
}

static void emit_andimm(u_int rs,int imm,u_int rt)
{
  assert(0);
}

static void emit_orimm(u_int rs,int imm,u_int rt)
{
  assert(0);
}

static void emit_xorimm(u_int rs,int imm,u_int rt)
{
  assert(0);
}

static void emit_shlimm(u_int rs,u_int imm,u_int rt)
{
  assert(imm>0);
  assert(imm<32);
  assem_debug("lsl %s,%s,#%d\n",regname[rt],regname[rs],imm);
  assert(0);
}

static void emit_lsls_imm(u_int rs,int imm,u_int rt)
{
  assert(imm>0);
  assert(imm<32);
  assem_debug("lsls %s,%s,#%d\n",regname[rt],regname[rs],imm);
  assert(0);
}

static unused void emit_lslpls_imm(u_int rs,int imm,u_int rt)
{
  assert(imm>0);
  assert(imm<32);
  assem_debug("lslpls %s,%s,#%d\n",regname[rt],regname[rs],imm);
  assert(0);
}

static void emit_shrimm(u_int rs,u_int imm,u_int rt)
{
  assert(imm>0);
  assert(imm<32);
  assem_debug("lsr %s,%s,#%d\n",regname[rt],regname[rs],imm);
  assert(0);
}

static void emit_sarimm(u_int rs,u_int imm,u_int rt)
{
  assert(imm>0);
  assert(imm<32);
  assem_debug("asr %s,%s,#%d\n",regname[rt],regname[rs],imm);
  assert(0);
}

static void emit_rorimm(u_int rs,u_int imm,u_int rt)
{
  assert(imm>0);
  assert(imm<32);
  assem_debug("ror %s,%s,#%d\n",regname[rt],regname[rs],imm);
  assert(0);
}

static void emit_signextend16(u_int rs, u_int rt)
{
  assem_debug("sxth %s,%s\n", regname[rt], regname[rs]);
  assert(0);
}

static void emit_shl(u_int rs,u_int shift,u_int rt)
{
  assert(rs < 31);
  assert(rt < 31);
  assert(shift < 16);
  assert(0);
}

static void emit_shr(u_int rs,u_int shift,u_int rt)
{
  assert(rs < 31);
  assert(rt < 31);
  assert(shift<16);
  assem_debug("lsr %s,%s,%s\n",regname[rt],regname[rs],regname[shift]);
  assert(0);
}

static void emit_sar(u_int rs,u_int shift,u_int rt)
{
  assert(rs < 31);
  assert(rt < 31);
  assert(shift<16);
  assem_debug("asr %s,%s,%s\n",regname[rt],regname[rs],regname[shift]);
  assert(0);
}

static void emit_orrshl(u_int rs,u_int shift,u_int rt)
{
  assert(rs < 31);
  assert(rt < 31);
  assert(shift<16);
  assem_debug("orr %s,%s,%s,lsl %s\n",regname[rt],regname[rt],regname[rs],regname[shift]);
  assert(0);
}

static void emit_orrshr(u_int rs,u_int shift,u_int rt)
{
  assert(rs < 31);
  assert(rt < 31);
  assert(shift<16);
  assem_debug("orr %s,%s,%s,lsr %s\n",regname[rt],regname[rt],regname[rs],regname[shift]);
  assert(0);
}

static void emit_cmpimm(u_int rs,int imm)
{
  assert(0);
}

static void emit_cmovne_imm(int imm,u_int rt)
{
  assem_debug("movne %s,#%d\n",regname[rt],imm);
  assert(0);
}

static void emit_cmovl_imm(int imm,u_int rt)
{
  assem_debug("movlt %s,#%d\n",regname[rt],imm);
  assert(0);
}

static void emit_cmovb_imm(int imm,u_int rt)
{
  assem_debug("movcc %s,#%d\n",regname[rt],imm);
  assert(0);
}

static void emit_cmovs_imm(int imm,u_int rt)
{
  assem_debug("movmi %s,#%d\n",regname[rt],imm);
  assert(0);
}

static void emit_cmovne_reg(u_int rs,u_int rt)
{
  assem_debug("movne %s,%s\n",regname[rt],regname[rs]);
  assert(0);
}

static void emit_cmovl_reg(u_int rs,u_int rt)
{
  assem_debug("movlt %s,%s\n",regname[rt],regname[rs]);
  assert(0);
}

static void emit_cmovs_reg(u_int rs,u_int rt)
{
  assem_debug("movmi %s,%s\n",regname[rt],regname[rs]);
  assert(0);
}

static void emit_slti32(u_int rs,int imm,u_int rt)
{
  if(rs!=rt) emit_zeroreg(rt);
  emit_cmpimm(rs,imm);
  if(rs==rt) emit_movimm(0,rt);
  emit_cmovl_imm(1,rt);
}

static void emit_sltiu32(u_int rs,int imm,u_int rt)
{
  if(rs!=rt) emit_zeroreg(rt);
  emit_cmpimm(rs,imm);
  if(rs==rt) emit_movimm(0,rt);
  emit_cmovb_imm(1,rt);
}

static void emit_cmp(u_int rs,u_int rt)
{
  assem_debug("cmp %s,%s\n",regname[rs],regname[rt]);
  assert(0);
}

static void emit_set_gz32(u_int rs, u_int rt)
{
  //assem_debug("set_gz32\n");
  emit_cmpimm(rs,1);
  emit_movimm(1,rt);
  emit_cmovl_imm(0,rt);
}

static void emit_set_nz32(u_int rs, u_int rt)
{
  //assem_debug("set_nz32\n");
  assert(0);
}

static void emit_set_if_less32(u_int rs1, u_int rs2, u_int rt)
{
  //assem_debug("set if less (%%%s,%%%s),%%%s\n",regname[rs1],regname[rs2],regname[rt]);
  if(rs1!=rt&&rs2!=rt) emit_zeroreg(rt);
  emit_cmp(rs1,rs2);
  if(rs1==rt||rs2==rt) emit_movimm(0,rt);
  emit_cmovl_imm(1,rt);
}

static void emit_set_if_carry32(u_int rs1, u_int rs2, u_int rt)
{
  //assem_debug("set if carry (%%%s,%%%s),%%%s\n",regname[rs1],regname[rs2],regname[rt]);
  if(rs1!=rt&&rs2!=rt) emit_zeroreg(rt);
  emit_cmp(rs1,rs2);
  if(rs1==rt||rs2==rt) emit_movimm(0,rt);
  emit_cmovb_imm(1,rt);
}

#pragma GCC diagnostic ignored "-Wunused-variable"
static void emit_call(const void *a_)
{
  uintptr_t a = (uintptr_t)a_;
  assem_debug("bl %p (%p+%lx)%s\n", a_, out, (u_char *)a_ - out, func_name(a));
  assert(0);
}

static void emit_jmp(const void *a_)
{
  uintptr_t a = (uintptr_t)a_;
  assem_debug("b %p (%p+%lx)%s\n", a_, out, (u_char *)a_ - out, func_name(a));
  assert(0);
}

static void emit_jne(const void *a_)
{
  uintptr_t a = (uintptr_t)a_;
  assem_debug("bne %p\n", a_);
  assert(0);
}

static void emit_jeq(int a)
{
  assem_debug("beq %x\n",a);
  assert(0);
}

static void emit_js(int a)
{
  assem_debug("bmi %x\n",a);
  assert(0);
}

static void emit_jns(int a)
{
  assem_debug("bpl %x\n",a);
  assert(0);
}

static void emit_jl(int a)
{
  assem_debug("blt %x\n",a);
  assert(0);
}

static void emit_jge(int a)
{
  assem_debug("bge %x\n",a);
  assert(0);
}

static void emit_jno(int a)
{
  assem_debug("bvc %x\n",a);
  assert(0);
}

static void emit_jc(int a)
{
  assem_debug("bcs %x\n",a);
  assert(0);
}

static void emit_jcc(void *a_)
{
  uintptr_t a = (uintptr_t)a_;
  assem_debug("bcc %p\n", a_);
  assert(0);
}

static void emit_callreg(u_int r)
{
  assert(r < 31);
  assem_debug("blx %s\n", regname[r]);
  assert(0);
}

static void emit_jmpreg(u_int r)
{
  assem_debug("mov pc,%s\n",regname[r]);
  assert(0);
}

static void emit_retreg(u_int r)
{
  assem_debug("ret %s\n", r == LR ? "" : regname[r]);
  output_w32(0xd65f0000 | rm_rn_rd(0, r, 0));
}

static void emit_ret(void)
{
  emit_retreg(LR);
}

static void emit_readword_indexed(int offset, u_int rs, u_int rt)
{
  assem_debug("ldr %s,%s+%d\n",regname[rt],regname[rs],offset);
  assert(0);
}

static void emit_readword_dualindexedx4(u_int rs1, u_int rs2, u_int rt)
{
  assem_debug("ldr %s,%s,%s lsl #2\n",regname[rt],regname[rs1],regname[rs2]);
  assert(0);
}

static void emit_ldrcc_dualindexed(u_int rs1, u_int rs2, u_int rt)
{
  assem_debug("ldrcc %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  assert(0);
}

static void emit_ldrccb_dualindexed(u_int rs1, u_int rs2, u_int rt)
{
  assem_debug("ldrccb %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  assert(0);
}

static void emit_ldrccsb_dualindexed(u_int rs1, u_int rs2, u_int rt)
{
  assem_debug("ldrccsb %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  assert(0);
}

static void emit_ldrcch_dualindexed(u_int rs1, u_int rs2, u_int rt)
{
  assem_debug("ldrcch %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  assert(0);
}

static void emit_ldrccsh_dualindexed(u_int rs1, u_int rs2, u_int rt)
{
  assem_debug("ldrccsh %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  assert(0);
}

static void emit_movsbl_indexed(int offset, u_int rs, u_int rt)
{
  assem_debug("ldrsb %s,%s+%d\n",regname[rt],regname[rs],offset);
  assert(0);
}

static void emit_movswl_indexed(int offset, u_int rs, u_int rt)
{
  assem_debug("ldrsh %s,%s+%d\n",regname[rt],regname[rs],offset);
  assert(0);
}

static void emit_movzbl_indexed(int offset, u_int rs, u_int rt)
{
  assem_debug("ldrb %s,%s+%d\n",regname[rt],regname[rs],offset);
  assert(0);
}

static void emit_movzwl_indexed(int offset, u_int rs, u_int rt)
{
  assem_debug("ldrh %s,%s+%d\n",regname[rt],regname[rs],offset);
  assert(0);
}

static void emit_ldrd(int offset, u_int rs, u_int rt)
{
  assem_debug("ldrd %s,%s+%d\n",regname[rt],regname[rs],offset);
  assert(0);
}

static void emit_readword(void *addr, u_int rt)
{
  uintptr_t offset = (u_char *)addr - (u_char *)&dynarec_local;
  assert(offset<4096);
  assem_debug("ldr %s,fp+%lx\n", regname[rt], offset);
  assert(0);
}

static void emit_writeword_indexed(u_int rt, int offset, u_int rs)
{
  assert(offset>-4096&&offset<4096);
  assem_debug("str %s,%s+%x\n",regname[rt],regname[rs],offset);
  assert(0);
}

static void emit_writehword_indexed(u_int rt, int offset, u_int rs)
{
  assert(offset>-256&&offset<256);
  assem_debug("strh %s,%s+%d\n",regname[rt],regname[rs],offset);
  assert(0);
}

static void emit_writebyte_indexed(u_int rt, int offset, u_int rs)
{
  assert(offset>-4096&&offset<4096);
  assem_debug("strb %s,%s+%d\n",regname[rt],regname[rs],offset);
  assert(0);
}

static void emit_strcc_dualindexed(u_int rs1, u_int rs2, u_int rt)
{
  assem_debug("strcc %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  assert(0);
}

static void emit_strccb_dualindexed(u_int rs1, u_int rs2, u_int rt)
{
  assem_debug("strccb %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  assert(0);
}

static void emit_strcch_dualindexed(u_int rs1, u_int rs2, u_int rt)
{
  assem_debug("strcch %s,%s,%s\n",regname[rt],regname[rs1],regname[rs2]);
  assert(0);
}

static void emit_writeword(u_int rt, void *addr)
{
  uintptr_t offset = (u_char *)addr - (u_char *)&dynarec_local;
  assert(offset<4096);
  assem_debug("str %s,fp+%lx\n", regname[rt], offset);
  assert(0);
}

static void emit_umull(u_int rs1, u_int rs2, u_int hi, u_int lo)
{
  assem_debug("umull %s, %s, %s, %s\n",regname[lo],regname[hi],regname[rs1],regname[rs2]);
  assert(rs1<16);
  assert(rs2<16);
  assert(hi<16);
  assert(lo<16);
  assert(0);
}

static void emit_smull(u_int rs1, u_int rs2, u_int hi, u_int lo)
{
  assem_debug("smull %s, %s, %s, %s\n",regname[lo],regname[hi],regname[rs1],regname[rs2]);
  assert(rs1<16);
  assert(rs2<16);
  assert(hi<16);
  assert(lo<16);
  assert(0);
}

static void emit_clz(u_int rs,u_int rt)
{
  assem_debug("clz %s,%s\n",regname[rt],regname[rs]);
  assert(0);
}

// Load 2 immediates optimizing for small code size
static void emit_mov2imm_compact(int imm1,u_int rt1,int imm2,u_int rt2)
{
  assert(0);
}

// Conditionally select one of two immediates, optimizing for small code size
// This will only be called if HAVE_CMOV_IMM is defined
static void emit_cmov2imm_e_ne_compact(int imm1,int imm2,u_int rt)
{
  assert(0);
}

// special case for checking invalid_code
static void emit_cmpmem_indexedsr12_reg(int base,u_int r,int imm)
{
  assert(imm<128&&imm>=0);
  assert(r>=0&&r<16);
  assem_debug("ldrb lr,%s,%s lsr #12\n",regname[base],regname[r]);
  assert(0);
}

// Used to preload hash table entries
static unused void emit_prefetchreg(u_int r)
{
  assem_debug("pld %s\n",regname[r]);
  assert(0);
}

// Special case for mini_ht
static void emit_ldreq_indexed(u_int rs, u_int offset, u_int rt)
{
  assert(offset<4096);
  assem_debug("ldreq %s,[%s, #%d]\n",regname[rt],regname[rs],offset);
  assert(0);
}

static void emit_orrne_imm(u_int rs,int imm,u_int rt)
{
  assem_debug("orrne %s,%s,#%d\n",regname[rt],regname[rs],imm);
  assert(0);
}

static void emit_andne_imm(u_int rs,int imm,u_int rt)
{
  assem_debug("andne %s,%s,#%d\n",regname[rt],regname[rs],imm);
  assert(0);
}

static unused void emit_addpl_imm(u_int rs,int imm,u_int rt)
{
  assem_debug("addpl %s,%s,#%d\n",regname[rt],regname[rs],imm);
  assert(0);
}

static void save_regs_all(u_int reglist)
{
  if(!reglist) return;
  assert(0);
}

static void restore_regs_all(u_int reglist)
{
  if(!reglist) return;
  assert(0);
}

// Save registers before function call
static void save_regs(u_int reglist)
{
  reglist &= CALLER_SAVE_REGS; // only save the caller-save registers
  save_regs_all(reglist);
}

// Restore registers after function call
static void restore_regs(u_int reglist)
{
  reglist &= CALLER_SAVE_REGS;
  restore_regs_all(reglist);
}

/* Stubs/epilogue */

static void literal_pool(int n)
{
  (void)literals;
}

static void literal_pool_jumpover(int n)
{
}

static void emit_extjump2(u_char *addr, int target, void *linker)
{
  assert(0);
}

static void emit_extjump(void *addr, int target)
{
  emit_extjump2(addr, target, dyna_linker);
}

static void emit_extjump_ds(void *addr, int target)
{
  emit_extjump2(addr, target, dyna_linker_ds);
}

// put rt_val into rt, potentially making use of rs with value rs_val
static void emit_movimm_from(u_int rs_val,u_int rs,u_int rt_val,u_int rt)
{
  assert(0);
}

// return 1 if above function can do it's job cheaply
static int is_similar_value(u_int v1,u_int v2)
{
  assert(0);
  return 0;
}

//#include "pcsxmem.h"
//#include "pcsxmem_inline.c"

static void do_readstub(int n)
{
  assem_debug("do_readstub %x\n",start+stubs[n].a*4);
  assert(0);
}

static void inline_readstub(enum stub_type type, int i, u_int addr, signed char regmap[], int target, int adj, u_int reglist)
{
  assert(0);
}

static void do_writestub(int n)
{
  assem_debug("do_writestub %x\n",start+stubs[n].a*4);
  assert(0);
}

static void inline_writestub(enum stub_type type, int i, u_int addr, signed char regmap[], int target, int adj, u_int reglist)
{
  assert(0);
}

static void do_unalignedwritestub(int n)
{
  assem_debug("do_unalignedwritestub %x\n",start+stubs[n].a*4);
  assert(0);
}

static void do_invstub(int n)
{
  assert(0);
}

void *do_dirty_stub(int i)
{
  assem_debug("do_dirty_stub %x\n",start+i*4);
  // Careful about the code output here, verify_dirty needs to parse it.
  assert(0);
  load_regs_entry(i);
  return NULL;
}

static void do_dirty_stub_ds()
{
  // Careful about the code output here, verify_dirty needs to parse it.
  assert(0);
}

/* Special assem */

#define shift_assemble shift_assemble_arm64

static void shift_assemble_arm64(int i,struct regstat *i_regs)
{
  assert(0);
}
#define loadlr_assemble loadlr_assemble_arm64

static void loadlr_assemble_arm64(int i,struct regstat *i_regs)
{
  assert(0);
}

static void c2op_assemble(int i,struct regstat *i_regs)
{
  assert(0);
}

static void multdiv_assemble_arm64(int i,struct regstat *i_regs)
{
  assert(0);
}
#define multdiv_assemble multdiv_assemble_arm64

static void do_preload_rhash(u_int r) {
  // Don't need this for ARM.  On x86, this puts the value 0xf8 into the
  // register.  On ARM the hash can be done with a single instruction (below)
}

static void do_preload_rhtbl(u_int ht) {
  emit_addimm(FP, (u_char *)&mini_ht - (u_char *)&dynarec_local, ht);
}

static void do_rhash(u_int rs,u_int rh) {
  emit_andimm(rs, 0xf8, rh);
}

static void do_miniht_load(int ht,u_int rh) {
  assem_debug("ldr %s,[%s,%s]!\n",regname[rh],regname[ht],regname[rh]);
  assert(0);
}

static void do_miniht_jump(u_int rs,u_int rh,int ht) {
  emit_cmp(rh,rs);
  emit_ldreq_indexed(ht,4,15);
  //emit_jmp(jump_vaddr_reg[rs]);
  assert(0);
}

static void do_miniht_insert(u_int return_address,u_int rt,int temp) {
  assert(0);
}

static void mark_clear_cache(void *target)
{
  u_long offset = (u_char *)target - translation_cache;
  u_int mask = 1u << ((offset >> 12) & 31);
  if (!(needs_clear_cache[offset >> 17] & mask)) {
    char *start = (char *)((u_long)target & ~4095ul);
    start_tcache_write(start, start + 4096);
    needs_clear_cache[offset >> 17] |= mask;
  }
}

// Clearing the cache is rather slow on ARM Linux, so mark the areas
// that need to be cleared, and then only clear these areas once.
static void do_clear_cache()
{
  int i,j;
  for (i=0;i<(1<<(TARGET_SIZE_2-17));i++)
  {
    u_int bitmap=needs_clear_cache[i];
    if(bitmap) {
      u_char *start, *end;
      for(j=0;j<32;j++)
      {
        if(bitmap&(1<<j)) {
          start=translation_cache+i*131072+j*4096;
          end=start+4095;
          j++;
          while(j<32) {
            if(bitmap&(1<<j)) {
              end+=4096;
              j++;
            }else{
              end_tcache_write(start, end);
              break;
            }
          }
        }
      }
      needs_clear_cache[i]=0;
    }
  }
}

// CPU-architecture-specific initialization
static void arch_init() {
}

// vim:shiftwidth=2:expandtab
