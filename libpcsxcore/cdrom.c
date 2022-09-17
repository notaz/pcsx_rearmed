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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

/*
* Handles all CD-ROM registers and functions.
*/

#include "cdrom.h"
#include "ppf.h"
#include "psxdma.h"
#include "arm_features.h"

/* logging */
#if 0
#define CDR_LOG SysPrintf
#else
#define CDR_LOG(...)
#endif
#if 0
#define CDR_LOG_I SysPrintf
#else
#define CDR_LOG_I log_unhandled
#endif
#if 0
#define CDR_LOG_IO SysPrintf
#else
#define CDR_LOG_IO(...)
#endif
//#define CDR_LOG_CMD_IRQ

static struct {
	// unused members maintain savesate compatibility
	unsigned char unused0;
	unsigned char unused1;
	unsigned char Reg2;
	unsigned char unused2;
	unsigned char Ctrl;
	unsigned char Stat;

	unsigned char StatP;

	unsigned char Transfer[DATA_SIZE];
	struct {
		unsigned char Track;
		unsigned char Index;
		unsigned char Relative[3];
		unsigned char Absolute[3];
	} subq;
	unsigned char TrackChanged;
	unsigned char unused3[3];
	unsigned int  freeze_ver;

	unsigned char Prev[4];
	unsigned char Param[8];
	unsigned char Result[16];

	unsigned char ParamC;
	unsigned char ParamP;
	unsigned char ResultC;
	unsigned char ResultP;
	unsigned char ResultReady;
	unsigned char Cmd;
	unsigned char unused4;
	unsigned char SetlocPending;
	u32 Reading;

	unsigned char ResultTN[6];
	unsigned char ResultTD[4];
	unsigned char SetSectorPlay[4];
	unsigned char SetSectorEnd[4];
	unsigned char SetSector[4];
	unsigned char Track;
	boolean Play, Muted;
	int CurTrack;
	int Mode, File, Channel;
	int Reset;
	int NoErr;
	int FirstSector;

	xa_decode_t Xa;

	u16 FifoOffset;
	u16 FifoSize;

	u16 CmdInProgress;
	u8 Irq1Pending;
	u8 unused5;
	u32 unused6;

	u8 unused7;

	u8 DriveState;
	u8 FastForward;
	u8 FastBackward;
	u8 unused8;

	u8 AttenuatorLeftToLeft, AttenuatorLeftToRight;
	u8 AttenuatorRightToRight, AttenuatorRightToLeft;
	u8 AttenuatorLeftToLeftT, AttenuatorLeftToRightT;
	u8 AttenuatorRightToRightT, AttenuatorRightToLeftT;
} cdr;
static s16 read_buf[CD_FRAMESIZE_RAW/2];

/* CD-ROM magic numbers */
#define CdlSync        0  /* nocash documentation : "Uh, actually, returns error code 40h = Invalid Command...?" */
#define CdlNop         1
#define CdlSetloc      2
#define CdlPlay        3
#define CdlForward     4
#define CdlBackward    5
#define CdlReadN       6
#define CdlStandby     7
#define CdlStop        8
#define CdlPause       9
#define CdlReset       10
#define CdlMute        11
#define CdlDemute      12
#define CdlSetfilter   13
#define CdlSetmode     14
#define CdlGetparam    15
#define CdlGetlocL     16
#define CdlGetlocP     17
#define CdlReadT       18
#define CdlGetTN       19
#define CdlGetTD       20
#define CdlSeekL       21
#define CdlSeekP       22
#define CdlSetclock    23
#define CdlGetclock    24
#define CdlTest        25
#define CdlID          26
#define CdlReadS       27
#define CdlInit        28
#define CdlGetQ        29
#define CdlReadToc     30

#ifdef CDR_LOG_CMD_IRQ
static const char * const CmdName[0x100] = {
    "CdlSync",     "CdlNop",       "CdlSetloc",  "CdlPlay",
    "CdlForward",  "CdlBackward",  "CdlReadN",   "CdlStandby",
    "CdlStop",     "CdlPause",     "CdlReset",    "CdlMute",
    "CdlDemute",   "CdlSetfilter", "CdlSetmode", "CdlGetparam",
    "CdlGetlocL",  "CdlGetlocP",   "CdlReadT",   "CdlGetTN",
    "CdlGetTD",    "CdlSeekL",     "CdlSeekP",   "CdlSetclock",
    "CdlGetclock", "CdlTest",      "CdlID",      "CdlReadS",
    "CdlInit",     NULL,           "CDlReadToc", NULL
};
#endif

unsigned char Test04[] = { 0 };
unsigned char Test05[] = { 0 };
unsigned char Test20[] = { 0x98, 0x06, 0x10, 0xC3 };
unsigned char Test22[] = { 0x66, 0x6F, 0x72, 0x20, 0x45, 0x75, 0x72, 0x6F };
unsigned char Test23[] = { 0x43, 0x58, 0x44, 0x32, 0x39 ,0x34, 0x30, 0x51 };

// cdr.Stat:
#define NoIntr		0
#define DataReady	1
#define Complete	2
#define Acknowledge	3
#define DataEnd		4
#define DiskError	5

/* Modes flags */
#define MODE_SPEED       (1<<7) // 0x80
#define MODE_STRSND      (1<<6) // 0x40 ADPCM on/off
#define MODE_SIZE_2340   (1<<5) // 0x20
#define MODE_SIZE_2328   (1<<4) // 0x10
#define MODE_SIZE_2048   (0<<4) // 0x00
#define MODE_SF          (1<<3) // 0x08 channel on/off
#define MODE_REPORT      (1<<2) // 0x04
#define MODE_AUTOPAUSE   (1<<1) // 0x02
#define MODE_CDDA        (1<<0) // 0x01

/* Status flags */
#define STATUS_PLAY      (1<<7) // 0x80
#define STATUS_SEEK      (1<<6) // 0x40
#define STATUS_READ      (1<<5) // 0x20
#define STATUS_SHELLOPEN (1<<4) // 0x10
#define STATUS_UNKNOWN3  (1<<3) // 0x08
#define STATUS_UNKNOWN2  (1<<2) // 0x04
#define STATUS_ROTATING  (1<<1) // 0x02
#define STATUS_ERROR     (1<<0) // 0x01

/* Errors */
#define ERROR_NOTREADY   (1<<7) // 0x80
#define ERROR_INVALIDCMD (1<<6) // 0x40
#define ERROR_INVALIDARG (1<<5) // 0x20

// 1x = 75 sectors per second
// PSXCLK = 1 sec in the ps
// so (PSXCLK / 75) = cdr read time (linuzappz)
#define cdReadTime (PSXCLK / 75)

enum drive_state {
	DRIVESTATE_STANDBY = 0, // pause, play, read
	DRIVESTATE_LID_OPEN,
	DRIVESTATE_RESCAN_CD,
	DRIVESTATE_PREPARE_CD,
	DRIVESTATE_STOPPED,
};

static struct CdrStat stat;

static unsigned int msf2sec(const u8 *msf) {
	return ((msf[0] * 60 + msf[1]) * 75) + msf[2];
}

// for that weird psemu API..
static unsigned int fsm2sec(const u8 *msf) {
	return ((msf[2] * 60 + msf[1]) * 75) + msf[0];
}

static void sec2msf(unsigned int s, u8 *msf) {
	msf[0] = s / 75 / 60;
	s = s - msf[0] * 75 * 60;
	msf[1] = s / 75;
	s = s - msf[1] * 75;
	msf[2] = s;
}

// cdrInterrupt
#define CDR_INT(eCycle) { \
	psxRegs.interrupt |= (1 << PSXINT_CDR); \
	psxRegs.intCycle[PSXINT_CDR].cycle = eCycle; \
	psxRegs.intCycle[PSXINT_CDR].sCycle = psxRegs.cycle; \
	new_dyna_set_event(PSXINT_CDR, eCycle); \
}

// cdrPlaySeekReadInterrupt
#define CDRPLAYSEEKREAD_INT(eCycle, isFirst) { \
	u32 e_ = eCycle; \
	psxRegs.interrupt |= (1 << PSXINT_CDREAD); \
	if (isFirst) \
		psxRegs.intCycle[PSXINT_CDREAD].sCycle = psxRegs.cycle; \
	else \
		psxRegs.intCycle[PSXINT_CDREAD].sCycle += psxRegs.intCycle[PSXINT_CDREAD].cycle; \
	psxRegs.intCycle[PSXINT_CDREAD].cycle = e_; \
	new_dyna_set_event_abs(PSXINT_CDREAD, psxRegs.intCycle[PSXINT_CDREAD].sCycle + e_); \
}

// cdrLidSeekInterrupt
#define CDRLID_INT(eCycle) { \
	psxRegs.interrupt |= (1 << PSXINT_CDRLID); \
	psxRegs.intCycle[PSXINT_CDRLID].cycle = eCycle; \
	psxRegs.intCycle[PSXINT_CDRLID].sCycle = psxRegs.cycle; \
	new_dyna_set_event(PSXINT_CDRLID, eCycle); \
}

#define StopReading() { \
	cdr.Reading = 0; \
	psxRegs.interrupt &= ~(1 << PSXINT_CDREAD); \
}

#define StopCdda() { \
	if (cdr.Play && !Config.Cdda) CDR_stop(); \
	cdr.Play = FALSE; \
	cdr.FastForward = 0; \
	cdr.FastBackward = 0; \
}

#define SetPlaySeekRead(x, f) { \
	x &= ~(STATUS_PLAY | STATUS_SEEK | STATUS_READ); \
	x |= f; \
}

#define SetResultSize(size) { \
	cdr.ResultP = 0; \
	cdr.ResultC = size; \
	cdr.ResultReady = 1; \
}

static void setIrq(int log_cmd)
{
	if (cdr.Stat & cdr.Reg2)
		psxHu32ref(0x1070) |= SWAP32((u32)0x4);

#ifdef CDR_LOG_CMD_IRQ
	if (cdr.Stat)
	{
		int i;
		SysPrintf("%u cdrom: CDR IRQ=%d cmd %02x stat %02x: ",
			psxRegs.cycle, !!(cdr.Stat & cdr.Reg2), log_cmd, cdr.Stat);
		for (i = 0; i < cdr.ResultC; i++)
			SysPrintf("%02x ", cdr.Result[i]);
		SysPrintf("\n");
	}
#endif
}

// timing used in this function was taken from tests on real hardware
// (yes it's slow, but you probably don't want to modify it)
void cdrLidSeekInterrupt(void)
{
	CDR_LOG_I("%u %s cdr.DriveState=%d\n", psxRegs.cycle, __func__, cdr.DriveState);

	switch (cdr.DriveState) {
	default:
	case DRIVESTATE_STANDBY:
		StopCdda();
		StopReading();
		SetPlaySeekRead(cdr.StatP, 0);

		if (CDR_getStatus(&stat) == -1)
			return;

		if (stat.Status & STATUS_SHELLOPEN)
		{
			cdr.DriveState = DRIVESTATE_LID_OPEN;
			CDRLID_INT(0x800);
		}
		break;

	case DRIVESTATE_LID_OPEN:
		if (CDR_getStatus(&stat) == -1)
			stat.Status &= ~STATUS_SHELLOPEN;

		// 02, 12, 10
		if (!(cdr.StatP & STATUS_SHELLOPEN)) {
			cdr.StatP |= STATUS_SHELLOPEN;

			// could generate error irq here, but real hardware
			// only sometimes does that
			// (not done when lots of commands are sent?)

			CDRLID_INT(cdReadTime * 30);
			break;
		}
		else if (cdr.StatP & STATUS_ROTATING) {
			cdr.StatP &= ~STATUS_ROTATING;
		}
		else if (!(stat.Status & STATUS_SHELLOPEN)) {
			// closed now
			CheckCdrom();

			// cdr.StatP STATUS_SHELLOPEN is "sticky"
			// and is only cleared by CdlNop

			cdr.DriveState = DRIVESTATE_RESCAN_CD;
			CDRLID_INT(cdReadTime * 105);
			break;
		}

		// recheck for close
		CDRLID_INT(cdReadTime * 3);
		break;

	case DRIVESTATE_RESCAN_CD:
		cdr.StatP |= STATUS_ROTATING;
		cdr.DriveState = DRIVESTATE_PREPARE_CD;

		// this is very long on real hardware, over 6 seconds
		// make it a bit faster here...
		CDRLID_INT(cdReadTime * 150);
		break;

	case DRIVESTATE_PREPARE_CD:
		if (cdr.StatP & STATUS_SEEK) {
			SetPlaySeekRead(cdr.StatP, 0);
			cdr.DriveState = DRIVESTATE_STANDBY;
		}
		else {
			SetPlaySeekRead(cdr.StatP, STATUS_SEEK);
			CDRLID_INT(cdReadTime * 26);
		}
		break;
	}
}

static void Find_CurTrack(const u8 *time)
{
	int current, sect;

	current = msf2sec(time);

	for (cdr.CurTrack = 1; cdr.CurTrack < cdr.ResultTN[1]; cdr.CurTrack++) {
		CDR_getTD(cdr.CurTrack + 1, cdr.ResultTD);
		sect = fsm2sec(cdr.ResultTD);
		if (sect - current >= 150)
			break;
	}
}

static void generate_subq(const u8 *time)
{
	unsigned char start[3], next[3];
	unsigned int this_s, start_s, next_s, pregap;
	int relative_s;

	CDR_getTD(cdr.CurTrack, start);
	if (cdr.CurTrack + 1 <= cdr.ResultTN[1]) {
		pregap = 150;
		CDR_getTD(cdr.CurTrack + 1, next);
	}
	else {
		// last track - cd size
		pregap = 0;
		next[0] = cdr.SetSectorEnd[2];
		next[1] = cdr.SetSectorEnd[1];
		next[2] = cdr.SetSectorEnd[0];
	}

	this_s = msf2sec(time);
	start_s = fsm2sec(start);
	next_s = fsm2sec(next);

	cdr.TrackChanged = FALSE;

	if (next_s - this_s < pregap) {
		cdr.TrackChanged = TRUE;
		cdr.CurTrack++;
		start_s = next_s;
	}

	cdr.subq.Index = 1;

	relative_s = this_s - start_s;
	if (relative_s < 0) {
		cdr.subq.Index = 0;
		relative_s = -relative_s;
	}
	sec2msf(relative_s, cdr.subq.Relative);

	cdr.subq.Track = itob(cdr.CurTrack);
	cdr.subq.Relative[0] = itob(cdr.subq.Relative[0]);
	cdr.subq.Relative[1] = itob(cdr.subq.Relative[1]);
	cdr.subq.Relative[2] = itob(cdr.subq.Relative[2]);
	cdr.subq.Absolute[0] = itob(time[0]);
	cdr.subq.Absolute[1] = itob(time[1]);
	cdr.subq.Absolute[2] = itob(time[2]);
}

static void ReadTrack(const u8 *time) {
	unsigned char tmp[3];
	struct SubQ *subq;
	u16 crc;

	tmp[0] = itob(time[0]);
	tmp[1] = itob(time[1]);
	tmp[2] = itob(time[2]);

	if (memcmp(cdr.Prev, tmp, 3) == 0)
		return;

	CDR_LOG("ReadTrack *** %02x:%02x:%02x\n", tmp[0], tmp[1], tmp[2]);

	cdr.NoErr = CDR_readTrack(tmp);
	memcpy(cdr.Prev, tmp, 3);

	if (CheckSBI(time))
		return;

	subq = (struct SubQ *)CDR_getBufferSub();
	if (subq != NULL && cdr.CurTrack == 1) {
		crc = calcCrc((u8 *)subq + 12, 10);
		if (crc == (((u16)subq->CRC[0] << 8) | subq->CRC[1])) {
			cdr.subq.Track = subq->TrackNumber;
			cdr.subq.Index = subq->IndexNumber;
			memcpy(cdr.subq.Relative, subq->TrackRelativeAddress, 3);
			memcpy(cdr.subq.Absolute, subq->AbsoluteAddress, 3);
		}
		else {
			CDR_LOG_I("subq bad crc @%02x:%02x:%02x\n",
				tmp[0], tmp[1], tmp[2]);
		}
	}
	else {
		generate_subq(time);
	}

	CDR_LOG(" -> %02x,%02x %02x:%02x:%02x %02x:%02x:%02x\n",
		cdr.subq.Track, cdr.subq.Index,
		cdr.subq.Relative[0], cdr.subq.Relative[1], cdr.subq.Relative[2],
		cdr.subq.Absolute[0], cdr.subq.Absolute[1], cdr.subq.Absolute[2]);
}

static void cdrPlayInterrupt_Autopause()
{
	u32 abs_lev_max = 0;
	boolean abs_lev_chselect;
	u32 i;

	if ((cdr.Mode & MODE_AUTOPAUSE) && cdr.TrackChanged) {
		CDR_LOG( "CDDA STOP\n" );

		// Magic the Gathering
		// - looping territory cdda

		// ...?
		//cdr.ResultReady = 1;
		//cdr.Stat = DataReady;
		cdr.Stat = DataEnd;
		setIrq(0x1000); // 0x1000 just for logging purposes

		StopCdda();
		SetPlaySeekRead(cdr.StatP, 0);
	}
	else if (((cdr.Mode & MODE_REPORT) || cdr.FastForward || cdr.FastBackward)) {
		cdr.Result[0] = cdr.StatP;
		cdr.Result[1] = cdr.subq.Track;
		cdr.Result[2] = cdr.subq.Index;
		
		abs_lev_chselect = cdr.subq.Absolute[1] & 0x01;
		
		/* 8 is a hack. For accuracy, it should be 588. */
		for (i = 0; i < 8; i++)
		{
			abs_lev_max = MAX_VALUE(abs_lev_max, abs(read_buf[i * 2 + abs_lev_chselect]));
		}
		abs_lev_max = MIN_VALUE(abs_lev_max, 32767);
		abs_lev_max |= abs_lev_chselect << 15;

		if (cdr.subq.Absolute[2] & 0x10) {
			cdr.Result[3] = cdr.subq.Relative[0];
			cdr.Result[4] = cdr.subq.Relative[1] | 0x80;
			cdr.Result[5] = cdr.subq.Relative[2];
		}
		else {
			cdr.Result[3] = cdr.subq.Absolute[0];
			cdr.Result[4] = cdr.subq.Absolute[1];
			cdr.Result[5] = cdr.subq.Absolute[2];
		}

		cdr.Result[6] = abs_lev_max >> 0;
		cdr.Result[7] = abs_lev_max >> 8;

		// Rayman: Logo freeze (resultready + dataready)
		cdr.ResultReady = 1;
		cdr.Stat = DataReady;

		SetResultSize(8);
		setIrq(0x1001);
	}
}

static int cdrSeekTime(unsigned char *target)
{
	int diff = msf2sec(cdr.SetSectorPlay) - msf2sec(target);
	int seekTime = abs(diff) * (cdReadTime / 200);
	/*
	* Gameblabla :
	* It was originally set to 1000000 for Driver, however it is not high enough for Worms Pinball
	* and was unreliable for that game.
	* I also tested it against Mednafen and Driver's titlescreen music starts 25 frames later, not immediatly.
	*
	* Obviously, this isn't perfect but right now, it should be a bit better.
	* Games to test this against if you change that setting :
	* - Driver (titlescreen music delay and retry mission)
	* - Worms Pinball (Will either not boot or crash in the memory card screen)
	* - Viewpoint (short pauses if the delay in the ingame music is too long)
	*
	* It seems that 3386880 * 5 is too much for Driver's titlescreen and it starts skipping.
	* However, 1000000 is not enough for Worms Pinball to reliably boot.
	*/
	if(seekTime > 3386880 * 2) seekTime = 3386880 * 2;
	CDR_LOG("seek: %.2f %.2f\n", (float)seekTime / PSXCLK, (float)seekTime / cdReadTime);
	return seekTime;
}

static void cdrUpdateTransferBuf(const u8 *buf);
static void cdrReadInterrupt(void);
static void cdrPrepCdda(s16 *buf, int samples);
static void cdrAttenuate(s16 *buf, int samples, int stereo);

void cdrPlaySeekReadInterrupt(void)
{
	if (cdr.Reading) {
		cdrReadInterrupt();
		return;
	}

	if (!cdr.Play && (cdr.StatP & STATUS_SEEK)) {
		if (cdr.Stat) {
			CDR_LOG_I("cdrom: seek stat hack\n");
			CDRPLAYSEEKREAD_INT(0x1000, 1);
			return;
		}
		SetResultSize(1);
		cdr.StatP |= STATUS_ROTATING;
		SetPlaySeekRead(cdr.StatP, 0);
		cdr.Result[0] = cdr.StatP;
		cdr.Stat = Complete;
		setIrq(0x1002);

		Find_CurTrack(cdr.SetSectorPlay);
		ReadTrack(cdr.SetSectorPlay);
		cdr.TrackChanged = FALSE;
		return;
	}

	if (!cdr.Play) return;

	CDR_LOG( "CDDA - %d:%d:%d\n",
		cdr.SetSectorPlay[0], cdr.SetSectorPlay[1], cdr.SetSectorPlay[2] );

	SetPlaySeekRead(cdr.StatP, STATUS_PLAY);
	if (memcmp(cdr.SetSectorPlay, cdr.SetSectorEnd, 3) == 0) {
		StopCdda();
		SetPlaySeekRead(cdr.StatP, 0);
		cdr.TrackChanged = TRUE;
	}
	else {
		CDR_readCDDA(cdr.SetSectorPlay[0], cdr.SetSectorPlay[1], cdr.SetSectorPlay[2], (u8 *)read_buf);
	}

	if (!cdr.Stat && (cdr.Mode & (MODE_AUTOPAUSE|MODE_REPORT)))
		cdrPlayInterrupt_Autopause();

	if (!cdr.Muted && !Config.Cdda) {
		cdrPrepCdda(read_buf, CD_FRAMESIZE_RAW / 4);
		cdrAttenuate(read_buf, CD_FRAMESIZE_RAW / 4, 1);
		SPU_playCDDAchannel(read_buf, CD_FRAMESIZE_RAW, psxRegs.cycle, cdr.FirstSector);
		cdr.FirstSector = 0;
	}

	cdr.SetSectorPlay[2]++;
	if (cdr.SetSectorPlay[2] == 75) {
		cdr.SetSectorPlay[2] = 0;
		cdr.SetSectorPlay[1]++;
		if (cdr.SetSectorPlay[1] == 60) {
			cdr.SetSectorPlay[1] = 0;
			cdr.SetSectorPlay[0]++;
		}
	}

	// update for CdlGetlocP/autopause
	generate_subq(cdr.SetSectorPlay);

	CDRPLAYSEEKREAD_INT(cdReadTime, 0);
}

#define CMD_PART2           0x100
#define CMD_WHILE_NOT_READY 0x200

void cdrInterrupt(void) {
	int start_rotating = 0;
	int error = 0;
	unsigned int seekTime = 0;
	u32 second_resp_time = 0;
	u8 ParamC;
	u8 set_loc[3];
	u16 not_ready = 0;
	u16 Cmd;
	int i;

	if (cdr.Stat) {
		CDR_LOG_I("%u cdrom: cmd %02x with irqstat %x\n",
			psxRegs.cycle, cdr.CmdInProgress, cdr.Stat);
		return;
	}
	if (cdr.Irq1Pending) {
		// hand out the "newest" sector, according to nocash
		cdrUpdateTransferBuf(CDR_getBuffer());
		CDR_LOG_I("cdrom: %x:%02x:%02x loaded on ack\n",
			cdr.Transfer[0], cdr.Transfer[1], cdr.Transfer[2]);
		SetResultSize(1);
		cdr.Result[0] = cdr.Irq1Pending;
		cdr.Stat = (cdr.Irq1Pending & STATUS_ERROR) ? DiskError : DataReady;
		cdr.Irq1Pending = 0;
		setIrq(0x1003);
		return;
	}

	// default response
	SetResultSize(1);
	cdr.Result[0] = cdr.StatP;
	cdr.Stat = Acknowledge;

	Cmd = cdr.CmdInProgress;
	cdr.CmdInProgress = 0;
	ParamC = cdr.ParamC;

	if (Cmd < 0x100) {
		cdr.Ctrl &= ~0x80;
		cdr.ParamC = 0;
		cdr.Cmd = 0;
	}

	switch (cdr.DriveState) {
	case DRIVESTATE_PREPARE_CD:
		if (Cmd > 2) {
			// Syphon filter 2 expects commands to work shortly after it sees
			// STATUS_ROTATING, so give up trying to emulate the startup seq
			cdr.DriveState = DRIVESTATE_STANDBY;
			cdr.StatP &= ~STATUS_SEEK;
			psxRegs.interrupt &= ~(1 << PSXINT_CDRLID);
			break;
		}
		// fallthrough
	case DRIVESTATE_LID_OPEN:
	case DRIVESTATE_RESCAN_CD:
		// no disk or busy with the initial scan, allowed cmds are limited
		not_ready = CMD_WHILE_NOT_READY;
		break;
	}

	switch (Cmd | not_ready) {
		case CdlNop:
		case CdlNop + CMD_WHILE_NOT_READY:
			if (cdr.DriveState != DRIVESTATE_LID_OPEN)
				cdr.StatP &= ~STATUS_SHELLOPEN;
			break;

		case CdlSetloc:
		case CdlSetloc + CMD_WHILE_NOT_READY:
			CDR_LOG("CDROM setloc command (%02X, %02X, %02X)\n", cdr.Param[0], cdr.Param[1], cdr.Param[2]);

			// MM must be BCD, SS must be BCD and <0x60, FF must be BCD and <0x75
			if (((cdr.Param[0] & 0x0F) > 0x09) || (cdr.Param[0] > 0x99) || ((cdr.Param[1] & 0x0F) > 0x09) || (cdr.Param[1] >= 0x60) || ((cdr.Param[2] & 0x0F) > 0x09) || (cdr.Param[2] >= 0x75))
			{
				CDR_LOG("Invalid/out of range seek to %02X:%02X:%02X\n", cdr.Param[0], cdr.Param[1], cdr.Param[2]);
				error = ERROR_INVALIDARG;
				goto set_error;
			}
			else
			{
				for (i = 0; i < 3; i++)
					set_loc[i] = btoi(cdr.Param[i]);
				memcpy(cdr.SetSector, set_loc, 3);
				cdr.SetSector[3] = 0;
				cdr.SetlocPending = 1;
			}
			break;

		do_CdlPlay:
		case CdlPlay:
			StopCdda();
			StopReading();

			cdr.FastBackward = 0;
			cdr.FastForward = 0;

			// BIOS CD Player
			// - Pause player, hit Track 01/02/../xx (Setloc issued!!)

			if (ParamC != 0 && cdr.Param[0] != 0) {
				int track = btoi( cdr.Param[0] );

				if (track <= cdr.ResultTN[1])
					cdr.CurTrack = track;

				CDR_LOG("PLAY track %d\n", cdr.CurTrack);

				if (CDR_getTD((u8)cdr.CurTrack, cdr.ResultTD) != -1) {
					for (i = 0; i < 3; i++)
						set_loc[i] = cdr.ResultTD[2 - i];
					seekTime = cdrSeekTime(set_loc);
					memcpy(cdr.SetSectorPlay, set_loc, 3);
				}
			}
			else if (cdr.SetlocPending) {
				seekTime = cdrSeekTime(cdr.SetSector);
				memcpy(cdr.SetSectorPlay, cdr.SetSector, 4);
			}
			else {
				CDR_LOG("PLAY Resume @ %d:%d:%d\n",
					cdr.SetSectorPlay[0], cdr.SetSectorPlay[1], cdr.SetSectorPlay[2]);
			}
			cdr.SetlocPending = 0;

			/*
			Rayman: detect track changes
			- fixes logo freeze

			Twisted Metal 2: skip PREGAP + starting accurate SubQ
			- plays tracks without retry play

			Wild 9: skip PREGAP + starting accurate SubQ
			- plays tracks without retry play
			*/
			Find_CurTrack(cdr.SetSectorPlay);
			ReadTrack(cdr.SetSectorPlay);
			cdr.TrackChanged = FALSE;
			cdr.FirstSector = 1;

			if (!Config.Cdda)
				CDR_play(cdr.SetSectorPlay);

			SetPlaySeekRead(cdr.StatP, STATUS_SEEK | STATUS_ROTATING);
			
			// BIOS player - set flag again
			cdr.Play = TRUE;

			CDRPLAYSEEKREAD_INT(cdReadTime + seekTime, 1);
			start_rotating = 1;
			break;

		case CdlForward:
			// TODO: error 80 if stopped
			cdr.Stat = Complete;

			// GameShark CD Player: Calls 2x + Play 2x
			cdr.FastForward = 1;
			cdr.FastBackward = 0;
			break;

		case CdlBackward:
			cdr.Stat = Complete;

			// GameShark CD Player: Calls 2x + Play 2x
			cdr.FastBackward = 1;
			cdr.FastForward = 0;
			break;

		case CdlStandby:
			if (cdr.DriveState != DRIVESTATE_STOPPED) {
				error = ERROR_INVALIDARG;
				goto set_error;
			}
			second_resp_time = cdReadTime * 125 / 2;
			start_rotating = 1;
			break;

		case CdlStandby + CMD_PART2:
			cdr.Stat = Complete;
			break;

		case CdlStop:
			if (cdr.Play) {
				// grab time for current track
				CDR_getTD((u8)(cdr.CurTrack), cdr.ResultTD);

				cdr.SetSectorPlay[0] = cdr.ResultTD[2];
				cdr.SetSectorPlay[1] = cdr.ResultTD[1];
				cdr.SetSectorPlay[2] = cdr.ResultTD[0];
			}

			StopCdda();
			StopReading();
			SetPlaySeekRead(cdr.StatP, 0);
			cdr.StatP &= ~STATUS_ROTATING;

			second_resp_time = 0x800;
			if (cdr.DriveState == DRIVESTATE_STANDBY)
				second_resp_time = cdReadTime * 30 / 2;

			cdr.DriveState = DRIVESTATE_STOPPED;
			break;

		case CdlStop + CMD_PART2:
			cdr.Stat = Complete;
			break;

		case CdlPause:
			StopCdda();
			StopReading();
			/*
			Gundam Battle Assault 2: much slower (*)
			- Fixes boot, gameplay

			Hokuto no Ken 2: slower
			- Fixes intro + subtitles

			InuYasha - Feudal Fairy Tale: slower
			- Fixes battles
			*/
			/* Gameblabla - Tightening the timings (as taken from Duckstation). 
			 * The timings from Duckstation are based upon hardware tests.
			 * Mednafen's timing don't work for Gundam Battle Assault 2 in PAL/50hz mode,
			 * seems to be timing sensitive as it can depend on the CPU's clock speed.
			 * */
			if (!(cdr.StatP & (STATUS_PLAY | STATUS_READ)))
			{
				second_resp_time = 7000;
			}
			else
			{
				second_resp_time = (((cdr.Mode & MODE_SPEED) ? 2 : 1) * 1000000);
			}
			SetPlaySeekRead(cdr.StatP, 0);
			break;

		case CdlPause + CMD_PART2:
			cdr.Stat = Complete;
			break;

		case CdlReset:
		case CdlReset + CMD_WHILE_NOT_READY:
			StopCdda();
			StopReading();
			SetPlaySeekRead(cdr.StatP, 0);
			cdr.Muted = FALSE;
			cdr.Mode = 0x20; /* This fixes This is Football 2, Pooh's Party lockups */
			second_resp_time = not_ready ? 70000 : 4100000;
			start_rotating = 1;
			break;

		case CdlReset + CMD_PART2:
		case CdlReset + CMD_PART2 + CMD_WHILE_NOT_READY:
			cdr.Stat = Complete;
			break;

		case CdlMute:
			cdr.Muted = TRUE;
			break;

		case CdlDemute:
			cdr.Muted = FALSE;
			break;

		case CdlSetfilter:
			cdr.File = cdr.Param[0];
			cdr.Channel = cdr.Param[1];
			break;

		case CdlSetmode:
		case CdlSetmode + CMD_WHILE_NOT_READY:
			CDR_LOG("cdrWrite1() Log: Setmode %x\n", cdr.Param[0]);
			cdr.Mode = cdr.Param[0];
			break;

		case CdlGetparam:
		case CdlGetparam + CMD_WHILE_NOT_READY:
			/* Gameblabla : According to mednafen, Result size should be 5 and done this way. */
			SetResultSize(5);
			cdr.Result[1] = cdr.Mode;
			cdr.Result[2] = 0;
			cdr.Result[3] = cdr.File;
			cdr.Result[4] = cdr.Channel;
			break;

		case CdlGetlocL:
			SetResultSize(8);
			memcpy(cdr.Result, cdr.Transfer, 8);
			break;

		case CdlGetlocP:
			SetResultSize(8);
			memcpy(&cdr.Result, &cdr.subq, 8);
			break;

		case CdlReadT: // SetSession?
			// really long
			second_resp_time = cdReadTime * 290 / 4;
			start_rotating = 1;
			break;

		case CdlReadT + CMD_PART2:
			cdr.Stat = Complete;
			break;

		case CdlGetTN:
			SetResultSize(3);
			if (CDR_getTN(cdr.ResultTN) == -1) {
				cdr.Stat = DiskError;
				cdr.Result[0] |= STATUS_ERROR;
			} else {
				cdr.Stat = Acknowledge;
				cdr.Result[1] = itob(cdr.ResultTN[0]);
				cdr.Result[2] = itob(cdr.ResultTN[1]);
			}
			break;

		case CdlGetTD:
			cdr.Track = btoi(cdr.Param[0]);
			SetResultSize(4);
			if (CDR_getTD(cdr.Track, cdr.ResultTD) == -1) {
				cdr.Stat = DiskError;
				cdr.Result[0] |= STATUS_ERROR;
			} else {
				cdr.Stat = Acknowledge;
				cdr.Result[0] = cdr.StatP;
				cdr.Result[1] = itob(cdr.ResultTD[2]);
				cdr.Result[2] = itob(cdr.ResultTD[1]);
				/* According to Nocash's documentation, the function doesn't care about ff.
				 * This can be seen also in Mednafen's implementation. */
				//cdr.Result[3] = itob(cdr.ResultTD[0]);
			}
			break;

		case CdlSeekL:
		case CdlSeekP:
			StopCdda();
			StopReading();
			SetPlaySeekRead(cdr.StatP, STATUS_SEEK);

			seekTime = cdrSeekTime(cdr.SetSector);
			memcpy(cdr.SetSectorPlay, cdr.SetSector, 4);
			/*
			Crusaders of Might and Magic = 0.5x-4x
			- fix cutscene speech start

			Eggs of Steel = 2x-?
			- fix new game

			Medievil = ?-4x
			- fix cutscene speech

			Rockman X5 = 0.5-4x
			- fix capcom logo
			*/
			CDRPLAYSEEKREAD_INT(cdReadTime + seekTime, 1);
			start_rotating = 1;
			break;

		case CdlTest:
		case CdlTest + CMD_WHILE_NOT_READY:
			switch (cdr.Param[0]) {
				case 0x20: // System Controller ROM Version
					SetResultSize(4);
					memcpy(cdr.Result, Test20, 4);
					break;
				case 0x22:
					SetResultSize(8);
					memcpy(cdr.Result, Test22, 4);
					break;
				case 0x23: case 0x24:
					SetResultSize(8);
					memcpy(cdr.Result, Test23, 4);
					break;
			}
			break;

		case CdlID:
			second_resp_time = 20480;
			break;

		case CdlID + CMD_PART2:
			SetResultSize(8);
			cdr.Result[0] = cdr.StatP;
			cdr.Result[1] = 0;
			cdr.Result[2] = 0;
			cdr.Result[3] = 0;

			// 0x10 - audio | 0x40 - disk missing | 0x80 - unlicensed
			if (CDR_getStatus(&stat) == -1 || stat.Type == 0 || stat.Type == 0xff) {
				cdr.Result[1] = 0xc0;
			}
			else {
				if (stat.Type == 2)
					cdr.Result[1] |= 0x10;
				if (CdromId[0] == '\0')
					cdr.Result[1] |= 0x80;
			}
			cdr.Result[0] |= (cdr.Result[1] >> 4) & 0x08;

			/* This adds the string "PCSX" in Playstation bios boot screen */
			memcpy((char *)&cdr.Result[4], "PCSX", 4);
			cdr.Stat = Complete;
			break;

		case CdlInit:
		case CdlInit + CMD_WHILE_NOT_READY:
			StopCdda();
			StopReading();
			SetPlaySeekRead(cdr.StatP, 0);
			// yes, it really sets STATUS_SHELLOPEN
			cdr.StatP |= STATUS_SHELLOPEN;
			cdr.DriveState = DRIVESTATE_RESCAN_CD;
			CDRLID_INT(20480);
			start_rotating = 1;
			break;

		case CdlGetQ:
		case CdlGetQ + CMD_WHILE_NOT_READY:
			break;

		case CdlReadToc:
		case CdlReadToc + CMD_WHILE_NOT_READY:
			second_resp_time = cdReadTime * 180 / 4;
			start_rotating = 1;
			break;

		case CdlReadToc + CMD_PART2:
		case CdlReadToc + CMD_PART2 + CMD_WHILE_NOT_READY:
			cdr.Stat = Complete;
			break;

		case CdlReadN:
		case CdlReadS:
			Find_CurTrack(cdr.SetlocPending ? cdr.SetSector : cdr.SetSectorPlay);

			if ((cdr.Mode & MODE_CDDA) && cdr.CurTrack > 1)
				// Read* acts as play for cdda tracks in cdda mode
				goto do_CdlPlay;

			StopCdda();
			if (cdr.SetlocPending) {
				seekTime = cdrSeekTime(cdr.SetSector);
				memcpy(cdr.SetSectorPlay, cdr.SetSector, 4);
				cdr.SetlocPending = 0;
			}
			cdr.Reading = 1;
			cdr.FirstSector = 1;

			// Fighting Force 2 - update subq time immediately
			// - fixes new game
			ReadTrack(cdr.SetSectorPlay);

			CDRPLAYSEEKREAD_INT(((cdr.Mode & 0x80) ? (cdReadTime) : cdReadTime * 2) + seekTime, 1);

			SetPlaySeekRead(cdr.StatP, STATUS_SEEK);
			start_rotating = 1;
			break;
		case CdlSync:
		default:
			CDR_LOG_I("Invalid command: %02x%s\n",
				Cmd, not_ready ? " (not_ready)" : "");
			error = ERROR_INVALIDCMD;
			// FALLTHROUGH

		set_error:
			SetResultSize(2);
			cdr.Result[0] = cdr.StatP | STATUS_ERROR;
			cdr.Result[1] = not_ready ? ERROR_NOTREADY : error;
			cdr.Stat = DiskError;
			break;
	}

	if (cdr.DriveState == DRIVESTATE_STOPPED && start_rotating) {
 printf("cdr.DriveState %d->%d\n", cdr.DriveState, DRIVESTATE_STANDBY);
		cdr.DriveState = DRIVESTATE_STANDBY;
		cdr.StatP |= STATUS_ROTATING;
	}

	if (second_resp_time) {
		cdr.CmdInProgress = Cmd | 0x100;
		CDR_INT(second_resp_time);
	}
	else if (cdr.Cmd && cdr.Cmd != (Cmd & 0xff)) {
		cdr.CmdInProgress = cdr.Cmd;
		CDR_LOG_I("%u cdrom: cmd %02x came before %02x finished\n",
			psxRegs.cycle, cdr.Cmd, Cmd);
	}

	setIrq(Cmd);
}

#ifdef HAVE_ARMV7
 #define ssat32_to_16(v) \
  asm("ssat %0,#16,%1" : "=r" (v) : "r" (v))
#else
 #define ssat32_to_16(v) do { \
  if (v < -32768) v = -32768; \
  else if (v > 32767) v = 32767; \
 } while (0)
#endif

static void cdrPrepCdda(s16 *buf, int samples)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	int i;
	for (i = 0; i < samples; i++) {
		buf[i * 2 + 0] = SWAP16(buf[i * 2 + 0]);
		buf[i * 2 + 1] = SWAP16(buf[i * 2 + 1]);
	}
#endif
}

static void cdrAttenuate(s16 *buf, int samples, int stereo)
{
	int i, l, r;
	int ll = cdr.AttenuatorLeftToLeft;
	int lr = cdr.AttenuatorLeftToRight;
	int rl = cdr.AttenuatorRightToLeft;
	int rr = cdr.AttenuatorRightToRight;

	if (lr == 0 && rl == 0 && 0x78 <= ll && ll <= 0x88 && 0x78 <= rr && rr <= 0x88)
		return;

	if (!stereo && ll == 0x40 && lr == 0x40 && rl == 0x40 && rr == 0x40)
		return;

	if (stereo) {
		for (i = 0; i < samples; i++) {
			l = buf[i * 2];
			r = buf[i * 2 + 1];
			l = (l * ll + r * rl) >> 7;
			r = (r * rr + l * lr) >> 7;
			ssat32_to_16(l);
			ssat32_to_16(r);
			buf[i * 2] = l;
			buf[i * 2 + 1] = r;
		}
	}
	else {
		for (i = 0; i < samples; i++) {
			l = buf[i];
			l = l * (ll + rl) >> 7;
			//r = r * (rr + lr) >> 7;
			ssat32_to_16(l);
			//ssat32_to_16(r);
			buf[i] = l;
		}
	}
}

static void cdrReadInterruptSetResult(unsigned char result)
{
	if (cdr.Stat) {
		CDR_LOG_I("cdrom: %d:%02d:%02d irq miss, cmd=%02x irqstat=%02x\n",
			cdr.SetSectorPlay[0], cdr.SetSectorPlay[1], cdr.SetSectorPlay[2],
			cdr.CmdInProgress, cdr.Stat);
		cdr.Irq1Pending = result;
		return;
	}
	SetResultSize(1);
	cdr.Result[0] = result;
	cdr.Stat = (result & STATUS_ERROR) ? DiskError : DataReady;
	setIrq(0x1004);
}

static void cdrUpdateTransferBuf(const u8 *buf)
{
	if (!buf)
		return;
	memcpy(cdr.Transfer, buf, DATA_SIZE);
	CheckPPFCache(cdr.Transfer, cdr.Prev[0], cdr.Prev[1], cdr.Prev[2]);
	CDR_LOG("cdr.Transfer %x:%x:%x\n", cdr.Transfer[0], cdr.Transfer[1], cdr.Transfer[2]);
	if (cdr.FifoOffset < 2048 + 12)
		CDR_LOG("cdrom: FifoOffset(1) %d/%d\n", cdr.FifoOffset, cdr.FifoSize);
}

static void cdrReadInterrupt(void)
{
	u8 *buf = NULL, *hdr;

	SetPlaySeekRead(cdr.StatP, STATUS_READ | STATUS_ROTATING);

	ReadTrack(cdr.SetSectorPlay);
	if (cdr.NoErr)
		buf = CDR_getBuffer();
	if (buf == NULL)
		cdr.NoErr = 0;

	if (!cdr.NoErr) {
		CDR_LOG_I("cdrReadInterrupt() Log: err\n");
		memset(cdr.Transfer, 0, DATA_SIZE);
		cdrReadInterruptSetResult(cdr.StatP | STATUS_ERROR);
		return;
	}

	if (!cdr.Irq1Pending)
		cdrUpdateTransferBuf(buf);

	if ((!cdr.Muted) && (cdr.Mode & MODE_STRSND) && (!Config.Xa) && (cdr.FirstSector != -1)) { // CD-XA
		hdr = buf + 4;
		// Firemen 2: Multi-XA files - briefings, cutscenes
		if( cdr.FirstSector == 1 && (cdr.Mode & MODE_SF)==0 ) {
			cdr.File = hdr[0];
			cdr.Channel = hdr[1];
		}

		/* Gameblabla 
		 * Skips playing on channel 255.
		 * Fixes missing audio in Blue's Clues : Blue's Big Musical. (Should also fix Taxi 2)
		 * TODO : Check if this is the proper behaviour.
		 * */
		if ((hdr[2] & 0x4) && hdr[0] == cdr.File && hdr[1] == cdr.Channel && cdr.Channel != 255) {
			int ret = xa_decode_sector(&cdr.Xa, buf + 4, cdr.FirstSector);
			if (!ret) {
				cdrAttenuate(cdr.Xa.pcm, cdr.Xa.nsamples, cdr.Xa.stereo);
				SPU_playADPCMchannel(&cdr.Xa, psxRegs.cycle, cdr.FirstSector);
				cdr.FirstSector = 0;
			}
			else cdr.FirstSector = -1;
		}
	}

	/*
	Croc 2: $40 - only FORM1 (*)
	Judge Dredd: $C8 - only FORM1 (*)
	Sim Theme Park - no adpcm at all (zero)
	*/

	if (!(cdr.Mode & MODE_STRSND) || !(buf[4+2] & 0x4))
		cdrReadInterruptSetResult(cdr.StatP);

	cdr.SetSectorPlay[2]++;
	if (cdr.SetSectorPlay[2] == 75) {
		cdr.SetSectorPlay[2] = 0;
		cdr.SetSectorPlay[1]++;
		if (cdr.SetSectorPlay[1] == 60) {
			cdr.SetSectorPlay[1] = 0;
			cdr.SetSectorPlay[0]++;
		}
	}

	if (!cdr.Irq1Pending) {
		// update for CdlGetlocP
		ReadTrack(cdr.SetSectorPlay);
	}

	CDRPLAYSEEKREAD_INT((cdr.Mode & MODE_SPEED) ? (cdReadTime / 2) : cdReadTime, 0);
}

/*
cdrRead0:
	bit 0,1 - mode
	bit 2 - unknown
	bit 3 - unknown
	bit 4 - unknown
	bit 5 - 1 result ready
	bit 6 - 1 dma ready
	bit 7 - 1 command being processed
*/

unsigned char cdrRead0(void) {
	if (cdr.ResultReady)
		cdr.Ctrl |= 0x20;
	else
		cdr.Ctrl &= ~0x20;

	cdr.Ctrl |= 0x40; // data fifo not empty

	// What means the 0x10 and the 0x08 bits? I only saw it used by the bios
	cdr.Ctrl |= 0x18;

	CDR_LOG_IO("cdr r0.sta: %02x\n", cdr.Ctrl);

	return psxHu8(0x1800) = cdr.Ctrl;
}

void cdrWrite0(unsigned char rt) {
	CDR_LOG_IO("cdr w0.idx: %02x\n", rt);

	cdr.Ctrl = (rt & 3) | (cdr.Ctrl & ~3);
}

unsigned char cdrRead1(void) {
	if ((cdr.ResultP & 0xf) < cdr.ResultC)
		psxHu8(0x1801) = cdr.Result[cdr.ResultP & 0xf];
	else
		psxHu8(0x1801) = 0;
	cdr.ResultP++;
	if (cdr.ResultP == cdr.ResultC)
		cdr.ResultReady = 0;

	CDR_LOG_IO("cdr r1.rsp: %02x #%u\n", psxHu8(0x1801), cdr.ResultP - 1);

	return psxHu8(0x1801);
}

void cdrWrite1(unsigned char rt) {
	const char *rnames[] = { "cmd", "smd", "smc", "arr" }; (void)rnames;
	CDR_LOG_IO("cdr w1.%s: %02x\n", rnames[cdr.Ctrl & 3], rt);

	switch (cdr.Ctrl & 3) {
	case 0:
		break;
	case 3:
		cdr.AttenuatorRightToRightT = rt;
		return;
	default:
		return;
	}

#ifdef CDR_LOG_CMD_IRQ
	SysPrintf("%u cdrom: CD1 write: %x (%s)", psxRegs.cycle, rt, CmdName[rt]);
	if (cdr.ParamC) {
		int i;
		SysPrintf(" Param[%d] = {", cdr.ParamC);
		for (i = 0; i < cdr.ParamC; i++)
			SysPrintf(" %x,", cdr.Param[i]);
		SysPrintf("}\n");
	} else {
		SysPrintf("\n");
	}
#endif

	cdr.ResultReady = 0;
	cdr.Ctrl |= 0x80;

	if (!cdr.CmdInProgress) {
		cdr.CmdInProgress = rt;
		// should be something like 12k + controller delays
		CDR_INT(5000);
	}
	else {
		CDR_LOG_I("%u cdrom: cmd while busy: %02x, prev %02x, busy %02x\n",
			psxRegs.cycle, rt, cdr.Cmd, cdr.CmdInProgress);
		if (cdr.CmdInProgress < 0x100) // no pending 2nd response
			cdr.CmdInProgress = rt;
	}

	cdr.Cmd = rt;
}

unsigned char cdrRead2(void) {
	unsigned char ret = 0;

	if (cdr.FifoOffset < cdr.FifoSize)
		ret = cdr.Transfer[cdr.FifoOffset++];
	else
		CDR_LOG_I("cdrom: read empty fifo (%d)\n", cdr.FifoSize);

	CDR_LOG_IO("cdr r2.dat: %02x\n", ret);
	return ret;
}

void cdrWrite2(unsigned char rt) {
	const char *rnames[] = { "prm", "ien", "all", "arl" }; (void)rnames;
	CDR_LOG_IO("cdr w2.%s: %02x\n", rnames[cdr.Ctrl & 3], rt);

	switch (cdr.Ctrl & 3) {
	case 0:
		if (cdr.ParamC < 8) // FIXME: size and wrapping
			cdr.Param[cdr.ParamC++] = rt;
		return;
	case 1:
		cdr.Reg2 = rt;
		setIrq(0x1005);
		return;
	case 2:
		cdr.AttenuatorLeftToLeftT = rt;
		return;
	case 3:
		cdr.AttenuatorRightToLeftT = rt;
		return;
	}
}

unsigned char cdrRead3(void) {
	if (cdr.Ctrl & 0x1)
		psxHu8(0x1803) = cdr.Stat | 0xE0;
	else
		psxHu8(0x1803) = cdr.Reg2 | 0xE0;

	CDR_LOG_IO("cdr r3.%s: %02x\n", (cdr.Ctrl & 1) ? "ifl" : "ien", psxHu8(0x1803));
	return psxHu8(0x1803);
}

void cdrWrite3(unsigned char rt) {
	const char *rnames[] = { "req", "ifl", "alr", "ava" }; (void)rnames;
	CDR_LOG_IO("cdr w3.%s: %02x\n", rnames[cdr.Ctrl & 3], rt);

	switch (cdr.Ctrl & 3) {
	case 0:
		break; // transfer
	case 1:
		if (cdr.Stat & rt) {
#ifdef CDR_LOG_CMD_IRQ
			SysPrintf("%u cdrom: ack %02x (w %02x)\n",
				psxRegs.cycle, cdr.Stat & rt, rt);
#endif
			// note: Croc vs Discworld Noir
			if (!(psxRegs.interrupt & (1 << PSXINT_CDR)) &&
			    (cdr.CmdInProgress || cdr.Irq1Pending))
				CDR_INT(850); // 711-993
		}
		cdr.Stat &= ~rt;

		if (rt & 0x40)
			cdr.ParamC = 0;
		return;
	case 2:
		cdr.AttenuatorLeftToRightT = rt;
		return;
	case 3:
		if (rt & 0x20) {
			memcpy(&cdr.AttenuatorLeftToLeft, &cdr.AttenuatorLeftToLeftT, 4);
			CDR_LOG("CD-XA Volume: %02x %02x | %02x %02x\n",
				cdr.AttenuatorLeftToLeft, cdr.AttenuatorLeftToRight,
				cdr.AttenuatorRightToLeft, cdr.AttenuatorRightToRight);
		}
		return;
	}

	// test: Viewpoint
	if ((rt & 0x80) && cdr.FifoOffset < cdr.FifoSize) {
		CDR_LOG("cdrom: FifoOffset(2) %d/%d\n", cdr.FifoOffset, cdr.FifoSize);
	}
	else if (rt & 0x80) {
		switch (cdr.Mode & 0x30) {
			case MODE_SIZE_2328:
			case 0x00:
				cdr.FifoOffset = 12;
				cdr.FifoSize = 2048 + 12;
				break;

			case MODE_SIZE_2340:
			default:
				cdr.FifoOffset = 0;
				cdr.FifoSize = 2340;
				break;
		}
	}
	else if (!(rt & 0xc0))
		cdr.FifoOffset = DATA_SIZE; // fifo empty
}

void psxDma3(u32 madr, u32 bcr, u32 chcr) {
	u32 cdsize;
	int size;
	u8 *ptr;

	CDR_LOG("psxDma3() Log: *** DMA 3 *** %x addr = %x size = %x\n", chcr, madr, bcr);

	switch (chcr & 0x71000000) {
		case 0x11000000:
			ptr = (u8 *)PSXM(madr);
			if (ptr == INVALID_PTR) {
				CDR_LOG_I("psxDma3() Log: *** DMA 3 *** NULL Pointer!\n");
				break;
			}

			cdsize = (((bcr - 1) & 0xffff) + 1) * 4;

			/*
			GS CDX: Enhancement CD crash
			- Setloc 0:0:0
			- CdlPlay
			- Spams DMA3 and gets buffer overrun
			*/
			size = DATA_SIZE - cdr.FifoOffset;
			if (size > cdsize)
				size = cdsize;
			if (size > 0)
			{
				memcpy(ptr, cdr.Transfer + cdr.FifoOffset, size);
				cdr.FifoOffset += size;
				psxCpu->Clear(madr, size / 4);
			}
			if (size < cdsize)
				CDR_LOG_I("cdrom: dma3 %d/%d\n", size, cdsize);

			CDRDMA_INT((cdsize/4) * 24);

			HW_DMA3_CHCR &= SWAPu32(~0x10000000);
			if (chcr & 0x100) {
				HW_DMA3_MADR = SWAPu32(madr + cdsize);
				HW_DMA3_BCR &= SWAPu32(0xffff0000);
			}
			else {
				// halted
				psxRegs.cycle += (cdsize/4) * 24 - 20;
			}
			return;

		default:
			CDR_LOG_I("psxDma3() Log: Unknown cddma %x\n", chcr);
			break;
	}

	HW_DMA3_CHCR &= SWAP32(~0x01000000);
	DMA_INTERRUPT(3);
}

void cdrDmaInterrupt(void)
{
	if (HW_DMA3_CHCR & SWAP32(0x01000000))
	{
		HW_DMA3_CHCR &= SWAP32(~0x01000000);
		DMA_INTERRUPT(3);
	}
}

static void getCdInfo(void)
{
	u8 tmp;

	CDR_getTN(cdr.ResultTN);
	CDR_getTD(0, cdr.SetSectorEnd);
	tmp = cdr.SetSectorEnd[0];
	cdr.SetSectorEnd[0] = cdr.SetSectorEnd[2];
	cdr.SetSectorEnd[2] = tmp;
}

void cdrReset() {
	memset(&cdr, 0, sizeof(cdr));
	cdr.CurTrack = 1;
	cdr.File = 1;
	cdr.Channel = 1;
	cdr.Reg2 = 0x1f;
	cdr.Stat = NoIntr;
	cdr.FifoOffset = DATA_SIZE; // fifo empty
	if (CdromId[0] == '\0') {
		cdr.DriveState = DRIVESTATE_STOPPED;
		cdr.StatP = 0;
	}
	else {
		cdr.DriveState = DRIVESTATE_STANDBY;
		cdr.StatP = STATUS_ROTATING;
	}
	
	// BIOS player - default values
	cdr.AttenuatorLeftToLeft = 0x80;
	cdr.AttenuatorLeftToRight = 0x00;
	cdr.AttenuatorRightToLeft = 0x00;
	cdr.AttenuatorRightToRight = 0x80;

	getCdInfo();
}

int cdrFreeze(void *f, int Mode) {
	u32 tmp;
	u8 tmpp[3];

	if (Mode == 0 && !Config.Cdda)
		CDR_stop();
	
	cdr.freeze_ver = 0x63647202;
	gzfreeze(&cdr, sizeof(cdr));
	
	if (Mode == 1) {
		cdr.ParamP = cdr.ParamC;
		tmp = cdr.FifoOffset;
	}

	gzfreeze(&tmp, sizeof(tmp));

	if (Mode == 0) {
		getCdInfo();

		cdr.FifoOffset = tmp;
		cdr.FifoSize = (cdr.Mode & 0x20) ? 2340 : 2048 + 12;

		// read right sub data
		tmpp[0] = btoi(cdr.Prev[0]);
		tmpp[1] = btoi(cdr.Prev[1]);
		tmpp[2] = btoi(cdr.Prev[2]);
		cdr.Prev[0]++;
		ReadTrack(tmpp);

		if (cdr.Play) {
			if (cdr.freeze_ver < 0x63647202)
				memcpy(cdr.SetSectorPlay, cdr.SetSector, 3);

			Find_CurTrack(cdr.SetSectorPlay);
			if (!Config.Cdda)
				CDR_play(cdr.SetSectorPlay);
			if (psxRegs.interrupt & (1 << PSXINT_CDRPLAY_OLD))
				CDRPLAYSEEKREAD_INT((cdr.Mode & 0x80) ? (cdReadTime / 2) : cdReadTime, 1);
		}

		if ((cdr.freeze_ver & 0xffffff00) != 0x63647200) {
			// old versions did not latch Reg2, have to fixup..
			if (cdr.Reg2 == 0) {
				SysPrintf("cdrom: fixing up old savestate\n");
				cdr.Reg2 = 7;
			}
			// also did not save Attenuator..
			if ((cdr.AttenuatorLeftToLeft | cdr.AttenuatorLeftToRight
			     | cdr.AttenuatorRightToLeft | cdr.AttenuatorRightToRight) == 0)
			{
				cdr.AttenuatorLeftToLeft = cdr.AttenuatorRightToRight = 0x80;
			}
		}
	}

	return 0;
}

void LidInterrupt(void) {
	getCdInfo();
	cdrLidSeekInterrupt();
}
