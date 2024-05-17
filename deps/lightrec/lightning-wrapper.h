/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2022 Paul Cercueil <paul@crapouillou.net>
 */

#ifndef __LIGHTNING_WRAPPER_H__
#define __LIGHTNING_WRAPPER_H__

#include <lightning.h>

#if __WORDSIZE == 32

#define jit_ldxi_ui(u,v,w)	jit_ldxi_i(u,v,w)
#define jit_stxi_ui(u,v,w)	jit_stxi_i(u,v,w)
#define jit_extr_i(u,v)		jit_movr(u,v)
#define jit_extr_ui(u,v)	jit_movr(u,v)
#define jit_retval_ui(u)	jit_retval(u)
#define jit_getarg_ui(u,v)	jit_getarg_i(u,v)

#endif

#define jit_b()			jit_beqr(0, 0)

#define jit_add_state(u,v)	jit_addr(u,v,LIGHTREC_REG_STATE)

#endif /* __LIGHTNING_WRAPPER_H__ */
