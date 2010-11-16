/*
 * Copyright (c) 2010, Wei Mingzhi <whistler@openoffice.org>.
 * All Rights Reserved.
 *
 * Based on: Cdrom for Psemu Pro like Emulators
 * By: linuzappz <linuzappz@hotmail.com>
 *
 * Portions based on: cdrdao - write audio CD-Rs in disc-at-once mode
 * Copyright (C) 2007 Denis Leroy <denis@poolshark.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses>.
 */

#if defined (__linux__) && !defined (USE_LIBCDIO)

#include "cdr.h"

static int cdHandle = -1;
static int ReadMMC = 0, SubQMMC = 0;

static int SendMMCCmd(const unsigned char *cmd, int cmdLen, const unsigned char *dataOut,
	int dataOutLen, unsigned char *dataIn, int dataInLen)
{
	sg_io_hdr_t io_hdr;

	memset(&io_hdr, 0, sizeof(io_hdr));

	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = cmdLen;
	io_hdr.cmdp = (unsigned char *)cmd;
	io_hdr.timeout = 10000;
	io_hdr.sbp = NULL;
	io_hdr.mx_sb_len = 0;
	io_hdr.flags = 1;

	if (dataOut != NULL) {
		io_hdr.dxferp = (void *)dataOut;
		io_hdr.dxfer_len = dataOutLen;
		io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	} else if (dataIn != NULL) {
		io_hdr.dxferp = (void *)dataIn;
		io_hdr.dxfer_len = dataInLen;
		io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	}

	if (ioctl(cdHandle, SG_IO, &io_hdr) < 0) {
		return -1;
	}

	return io_hdr.status;
}

static int CheckReadMMC() {
	MMC_READ_CD			cdb;
	unsigned char		buf[CD_FRAMESIZE_RAW];

	memset(&cdb, 0, sizeof(cdb));
	memset(buf, 0xAA, sizeof(buf));

	cdb.Code = GPCMD_READ_CD;
	cdb.IncludeEDC = 0;
	cdb.IncludeUserData = 1;
	cdb.HeaderCode = 3;
	cdb.IncludeSyncData = 1;
	cdb.TransferBlocks[2] = 1;

	if (SendMMCCmd((unsigned char *)&cdb, sizeof(cdb), NULL, 0, buf, sizeof(buf)) == 0) {
		if (buf[0] != 0xAA) {
			PRINTF("Using MMC for data\n");
			return 1; // supported
		}
	}

	return 0; // NOT supported
}

static int CheckSubQMMC() {
	MMC_READ_CD			cdb;
	unsigned char		buf[CD_FRAMESIZE_RAW + 96];

	memset(&cdb, 0, sizeof(cdb));
	memset(buf, 0xAA, sizeof(buf));

	cdb.Code = GPCMD_READ_CD;
	cdb.IncludeEDC = 1;
	cdb.IncludeUserData = 1;
	cdb.HeaderCode = 3;
	cdb.IncludeSyncData = 1;
	cdb.SubChannelSelection = 1;
	cdb.TransferBlocks[2] = 1;

	if (SendMMCCmd((unsigned char *)&cdb, sizeof(cdb), NULL, 0, buf, sizeof(buf)) == 0) {
		if (buf[0] != 0xAA && (buf[2352] != 0xAA || buf[2353] != 0xAA)) {
			PRINTF("Using MMC for subchannel\n");
			return 1; // supported
		}
	}

	return 0; // NOT supported
}

int OpenCdHandle(const char *dev) {
	char spindown;

	cdHandle = open(dev, O_RDONLY);

	if (cdHandle != -1) {
		ioctl(cdHandle, CDROM_LOCKDOOR, 0);

		spindown = (char)SpinDown;
		ioctl(cdHandle, CDROMSETSPINDOWN, &spindown);

		ioctl(cdHandle, CDROM_SELECT_SPEED, CdrSpeed);

		ReadMMC = CheckReadMMC();
		SubQMMC = CheckSubQMMC();

		return 0;
	}

	return -1;
}

void CloseCdHandle() {
	char spindown = SPINDOWN_VENDOR_SPECIFIC;
	ioctl(cdHandle, CDROMSETSPINDOWN, &spindown);

	close(cdHandle);

	cdHandle = -1;
}

int IsCdHandleOpen() {
	return (cdHandle != -1);
}

long GetTN(unsigned char *buffer) {
	struct cdrom_tochdr toc;

	if (ioctl(cdHandle, CDROMREADTOCHDR, &toc) == -1)
		return -1;

	buffer[0] = toc.cdth_trk0;	// start track
	buffer[1] = toc.cdth_trk1;	// end track

	return 0;
}

long GetTD(unsigned char track, unsigned char *buffer) {
	struct cdrom_tocentry entry;

	if (track == 0)
		track = 0xAA; // total time (leadout)
	entry.cdte_track = track;
	entry.cdte_format = CDROM_MSF;

	if (ioctl(cdHandle, CDROMREADTOCENTRY, &entry) == -1)
		return -1;

	buffer[0] = entry.cdte_addr.msf.frame;
	buffer[1] = entry.cdte_addr.msf.second;
	buffer[2] = entry.cdte_addr.msf.minute;

	return 0;
}

long GetTE(unsigned char track, unsigned char *m, unsigned char *s, unsigned char *f) {
	struct cdrom_tocentry entry;
	unsigned char msf[3];

	if (GetTN(msf) == -1) return -1;

	entry.cdte_track = track + 1;
	if (entry.cdte_track > msf[1]) entry.cdte_track = 0xaa;

	entry.cdte_format = CDROM_MSF;

	if (ioctl(cdHandle, CDROMREADTOCENTRY, &entry) == -1)
		return -1;

	lba_to_msf(msf_to_lba(entry.cdte_addr.msf.minute, entry.cdte_addr.msf.second, entry.cdte_addr.msf.frame) - CD_MSF_OFFSET, msf);

	*m = msf[0];
	*s = msf[1];
	*f = msf[2];

	return 0;
}

long ReadSector(crdata *cr) {
	if (ReadMMC) {
		MMC_READ_CD			cdb;
		int					lba;

		memset(&cdb, 0, sizeof(cdb));

		lba = msf_to_lba(cr->msf.cdmsf_min0, cr->msf.cdmsf_sec0, cr->msf.cdmsf_frame0);

		cdb.Code = GPCMD_READ_CD;
		cdb.IncludeEDC = 1;
		cdb.IncludeUserData = 1;
		cdb.HeaderCode = 3;
		cdb.IncludeSyncData = 1;
		cdb.SubChannelSelection = 0;
		cdb.StartingLBA[1] = lba >> 16;
		cdb.StartingLBA[2] = lba >> 8;
		cdb.StartingLBA[3] = lba;
		cdb.TransferBlocks[2] = 1;

		if (SendMMCCmd((unsigned char *)&cdb, sizeof(cdb), NULL, 0, (unsigned char *)cr, sizeof(*cr)) != 0)
			return -1;
	} else {
		if (ioctl(cdHandle, CDROMREADRAW, cr) == -1)
			return -1;
	}

	return 0;
}

long PlayCDDA(unsigned char *sector) {
	struct cdrom_msf addr;
	unsigned char ptmp[4];

	// 0 is the last track of every cdrom, so play up to there
	if (GetTD(0, ptmp) == -1)
		return -1;

	addr.cdmsf_min0 = sector[0];
	addr.cdmsf_sec0 = sector[1];
	addr.cdmsf_frame0 = sector[2];
	addr.cdmsf_min1 = ptmp[2];
	addr.cdmsf_sec1 = ptmp[1];
	addr.cdmsf_frame1 = ptmp[0];

	if (ioctl(cdHandle, CDROMPLAYMSF, &addr) == -1)
		return -1;

	return 0;
}

long StopCDDA() {
	struct cdrom_subchnl sc;

	sc.cdsc_format = CDROM_MSF;
	if (ioctl(cdHandle, CDROMSUBCHNL, &sc) == -1)
		return -1;

	switch (sc.cdsc_audiostatus) {
		case CDROM_AUDIO_PAUSED:
		case CDROM_AUDIO_PLAY:
			ioctl(cdHandle, CDROMSTOP);
			break;
	}

	return 0;
}

long GetStatus(int playing, struct CdrStat *stat) {
	struct cdrom_subchnl sc;
	int ret;
	char spindown;

	memset(stat, 0, sizeof(struct CdrStat));

	if (playing) { // return Time only if playing
		sc.cdsc_format = CDROM_MSF;
		if (ioctl(cdHandle, CDROMSUBCHNL, &sc) != -1)
			memcpy(stat->Time, &sc.cdsc_absaddr.msf, 3);
	}

	ret = ioctl(cdHandle, CDROM_DISC_STATUS);
	switch (ret) {
		case CDS_AUDIO:
			stat->Type = 0x02;
			break;
		case CDS_DATA_1:
		case CDS_DATA_2:
		case CDS_XA_2_1:
		case CDS_XA_2_2:
			stat->Type = 0x01;
			break;
	}
	ret = ioctl(cdHandle, CDROM_DRIVE_STATUS);
	switch (ret) {
		case CDS_NO_DISC:
		case CDS_TRAY_OPEN:
			stat->Type = 0xff;
			stat->Status |= 0x10;
			break;
		default:
			spindown = (char)SpinDown;
			ioctl(cdHandle, CDROMSETSPINDOWN, &spindown);
			ioctl(cdHandle, CDROM_SELECT_SPEED, CdrSpeed);
			ioctl(cdHandle, CDROM_LOCKDOOR, 0);
			break;
	}

	switch (sc.cdsc_audiostatus) {
		case CDROM_AUDIO_PLAY:
			stat->Status |= 0x80;
			break;
	}

	return 0;
}

static unsigned char *ReadSubMMC(const unsigned char *time) {
	static unsigned char buf[CD_FRAMESIZE_RAW + 96];
	int lba = msf_to_lba(btoi(time[0]), btoi(time[1]), btoi(time[2]));
	MMC_READ_CD cdb;

	memset(&cdb, 0, sizeof(cdb));

	cdb.Code = GPCMD_READ_CD;
	cdb.IncludeEDC = 1;
	cdb.IncludeUserData = 1;
	cdb.HeaderCode = 3;
	cdb.IncludeSyncData = 1;
	cdb.StartingLBA[1] = lba >> 16;
	cdb.StartingLBA[2] = lba >> 8;
	cdb.StartingLBA[3] = lba;
	cdb.TransferBlocks[2] = 1;
	cdb.SubChannelSelection = 1;

	if (SendMMCCmd((unsigned char *)&cdb, sizeof(cdb), NULL, 0, buf, sizeof(buf)) != 0)
		return NULL;

	DecodeRawSubData(buf + CD_FRAMESIZE_RAW);
	return buf + CD_FRAMESIZE_RAW;
}

static unsigned char *ReadSubIOCTL(const unsigned char *time) {
	static struct SubQ subq;
	struct cdrom_subchnl subchnl;
	int r;
	crdata cr;
	unsigned short crc;

	cr.msf.cdmsf_min0 = btoi(time[0]);
	cr.msf.cdmsf_sec0 = btoi(time[1]);
	cr.msf.cdmsf_frame0 = btoi(time[2]);

	if (ioctl(cdHandle, CDROMSEEK, &cr.msf) == -1) {
		// will be slower, but there's no other way to make it accurate
		if (ioctl(cdHandle, CDROMREADRAW, &cr) == -1) {
			return NULL;
		}
	}

	subchnl.cdsc_format = CDROM_MSF;
	r = ioctl(cdHandle, CDROMSUBCHNL, &subchnl);

	if (r == -1) return NULL;

	subq.ControlAndADR = 0x41;
	subq.TrackNumber = subchnl.cdsc_trk;
	subq.IndexNumber = subchnl.cdsc_ind;
	subq.TrackRelativeAddress[0] = itob(subchnl.cdsc_reladdr.msf.minute);
	subq.TrackRelativeAddress[1] = itob(subchnl.cdsc_reladdr.msf.second);
	subq.TrackRelativeAddress[2] = itob(subchnl.cdsc_reladdr.msf.frame);
	subq.AbsoluteAddress[0] = itob(subchnl.cdsc_absaddr.msf.minute);
	subq.AbsoluteAddress[1] = itob(subchnl.cdsc_absaddr.msf.second);
	subq.AbsoluteAddress[2] = itob(subchnl.cdsc_absaddr.msf.frame);

	// CRC is not supported with IOCTL, fake it.
	crc = calcCrc((unsigned char *)&subq + 12, 10);
	subq.CRC[0] = (crc >> 8);
	subq.CRC[1] = (crc & 0xFF);

	r = msf_to_lba(btoi(time[0]), btoi(time[1]), btoi(time[2]));

	if (GetTE(1, &cr.msf.cdmsf_min0, &cr.msf.cdmsf_sec0, &cr.msf.cdmsf_frame0) == -1) {
		cr.msf.cdmsf_min0 = 80;
		cr.msf.cdmsf_sec0 = 0;
		cr.msf.cdmsf_frame0 = 0;
	}

	if (msf_to_lba(cr.msf.cdmsf_min0, cr.msf.cdmsf_sec0, cr.msf.cdmsf_frame0) >= r &&
		(msf_to_lba(subchnl.cdsc_absaddr.msf.minute, subchnl.cdsc_absaddr.msf.second, subchnl.cdsc_absaddr.msf.frame) != r ||
		msf_to_lba(subchnl.cdsc_reladdr.msf.minute, subchnl.cdsc_reladdr.msf.second, subchnl.cdsc_reladdr.msf.frame) != r - CD_MSF_OFFSET))
		subq.CRC[1] ^= 1; // time mismatch; report wrong CRC

	PRINTF("subq : %x,%x : %x,%x,%x : %x,%x,%x\n",
		subchnl.cdsc_trk, subchnl.cdsc_ind,
		itob(subchnl.cdsc_reladdr.msf.minute), itob(subchnl.cdsc_reladdr.msf.second), itob(subchnl.cdsc_reladdr.msf.frame),
		itob(subchnl.cdsc_absaddr.msf.minute), itob(subchnl.cdsc_absaddr.msf.second), itob(subchnl.cdsc_absaddr.msf.frame));

	return (unsigned char *)&subq;
}

unsigned char *ReadSub(const unsigned char *time) {
	if (SubQMMC) return ReadSubMMC(time);
	else return ReadSubIOCTL(time);
}

#endif
