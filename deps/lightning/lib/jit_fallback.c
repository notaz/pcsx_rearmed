#if PROTO
#define fallback_save(r0)		_fallback_save(_jit, r0)
static void _fallback_save(jit_state_t*, jit_int32_t);
#define fallback_load(r0)		_fallback_load(_jit, r0)
static void _fallback_load(jit_state_t*, jit_int32_t);
#define fallback_save_regs(r0)		_fallback_save_regs(_jit, r0)
static void _fallback_save_regs(jit_state_t*, jit_int32_t);
#define fallback_load_regs(r0)		_fallback_load_regs(_jit, r0)
static void _fallback_load_regs(jit_state_t*, jit_int32_t);
#define fallback_calli(i0, i1)		_fallback_calli(_jit, i0, i1)
static void _fallback_calli(jit_state_t*, jit_word_t, jit_word_t);
#define fallback_casx(r0,r1,r2,r3,im)	_fallback_casx(_jit,r0,r1,r2,r3,im)
static void _fallback_casx(jit_state_t *, jit_int32_t, jit_int32_t,
			   jit_int32_t, jit_int32_t, jit_word_t);
#endif

#if CODE
static void
_fallback_save(jit_state_t *_jit, jit_int32_t r0)
{
    jit_int32_t		offset, regno, spec;
    for (offset = 0; offset < JIT_R_NUM; offset++) {
	spec =  _rvs[offset].spec;
	regno = jit_regno(spec);
	if (regno == r0) {
	    if (!(spec & jit_class_sav))
		stxi(_jitc->function->regoff[offset], rn(JIT_FP), regno);
	    break;
	}
    }
}

static void
_fallback_load(jit_state_t *_jit, jit_int32_t r0)
{
    jit_int32_t		offset, regno, spec;
    for (offset = 0; offset < JIT_R_NUM; offset++) {
	spec =  _rvs[offset].spec;
	regno = jit_regno(spec);
	if (regno == r0) {
	    if (!(spec & jit_class_sav))
		ldxi(regno, rn(JIT_FP), _jitc->function->regoff[offset]);
	    break;
	}
    }
}

static void
_fallback_save_regs(jit_state_t *_jit, jit_int32_t r0)
{
    jit_int32_t		offset, regno, spec;
    for (offset = 0; offset < JIT_R_NUM; offset++) {
	regno = JIT_R(offset);
	spec =  _rvs[regno].spec;
	if ((spec & jit_class_gpr) && regno == r0)
	    continue;
	if (!(spec & jit_class_sav)) {
	    if (!_jitc->function->regoff[regno]) {
		_jitc->function->regoff[regno] =
		    jit_allocai(sizeof(jit_word_t));
		_jitc->again = 1;
	    }
	    jit_regset_setbit(&_jitc->regsav, regno);
	    emit_stxi(_jitc->function->regoff[regno], JIT_FP, regno);
	}
    }
    /* If knew for certain float registers are not used by
     * pthread_mutex_lock and pthread_mutex_unlock, could skip this */
    for (offset = 0; offset < JIT_F_NUM; offset++) {
	regno = JIT_F(offset);
	spec =  _rvs[regno].spec;
	if (!(spec & jit_class_sav)) {
	    if (!_jitc->function->regoff[regno]) {
		_jitc->function->regoff[regno] =
		    jit_allocai(sizeof(jit_word_t));
		_jitc->again = 1;
	    }
	    jit_regset_setbit(&_jitc->regsav, regno);
	    emit_stxi_d(_jitc->function->regoff[regno], JIT_FP, regno);
	}
    }
}

static void
_fallback_load_regs(jit_state_t *_jit, jit_int32_t r0)
{
    jit_int32_t		offset, regno, spec;
    for (offset = 0; offset < JIT_R_NUM; offset++) {
	regno = JIT_R(offset);
	spec =  _rvs[regno].spec;
	if ((spec & jit_class_gpr) && regno == r0)
	    continue;
	if (!(spec & jit_class_sav)) {
	    jit_regset_clrbit(&_jitc->regsav, regno);
	    emit_ldxi(regno, JIT_FP, _jitc->function->regoff[regno]);
	}
    }
    /* If knew for certain float registers are not used by
     * pthread_mutex_lock and pthread_mutex_unlock, could skip this */
    for (offset = 0; offset < JIT_F_NUM; offset++) {
	regno = JIT_F(offset);
	spec =  _rvs[regno].spec;
	if (!(spec & jit_class_sav)) {
	    jit_regset_clrbit(&_jitc->regsav, regno);
	    emit_ldxi_d(regno, JIT_FP, _jitc->function->regoff[regno]);
	}
    }
}

static void
_fallback_calli(jit_state_t *_jit, jit_word_t i0, jit_word_t i1)
{
#  if defined(__mips__)
    movi(rn(_A0), i1);
#  elif defined(__arm__)
    movi(rn(_R0), i1);
#  elif defined(__sparc__)
    movi(rn(_O0), i1);
#  elif defined(__ia64__)
    /* avoid confusion with pushargi patching */
    if (i1 >= -2097152 && i1 <= 2097151)
	MOVI(_jitc->rout, i1);
    else
	MOVL(_jitc->rout, i1);
#  elif defined(__hppa__)
    movi(_R26_REGNO, i1);
#  elif defined(__s390__) || defined(__s390x__)
    movi(rn(_R2), i1);
#  elif defined(__alpha__)
    movi(rn(_A0), i1);
#  elif defined(__riscv__)
    movi(rn(JIT_RA0), i1);
#  endif
    calli(i0);
}

static void
_fallback_casx(jit_state_t *_jit, jit_int32_t r0, jit_int32_t r1,
	       jit_int32_t r2, jit_int32_t r3, jit_word_t i0)
{
    jit_int32_t		r1_reg, iscasi;
    jit_word_t		jump, done;
    /* XXX only attempts to fallback cas for lightning jit code */
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    if ((iscasi = r1 == _NOREG)) {
	r1_reg = jit_get_reg(jit_class_gpr);
	r1 = rn(r1_reg);
	movi(r1, i0);
    }
    fallback_save_regs(r0);
    fallback_calli((jit_word_t)pthread_mutex_lock, (jit_word_t)&mutex);
    fallback_load(r1);
    ldr(r0, r1);
    fallback_load(r2);
    eqr(r0, r0, r2);
    fallback_save(r0);
    jump = bnei(_jit->pc.w, r0, 1);
    fallback_load(r3);
#  if __WORDSIZE == 32
    str_i(r1, r3);
#  else
    str_l(r1, r3);
#  endif
    /* done: */
    done = _jit->pc.w;
    fallback_calli((jit_word_t)pthread_mutex_unlock, (jit_word_t)&mutex);
    fallback_load(r0);
#  if defined(__arm__)
    patch_at(arm_patch_jump, jump, done);
#  else
    patch_at(jump, done);
#  endif
    fallback_load_regs(r0);
    if (iscasi)
	jit_unget_reg(r1_reg);
}
#endif
