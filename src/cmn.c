////////////////////////////////////////////////////////////////////////////
//
// cmn.c                           I    OOO                           h
//                                 I   O   O     t      eee    cccc   h
//                                 I  O     O  ttttt   e   e  c       h hhh
// -----------------------------   I  O     O    t     eeeee  c       hh   h
// copyright: (C) 2002 by IOtech   I   O   O     t     e      c       h    h
//    email: linux@iotech.com      I    OOO       tt    eee    cccc   h    h
//
////////////////////////////////////////////////////////////////////////////
//
//   Copyright (C) 86, 91, 1995-2002, Free Software Foundation, Inc.
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2, or (at your option)
//   any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software Foundation,
//   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
//  
////////////////////////////////////////////////////////////////////////////
//
// Thanks to Amish S. Dave for getting us started up!
// Savagely hacked by Paul Knowles <Paul.Knowles@unifr.ch>,  May, 2003
//
// Many Thanks Paul, MAH
//
////////////////////////////////////////////////////////////////////////////
//
// Please email liunx@iotech with any bug reports questions or comments
//
////////////////////////////////////////////////////////////////////////////

#ifdef DLINUX
#include <linux/limits.h>
#endif
#define _GNU_SOURCE
#include <stdio.h>
#include <ctype.h>

#include "iottypes.h"
#include "daqx.h"
#include "cmnapi.h"
#include "api.h"
#include "ddapi.h"
#include "itfapi.h"

static DaqHandleT hMutex[MaxMutexes];


VOID
cmnBroadcastMessage(MsgPT msg)
{
	switch (msg->msg) {
	case MsgAttach:
	case MsgInit:
		itfCtrlBroadcastMessage(msg);        
		apiCtrlBroadcastMessage(msg);
		break;

	case MsgDetach:
	case MsgClose:
		apiCtrlBroadcastMessage(msg);
		itfCtrlBroadcastMessage(msg);
		break;
	}

}

BOOL WINAPI
DllMain(HINSTANCE hDLL, DWORD dwReason, PVOID lpReserved)
{
	MsgT msg;
	DaqError err = DerrNoError;
	int i;

	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		msg.msg = MsgAttach;
		msg.errCode = DerrNoError;
		cmnBroadcastMessage(&msg);
		err = msg.errCode;
		break;

	case DLL_PROCESS_DETACH:
		msg.errCode = DerrNoError;
		msg.msg = MsgClose;
		{
			for (i = 0; i < MaxSessions; i++) {
				if (apiCtrlTestHandle(i, DlmAll) == DerrNoError) {
					msg.handle = i;
					msg.lockMask = DlmAll;
					cmnBroadcastMessage(&msg);
				}
			}
		}
		msg.msg = MsgDetach;
		cmnBroadcastMessage(&msg);
		err = msg.errCode;
		break;

	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		break;

	default:
		break;
	}
	return (err == DerrNoError);
}

VOID
cmnWaitForMutex(MutexType hMutexIndex)
{
	if (hMutex[hMutexIndex] == 0) {
		CHAR mutexName[32] = "IOtech Daq Mutex";
		hMutex[hMutexIndex] = CreateMutex(NULL, FALSE, mutexName);
	}

	WaitForSingleObject(hMutex[hMutexIndex], INFINITE);
}

VOID
cmnReleaseMutex(MutexType hMutexIndex)
{
	ReleaseMutex(hMutex[hMutexIndex]);
}

static DaqHandleT(WINAPI * GetAddressOfOpenVxDHandle(void)) (DaqHandleT) {
	CHAR K32Path[PATH_MAX];
	HINSTANCE hK32;

	GetSystemDirectory(K32Path, PATH_MAX);
	strcat(K32Path, "\\kernel32.dll");
	if ((hK32 = LoadLibrary(K32Path)) == 0) {

		return NULL;
	}

	return (DaqHandleT(WINAPI *) (DaqHandleT)) GetProcAddress(hK32, "OpenVxDHandle");
}

BOOL
cmnCreateCommonEvent(DaqHandleT * pr3Evt, DaqHandleT * pr0Evt, BOOL bManualReset, BOOL bInitialState)
{
	static DaqHandleT(WINAPI * pOpenVxDHandle) (DaqHandleT) = 0;

	if (pOpenVxDHandle == 0) {
		pOpenVxDHandle = GetAddressOfOpenVxDHandle();
	}

	if (pOpenVxDHandle) {
		*pr3Evt = CreateEvent(0, bManualReset, bInitialState, NULL);

		if (*pr3Evt) {
			*pr0Evt = pOpenVxDHandle(*pr3Evt);
			if (*pr0Evt) {
				return TRUE;
			} else {
				CloseHandle(*pr3Evt);

			}
		}
	}

	*pr0Evt = 0;
	*pr3Evt = 0;
	return FALSE;
}

PCHAR
iotReadLineFile(PCHAR lineBuffer, int bufLength, PVOID file)
{
	return fgets(lineBuffer, bufLength, (FILE *) file);
}


int
iotCloseFile(PVOID file)
{
	return fclose((FILE *) file);
}


PVOID
iotOpenFile(PCHAR filename, PCHAR mode)
{
	return (PVOID ) fopen(filename, mode);
}



PVOID
iotMalloc(size_t size)
{
	PVOID ret;
	ret =  malloc(size);
	if (ret)
		return ret;
	fprintf(stderr, "Memory allocation failed! size = %d\n", size);
	return NULL;
}

VOID
iotFarfree(PVOID block)
{
	free(block);
}

/*
 PEK: Great Maker!
 why is this function here?  
 isn't atol() or strtol() sufficient?
 */
long
iotAtol(PCHAR s)
{
	long tmpL = 0;
	int sIndex = 0, signFlag = 1;

	while (isblank(s[sIndex])) {
		sIndex++;
	}

	if ((s[sIndex] == '-') || (s[sIndex] == '+')) {
		if (s[sIndex] == '-') {
			signFlag = -1;
		}
		sIndex++;
	}

	while (isdigit(s[sIndex])) {
		tmpL *= 10;
		tmpL = tmpL + (long) s[sIndex] - 48l;
		sIndex++;
	}

	tmpL *= signFlag;
	return tmpL;
}

BOOL
MapApiToIoctl(int idApi, PINT pidIoctl, ID_LINK_PT Map, int nListLen)
{
	int i;
	for (i = 0; i < nListLen; i++) {
		if (idApi == Map[i].id1) {
			*pidIoctl = Map[i].id2;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL
IsWaveBook(const DaqHardwareVersion devType)
{
	static const int
	 dhvWaveBooks[] = {
		WaveBook512,
		WaveBook512_10V,
		WaveBook516,
		WaveBook516_250,
		WaveBook512A,
		WaveBook516A,
	};

	return IsInList(devType, dhvWaveBooks, NUM(dhvWaveBooks));
}

BOOL
IsWBK516X(const DaqHardwareVersion devType)
{
	static const int
	 dhvWaveBook516s[] = {
		WaveBook516,
		WaveBook516_250,
		WaveBook512A,
		WaveBook516A,
	};

	return IsInList(devType, dhvWaveBook516s, NUM(dhvWaveBook516s));
}

BOOL
IsWBK51_A(const DaqHardwareVersion devType)
{
	static const int
	 dhvWaveBook51_As[] = {
		WaveBook512A,
		WaveBook516A,
	};

	return IsInList(devType, dhvWaveBook51_As, NUM(dhvWaveBook51_As));
}

BOOL
IsDaq1000(const DaqHardwareVersion devType)
{
	static const int
	 dhvDaq1000[] = {
		DaqBoard1000,		
		DaqBoard1005,
	};

	return IsInList(devType, dhvDaq1000, NUM(dhvDaq1000));
}

BOOL
IsDaq2000(const DaqHardwareVersion devType)
{
	static const int
	 dhvDaq2000[] = {
		DaqBoard2000,
		DaqBoard2001,
		DaqBoard2002,
		DaqBoard2003,
		DaqBoard2004,
		DaqBoard2005,
	};

	return IsInList(devType, dhvDaq2000, NUM(dhvDaq2000));
}

BOOL
Is10V(const DaqHardwareVersion devType)
{
	static const int
	 dhv10V[] = {
		DaqBoard2000,
		DaqBoard2001,
		DaqBoard2002,
		DaqBoard2003,
		DaqBoard2004,
		DaqBoard2005,
		Daq112,
		Daq216,
		WaveBook516,
		WaveBook516_250,
		WaveBook512_10V,
		WaveBook512A,
		WaveBook516A,
        DaqBoard1000,
		DaqBoard1005,
	};

	return IsInList(devType, dhv10V, NUM(dhv10V));
}

BOOL
Is16Bit(const DaqHardwareVersion devType)
{
	static const int
	 dhv16Bit[] = {
		DaqBoard2000,
		DaqBoard2001,
		DaqBoard2002,
		DaqBoard2003,
		DaqBoard2004,
		DaqBoard2005,
        DaqBoard1000,
		DaqBoard1005,
		DaqBook200,
		DaqBook216,
		DaqBoard200,
		DaqBoard216,
		Daq216,
		WaveBook516,
		WaveBook516_250,
		WaveBook516A,
	};

	return IsInList(devType, dhv16Bit, NUM(dhv16Bit));
}

BOOL
IsTempBook(const DaqHardwareVersion devType)
{
	static const int
	 dhvTempBook[] = {
		TempBook66
	};

	return IsInList(devType, dhvTempBook, NUM(dhvTempBook));
}

BOOL
IsDaqDevice(const DaqHardwareVersion devType)
{

	return (!((IsTempBook(devType)) || IsWaveBook(devType)));
}

BOOL
IsInList(const int iTgt, const int *pList, int nLen)
{
	int i;
	for (i = 0; i < nLen; i++) {
		if (iTgt == pList[i]) {
			return TRUE;
		}
	}
	return FALSE;
}
