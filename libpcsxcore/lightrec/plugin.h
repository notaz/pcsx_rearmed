/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2022 Paul Cercueil <paul@crapouillou.net>
 */

#ifndef __LIGHTREC_PLUGIN_H__
#define __LIGHTREC_PLUGIN_H__

#ifdef LIGHTREC

#define drc_is_lightrec() 1
void lightrec_plugin_prepare_save_state(void);
void lightrec_plugin_prepare_load_state(void);

#else /* if !LIGHTREC */

#define drc_is_lightrec() 0
#define lightrec_plugin_prepare_save_state()
#define lightrec_plugin_prepare_load_state()

#endif

#endif /* __LIGHTREC_PLUGIN_H__ */

