////////////////////////////////////////////////////////////////////////////
//
// cmnapi.h                        I    OOO                           h
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


#ifndef COMMON_H
#define COMMON_H

#define DaqParamchecking

#define  HighByte(word)    (((PBYTE) &word)[1])
#define  LowByte(word)     (((PBYTE) &word)[0])
#define  HighWord(dword)   (((PWORD) &dword)[1])
#define  LowWord(dword)    (((PWORD) &dword)[0])

#define BIT_CLR(by,bits) ((by) &= ~(bits))
#define BIT_SET(by,bits) ((by) |=  (bits))
#define BIT_TST(by,bits) ((BOOL)(0 != ((by) & (bits))))

#define NUM(a) (sizeof(a)/sizeof(a)[0])

#define  MaxBanks    16
#define  GetBank(chan)                    ((BYTE)(((chan) / 16) - 1))
#define  GetSubAddr(chan, chansPerCard)   ((BYTE)(((0x0F & (chan)) / (chansPerCard))))
#define MaxSessions 4

typedef enum {
	MsgAttach,
	MsgDetach,
	MsgInit,
	MsgClose
} MessageType;

typedef struct {
	MessageType msg;
	DaqError errCode;
	DaqHandleT handle;
	LONG lockMask;
	PCHAR daqName;
	DaqHardwareVersion deviceType;
	DaqProtocol protocol;
} MsgT, *MsgPT;

VOID cmnBroadcastMessage(MsgPT msg);

#define  MaxMutexes  3

typedef enum {
	LockTableMutex = 0x00,
	IoctlMutex = 0x01,
	AdcStatMutex = 0x02,
} MutexType;

VOID cmnWaitForMutex(MutexType hMutexIndex);
VOID cmnReleaseMutex(MutexType hMutexIndex);

BOOL cmnCreateCommonEvent(DaqHandleT * pr3Evt, DaqHandleT * pr0Evt, BOOL bManualReset, BOOL bInitialState);

typedef struct {
	int id1;
	int id2;
} ID_LINK_T, *ID_LINK_PT;

BOOL MapApiToIoctl(int idApi, PINT pidIoctl, ID_LINK_PT Map, int nListLen);

BOOL IsWaveBook(const DaqHardwareVersion devType);

BOOL IsWBK516X(const DaqHardwareVersion devType);

BOOL IsWBK51_A(const DaqHardwareVersion devType);

BOOL IsDaq2000(const DaqHardwareVersion devType);

BOOL IsDaq1000(const DaqHardwareVersion devType);

BOOL Is10V(const DaqHardwareVersion devType);

BOOL Is16Bit(const DaqHardwareVersion devType);

BOOL IsTempBook(const DaqHardwareVersion devType);

BOOL IsDaqDevice(const DaqHardwareVersion devType);

BOOL IsInList(const int iTgt, const int *pList, int nLen);

PVOID iotMalloc(size_t size);
VOID iotFarfree(void *block);

PVOID iotOpenFile(PCHAR filename, PCHAR mode);
int iotCloseFile(PVOID file);
PCHAR iotReadLineFile(PCHAR lineBuffer, int bufLength, PVOID file);

long iotAtol(PCHAR s);

typedef struct {
	DaqChannelType DBKType;
	float * GainArray;
} GainInfoT, *GainInfoPT;


#ifdef _DEBUG_WEIRD_WINDOWS_DEBUGGING_MACROS
/* This is just so sad... */
#define DebugWinString(macroStr) OutputDebugString(macroStr);
#define DebugWinString1(macroStr, p1) \
            {                                \
            CHAR thisStr[256];                \
            sprintf( thisStr, macroStr, p1); \
            OutputDebugString( thisStr );    \
            }
#define DebugWinString2(macroStr, p1, p2) \
            {                                   \
            CHAR thisStr[256];                   \
            sprintf( thisStr, macroStr, p1, p2);\
            OutputDebugString( thisStr );       \
            }
#define DebugWinString3(macroStr, p1, p2, p3) \
            {                                         \
            CHAR thisStr[256];                         \
            sprintf( thisStr, macroStr, p1, p2, p3);  \
            OutputDebugString( thisStr );             \
            }
#define DebugWinString4(macroStr, p1, p2, p3, p4) \
            {                                         \
            CHAR thisStr[256];                         \
            sprintf( thisStr, macroStr, p1, p2, p3, p4);  \
            OutputDebugString( thisStr );             \
            }
#define DebugWinString5(macroStr, p1, p2, p3, p4, p5) \
            {                                         \
            CHAR thisStr[256];                         \
            sprintf( thisStr, macroStr, p1, p2, p3, p4, p5);  \
            OutputDebugString( thisStr );             \
            }
#define DebugWinString6(macroStr, p1, p2, p3, p4, p5, p6) \
            {                                         \
            CHAR thisStr[256];                         \
            sprintf( thisStr, macroStr, p1, p2, p3, p4, p5, p6);  \
            OutputDebugString( thisStr );             \
            }
#define DebugString(macroStr)  DebugWinString(macroStr)
#define DebugString1(macroStr, p1)  DebugWinString1(macroStr, p1)
#define DebugString2(macroStr, p1, p2)  DebugWinString2(macroStr, p1, p2)
#define DebugString3(macroStr, p1, p2, p3)  DebugWinString3(macroStr, p1, p2, p3)
#define DebugString4(macroStr, p1, p2, p3, p4)  DebugWinString4(macroStr, p1, p2, p3, p4)
#define DebugString5(macroStr, p1, p2, p3, p4, p5)  DebugWinString5(macroStr, p1, p2, p3, p4, p5)
#define DebugString6(macroStr, p1, p2, p3, p4, p5, p6)  DebugWinString6(macroStr, p1, p2, p3, p4, p5, p6)
#else
#define DebugWinString(macroStr)
#define DebugWinString1(macroStr, p1)
#define DebugWinString2(macroStr, p1, p2)
#define DebugWinString3(macroStr, p1, p2, p3)
#define DebugWinString4(macroStr, p1, p2, p3, p4)
#define DebugWinString5(macroStr, p1, p2, p3, p4, p5)
#define DebugWinString6(macroStr, p1, p2, p3, p4, p5, p6)
#endif

#endif
