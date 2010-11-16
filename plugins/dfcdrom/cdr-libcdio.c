/*
 * Copyright (c) 2010, Wei Mingzhi <whistler@openoffice.org>.
 * All Rights Reserved.
 *
 * Based on: Cdrom for Psemu Pro like Emulators
 * By: linuzappz <linuzappz@hotmail.com>
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

#ifdef USE_LIBCDIO

#include <cdio/cdio.h>
#include <cdio/mmc.h>

#include "cdr.h"

static CdIo_t *cdHandle = NULL;

static void SetSpeed(int speed) {
	speed *= 176;
	if (speed == 0) speed = 0xFFFF;

	cdio_set_speed(cdHandle, speed);
}

static void SetSpinDown(unsigned char spindown) {
	mmc_cdb_t		cdb;
	char			buf[16];

	memset(&cdb, 0, sizeof(cdb));

	cdb.field[0] = 0x5A;
	cdb.field[2] = 0x0D;
	cdb.field[8] = sizeof(buf);

	if (mmc_run_cmd(cdHandle, 10000, &cdb, SCSI_MMC_DATA_READ, sizeof(buf), buf) != DRIVER_OP_SUCCESS)
		return;

	buf[11] = (buf[11] & 0xF0) | (spindown & 0x0F);

	memset(&cdb, 0, sizeof(cdb));
	memset(buf, 0, 2);

	cdb.field[0] = 0x55;
	cdb.field[1] = 0x10;
	cdb.field[8] = sizeof(buf);

	mmc_run_cmd(cdHandle, 10000, &cdb, SCSI_MMC_DATA_WRITE, sizeof(buf), buf);
}

static void UnlockDoor() {
	mmc_cdb_t	   cdb;

	memset(&cdb, 0, sizeof(cdb));

	cdb.field[0] = 0x1E;
	cdb.field[4] = 0;

	mmc_run_cmd(cdHandle, 10000, &cdb, SCSI_MMC_DATA_WRITE, 0, NULL);
}

int OpenCdHandle(const char *dev) {
	if (dev == NULL || dev[0] == '\0') {
		if ((dev = cdio_get_default_device(NULL)) == NULL) {
			return -1;
		}
	}

#ifdef __FreeBSD__
	cdHandle = cdio_open_am_cd(dev, "CAM");
#else
	cdHandle = cdio_open_cd(dev);
#endif

	if (cdHandle != NULL) {
		SetSpeed(CdrSpeed);
		SetSpinDown(SpinDown);
		UnlockDoor();

		return 0;
	}

	return -1;
}

void CloseCdHandle() {
	if (cdHandle != NULL) {
		cdio_set_speed(cdHandle, 0xFFFF);
		SetSpinDown(SPINDOWN_VENDOR_SPECIFIC);

		cdio_destroy(cdHandle);
	}

	cdHandle = NULL;
}

int IsCdHandleOpen() {
	return (cdHandle != NULL);
}

long GetTN(unsigned char *buffer) {
	buffer[0] = cdio_get_first_track_num(cdHandle);
	buffer[1] = cdio_get_last_track_num(cdHandle);

	return 0;
}

long GetTD(unsigned char track, unsigned char *buffer) {
	msf_t msf;

	if (track == 0) track = CDIO_CDROM_LEADOUT_TRACK;

	if (!cdio_get_track_msf(cdHandle, track, &msf)) {
		memset(buffer + 1, 0, 3);
		return 0;
	}

	buffer[0] = btoi(msf.f);
	buffer[1] = btoi(msf.s);
	buffer[2] = btoi(msf.m);

	return 0;
}

long GetTE(unsigned char track, unsigned char *m, unsigned char *s, unsigned char *f) {
	unsigned char msf[3];

	lba_to_msf(cdio_get_track_lba(cdHandle, track + 1) - CD_MSF_OFFSET, msf);

	*m = msf[0];
	*s = msf[1];
	*f = msf[2];

	return 0;
}

long ReadSector(crdata *cr) {
	int					lba;
	MMC_READ_CD			cdb;

	lba = msf_to_lba(cr->msf.cdmsf_min0, cr->msf.cdmsf_sec0, cr->msf.cdmsf_frame0);
	memset(&cdb, 0, sizeof(cdb));

	cdb.Code = 0xBE;
	cdb.IncludeEDC = 1;
	cdb.IncludeUserData = 1;
	cdb.HeaderCode = 3;
	cdb.IncludeSyncData = 1;
	cdb.SubChannelSelection = 0;
	cdb.StartingLBA[1] = lba >> 16;
	cdb.StartingLBA[2] = lba >> 8;
	cdb.StartingLBA[3] = lba;
	cdb.TransferBlocks[2] = 1;

	if (mmc_run_cmd(cdHandle, 10000, (mmc_cdb_t *)&cdb, SCSI_MMC_DATA_READ, sizeof(*cr), cr) != DRIVER_OP_SUCCESS)
		return -1;

	return 0;
}

long PlayCDDA(unsigned char *sector) {
	msf_t start, end;

	if (!cdio_get_track_msf(cdHandle, CDIO_CDROM_LEADOUT_TRACK, &end))
		return -1;

	start.m = itob(sector[0]);
	start.s = itob(sector[1]);
	start.f = itob(sector[2]);

	if (cdio_audio_play_msf(cdHandle, &start, &end) != DRIVER_OP_SUCCESS)
		return -1;

	return 0;
}

long StopCDDA() {
	cdio_subchannel_t subchnl;

	if (cdio_audio_read_subchannel(cdHandle, &subchnl) != DRIVER_OP_SUCCESS)
		return -1;

	switch (subchnl.audio_status) {
		case CDIO_MMC_READ_SUB_ST_PLAY:
		case CDIO_MMC_READ_SUB_ST_PAUSED:
			cdio_audio_stop(cdHandle);
	}

	return 0;
}

long GetStatus(int playing, struct CdrStat *stat) {
	cdio_subchannel_t subchnl;

	memset(stat, 0, sizeof(struct CdrStat));

	if (playing) {
		if (cdio_audio_read_subchannel(cdHandle, &subchnl) == DRIVER_OP_SUCCESS) {
			stat->Time[0] = btoi(subchnl.abs_addr.m);
			stat->Time[1] = btoi(subchnl.abs_addr.s);
			stat->Time[2] = btoi(subchnl.abs_addr.f);
		}
	}

	stat->Type = 0x01;

	if (mmc_get_tray_status(cdHandle)) {
		stat->Type = 0xff;
		stat->Status |= 0x10;
	} else {
		SetSpeed(CdrSpeed);
		SetSpinDown(SpinDown);
		UnlockDoor();
	}

	return 0;
}

unsigned char *ReadSub(const unsigned char *time) {
	int lba = msf_to_lba(btoi(time[0]), btoi(time[1]), btoi(time[2]));
	static unsigned char buf[CD_FRAMESIZE_RAW + 96];

	MMC_READ_CD cdb;

	memset(&cdb, 0, sizeof(cdb));

	cdb.Code = 0xBE;
	cdb.IncludeEDC = 1;
	cdb.IncludeUserData = 1;
	cdb.HeaderCode = 3;
	cdb.IncludeSyncData = 1;
	cdb.StartingLBA[1] = lba >> 16;
	cdb.StartingLBA[2] = lba >> 8;
	cdb.StartingLBA[3] = lba;
	cdb.TransferBlocks[2] = 1;
	cdb.SubChannelSelection = 1;

	if (mmc_run_cmd(cdHandle, 10000, (mmc_cdb_t *)&cdb, SCSI_MMC_DATA_READ, sizeof(buf), buf) != DRIVER_OP_SUCCESS)
		return NULL;

	DecodeRawSubData(buf + CD_FRAMESIZE_RAW);
	return buf + CD_FRAMESIZE_RAW;
}

#endif
