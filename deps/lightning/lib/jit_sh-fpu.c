/*
 * Copyright (C) 2022  Free Software Foundation, Inc.
 *
 * This file is part of GNU lightning.
 *
 * GNU lightning is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU lightning is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * Authors:
 *	Paul Cercueil
 */

#if PROTO
static void set_fmode(jit_state_t *_jit, jit_bool_t is_double);
static void set_fmode_no_r0(jit_state_t *_jit, jit_bool_t is_double);
static void reset_fpu(jit_state_t *_jit, jit_bool_t no_r0);

static void _extr_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_bool_t);
#  define extr_f(r0,r1)			_extr_f(_jit,r0,r1,0)
#  define extr_d(r0,r1)			_extr_f(_jit,r0,r1,1)
static void _truncr_f_i(jit_state_t*,jit_int16_t,jit_int16_t,jit_bool_t);
#  define truncr_f_i(r0,r1)		_truncr_f_i(_jit,r0,r1,0)
#  define truncr_d_i(r0,r1)		_truncr_f_i(_jit,r0,r1,1)
static void _fmar_f(jit_state_t*,jit_uint16_t,jit_uint16_t,
		    jit_uint16_t,jit_uint16_t);
#  define fmar_f(r0, r1, r2, r3)	_fmar_f(_jit, r0, r1, r2, r3)
static void _fmar_d(jit_state_t*,jit_uint16_t,jit_uint16_t,
		    jit_uint16_t,jit_uint16_t);
#  define fmar_d(r0, r1, r2, r3)	_fmar_d(_jit, r0, r1, r2, r3)
static void _fmsr_f(jit_state_t*,jit_uint16_t,jit_uint16_t,
		    jit_uint16_t,jit_uint16_t);
#  define fmsr_f(r0, r1, r2, r3)	_fmsr_f(_jit, r0, r1, r2, r3)
static void _fmsr_d(jit_state_t*,jit_uint16_t,jit_uint16_t,
		    jit_uint16_t,jit_uint16_t);
#  define fmsr_d(r0, r1, r2, r3)	_fmsr_d(_jit, r0, r1, r2, r3)
static void _fnmar_f(jit_state_t*,jit_uint16_t,jit_uint16_t,
		     jit_uint16_t,jit_uint16_t);
#  define fnmar_f(r0, r1, r2, r3)	_fnmar_f(_jit, r0, r1, r2, r3)
static void _fnmar_d(jit_state_t*,jit_uint16_t,jit_uint16_t,
		     jit_uint16_t,jit_uint16_t);
#  define fnmar_d(r0, r1, r2, r3)	_fnmar_d(_jit, r0, r1, r2, r3)
static void _fnmsr_f(jit_state_t*,jit_uint16_t,jit_uint16_t,
		     jit_uint16_t,jit_uint16_t);
#  define fnmsr_f(r0, r1, r2, r3)	_fnmsr_f(_jit, r0, r1, r2, r3)
static void _fnmsr_d(jit_state_t*,jit_uint16_t,jit_uint16_t,
		     jit_uint16_t,jit_uint16_t);
#  define fnmsr_d(r0, r1, r2, r3)	_fnmsr_d(_jit, r0, r1, r2, r3)
static void _movr_f(jit_state_t*,jit_uint16_t,jit_uint16_t);
#  define movr_f(r0,r1)			_movr_f(_jit,r0,r1)
static void _movr_d(jit_state_t*,jit_uint16_t,jit_uint16_t);
#  define movr_d(r0,r1)			_movr_d(_jit,r0,r1)
static void _movi_f(jit_state_t*,jit_uint16_t,jit_float32_t);
#  define movi_f(r0,i0)			_movi_f(_jit,r0,i0)
static void _movi_d(jit_state_t*,jit_uint16_t,jit_float64_t);
#  define movi_d(r0,i0)			_movi_d(_jit,r0,i0)
static void _ltr_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_int16_t,jit_bool_t);
#  define ltr_f(r0,r1,r2)		_ltr_f(_jit,r0,r1,r2,0)
#  define ltr_d(r0,r1,r2)		_ltr_f(_jit,r0,r1,r2,1)
static void _lti_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_float32_t);
#  define lti_f(r0,r1,i0)		_lti_f(_jit,r0,r1,i0)
static void _lti_d(jit_state_t*,jit_int16_t,jit_int16_t,jit_float64_t);
#  define lti_d(r0,r1,i0)		_lti_d(_jit,r0,r1,i0)
static void _ler_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_int16_t,jit_bool_t);
#  define ler_f(r0,r1,r2)		_ler_f(_jit,r0,r1,r2,0)
#  define ler_d(r0,r1,r2)		_ler_f(_jit,r0,r1,r2,1)
static void _lei_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_float32_t);
#  define lei_f(r0,r1,i0)		_lei_f(_jit,r0,r1,i0)
static void _lei_d(jit_state_t*,jit_int16_t,jit_int16_t,jit_float64_t);
#  define lei_d(r0,r1,i0)		_lei_d(_jit,r0,r1,i0)
static void _eqr_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_int16_t,jit_bool_t);
#  define eqr_f(r0,r1,r2)		_eqr_f(_jit,r0,r1,r2,0)
#  define eqr_d(r0,r1,r2)		_eqr_f(_jit,r0,r1,r2,1)
static void _eqi_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_float32_t);
#  define eqi_f(r0,r1,i0)		_eqi_f(_jit,r0,r1,i0)
static void _eqi_d(jit_state_t*,jit_int16_t,jit_int16_t,jit_float64_t);
#  define eqi_d(r0,r1,i0)		_eqi_d(_jit,r0,r1,i0)
#  define ger_f(r0,r1,r2)		ler_f(r0,r2,r1)
#  define ger_d(r0,r1,r2)		ler_d(r0,r2,r1)
static void _gei_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_float32_t);
#  define gei_f(r0,r1,i0)		_gei_f(_jit,r0,r1,i0)
static void _gei_d(jit_state_t*,jit_int16_t,jit_int16_t,jit_float64_t);
#  define gei_d(r0,r1,i0)		_gei_d(_jit,r0,r1,i0)
#  define gtr_f(r0,r1,r2)		ltr_f(r0,r2,r1)
#  define gtr_d(r0,r1,r2)		ltr_d(r0,r2,r1)
static void _gti_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_float32_t);
#  define gti_f(r0,r1,i0)		_gti_f(_jit,r0,r1,i0)
static void _gti_d(jit_state_t*,jit_int16_t,jit_int16_t,jit_float64_t);
#  define gti_d(r0,r1,i0)		_gti_d(_jit,r0,r1,i0)
static void _ner_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_int16_t,jit_bool_t);
#  define ner_f(r0,r1,r2)		_ner_f(_jit,r0,r1,r2,0)
#  define ner_d(r0,r1,r2)		_ner_f(_jit,r0,r1,r2,1)
static void _nei_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_float32_t);
#  define nei_f(r0,r1,i0)		_nei_f(_jit,r0,r1,i0)
static void _nei_d(jit_state_t*,jit_int16_t,jit_int16_t,jit_float64_t);
#  define nei_d(r0,r1,i0)		_nei_d(_jit,r0,r1,i0)
static void _unltr_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_int16_t,jit_bool_t);
#  define unltr_f(r0,r1,r2)		_unltr_f(_jit,r0,r1,r2,0)
#  define unltr_d(r0,r1,r2)		_unltr_f(_jit,r0,r1,r2,1)
static void _unlti_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_float32_t);
#  define unlti_f(r0,r1,i0)		_unlti_f(_jit,r0,r1,i0)
static void _unlti_d(jit_state_t*,jit_int16_t,jit_int16_t,jit_float64_t);
#  define unlti_d(r0,r1,i0)		_unlti_d(_jit,r0,r1,i0)
static void _unler_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_int16_t,
		     jit_bool_t);
#  define unler_f(r0,r1,r2)		_unler_f(_jit,r0,r1,r2,0)
#  define unler_d(r0,r1,r2)		_unler_f(_jit,r0,r1,r2,1)
static void _unlei_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_float32_t);
#  define unlei_f(r0,r1,i0)		_unlei_f(_jit,r0,r1,i0)
static void _unlei_d(jit_state_t*,jit_int16_t,jit_int16_t,jit_float64_t);
#  define unlei_d(r0,r1,i0)		_unlei_d(_jit,r0,r1,i0)
#  define ungtr_f(r0,r1,r2)		unltr_f(r0,r2,r1)
#  define ungtr_d(r0,r1,r2)		unltr_d(r0,r2,r1)
static void _ungti_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_float32_t);
#  define ungti_f(r0,r1,i0)		_ungti_f(_jit,r0,r1,i0)
static void _ungti_d(jit_state_t*,jit_int16_t,jit_int16_t,jit_float64_t);
#  define ungti_d(r0,r1,i0)		_ungti_d(_jit,r0,r1,i0)
#  define unger_f(r0,r1,r2)		_unler_f(_jit,r0,r2,r1,0)
#  define unger_d(r0,r1,r2)		_unler_f(_jit,r0,r2,r1,1)
static void _ungei_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_float32_t);
#  define ungei_f(r0,r1,i0)		_ungei_f(_jit,r0,r1,i0)
static void _ungei_d(jit_state_t*,jit_int16_t,jit_int16_t,jit_float64_t);
#  define ungei_d(r0,r1,i0)		_ungei_d(_jit,r0,r1,i0)
static void _uneqr_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_int16_t,
		     jit_bool_t);
#  define uneqr_f(r0,r1,r2)		_uneqr_f(_jit,r0,r1,r2,0)
#  define uneqr_d(r0,r1,r2)		_uneqr_f(_jit,r0,r1,r2,1)
static void _uneqi_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_float32_t);
#  define uneqi_f(r0,r1,i0)		_uneqi_f(_jit,r0,r1,i0)
static void _uneqi_d(jit_state_t*,jit_int16_t,jit_int16_t,jit_float64_t);
#  define uneqi_d(r0,r1,i0)		_uneqi_d(_jit,r0,r1,i0)
static void _ltgtr_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_int16_t,jit_bool_t);
#  define ltgtr_f(r0,r1,r2)		_ltgtr_f(_jit,r0,r1,r2,0)
#  define ltgtr_d(r0,r1,r2)		_ltgtr_f(_jit,r0,r1,r2,1)
static void _ltgti_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_float32_t);
#  define ltgti_f(r0,r1,i0)		_ltgti_f(_jit,r0,r1,i0)
static void _ltgti_d(jit_state_t*,jit_int16_t,jit_int16_t,jit_float64_t);
#  define ltgti_d(r0,r1,i0)		_ltgti_d(_jit,r0,r1,i0)
static void _ordr_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_int16_t,jit_bool_t);
#  define ordr_f(r0,r1,r2)		_ordr_f(_jit,r0,r1,r2,0)
#  define ordr_d(r0,r1,r2)		_ordr_f(_jit,r0,r1,r2,1)
static void _ordi_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_float32_t);
#  define ordi_f(r0,r1,i0)		_ordi_f(_jit,r0,r1,i0)
static void _ordi_d(jit_state_t*,jit_int16_t,jit_int16_t,jit_float64_t);
#  define ordi_d(r0,r1,i0)		_ordi_d(_jit,r0,r1,i0)
static void _unordr_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_int16_t,jit_bool_t);
#  define unordr_f(r0,r1,r2)		_unordr_f(_jit,r0,r1,r2,0)
#  define unordr_d(r0,r1,r2)		_unordr_f(_jit,r0,r1,r2,1)
static void _unordi_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_float32_t);
#  define unordi_f(r0,r1,i0)		_unordi_f(_jit,r0,r1,i0)
static void _unordi_d(jit_state_t*,jit_int16_t,jit_int16_t,jit_float64_t);
#  define unordi_d(r0,r1,i0)		_unordi_d(_jit,r0,r1,i0)
static void _addr_f(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_uint16_t,jit_bool_t);
#  define addr_f(r0,r1,r2)		_addr_f(_jit,r0,r1,r2,0)
#  define addr_d(r0,r1,r2)		_addr_f(_jit,r0,r1,r2,1)
static void _addi_f(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_float32_t);
#  define addi_f(r0,r1,i0)		_addi_f(_jit,r0,r1,i0)
static void _addi_d(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_float64_t);
#  define addi_d(r0,r1,i0)		_addi_d(_jit,r0,r1,i0)
static void _subr_f(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_uint16_t);
#  define subr_f(r0,r1,r2)		_subr_f(_jit,r0,r1,r2)
static void _subr_d(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_uint16_t);
#  define subr_d(r0,r1,r2)		_subr_d(_jit,r0,r1,r2)
static void _subi_f(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_float32_t);
#  define subi_f(r0,r1,i0)		_subi_f(_jit,r0,r1,i0)
static void _subi_d(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_float64_t);
#  define subi_d(r0,r1,i0)		_subi_d(_jit,r0,r1,i0)
static void _negr_f(jit_state_t*,jit_uint16_t,jit_uint16_t);
#  define negr_f(r0,r1)			_negr_f(_jit,r0,r1)
static void _negr_d(jit_state_t*,jit_uint16_t,jit_uint16_t);
#  define negr_d(r0,r1)			_negr_d(_jit,r0,r1)
#  define rsbr_f(r0,r1,r2)		subr_f(r0,r2,r1)
static void _rsbi_f(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_float32_t);
#  define rsbi_f(r0,r1,i0)		_rsbi_f(_jit,r0,r1,i0)
#  define rsbr_d(r0,r1,r2)		subr_d(r0,r2,r1)
static void _rsbi_d(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_float64_t);
#  define rsbi_d(r0,r1,i0)		_rsbi_d(_jit,r0,r1,i0)
static void _mulr_f(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_uint16_t);
#  define mulr_f(r0,r1,r2)		_mulr_f(_jit,r0,r1,r2)
static void _muli_f(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_float32_t);
#  define muli_f(r0,r1,i0)		_muli_f(_jit,r0,r1,i0)
static void _mulr_d(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_uint16_t);
#  define mulr_d(r0,r1,r2)		_mulr_d(_jit,r0,r1,r2)
static void _muli_d(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_float64_t);
#  define muli_d(r0,r1,i0)		_muli_d(_jit,r0,r1,i0)
static void _divr_f(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_uint16_t);
#  define divr_f(r0,r1,r2)		_divr_f(_jit,r0,r1,r2)
static void _divi_f(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_float32_t);
#  define divi_f(r0,r1,i0)		_divi_f(_jit,r0,r1,i0)
static void _divr_d(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_uint16_t);
#  define divr_d(r0,r1,r2)		_divr_d(_jit,r0,r1,r2)
static void _divi_d(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_float64_t);
#  define divi_d(r0,r1,i0)		_divi_d(_jit,r0,r1,i0)
static void _movr_w_f(jit_state_t*,jit_uint16_t,jit_int16_t);
#define movr_w_f(r0,r1)			_movr_w_f(_jit,r0,r1)
static void _movr_f_w(jit_state_t*,jit_uint16_t,jit_int16_t);
#define movr_f_w(r0,r1)			_movr_f_w(_jit,r0,r1)
static void _movi_w_f(jit_state_t*,jit_int16_t,jit_word_t);
# define movi_w_f(r0,i0)		_movi_w_f(_jit,r0,i0)
static void _movr_ww_d(jit_state_t*,jit_uint16_t,jit_int16_t, jit_int16_t);
# define movr_ww_d(r0,r1,r2)		_movr_ww_d(_jit,r0,r1,r2)
static void _movr_d_ww(jit_state_t*,jit_uint16_t,jit_int16_t, jit_int16_t);
# define movr_d_ww(r0,r1,r2)		_movr_d_ww(_jit,r0,r1,r2)
static void _movi_ww_d(jit_state_t*,jit_int16_t,jit_word_t, jit_word_t);
# define movi_ww_d(r0,i0,i1)		_movi_ww_d(_jit,r0,i0,i1)
static void _absr_f(jit_state_t*,jit_uint16_t,jit_uint16_t);
#  define absr_f(r0,r1)			_absr_f(_jit,r0,r1)
static void _absr_d(jit_state_t*,jit_uint16_t,jit_uint16_t);
#  define absr_d(r0,r1)			_absr_d(_jit,r0,r1)
static void _sqrtr_f(jit_state_t*,jit_uint16_t,jit_uint16_t);
#  define sqrtr_f(r0,r1)		_sqrtr_f(_jit,r0,r1)
static void _sqrtr_d(jit_state_t*,jit_uint16_t,jit_uint16_t);
#  define sqrtr_d(r0,r1)		_sqrtr_d(_jit,r0,r1)
static void _extr_d_f(jit_state_t*,jit_uint16_t,jit_uint16_t);
#  define extr_d_f(r0,r1)		_extr_d_f(_jit,r0,r1)
static void _extr_f_d(jit_state_t*,jit_uint16_t,jit_uint16_t);
#  define extr_f_d(r0,r1)		_extr_f_d(_jit,r0,r1)
#  define ldr_f(r0,r1)			LDF(r0,r1)
static void _ldr_d(jit_state_t*,jit_uint16_t,jit_uint16_t);
#  define ldr_d(r0,r1)			_ldr_d(_jit,r0,r1)
static void _ldi_f(jit_state_t*,jit_uint16_t,jit_word_t);
#  define ldi_f(r0,i0)			_ldi_f(_jit,r0,i0)
static void _ldi_d(jit_state_t*,jit_uint16_t,jit_word_t);
#  define ldi_d(r0,i0)			_ldi_d(_jit,r0,i0)
static void _ldxr_f(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_uint16_t);
#  define ldxr_f(r0,r1,r2)		_ldxr_f(_jit,r0,r1,r2)
static void _ldxr_d(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_uint16_t);
#  define ldxr_d(r0,r1,r2)		_ldxr_d(_jit,r0,r1,r2)
static void _ldxi_f(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_word_t);
#  define ldxi_f(r0,r1,i0)		_ldxi_f(_jit,r0,r1,i0)
static void _ldxi_d(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_word_t);
#  define ldxi_d(r0,r1,i0)		_ldxi_d(_jit,r0,r1,i0)
#  define unldr_x(r0,r1,i0)		fallback_unldr_x(r0,r1,i0)
#  define unldi_x(r0,i0,i1)		fallback_unldi_x(r0,i0,i1)
#  define str_f(r0,r1)			STF(r0,r1)
static void _str_d(jit_state_t*,jit_uint16_t,jit_uint16_t);
#  define str_d(r0,r1)			_str_d(_jit,r0,r1)
static void _sti_f(jit_state_t*,jit_word_t,jit_uint16_t);
#  define sti_f(i0,r0)			_sti_f(_jit,i0,r0)
static void _sti_d(jit_state_t*,jit_word_t,jit_uint16_t);
#  define sti_d(i0,r0)			_sti_d(_jit,i0,r0)
static void _stxr_f(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_uint16_t);
#  define stxr_f(r0,r1,r2)		_stxr_f(_jit,r0,r1,r2)
static void _stxr_d(jit_state_t*,jit_uint16_t,jit_uint16_t,jit_uint16_t);
#  define stxr_d(r0,r1,r2)		_stxr_d(_jit,r0,r1,r2)
static void _stxi_f(jit_state_t*,jit_word_t,jit_uint16_t,jit_uint16_t);
#  define stxi_f(i0,r0,r1)		_stxi_f(_jit,i0,r0,r1)
static void _stxi_d(jit_state_t*,jit_word_t,jit_uint16_t,jit_uint16_t);
#  define stxi_d(i0,r0,r1)		_stxi_d(_jit,i0,r0,r1)
#  define unstr_x(r0,r1,i0)		fallback_unstr_x(r0,r1,i0)
#  define unsti_x(i0,r0,i1)		fallback_unsti_x(i0,r0,i1)
static jit_word_t _beqr_f(jit_state_t*,jit_word_t,jit_uint16_t,jit_uint16_t,
			  jit_bool_t,jit_bool_t,jit_bool_t);
#  define beqr_f(i0,r0,r1)		beqr_f_p(i0,r0,r1,0)
#  define bner_f(i0,r0,r1)		bner_f_p(i0,r0,r1,0)
#  define beqr_d(i0,r0,r1)		beqr_f_p(i0,r0,r1,0)
#  define bner_d(i0,r0,r1)		bner_f_p(i0,r0,r1,0)
#  define beqr_f_p(i0,r0,r1,p)		_beqr_f(_jit,i0,r0,r1,0,1,p)
#  define bner_f_p(i0,r0,r1,p)		_beqr_f(_jit,i0,r0,r1,0,0,p)
#  define beqr_d_p(i0,r0,r1,p)		_beqr_f(_jit,i0,r0,r1,1,1,p)
#  define bner_d_p(i0,r0,r1,p)		_beqr_f(_jit,i0,r0,r1,1,0,p)
static jit_word_t _beqi_f(jit_state_t*,jit_word_t,jit_uint16_t,
			  jit_float32_t,jit_bool_t,jit_bool_t);
#  define beqi_f(i0,r0,i1)		beqi_f_p(i0,r0,i1,0)
#  define bnei_f(i0,r0,i1)		bnei_f_p(i0,r0,i1,0)
#  define beqi_f_p(i0,r0,i1,p)		_beqi_f(_jit,i0,r0,i1,1,p)
#  define bnei_f_p(i0,r0,i1,p)		_beqi_f(_jit,i0,r0,i1,0,p)
static jit_word_t _beqi_d(jit_state_t*,jit_word_t,jit_uint16_t,
			  jit_float64_t,jit_bool_t,jit_bool_t);
#  define beqi_d(i0,r0,i1)		beqi_d_p(i0,r0,i1,0)
#  define bnei_d(i0,r0,i1)		bnei_d_p(i0,r0,i1,0)
#  define beqi_d_p(i0,r0,i1,p)		_beqi_d(_jit,i0,r0,i1,1,p)
#  define bnei_d_p(i0,r0,i1,p)		_beqi_d(_jit,i0,r0,i1,0,p)
static jit_word_t
_blti_f(jit_state_t*,jit_word_t,jit_int16_t,jit_float32_t,jit_bool_t);
#  define blti_f(i0,r0,i1)		blti_f_p(i0,r0,i1,0)
#  define blti_f_p(i0,r0,i1,p)		_blti_f(_jit,i0,r0,i1,p)
static jit_word_t
_blti_d(jit_state_t*,jit_word_t,jit_int16_t,jit_float64_t,jit_bool_t);
#  define blti_d(i0,r0,i1)		blti_d_p(i0,r0,i1,0)
#  define blti_d_p(i0,r0,i1,p)		_blti_d(_jit,i0,r0,i1,p)
static jit_word_t _bgtr_f(jit_state_t*,jit_word_t,jit_int16_t,jit_int16_t,
			  jit_bool_t,jit_bool_t,jit_bool_t);
#  define bgtr_f(i0,r0,r1)		bgtr_f_p(i0,r0,r1,0)
#  define bgtr_d(i0,r0,r1)		bgtr_d_p(i0,r0,r1,0)
#  define bltr_f(i0,r0,r1)		bltr_f_p(i0,r1,r0,0)
#  define bltr_d(i0,r0,r1)		bltr_d_p(i0,r1,r0,0)
#  define bgtr_f_p(i0,r0,r1,p)		_bgtr_f(_jit,i0,r0,r1,0,1,p)
#  define bgtr_d_p(i0,r0,r1,p)		_bgtr_f(_jit,i0,r0,r1,1,1,p)
#  define bltr_f_p(i0,r0,r1,p)		_bgtr_f(_jit,i0,r1,r0,0,1,p)
#  define bltr_d_p(i0,r0,r1,p)		_bgtr_f(_jit,i0,r1,r0,1,1,p)
static jit_word_t
_bgti_f(jit_state_t*,jit_word_t,jit_int16_t,jit_float32_t,jit_bool_t);
#  define bgti_f(i0,r0,i1)		bgti_f_p(i0,r0,i1,0)
#  define bgti_f_p(i0,r0,i1,p)		_bgti_f(_jit,i0,r0,i1,p)
static jit_word_t
_bgti_d(jit_state_t*,jit_word_t,jit_int16_t,jit_float64_t,jit_bool_t);
#  define bgti_d(i0,r0,i1)		bgti_d_p(i0,r0,i1,0)
#  define bgti_d_p(i0,r0,i1,p)		_bgti_d(_jit,i0,r0,i1,p)
static jit_word_t _bler_f(jit_state_t*,jit_word_t,jit_int16_t,jit_int16_t,
			  jit_bool_t,jit_bool_t,jit_bool_t);
#  define bler_f(i0,r0,r1)		bler_f_p(i0,r0,r1,0)
#  define bler_d(i0,r0,r1)		bler_d_p(i0,r0,r1,0)
#  define bler_f_p(i0,r0,r1,p)		_bler_f(_jit,i0,r0,r1,0,0,p)
#  define bler_d_p(i0,r0,r1,p)		_bler_f(_jit,i0,r0,r1,1,0,p)
static jit_word_t
_blei_f(jit_state_t*,jit_word_t,jit_int16_t,jit_float32_t,jit_bool_t);
#  define blei_f(i0,r0,i1)		blei_f_p(i0,r0,i1,0)
#  define blei_f_p(i0,r0,i1,p)		_blei_f(_jit,i0,r0,i1,p)
static jit_word_t
_blei_d(jit_state_t*,jit_word_t,jit_int16_t,jit_float64_t,jit_bool_t);
#  define blei_d(i0,r0,i1)		blei_d_p(i0,r0,i1,0)
#  define blei_d_p(i0,r0,i1,p)		_blei_d(_jit,i0,r0,i1,p)
#  define bger_f(i0,r0,r1)		bger_f_p(i0,r1,r0,0)
#  define bger_d(i0,r0,r1)		bger_d_p(i0,r1,r0,0)
#  define bger_f_p(i0,r0,r1,p)		bler_f_p(i0,r1,r0,p)
#  define bger_d_p(i0,r0,r1,p)		bler_d_p(i0,r1,r0,p)
static jit_word_t
_bgei_f(jit_state_t*,jit_word_t,jit_int16_t,jit_float32_t,jit_bool_t);
#  define bgei_f(i0,r0,i1)		bgei_f_p(i0,r0,i1,0)
#  define bgei_f_p(i0,r0,i1,p)		_bgei_f(_jit,i0,r0,i1,p)
static jit_word_t
_bgei_d(jit_state_t*,jit_word_t,jit_int16_t,jit_float64_t,jit_bool_t);
#  define bgei_d(i0,r0,i1)		bgei_d_p(i0,r0,i1,0)
#  define bgei_d_p(i0,r0,i1,p)		_bgei_d(_jit,i0,r0,i1,p)
#  define bunltr_f(i0,r0,r1)		bunltr_f_p(i0,r1,r0,0)
#  define bunltr_d(i0,r0,r1)		bunltr_d_p(i0,r1,r0,0)
#  define bunltr_f_p(i0,r0,r1,p)	_bler_f(_jit,i0,r1,r0,0,1,p)
#  define bunltr_d_p(i0,r0,r1,p)	_bler_f(_jit,i0,r1,r0,1,1,p)
static jit_word_t
_bunlti_f(jit_state_t*,jit_word_t,jit_int16_t,jit_float32_t,jit_bool_t);
#  define bunlti_f(i0,r0,i1)		bunlti_f_p(i0,r0,i1,0)
#  define bunlti_f_p(i0,r0,i1,p)	_bunlti_f(_jit,i0,r0,i1,p)
static jit_word_t
_bunlti_d(jit_state_t*,jit_word_t,jit_int16_t,jit_float64_t,jit_bool_t);
#  define bunlti_d(i0,r0,i1)		bunlti_d_p(i0,r0,i1,0)
#  define bunlti_d_p(i0,r0,i1,p)	_bunlti_d(_jit,i0,r0,i1,p)
#  define bunler_f(i0,r0,r1)		bunler_f_p(i0,r0,r1,0)
#  define bunler_d(i0,r0,r1)		bunler_d_p(i0,r0,r1,0)
#  define bunler_f_p(i0,r0,r1,p)	_bgtr_f(_jit,i0,r0,r1,0,0,p)
#  define bunler_d_p(i0,r0,r1,p)	_bgtr_f(_jit,i0,r0,r1,1,0,p)
static jit_word_t
_bunlei_f(jit_state_t*,jit_word_t,jit_int16_t,jit_float32_t,jit_bool_t);
#  define bunlei_f(i0,r0,i1)		bunlei_f_p(i0,r0,i1,0)
#  define bunlei_f_p(i0,r0,i1,p)	_bunlei_f(_jit,i0,r0,i1,p)
static jit_word_t
_bunlei_d(jit_state_t*,jit_word_t,jit_int16_t,jit_float64_t,jit_bool_t);
#  define bunlei_d(i0,r0,i1)		bunlei_d_p(i0,r0,i1,0)
#  define bunlei_d_p(i0,r0,i1,p)	_bunlei_d(_jit,i0,r0,i1,p)
#  define bungtr_f(i0,r0,r1)		bungtr_f_p(i0,r0,r1,0)
#  define bungtr_d(i0,r0,r1)		bungtr_d_p(i0,r0,r1,0)
#  define bungtr_f_p(i0,r0,r1,p)	_bler_f(_jit,i0,r0,r1,0,1,p)
#  define bungtr_d_p(i0,r0,r1,p)	_bler_f(_jit,i0,r0,r1,1,1,p)
static jit_word_t
_bungti_f(jit_state_t*,jit_word_t,jit_int16_t,jit_float32_t,jit_bool_t);
#  define bungti_f(i0,r0,i1)		bungti_f_p(i0,r0,i1,0)
#  define bungti_f_p(i0,r0,i1,p)	_bungti_f(_jit,i0,r0,i1,p)
static jit_word_t
_bungti_d(jit_state_t*,jit_word_t,jit_int16_t,jit_float64_t,jit_bool_t);
#  define bungti_d(i0,r0,i1)		bungti_d_p(i0,r0,i1,0)
#  define bungti_d_p(i0,r0,i1,p)	_bungti_d(_jit,i0,r0,i1,p)
#  define bunger_f(i0,r0,r1)		bunger_f_p(i0,r1,r0,0)
#  define bunger_d(i0,r0,r1)		bunger_d_p(i0,r1,r0,0)
#  define bunger_f_p(i0,r0,r1,p)	_bgtr_f(_jit,i0,r1,r0,0,0,p)
#  define bunger_d_p(i0,r0,r1,p)	_bgtr_f(_jit,i0,r1,r0,1,0,p)
static jit_word_t
_bungei_f(jit_state_t*,jit_word_t,jit_int16_t,jit_float32_t,jit_bool_t);
#  define bungei_f(i0,r0,i1)		bungei_f_p(i0,r0,i1,0)
#  define bungei_f_p(i0,r0,i1,p)	_bungei_f(_jit,i0,r0,i1,p)
static jit_word_t
_bungei_d(jit_state_t*,jit_word_t,jit_int16_t,jit_float64_t,jit_bool_t);
#  define bungei_d(i0,r0,i1)		bungei_d_p(i0,r0,i1,0)
#  define bungei_d_p(i0,r0,i1,p)	_bungei_d(_jit,i0,r0,i1,p)
static jit_word_t _buneqr_f(jit_state_t*,jit_word_t,jit_int16_t,
			    jit_int16_t,jit_bool_t,jit_bool_t);
#  define buneqr_f(i0,r0,r1)		buneqr_f_p(i0,r1,r0,0)
#  define buneqr_d(i0,r0,r1)		buneqr_d_p(i0,r1,r0,0)
#  define buneqr_f_p(i0,r0,r1,p)	_buneqr_f(_jit,i0,r1,r0,0,p)
#  define buneqr_d_p(i0,r0,r1,p)	_buneqr_f(_jit,i0,r1,r0,1,p)
static jit_word_t
_buneqi_f(jit_state_t*,jit_word_t,jit_int16_t,jit_float32_t,jit_bool_t);
#  define buneqi_f(i0,r0,i1)		buneqi_f_p(i0,r0,i1,0)
#  define buneqi_f_p(i0,r0,i1,p)	_buneqi_f(_jit,i0,r0,i1,p)
static jit_word_t
_buneqi_d(jit_state_t*,jit_word_t,jit_int16_t,jit_float64_t,jit_bool_t);
#  define buneqi_d(i0,r0,i1)		buneqi_d_p(i0,r0,i1,0)
#  define buneqi_d_p(i0,r0,i1,p)	_buneqi_d(_jit,i0,r0,i1,p)
static jit_word_t _bltgtr_f(jit_state_t*,jit_word_t,jit_int16_t,
			    jit_int16_t,jit_bool_t,jit_bool_t);
#  define bltgtr_f(i0,r0,r1)		bltgtr_f_p(i0,r1,r0,0)
#  define bltgtr_d(i0,r0,r1)		bltgtr_d_p(i0,r1,r0,0)
#  define bltgtr_f_p(i0,r0,r1,p)	_bltgtr_f(_jit,i0,r1,r0,0,p)
#  define bltgtr_d_p(i0,r0,r1,p)	_bltgtr_f(_jit,i0,r1,r0,1,p)
static jit_word_t
_bltgti_f(jit_state_t*,jit_word_t,jit_int16_t,jit_float32_t,jit_bool_t);
#  define bltgti_f(i0,r0,i1)		bltgti_f_p(i0,r0,i1,0)
#  define bltgti_f_p(i0,r0,i1,p)	_bltgti_f(_jit,i0,r0,i1,p)
static jit_word_t
_bltgti_d(jit_state_t*,jit_word_t,jit_int16_t,jit_float64_t,jit_bool_t);
#  define bltgti_d(i0,r0,i1)		bltgti_d_p(i0,r0,i1,0)
#  define bltgti_d_p(i0,r0,i1,p)	_bltgti_d(_jit,i0,r0,i1,p)
static jit_word_t _bordr_f(jit_state_t*,jit_word_t,jit_int16_t,jit_int16_t,
			   jit_bool_t,jit_bool_t,jit_bool_t);
#  define bordr_f(i0,r0,r1)		bordr_f_p(i0,r0,r1,0)
#  define bordr_d(i0,r0,r1)		bordr_d_p(i0,r0,r1,0)
#  define bordr_f_p(i0,r0,r1,p)		_bordr_f(_jit,i0,r0,r1,0,1,p)
#  define bordr_d_p(i0,r0,r1,p)		_bordr_f(_jit,i0,r0,r1,1,1,p)
static jit_word_t
_bordi_f(jit_state_t*,jit_word_t,jit_int16_t,jit_float32_t,jit_bool_t);
#  define bordi_f(i0,r0,i1)		bordi_f_p(i0,r0,i1,0)
#  define bordi_f_p(i0,r0,i1,p)		_bordi_f(_jit,i0,r0,i1,p)
static jit_word_t
_bordi_d(jit_state_t*,jit_word_t,jit_int16_t,jit_float64_t,jit_bool_t);
#  define bordi_d(i0,r0,i1)		bordi_d_p(i0,r0,i1,0)
#  define bordi_d_p(i0,r0,i1,p)		_bordi_d(_jit,i0,r0,i1,p)
#  define bunordr_f(i0,r0,r1)		bunordr_f_p(i0,r0,r1,0)
#  define bunordr_d(i0,r0,r1)		bunordr_d_p(i0,r0,r1,0)
#  define bunordr_f_p(i0,r0,r1,p)	_bordr_f(_jit,i0,r0,r1,0,0,p)
#  define bunordr_d_p(i0,r0,r1,p)	_bordr_f(_jit,i0,r0,r1,1,0,p)
static jit_word_t
_bunordi_f(jit_state_t*,jit_word_t,jit_int16_t,jit_float32_t,jit_bool_t);
#  define bunordi_f(i0,r0,i1)		bunordi_f_p(i0,r0,i1,0)
#  define bunordi_f_p(i0,r0,i1,p)	_bunordi_f(_jit,i0,r0,i1,p)
static jit_word_t
_bunordi_d(jit_state_t*,jit_word_t,jit_int16_t,jit_float64_t,jit_bool_t);
#  define bunordi_d(i0,r0,i1)		bunordi_d_p(i0,r0,i1,0)
#  define bunordi_d_p(i0,r0,i1,p)	_bunordi_d(_jit,i0,r0,i1,p)
#  define ldxbi_f(r0,r1,i0)		generic_ldxbi_f(r0,r1,i0)
#  define ldxbi_d(r0,r1,i0)		generic_ldxbi_d(r0,r1,i0)
static void
_ldxai_f(jit_state_t*,jit_int16_t,jit_int16_t,jit_word_t);
#  define ldxai_f(r0,r1,i0)		_ldxai_f(_jit,r0,r1,i0)
static void
_ldxai_d(jit_state_t*,jit_int16_t,jit_int16_t,jit_word_t);
#  define ldxai_d(r0,r1,i0)		_ldxai_d(_jit,r0,r1,i0)
static void
_stxbi_f(jit_state_t*,jit_word_t,jit_int16_t,jit_int16_t);
#  define stxbi_f(i0,r0,r1)		_stxbi_f(_jit,i0,r0,r1)
static void
_stxbi_d(jit_state_t*,jit_word_t,jit_int16_t,jit_int16_t);
#  define stxbi_d(i0,r0,r1)		_stxbi_d(_jit,i0,r0,r1)
#  define stxai_f(i0,r0,r1)		generic_stxai_f(i0,r0,r1)
#  define stxai_d(i0,r0,r1)		generic_stxai_d(i0,r0,r1)
static void _vaarg_d(jit_state_t*,jit_int32_t,jit_int32_t);
#  define vaarg_d(r0, r1)		_vaarg_d(_jit, r0, r1)
#endif /* PROTO */

#if CODE
static void set_fmode_mask(jit_state_t *_jit, jit_uint32_t mask, jit_bool_t no_r0)
{
	jit_uint16_t reg, reg2;

	if (SH_HAS_FPU && _jitc->uses_fpu) {
		if (no_r0) {
			reg = jit_get_reg(jit_class_gpr);
			reg2 = jit_get_reg(jit_class_gpr);

			movi(rn(reg2), mask);
			STSFP(rn(reg));
			xorr(rn(reg), rn(reg), rn(reg2));
			LDSFP(rn(reg));

			jit_unget_reg(reg);
			jit_unget_reg(reg2);
		} else {
			STSFP(_R0);
			SWAPW(_R0, _R0);
			XORI(mask >> 16);
			SWAPW(_R0, _R0);
			LDSFP(_R0);
		}
	}
}

static void set_fmode(jit_state_t *_jit, jit_bool_t is_double)
{
	if (SH_HAS_FPU && !SH_SINGLE_ONLY && _jitc->uses_fpu && _jitc->mode_d != is_double) {
		set_fmode_mask(_jit, PR_FLAG, 0);
		_jitc->mode_d = is_double;
	}
}

static void reset_fpu(jit_state_t *_jit, jit_bool_t no_r0)
{
	if (SH_HAS_FPU && _jitc->uses_fpu) {
		if (_jitc->mode_d != SH_DEFAULT_FPU_MODE)
			set_fmode_mask(_jit, PR_FLAG | FR_FLAG, no_r0);
		else if (SH_DEFAULT_FPU_MODE)
			set_fmode_mask(_jit, FR_FLAG, no_r0);
		else
			maybe_emit_frchg();

		_jitc->mode_d = SH_DEFAULT_FPU_MODE;
	}
}

static void set_fmode_no_r0(jit_state_t *_jit, jit_bool_t is_double)
{
	if (SH_HAS_FPU && _jitc->uses_fpu && !SH_SINGLE_ONLY && _jitc->mode_d != is_double) {
		set_fmode_mask(_jit, PR_FLAG, 1);
		_jitc->mode_d = is_double;
	}
}

static void _extr_f(jit_state_t *_jit, jit_int16_t r0,
		    jit_int16_t r1, jit_bool_t is_double)
{
	set_fmode(_jit, is_double);

	LDS(r1);
	FLOAT(r0);
}

static void _truncr_f_i(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1,
			jit_bool_t is_double)
{
	set_fmode(_jit, is_double);

	FTRC(r1);
	STSUL(r0);
}

static void _fmar_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1,
		    jit_uint16_t r2, jit_uint16_t r3)
{
	jit_uint16_t reg;

	set_fmode(_jit, 0);

	reg = jit_get_reg(_F0 | jit_class_fpr | jit_class_named | jit_class_chk);

	if (reg == JIT_NOREG) {
		reg = jit_get_reg(jit_class_fpr);
		mulr_f(rn(reg), r1, r2);
		addr_f(r0, rn(reg), r3);
	} else if (r0 == r2) {
		movr_f(rn(reg), r2);
		movr_f(r0, r3);
		FMAC(r0, r1);
	} else {
		movr_f(rn(reg), r1);
		movr_f(r0, r3);
		FMAC(r0, r2);
	}

	jit_unget_reg(reg);
}

static void _fmar_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1,
		    jit_uint16_t r2, jit_uint16_t r3)
{
	jit_uint16_t reg;

	if (r0 == r3) {
		reg = jit_get_reg(jit_class_fpr);

		mulr_d(rn(reg), r1, r2);
		addr_d(r0, rn(reg), r3);

		jit_unget_reg(reg);
	} else {
		mulr_d(r0, r1, r2);
		addr_d(r0, r0, r3);
	}
}

static void _fmsr_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1,
		    jit_uint16_t r2, jit_uint16_t r3)
{
	jit_uint16_t reg;

	set_fmode(_jit, 0);

	reg = jit_get_reg(_F0 | jit_class_fpr | jit_class_named | jit_class_chk);

	if (reg == JIT_NOREG) {
		reg = jit_get_reg(jit_class_fpr);
		mulr_f(rn(reg), r1, r2);
		subr_f(r0, rn(reg), r3);
	} else if (r0 == r2) {
		movr_f(rn(reg), r2);
		movr_f(r0, r3);
		FNEG(r0);
		FMAC(r0, r1);
	} else {
		movr_f(rn(reg), r1);
		movr_f(r0, r3);
		FNEG(r0);
		FMAC(r0, r2);
	}

	jit_unget_reg(reg);
}

static void _fmsr_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1,
		    jit_uint16_t r2, jit_uint16_t r3)
{
	jit_uint16_t reg;

	if (r0 == r3) {
		reg = jit_get_reg(jit_class_fpr);

		mulr_d(rn(reg), r1, r2);
		subr_d(r0, rn(reg), r3);

		jit_unget_reg(reg);
	} else {
		mulr_d(r0, r1, r2);
		subr_d(r0, r0, r3);
	}
}

static void _fnmsr_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1,
		     jit_uint16_t r2, jit_uint16_t r3)
{
	jit_uint16_t reg;

	set_fmode(_jit, 0);

	reg = jit_get_reg(_F0 | jit_class_fpr | jit_class_named | jit_class_chk);

	if (reg == JIT_NOREG) {
		fmsr_f(r0, r1, r2, r3);
		negr_f(r0, r0);
	} else {
		if (r0 == r2) {
			movr_f(rn(reg), r2);
			FNEG(rn(reg));
			movr_f(r0, r3);
			FMAC(r0, r1);
		} else {
			movr_f(rn(reg), r1);
			FNEG(rn(reg));
			movr_f(r0, r3);
			FMAC(r0, r2);
		}

		jit_unget_reg(reg);
	}
}

static void _fnmsr_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1,
		     jit_uint16_t r2, jit_uint16_t r3)
{
	fmsr_d(r0, r1, r2, r3);
	negr_d(r0, r0);
}

static void _fnmar_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1,
		     jit_uint16_t r2, jit_uint16_t r3)
{
	fmar_f(r0, r1, r2, r3);
	negr_f(r0, r0);
}

static void _fnmar_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1,
		     jit_uint16_t r2, jit_uint16_t r3)
{
	fmar_d(r0, r1, r2, r3);
	negr_d(r0, r0);
}

static void _movr_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1)
{
	if (r0 != r1) {
		if (r0 >= _XF0 || r1 >= _XF0) {
			set_fmode(_jit, 0);

			if (r0 >= _XF0 && r1 >= _XF0) {
				maybe_emit_frchg();
				FMOV(r0 - _XF0, r1 - _XF0);
				FRCHG();
			} else if (r0 >= _XF0) {
				FLDS(r1);
				FRCHG();
				FSTS(r0 - _XF0);
				FRCHG();
			} else {
				maybe_emit_frchg();
				FLDS(r1 - _XF0);
				FRCHG();
				FSTS(r0);
			}
		} else {
			FMOV(r0, r1);
		}
	}
}

static void _movr_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1)
{
	if (r0 != r1) {
		if (SH_SINGLE_ONLY) {
			movr_f(r0, r1);
		} else if (r0 >= _XF0 || r1 >= _XF0) {
			set_fmode(_jit, 0);
			maybe_emit_fschg();

			if (r0 >= _XF0 && r1 >= _XF0)
				FMOVXX(r0 - _XF0, r1 - _XF0);
			else if (r0 >= _XF0)
				FMOVXD(r0 - _XF0, r1);
			else
				FMOVDX(r0, r1 - _XF0);

			FSCHG();
		} else {
			FMOV(r0, r1);
			FMOV(r0 + 1, r1 + 1);
		}
	}
}

static void _movi_f(jit_state_t *_jit, jit_uint16_t r0, jit_float32_t i0)
{
	jit_bool_t is_bank = r0 >= _XF0;

	set_fmode(_jit, 0);

	if (is_bank) {
		maybe_emit_frchg();
		r0 -= _XF0;
	}

	if (i0 == 0.0f) {
		FLDI0(r0);
	} else if (i0 == -0.0f) {
		FLDI0(r0);
		FNEG(r0);
	} else if (i0 == 1.0f) {
		FLDI1(r0);
	} else if (i0 == -1.0f) {
		FLDI1(r0);
		FNEG(r0);
	} else {
		load_const_f(0, r0, i0);
	}

	if (is_bank)
		FRCHG();
}

static void _movi_d(jit_state_t *_jit, jit_uint16_t r0, jit_float64_t i0)
{
	union fl64 {
		struct {
			jit_uint32_t hi;
			jit_uint32_t lo;
		};
		jit_float64_t f;
	};

	if (SH_SINGLE_ONLY) {
		movi_f(r0, (jit_float32_t)i0);
	} else if (r0 >= _XF0) {
		set_fmode(_jit, 0);
		maybe_emit_frchg();

		movi_w_f(r0 + 1 - _XF0, ((union fl64)i0).hi);
		movi_w_f(r0 - _XF0, ((union fl64)i0).lo);

		FRCHG();
	} else {
		movi_w_f(r0 + 1, ((union fl64)i0).hi);
		movi_w_f(r0, ((union fl64)i0).lo);
	}
}

static void _ltr_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1,
		   jit_int16_t r2, jit_bool_t is_double)
{
	set_fmode(_jit, is_double);

	FCMPGT(r2, r1);
	MOVT(r0);
}

static void
_lti_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg;

	reg = jit_get_reg(jit_class_fpr);
	movi_f(rn(reg), i0);

	ltr_f(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_lti_d(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg;

	reg = jit_get_reg(jit_class_fpr);
	movi_d(rn(reg), i0);

	ltr_d(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void _ler_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1,
		   jit_int16_t r2, jit_bool_t is_double)
{
	jit_uint16_t reg;

	reg = jit_get_reg(jit_class_fpr);

	set_fmode(_jit, is_double);

	MOVI(_R0, 0);
	FCMPEQ(r1, r1);
	BF(5);
	FCMPEQ(r2, r2);
	BF(3);

	FCMPGT(r1, r2);
	MOVT(_R0);
	BRA(13 + is_double);
	XORI(1);

	if (is_double)
		movr_w_f(rn(reg), _R0);
	else
		FLDI0(rn(reg));
	FCMPGT(rn(reg), r1);
	MOVT(_R0);
	FCMPGT(r1, rn(reg));
	ROTL(_R0);
	TST(_R0, _R0);
	BT(5);

	FCMPGT(rn(reg), r2);
	MOVT(_R0);
	FCMPGT(r2, rn(reg));
	ROTL(_R0);
	TST(_R0, _R0);
	BF(-18 - is_double);

	movr(r0, _R0);

	jit_unget_reg(reg);
}

static void
_lei_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i0);
	ler_f(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_lei_d(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i0);
	ler_d(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void _eqr_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1,
		   jit_int16_t r2, jit_bool_t is_double)
{
	set_fmode(_jit, is_double);

	FCMPEQ(r1, r2);
	MOVT(r0);
}

static void
_eqi_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i0);
	eqr_f(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_eqi_d(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i0);
	eqr_d(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_gei_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i0);
	ger_f(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_gei_d(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i0);
	ger_d(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_gti_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i0);
	gtr_f(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_gti_d(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i0);
	gtr_d(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_ner_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_int16_t r2,
       jit_bool_t is_double)
{
	_eqr_f(_jit, _R0, r1, r2, is_double);
	XORI(1);
	movr(r0, _R0);
}

static void
_nei_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i0);
	ner_f(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_nei_d(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i0);
	ner_d(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_unltr_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_int16_t r2,
	 jit_bool_t is_double)
{
	_ler_f(_jit, _R0, r2, r1, is_double);
	XORI(1);
	movr(r0, _R0);
}

static void
_unlti_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i0);
	unltr_f(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_unlti_d(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i0);
	unltr_d(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_unler_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_int16_t r2,
	 jit_bool_t is_double)
{
	_ltr_f(_jit, _R0, r2, r1, is_double);
	XORI(1);
	movr(r0, _R0);
}

static void
_unlei_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i0);
	unler_f(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_unlei_d(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i0);
	unler_d(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_ungti_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i0);
	ungtr_f(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_ungti_d(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i0);
	ungtr_d(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_ungei_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i0);
	unger_f(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_ungei_d(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i0);
	unger_d(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_uneqr_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_int16_t r2,
	 jit_bool_t is_double)
{
	jit_uint16_t reg = jit_get_reg(jit_class_gpr);

	_unler_f(_jit, rn(reg), r2, r1, is_double);
	_unler_f(_jit, r0, r1, r2, is_double);
	andr(r0, r0, rn(reg));

	jit_unget_reg(reg);
}

static void
_uneqi_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i0);
	uneqr_f(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_uneqi_d(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i0);
	uneqr_d(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_ltgtr_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_int16_t r2,
	 jit_bool_t is_double)
{
	_uneqr_f(_jit, r0, r1, r2, is_double);
	xori(r0, r0, 1);
}

static void
_ltgti_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i0);
	ltgtr_f(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_ltgti_d(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i0);
	ltgtr_d(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_ordr_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_int16_t r2,
	jit_bool_t is_double)
{
	jit_uint16_t reg = jit_get_reg(jit_class_gpr);

	_eqr_f(_jit, rn(reg), r1, r1, is_double);
	_eqr_f(_jit, r0, r2, r2, is_double);
	andr(r0, r0, rn(reg));

	jit_unget_reg(reg);
}

static void
_ordi_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i0);
	ordr_f(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_ordi_d(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i0);
	ordr_d(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_unordr_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_int16_t r2,
	  jit_bool_t is_double)
{
	jit_uint16_t reg = jit_get_reg(jit_class_gpr);

	_ner_f(_jit, rn(reg), r1, r1, is_double);
	_ner_f(_jit, r0, r2, r2, is_double);
	orr(r0, r0, rn(reg));

	jit_unget_reg(reg);
}

static void
_unordi_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i0);
	unordr_f(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_unordi_d(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i0);
	unordr_d(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_addr_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1,
	jit_uint16_t r2, jit_bool_t is_double)
{
	set_fmode(_jit, is_double);

	if (r0 == r2) {
		FADD(r0, r1);
	} else {
		if (is_double)
			movr_d(r0, r1);
		else
			movr_f(r0, r1);
		FADD(r0, r2);
	}
}

static void
_addi_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg;

	set_fmode(_jit, 0);

	if (r0 == r1) {
		reg = jit_get_reg(jit_class_fpr);

		movi_f(rn(reg), i0);
		FADD(r0, rn(reg));

		jit_unget_reg(reg);
	} else {
		movi_f(r0, i0);
		FADD(r0, r1);
	}
}

static void _addi_d(jit_state_t *_jit, jit_uint16_t r0,
		    jit_uint16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg;

	set_fmode(_jit, 1);

	if (r0 == r1) {
		reg = jit_get_reg(jit_class_fpr);

		movi_d(rn(reg), i0);
		FADD(r0, rn(reg));

		jit_unget_reg(reg);
	} else {
		movi_d(r0, i0);
		FADD(r0, r1);
	}
}

static void
_subr_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1, jit_uint16_t r2)
{
	jit_uint16_t reg;

	set_fmode(_jit, 0);

	if (r1 == r2) {
		movi_f(r0, 0.0f);
	} else if (r0 == r2) {
		FNEG(r0);
		FADD(r0, r1);
	} else {
		movr_f(r0, r1);
		FSUB(r0, r2);
	}
}

static void
_subr_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1, jit_uint16_t r2)
{
	jit_uint16_t reg;

	set_fmode(_jit, 1);

	if (r1 == r2) {
		movi_d(r0, 0.0);
	} else if (r0 == r2) {
		FNEG(r0);
		FADD(r0, r1);
	} else {
		movr_d(r0, r1);
		FSUB(r0, r2);
	}
}

static void
_subi_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg;

	set_fmode(_jit, 0);

	if (r0 == r1) {
		reg = jit_get_reg(jit_class_fpr);

		movi_f(rn(reg), i0);
		FSUB(r0, rn(reg));

		jit_unget_reg(reg);
	} else {
		movi_f(r0, -i0);
		FADD(r0, r1);
	}
}

static void
_subi_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg;

	set_fmode(_jit, 1);

	if (r0 == r1) {
		reg = jit_get_reg(jit_class_fpr);

		movi_d(rn(reg), i0);
		FSUB(r0, rn(reg));

		jit_unget_reg(reg);
	} else {
		movi_d(r0, -i0);
		FADD(r0, r1);
	}
}

static void
_rsbi_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg;

	set_fmode(_jit, 0);

	if (r0 == r1) {
		reg = jit_get_reg(jit_class_fpr);

		movi_f(rn(reg), i0);
		subr_f(r0, rn(reg), r0);

		jit_unget_reg(reg);
	} else {
		movi_f(r0, i0);
		FSUB(r0, r1);
	}
}

static void
_rsbi_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg;

	set_fmode(_jit, 1);

	if (r0 == r1) {
		reg = jit_get_reg(jit_class_fpr);

		movi_d(rn(reg), i0);
		subr_d(r0, rn(reg), r0);

		jit_unget_reg(reg);
	} else {
		movi_d(r0, i0);
		FSUB(r0, r1);
	}
}

static void
_mulr_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1, jit_uint16_t r2)
{
	set_fmode(_jit, 0);

	if (r0 == r2) {
		FMUL(r0, r1);
	} else {
		movr_f(r0, r1);
		FMUL(r0, r2);
	}
}

static void
_muli_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg;

	if (r0 == r1) {
		reg = jit_get_reg(jit_class_fpr);

		movi_f(rn(reg), i0);
		mulr_f(r0, r1, rn(reg));

		jit_unget_reg(reg);
	} else {
		movi_f(r0, i0);
		mulr_f(r0, r0, r1);
	}
}

static void
_mulr_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1, jit_uint16_t r2)
{
	set_fmode(_jit, 1);

	if (r0 == r2) {
		FMUL(r0, r1);
	} else {
		movr_d(r0, r1);
		FMUL(r0, r2);
	}
}

static void
_muli_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg;

	if (r0 == r1) {
		reg = jit_get_reg(jit_class_fpr);

		movi_d(rn(reg), i0);
		mulr_d(r0, r1, rn(reg));

		jit_unget_reg(reg);
	} else {
		movi_d(r0, i0);
		mulr_d(r0, r0, r1);
	}
}

static void
_divr_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1, jit_uint16_t r2)
{
	jit_uint16_t reg;

	set_fmode(_jit, 0);

	if (r0 == r2) {
		reg = jit_get_reg(jit_class_fpr);

		movr_f(rn(reg), r2);
		movr_f(r0, r1);
		FDIV(r0, rn(reg));

		jit_unget_reg(reg);
	} else {
		movr_f(r0, r1);
		FDIV(r0, r2);
	}
}

static void
_divi_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1, jit_float32_t i0)
{
	jit_uint16_t reg;

	reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i0);
	divr_f(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void
_divr_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1, jit_uint16_t r2)
{
	jit_uint16_t reg;

	set_fmode(_jit, 1);

	if (r0 == r2) {
		reg = jit_get_reg(jit_class_fpr);

		movr_d(rn(reg), r2);
		movr_d(r0, r1);
		FDIV(r0, rn(reg));

		jit_unget_reg(reg);
	} else {
		movr_d(r0, r1);
		FDIV(r0, r2);
	}
}

static void
_divi_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1, jit_float64_t i0)
{
	jit_uint16_t reg;

	reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i0);
	divr_d(r0, r1, rn(reg));

	jit_unget_reg(reg);
}

static void _absr_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1)
{
	set_fmode(_jit, 0);

	movr_f(r0, r1);
	FABS(r0);
}

static void _absr_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1)
{
	set_fmode(_jit, 1);

	movr_d(r0, r1);
	FABS(r0);
}

static void _sqrtr_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1)
{
	set_fmode(_jit, 0);

	movr_f(r0, r1);
	FSQRT(r0);
}

static void _sqrtr_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1)
{
	set_fmode(_jit, 1);

	movr_d(r0, r1);
	FSQRT(r0);
}

static void _negr_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1)
{
	set_fmode(_jit, 0);

	movr_f(r0, r1);
	FNEG(r0);
}

static void _negr_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1)
{
	set_fmode(_jit, 1);

	movr_d(r0, r1);
	FNEG(r0);
}

static void _extr_d_f(jit_state_t *_jit,jit_uint16_t r0, jit_uint16_t r1)
{
	if (SH_SINGLE_ONLY) {
		movr_f(r0, r1);
	} else {
		set_fmode(_jit, 1);
		FCNVDS(r1);
		set_fmode(_jit, 0);
		FSTS(r0);
	}
}

static void _extr_f_d(jit_state_t *_jit,jit_uint16_t r0, jit_uint16_t r1)
{
	if (SH_SINGLE_ONLY) {
		movr_f(r0, r1);
	} else {
		set_fmode(_jit, 0);
		FLDS(r1);
		set_fmode(_jit, 1);
		FCNVSD(r0);
	}
}

static void _ldr_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1)
{
	if (SH_SINGLE_ONLY) {
		ldr_f(r0, r1);
	} else {
		movr(_R0, r1);
		LDFS(r0 + 1, _R0);
		LDF(r0, _R0);
	}
}

static void _ldi_f(jit_state_t *_jit, jit_uint16_t r0, jit_word_t i0)
{
	movi(_R0, i0);
	ldr_f(r0, _R0);
}

static void _ldi_d(jit_state_t *_jit, jit_uint16_t r0, jit_word_t i0)
{
	movi(_R0, i0);
	ldr_d(r0, _R0);
}

static void _ldxr_f(jit_state_t *_jit, jit_uint16_t r0,
		    jit_uint16_t r1, jit_uint16_t r2)
{
	movr(_R0, r2);
	LDXF(r0, r1);
}

static void _ldxr_d(jit_state_t *_jit, jit_uint16_t r0,
		    jit_uint16_t r1, jit_uint16_t r2)
{
	if (SH_SINGLE_ONLY) {
		ldxr_f(r0, r1, r2);
	} else {
		addr(_R0, r1, r2);
		ldr_d(r0, _R0);
	}
}

static void _ldxi_f(jit_state_t *_jit, jit_uint16_t r0,
		    jit_uint16_t r1, jit_word_t i0)
{
	movi(_R0, i0);
	ldxr_f(r0, r1, _R0);
}

static void _ldxi_d(jit_state_t *_jit, jit_uint16_t r0,
		    jit_uint16_t r1, jit_word_t i0)
{
	movi(_R0, i0);
	ldxr_d(r0, r1, _R0);
}

static void _str_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1)
{
	if (SH_SINGLE_ONLY) {
		str_f(r0, r1);
	} else {
		STF(r0, r1 + 1);
		movi(_R0, 4);
		STXF(r0, r1);
	}
}

static void _sti_f(jit_state_t *_jit, jit_word_t i0, jit_uint16_t r0)
{
	movi(_R0, i0);
	STF(_R0, r0);
}

static void _sti_d(jit_state_t *_jit, jit_word_t i0, jit_uint16_t r0)
{
	if (SH_SINGLE_ONLY) {
		sti_f(i0, r0);
	} else {
		movi(_R0, i0 + 8);
		STFS(_R0, r0);
		STFS(_R0, r0 + 1);
	}
}

static void _stxr_f(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1,
		    jit_uint16_t r2)
{
	movr(_R0, r0);
	STXF(r1, r2);
}

static void _stxr_d(jit_state_t *_jit, jit_uint16_t r0, jit_uint16_t r1,
		    jit_uint16_t r2)
{
	if (SH_SINGLE_ONLY) {
		stxr_f(r0, r1, r2);
	} else {
		movr(_R0, r0);
		STXF(r1, r2 + 1);
		addi(_R0, _R0, 4);
		STXF(r1, r2);
	}
}

static void _stxi_f(jit_state_t *_jit, jit_word_t i0, jit_uint16_t r0,
		    jit_uint16_t r1)
{
	movi(_R0, i0);
	stxr_f(_R0, r0, r1);
}

static void _stxi_d(jit_state_t *_jit, jit_word_t i0, jit_uint16_t r0,
		    jit_uint16_t r1)
{
	movi(_R0, i0);
	stxr_d(_R0, r0, r1);
}

static jit_word_t _beqr_f(jit_state_t *_jit, jit_word_t i0, jit_uint16_t r0,
			  jit_uint16_t r1, jit_bool_t is_double,
			  jit_bool_t set, jit_bool_t p)
{
	jit_word_t w;

	set_fmode(_jit, is_double);

	FCMPEQ(r0, r1);

	set_fmode(_jit, SH_DEFAULT_FPU_MODE);

	w = _jit->pc.w;
	emit_branch_opcode(_jit, i0, w, set, p);

	return (w);
}

static jit_word_t _beqi_f(jit_state_t *_jit, jit_word_t i0, jit_uint16_t r0,
			  jit_float32_t i1, jit_bool_t set, jit_bool_t p)
{
	jit_word_t w;
	jit_uint16_t reg;

	set_fmode(_jit, 0);

	reg = jit_get_reg(jit_class_fpr);
	movi_f(rn(reg), i1);

	FCMPEQ(r0, rn(reg));
	jit_unget_reg(reg);

	set_fmode(_jit, SH_DEFAULT_FPU_MODE);

	w = _jit->pc.w;
	emit_branch_opcode(_jit, i0, w, set, p);

	return (w);
}

static jit_word_t _beqi_d(jit_state_t *_jit, jit_word_t i0, jit_uint16_t r0,
			  jit_float64_t i1, jit_bool_t set, jit_bool_t p)
{
	jit_word_t w;
	jit_uint16_t reg;

	set_fmode(_jit, 1);

	reg = jit_get_reg(jit_class_fpr);
	movi_d(rn(reg), i1);

	FCMPEQ(r0, rn(reg));
	jit_unget_reg(reg);

	set_fmode(_jit, SH_DEFAULT_FPU_MODE);

	w = _jit->pc.w;
	emit_branch_opcode(_jit, i0, w, set, p);

	return (w);
}

static jit_word_t _bgtr_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
			  jit_int16_t r1, jit_bool_t is_double,
			  jit_bool_t set, jit_bool_t p)
{
	jit_word_t w;

	set_fmode(_jit, is_double);

	FCMPGT(r0, r1);

	set_fmode(_jit, SH_DEFAULT_FPU_MODE);

	w = _jit->pc.w;
	emit_branch_opcode(_jit, i0, w, set, p);

	return (w);
}

static jit_word_t
_blti_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	jit_float32_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i1);
	w = bltr_f_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_blti_d(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	jit_float64_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i1);
	w = bltr_d_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_bgti_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	jit_float32_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i1);
	w = bgtr_f_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_bgti_d(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	jit_float64_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i1);
	w = bgtr_d_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t _bler_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
			  jit_int16_t r1, jit_bool_t is_double,
			  jit_bool_t set, jit_bool_t p)
{
	jit_word_t w;

	set_fmode(_jit, is_double);

	FCMPGT(r1, r0);
	MOVT(_R0);
	FCMPEQ(r0, r1);
	ROTCL(_R0);
	TSTI(3);

	set_fmode(_jit, SH_DEFAULT_FPU_MODE);

	w = _jit->pc.w;
	emit_branch_opcode(_jit, i0, w, set, p);

	return (w);
}

static jit_word_t
_blei_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	jit_float32_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i1);
	w = bler_f_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_blei_d(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	jit_float64_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i1);
	w = bler_d_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_bgei_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	jit_float32_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i1);
	w = bger_f_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_bgei_d(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	jit_float64_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i1);
	w = bger_d_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t _buneqr_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
			    jit_int16_t r1, jit_bool_t is_double, jit_bool_t p)
{
	jit_word_t w;

	_uneqr_f(_jit, _R0, r0, r1, is_double);
	TST(_R0, _R0);

	set_fmode(_jit, SH_DEFAULT_FPU_MODE);

	w = _jit->pc.w;
	emit_branch_opcode(_jit, i0, w, 0, p);

	return (w);
}

static jit_word_t _bltgtr_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
			    jit_int16_t r1, jit_bool_t is_double, jit_bool_t p)
{
	jit_word_t w;

	_ltgtr_f(_jit, _R0, r0, r1, is_double);
	TST(_R0, _R0);

	set_fmode(_jit, SH_DEFAULT_FPU_MODE);

	w = _jit->pc.w;
	emit_branch_opcode(_jit, i0, w, 0, p);

	return (w);
}

static jit_word_t _bordr_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
			   jit_int16_t r1, jit_bool_t is_double,
			   jit_bool_t set, jit_bool_t p)
{
	jit_word_t w;

	_ordr_f(_jit, _R0, r0, r1, is_double);
	TST(_R0, _R0);

	set_fmode(_jit, SH_DEFAULT_FPU_MODE);

	w = _jit->pc.w;
	emit_branch_opcode(_jit, i0, w, !set, p);

	return (w);
}

static jit_word_t
_bunlti_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	  jit_float32_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i1);
	w = bunltr_f_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_bunlti_d(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	  jit_float64_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i1);
	w = bunltr_d_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_bunlei_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	  jit_float32_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i1);
	w = bunler_f_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_bunlei_d(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	  jit_float64_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i1);
	w = bunler_d_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_bungti_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	  jit_float32_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i1);
	w = bungtr_f_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_bungti_d(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	  jit_float64_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i1);
	w = bungtr_d_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_bungei_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	  jit_float32_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i1);
	w = bunger_f_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_bungei_d(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	  jit_float64_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i1);
	w = bunger_d_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_buneqi_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	  jit_float32_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i1);
	w = buneqr_f_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_buneqi_d(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	  jit_float64_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i1);
	w = buneqr_d_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_bltgti_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	  jit_float32_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i1);
	w = bltgtr_f_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_bltgti_d(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	  jit_float64_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i1);
	w = bltgtr_d_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_bordi_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	 jit_float32_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i1);
	w = bordr_f_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_bordi_d(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	 jit_float64_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i1);
	w = bordr_d_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_bunordi_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	   jit_float32_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_f(rn(reg), i1);
	w = bunordr_f_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static jit_word_t
_bunordi_d(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0,
	   jit_float64_t i1, jit_bool_t p)
{
	jit_uint16_t reg;
	jit_word_t w;

	reg = jit_get_reg(jit_class_fpr);

	movi_d(rn(reg), i1);
	w = bunordr_d_p(i0, r0, rn(reg), p);

	jit_unget_reg(reg);

	return w;
}

static void
_ldxai_f(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_word_t i0)
{
    if (i0 == 4)
        LDFS(r0, r1);
    else
        generic_ldxai_f(r0, r1, i0);
}

static void
_ldxai_d(jit_state_t *_jit, jit_int16_t r0, jit_int16_t r1, jit_word_t i0)
{
	if (SH_SINGLE_ONLY) {
		ldxai_f(r0, r1, i0);
	} else if (i0 == 8) {
		LDFS(r0 + 1, r1);
		LDFS(r0, r1);
	} else {
		generic_ldxai_d(r0, r1, i0);
	}
}

static void
_stxbi_f(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0, jit_int16_t r1)
{
    if (i0 == -4)
        STFS(r0, r1);
    else
        generic_stxbi_f(i0, r0, r1);
}

static void
_stxbi_d(jit_state_t *_jit, jit_word_t i0, jit_int16_t r0, jit_int16_t r1)
{
	if (SH_SINGLE_ONLY) {
		stxbi_f(i0, r0, r1);
	} else if (i0 == -8) {
		STFS(r0, r1);
		STFS(r0, r1 + 1);
	} else {
		generic_stxbi_d(i0, r0, r1);
	}
}

static void _movr_w_f(jit_state_t *_jit, jit_uint16_t r0, jit_int16_t r1)
{
	LDS(r1);
	FSTS(r0);
}

static void _movr_f_w(jit_state_t *_jit, jit_uint16_t r0, jit_int16_t r1)
{
	FLDS(r1);
	STSUL(r0);
}

static void _movi_w_f(jit_state_t *_jit, jit_int16_t r0, jit_word_t i0)
{
	movi(_R0, i0);
	movr_w_f(r0, _R0);
}

static void _movr_ww_d(jit_state_t *_jit, jit_uint16_t r0, jit_int16_t r1, jit_int16_t r2)
{
	/* TODO: single-only */
	movr_w_f(r0 + 1, r1);
	movr_w_f(r0, r2);
}

static void _movr_d_ww(jit_state_t *_jit, jit_uint16_t r0, jit_int16_t r1, jit_int16_t r2)
{
	/* TODO: single-only */
	movr_f_w(r0, r2 + 1);
	movr_f_w(r1, r2);
}

static void _movi_ww_d(jit_state_t *_jit, jit_int16_t r0, jit_word_t i0, jit_word_t i1)
{
	/* TODO: single-only */
	movi_w_f(r0, i1);
	movi_w_f(r0 + 1, i0);
}

static void
_vaarg_d(jit_state_t *_jit, jit_int32_t r0, jit_int32_t r1)
{
    jit_int32_t rg0, rg1;
    jit_word_t ge_code;

    assert(_jitc->function->self.call & jit_call_varargs);

    rg0 = jit_get_reg(jit_class_gpr);
    rg1 = jit_get_reg(jit_class_gpr);

    /* Load begin/end gpr pointers */
    ldxi(rn(rg1), r1, offsetof(jit_va_list_t, efpr));
    movi(_R0, offsetof(jit_va_list_t, bfpr));
    ldxr(rn(rg0), r1, _R0);

    /* Check that we didn't reach the end gpr pointer. */
    CMPHS(rn(rg0), rn(rg1));

    ge_code = _jit->pc.w;
    BF(0);

    /* If we did, load the stack pointer instead. */
    movi(_R0, offsetof(jit_va_list_t, over));
    ldxr(rn(rg0), r1, _R0);

    patch_at(ge_code, _jit->pc.w);

    /* All good, we can now load the actual value */
    ldxai_d(r0, rn(rg0), sizeof(jit_float64_t));

    /* Update the pointer (gpr or stack) to the next word */
    stxr(_R0, r1, rn(rg0));

    jit_unget_reg(rg0);
    jit_unget_reg(rg1);
}

#endif /* CODE */
