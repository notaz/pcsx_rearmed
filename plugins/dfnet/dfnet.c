//
// DF Netplay Plugin
//
// Based on netSock 0.2 by linuzappz.
// The Plugin is free source code.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
extern int errno;

#include "dfnet.h"

const unsigned char version  = 2;    // NET library v2
const unsigned char revision = 0;
const unsigned char build    = 3;    // increase that with each version

static char *libraryName      = N_("Socket Driver");

unsigned long CALLBACK PSEgetLibType() {
	return PSE_LT_NET;
}

char* CALLBACK PSEgetLibName() {
	return _(libraryName);
}

unsigned long CALLBACK PSEgetLibVersion() {
	return version << 16 | revision << 8 | build;
}

long CALLBACK NETinit() {
	return sockInit();
}

int SEND(const void *buf, int Size, int Mode) {
	int bytes;
	int count = 0;
	const char *pData = (const char *)buf;

	if (Mode & PSE_NET_NONBLOCKING) { // NONBLOCKING
		int ret;

		FD_ZERO(&wset);
		FD_SET(sock, &wset);

		ret = select(sock + 1, NULL, &wset, NULL, &tm);
		if (ret == -1) return -1;

		if (FD_ISSET(sock, &wset)) {
			return send(sock, pData, Size, 0);
		} else {
			return 0;
		}
	} else { // BLOCKING
		while (Size > 0) {
			bytes = send(sock, pData, Size, 0);
			if (bytes < 0) return -1;
			pData += bytes; Size -= bytes;
			count += bytes;
		}
	}

	return count;
}

int RECV(void *buf, int Size, int Mode) {
	int bytes;
	int count = 0;
	char *pData = (char *)buf;

	if (Mode & PSE_NET_NONBLOCKING) { // NONBLOCKING
		int ret;

		FD_ZERO(&rset);
		FD_SET(sock, &rset);

		ret = select(sock, &rset, NULL, NULL, &tm);

		if (FD_ISSET(sock, &rset)) {
			return recv(sock, pData, Size, 0);
		} else {
			return 0;
		}
	} else { // BLOCKING
		while (Size > 0) {
			bytes = recv(sock, pData, Size, 0);
			if (bytes == -1) return -1;
			pData+= bytes; Size-= bytes;
			count+= bytes;
		}
	}

	return count;
}

long CALLBACK NETopen(unsigned long *gpuDisp) {
	int ret = sockOpen();

	struct sockaddr_in address;

	if (ret == -1) return -1;

	if (conf.PlayerNum == 1) {
		int listen_sock, reuse_addr = 1;
		int ret;

		memset((char *)&address, 0, sizeof (address));

		address.sin_family = AF_INET;
		address.sin_port = htons(conf.PortNum);
		address.sin_addr.s_addr = INADDR_ANY;

		listen_sock = socket(AF_INET, SOCK_STREAM, 0);
		if (listen_sock == -1)
			return -1;

		setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse_addr, sizeof(reuse_addr));

		if (bind(listen_sock,(struct sockaddr *) &address, sizeof(address)) == -1)
			return -1;

		if (listen(listen_sock, 1) != 0) 
			return -1; 

		sock = -1;

		WaitCancel = 0;
		sockCreateWaitDlg();

		while (sock < 0) {
			FD_ZERO(&rset);
			FD_SET(listen_sock, &rset);

			ret = select(listen_sock + 1, &rset, NULL, NULL, &tm);
			if (FD_ISSET(listen_sock, &rset)) {
				sock = accept(listen_sock, NULL, NULL);
			}

			if (WaitCancel) break;
			sockDlgUpdate();
		}
		close(listen_sock);

		sockDestroyWaitDlg();
		if (WaitCancel == 1) return -1;
	} else {
		memset((char *)&address, 0, sizeof(address));
		address.sin_family = AF_INET;
		address.sin_port = htons(conf.PortNum);
		address.sin_addr.s_addr = inet_addr(conf.ipAddress);

		sock = socket(AF_INET, SOCK_STREAM, 0);

		if (connect(sock, (struct sockaddr *)&address, sizeof(address))!=0) {
			SysMessage(_("error connecting to %s: %s\n"), conf.ipAddress, strerror(errno));
			return -1;
		}
	}

	PadInit = 0;
	PadCount = 0;
	PadSize[0] = -1;
	PadSize[1] = -1;
	PadRecvSize = -1;
	PadSendSize = -1;
	Ping = sockPing();
	Ping = (sockPing() + Ping) / 2;
	Ping = (sockPing() + Ping) / 2;

	if (conf.PlayerNum == 1) {
		PadCountMax = (int)(((double)Ping / 1000.0) * 60.0);
		if (PadCountMax <= 0) PadCountMax = 1;
		SEND(&PadCountMax, 4, PSE_NET_BLOCKING);
	} else {
		RECV(&PadCountMax, 4, PSE_NET_BLOCKING);
	}

	PadSendData = (char *)malloc(PadCountMax * 128);
	if (PadSendData == NULL) {
		SysMessage(_("Error allocating memory!\n")); return -1;
	}
	memset(PadSendData, 0xff, PadCountMax);

	return ret;
}

long CALLBACK NETclose() {
	close(sock);

	return 0;
}

long CALLBACK NETshutdown() {
	return sockShutdown();
}

void CALLBACK NETpause() {
/*	unsigned char Code = 0x80;

	SEND(&Code, 1, PSE_NET_BLOCKING);*/
}

void CALLBACK NETresume() {
/*	unsigned char Code = 0x80;

	SEND(&Code, 1, PSE_NET_BLOCKING);*/
}

long CALLBACK NETsendData(void *pData, int Size, int Mode) {
	return SEND(pData, Size, Mode);
}

long CALLBACK NETrecvData(void *pData, int Size, int Mode) {
	return RECV(pData, Size, Mode);
}

long CALLBACK NETsendPadData(void *pData, int Size) {
	if (PadSendSize == -1) {
		PadSendSize = Size;

		if (SEND(&PadSendSize, 1, PSE_NET_BLOCKING) == -1)
			return -1;

		if (RECV(&PadRecvSize, 1, PSE_NET_BLOCKING) == -1)
			return -1;
	}

	memcpy(&PadSendData[PadCount], pData, Size);
	if (SEND(pData, PadSendSize, PSE_NET_BLOCKING) == -1)
		return -1;

	return 0;
}

long CALLBACK NETrecvPadData(void *pData, int Pad) {
	if (PadInit == 0) {
		if (conf.PlayerNum == Pad) {
			memset(pData, 0xff, PadSendSize);
		} else {
			memset(pData, 0xff, PadRecvSize);
		}
	} else {
		if (conf.PlayerNum == Pad) {
			memcpy(pData, &PadSendData[PadCount == 0 ? PadCountMax-1 : PadCount-1], PadSendSize);
		} else {
			if (RECV(pData, PadRecvSize, PSE_NET_BLOCKING) == -1)
				return -1;
		}
	}

	if (Pad == 2) {
		PadCount++;
		if (PadCount == PadCountMax) {
			PadCount = 0;
			PadInit = 1;
		}
	}

	return 0;
}

long CALLBACK NETqueryPlayer() {
	return conf.PlayerNum;
}

long CALLBACK NETtest() {
	return 0;
}
