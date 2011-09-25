/***************************************************************************
 *   PCSX-Revolution - PlayStation Emulator for Nintendo Wii               *
 *   Copyright (C) 2009-2010  PCSX-Revolution Dev Team                     *
 *   <http://code.google.com/p/pcsx-revolution/>                           *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
 ***************************************************************************/

#ifdef FLAGLESS

#define gteRTPS gteRTPS_nf
#define gteOP gteOP_nf
#define gteNCLIP gteNCLIP_nf
#define gteDPCS gteDPCS_nf
#define gteINTPL gteINTPL_nf
#define gteMVMVA gteMVMVA_nf
#define gteNCDS gteNCDS_nf
#define gteNCDT gteNCDT_nf
#define gteCDP gteCDP_nf
#define gteNCCS gteNCCS_nf
#define gteCC gteCC_nf
#define gteNCS gteNCS_nf
#define gteNCT gteNCT_nf
#define gteSQR gteSQR_nf
#define gteDCPL gteDCPL_nf
#define gteDPCT gteDPCT_nf
#define gteAVSZ3 gteAVSZ3_nf
#define gteAVSZ4 gteAVSZ4_nf
#define gteRTPT gteRTPT_nf
#define gteGPF gteGPF_nf
#define gteGPL gteGPL_nf
#define gteNCCT gteNCCT_nf

#endif

#ifndef __GTE_H__
#define __GTE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "psxcommon.h"
#include "r3000a.h"

void gteMFC2();
void gteCFC2();
void gteMTC2();
void gteCTC2();
void gteLWC2();
void gteSWC2();

void gteRTPS();
void gteOP();
void gteNCLIP();
void gteDPCS();
void gteINTPL();
void gteMVMVA();
void gteNCDS();
void gteNCDT();
void gteCDP();
void gteNCCS();
void gteCC();
void gteNCS();
void gteNCT();
void gteSQR();
void gteDCPL();
void gteDPCT();
void gteAVSZ3();
void gteAVSZ4();
void gteRTPT();
void gteGPF();
void gteGPL();
void gteNCCT();

#ifdef __cplusplus
}
#endif
#endif
