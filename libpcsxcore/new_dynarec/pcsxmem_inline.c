/*
 * (C) Gražvydas "notaz" Ignotas, 2011
 *
 * This work is licensed under the terms of GNU GPL version 2 or later.
 * See the COPYING file in the top-level directory.
 */

static int emit_ldr_type(int type, int offs, int rs, int rt)
{
  switch(type) {
    case LOADB_STUB:
      emit_movsbl_indexed(offs,rs,rt);
      break;
    case LOADBU_STUB:
      emit_movzbl_indexed(offs,rs,rt);
      break;
    case LOADH_STUB:
      emit_movswl_indexed(offs,rs,rt);
      break;
    case LOADHU_STUB:
      emit_movzwl_indexed(offs,rs,rt);
      break;
    case LOADW_STUB:
      emit_readword_indexed(offs,rs,rt);
      break;
    default:
      assert(0);
  }
}

static int emit_str_type(int type, int offs, int rs, int rt)
{
  switch(type) {
    case STOREB_STUB:
      emit_writebyte_indexed(rt,offs,rs);
      break;
    case STOREH_STUB:
      emit_writehword_indexed(rt,offs,rs);
      break;
    case STOREW_STUB:
      emit_writeword_indexed(rt,offs,rs);
      break;
    default:
      assert(0);
  }
}

static void convert_ram_addr(u_int a_rs, u_int a_rt, int rs, int rt)
{
  if(rs<0)
    emit_movimm(a_rt,rt);
  else if((a_rs&~0x60000000)==a_rt)
    emit_andimm(rs,~0x60000000,rt);
  else if((a_rs&~0x00600000)==a_rt)
    emit_andimm(rs,~0x00600000,rt);
  else
    emit_movimm(a_rt,rt);
}

static int pcsx_direct_read(int type, u_int addr, int rs, int rt)
{
  if((addr & 0x1f800000) == 0) {
    assem_debug("pcsx_direct_read %08x ram\n",addr);
    if(rt<0)
      return 1;
    u_int a=(addr&~0x60600000)|0x80000000;
    convert_ram_addr(addr,a,rs,rt);
    emit_ldr_type(type,0,rt,rt);
    return 1;
  }
  if((addr & 0x1ff80000) == 0x1fc00000) {
    assem_debug("pcsx_direct_read %08x bios\n",addr);
    if(rt<0)
      return 1;
    emit_movimm((u_int)&psxR[addr&0x7ffff],rt);
    emit_ldr_type(type,0,rt,rt);
    return 1;
  }
  if((addr & 0xfffff000) == 0x1f800000) {
    assem_debug("pcsx_direct_read %08x scratchpad\n",addr);
    if(rt<0)
      return 1;
    if(type==LOADW_STUB||type==LOADBU_STUB||(addr&0xf00)==0) {
      emit_readword((int)&psxH_ptr,rt);
      emit_ldr_type(type,addr&0xfff,rt,rt);
    } else {
      emit_movimm((u_int)&psxH[addr&0xfff],rt);
      emit_ldr_type(type,0,rt,rt);
    }
    return 1;
  }

  assem_debug("pcsx_direct_read %08x miss\n",addr);
  return 0;
}

static int pcsx_direct_write(int type, u_int addr, int rs, int rt, signed char *regmap)
{
  if((addr & 0x1f800000) == 0) {
    assem_debug("pcsx_direct_write %08x ram\n",addr);
    u_int a=(addr&~0x60600000)|0x80000000;
    convert_ram_addr(addr,a,rs,HOST_TEMPREG);
    emit_str_type(type,0,HOST_TEMPREG,rt);

    int ir=get_reg(regmap,INVCP);
    assert(ir>=0);
    emit_cmpmem_indexedsr12_reg(ir,rs,1);
    emit_callne(invalidate_addr_reg[rs]);
    return 1;
  }
  if((addr & 0xfffff000) == 0x1f800000) {
    assem_debug("pcsx_direct_write %08x scratchpad\n",addr);
    if(type==STOREW_STUB||type==STOREB_STUB||(addr&0xf00)==0) {
      emit_readword((int)&psxH_ptr,HOST_TEMPREG);
      emit_str_type(type,addr&0xfff,HOST_TEMPREG,rt);
    } else {
      emit_movimm((u_int)&psxH[addr&0xfff],HOST_TEMPREG);
      emit_str_type(type,0,HOST_TEMPREG,rt);
    }
    return 1;
  }

  assem_debug("pcsx_direct_write %08x miss\n",addr);
  return 0;
}

// vim:shiftwidth=2:expandtab
