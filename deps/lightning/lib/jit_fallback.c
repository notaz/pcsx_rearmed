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
		stxi(_jitc->function->regoff[JIT_R(offset)], rn(JIT_FP), regno);
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
		ldxi(regno, rn(JIT_FP), _jitc->function->regoff[JIT_R(offset)]);
	    break;
	}
    }
}

static void
_fallback_save_regs(jit_state_t *_jit, jit_int32_t r0)
{
    jit_int32_t		regno, spec;
    for (regno = 0; regno < _jitc->reglen; regno++) {
	spec =  _rvs[regno].spec;
	if ((jit_regset_tstbit(&_jitc->regarg, regno) ||
	     jit_regset_tstbit(&_jitc->reglive, regno)) &&
	    !(spec & jit_class_sav)) {
	    if (!_jitc->function->regoff[regno]) {
		_jitc->function->regoff[regno] =
		    jit_allocai(spec & jit_class_gpr ?
				sizeof(jit_word_t) : sizeof(jit_float64_t));
		_jitc->again = 1;
	    }
	    if ((spec & jit_class_gpr) && rn(regno) == r0)
		continue;
	    jit_regset_setbit(&_jitc->regsav, regno);
	    if (spec & jit_class_gpr)
		emit_stxi(_jitc->function->regoff[regno], JIT_FP, regno);
	    else
		emit_stxi_d(_jitc->function->regoff[regno], JIT_FP, regno);
	}
    }
}

static void
_fallback_load_regs(jit_state_t *_jit, jit_int32_t r0)
{
    jit_int32_t		regno, spec;
    for (regno = 0; regno < _jitc->reglen; regno++) {
	spec =  _rvs[regno].spec;
	if ((jit_regset_tstbit(&_jitc->regarg, regno) ||
	     jit_regset_tstbit(&_jitc->reglive, regno)) &&
	    !(spec & jit_class_sav)) {
	    if ((spec & jit_class_gpr) && rn(regno) == r0)
		continue;
	    jit_regset_setbit(&_jitc->regsav, regno);
	    if (spec & jit_class_gpr)
		emit_ldxi(regno, JIT_FP, _jitc->function->regoff[regno]);
	    else
		emit_ldxi_d(regno, JIT_FP, _jitc->function->regoff[regno]);
	}
    }
}

static void
_fallback_calli(jit_state_t *_jit, jit_word_t i0, jit_word_t i1)
{
#  if defined(__arm__)
    movi(rn(_R0), i1);
#  elif defined(__ia64__)
    /* avoid confusion with pushargi patching */
    if (i1 >= -2097152 && i1 <= 2097151)
	MOVI(_jitc->rout, i1);
    else
	MOVL(_jitc->rout, i1);
#  elif defined(__hppa__)
    movi(_R26_REGNO, i1);
#endif
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
	r1_reg = jit_get_reg(jit_class_gpr|jit_class_sav);
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
#  if defined(__ia64__)
    sync();
# endif
    done = _jit->pc.w;
    fallback_calli((jit_word_t)pthread_mutex_unlock, (jit_word_t)&mutex);
    fallback_load(r0);
#  if defined(__arm__)
    patch_at(arm_patch_jump, jump, done);
#  elif defined(__ia64__)
    patch_at(jit_code_bnei, jump, done);
#  else
    patch_at(jump, done);
#  endif
    fallback_load_regs(r0);
    if (iscasi)
	jit_unget_reg(r1_reg);
}
#endif
