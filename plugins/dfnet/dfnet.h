//
// DF Netplay Plugin
//
// Based on netSock 0.2 by linuzappz.
// The Plugin is free source code.
//

#ifndef __DFNET_H__
#define __DFNET_H__

#include "config.h"

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#include <locale.h>
#define _(x)  gettext(x)
#define N_(x) (x)
#else
#define _(x)  (x)
#define N_(x) (x)
#endif

typedef void* HWND;

struct timeval tm;

#define CALLBACK

long timeGetTime();

#include "psemu_plugin_defs.h"

typedef struct {
	int PlayerNum;
	unsigned short PortNum;
	char ipAddress[32];
} Config;

Config conf;

void LoadConf();
void SaveConf();

long sock;
char *PadSendData;
char *PadRecvData;
char PadSendSize;
char PadRecvSize;
char PadSize[2];
int PadCount;
int PadCountMax;
int PadInit;
int Ping;
volatile int WaitCancel;
fd_set rset;
fd_set wset;

long sockInit();
long sockShutdown();
long sockOpen();
void sockCreateWaitDlg();
void sockDlgUpdate();
void sockDestroyWaitDlg();
int sockPing();

int ShowPauseDlg();
void SysMessage(const char *fmt, ...);

int SEND(const void *pData, int Size, int Mode);
int RECV(void *pData, int Size, int Mode);

#endif
