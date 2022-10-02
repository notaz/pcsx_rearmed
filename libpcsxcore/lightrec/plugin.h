/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2022 Paul Cercueil <paul@crapouillou.net>
 */

#ifndef __LIGHTREC_PLUGIN_H__
#define __LIGHTREC_PLUGIN_H__

#ifdef LIGHTREC

#define drc_is_lightrec() 1
void lightrec_plugin_sync_regs_to_pcsx(void);
void lightrec_plugin_sync_regs_from_pcsx(void);

#else /* if !LIGHTREC */

#define drc_is_lightrec() 0
#define lightrec_plugin_sync_regs_to_pcsx()
#define lightrec_plugin_sync_regs_from_pcsx()

#endif

#endif /* __LIGHTREC_PLUGIN_H__ */

