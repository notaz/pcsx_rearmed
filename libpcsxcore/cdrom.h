/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
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

#ifndef __CDROM_H__
#define __CDROM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "psxcommon.h"
#include "decode_xa.h"
#include "r3000a.h"
#include "plugins.h"
#include "psxmem.h"
#include "psxhw.h"

#define btoi(b)     ((b) / 16 * 10 + (b) % 16) /* BCD to u_char */
#define itob(i)     ((i) / 10 * 16 + (i) % 10) /* u_char to BCD */

#define MSF2SECT(m, s, f)		(((m) * 60 + (s) - 2) * 75 + (f))

#define CD_FRAMESIZE_RAW		2352
#define DATA_SIZE				(CD_FRAMESIZE_RAW - 12)

#define SUB_FRAMESIZE			96

typedef struct {
	unsigned char OCUP;
	unsigned char Reg1Mode;
	unsigned char Reg2;
	unsigned char CmdProcess;
	unsigned char Ctrl;
	unsigned char Stat;

	unsigned char StatP;

	unsigned char Transfer[CD_FRAMESIZE_RAW];
	unsigned char *pTransfer;

	unsigned char Prev[4];
	unsigned char Param[8];
	unsigned char Result[8];

	unsigned char ParamC;
	unsigned char ParamP;
	unsigned char ResultC;
	unsigned char ResultP;
	unsigned char ResultReady;
	unsigned char Cmd;
	unsigned char Readed;
	u32 Reading;

	unsigned char ResultTN[6];
	unsigned char ResultTD[4];
	unsigned char SetSector[4];
	unsigned char SetSectorSeek[4];
	unsigned char Track;
	boolean Play, Muted;
	int CurTrack;
	int Mode, File, Channel;
	int Reset;
	int RErr;
	int FirstSector;

	xa_decode_t Xa;

	int Init;

	unsigned char Irq;
	u32 eCycle;

	boolean Seeked;
} cdrStruct;

extern cdrStruct cdr;

void cdrReset();
void cdrInterrupt();
void cdrReadInterrupt();
unsigned char cdrRead0(void);
unsigned char cdrRead1(void);
unsigned char cdrRead2(void);
unsigned char cdrRead3(void);
void cdrWrite0(unsigned char rt);
void cdrWrite1(unsigned char rt);
void cdrWrite2(unsigned char rt);
void cdrWrite3(unsigned char rt);
int cdrFreeze(gzFile f, int Mode);

#ifdef __cplusplus
}
#endif
#endif
