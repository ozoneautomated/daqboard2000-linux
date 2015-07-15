////////////////////////////////////////////////////////////////////////////
//
// api.h                           I    OOO                           h
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

#ifndef API_H
#define API_H

#define ChansPerSystem 272

enum ParamTypes {
	ParamTypeUnknown = 0x00,
	ParamTypeBank = 0x01,
	ParamTypeChannel = 0x02,
	ParamTypeGainOffset = 0x03,
	ParamBlankLine = 0x04,
	ParamDbk4 = 0x05
};

typedef enum {
	DlmNone = 0x00000000,
	DlmAll = 0xFFFFFFFF,
} DaqLockMask;

typedef enum {
	DacAvailNone = 0x00,
	DacAvailVoltage = 0x01,
	DacAvailStaticWave = 0x02,
	DacAvailDynamicWave = 0x04,
} DacAvailT;

typedef struct {
	BYTE month;
	BYTE day;
	BYTE year;
} wbkDateT, *wbkDatePT;

typedef struct {
	BYTE hour;
	BYTE minute;
	BYTE second;
} wbkTimeT, *wbkTimePT;

typedef struct {
	DWORD ChannelsPerSys;
	DWORD ChannelsPerCard;
	DWORD MainUnitChans;
	float AdMin;
	float AdMax;
	DWORD BlockSize;
} DeviceHardwareInfoT;

typedef enum {
	DtshtDbk14 = 0,
	DtshtDbk19 = 1,
	DtshtTbk66 = 2,
	DtshtDbk81 = 3,
    DtshtDbk90 = 4,
} DaqTCSetupHwType;

VOID apiCtrlBroadcastMessage(MsgPT msg);
VOID apiCtrlMessageHandler(MsgPT msg);
VOID apiAdcMessageHandler(MsgPT msg);
VOID apiDacMessageHandler(MsgPT msg);
VOID apiDbkMessageHandler(MsgPT msg);
VOID apiDioMessageHandler(MsgPT msg);
VOID apiCtrMessageHandler(MsgPT msg);
VOID apiCvtMessageHandler(MsgPT msg);
VOID apiCalMessageHandler(MsgPT msg);

DaqError apiCtrlProcessError(DaqHandleT handle, DaqError errCode);
DaqError apiCtrlTestHandle(DaqHandleT handle, LONG lockMask);
VOID apiCtrlSetTransferEvent(VOID);
DaqHandleT apiCtrlGetRing0TransferEventHandle(VOID);

DaqError apiAdcTransferGetFlag(DaqHandleT handle, DaqTransferEvent event, PBOOL eventSet);
DaqError apiAdcSendSmartDbkData(DaqHandleT handle, DWORD address,
				DWORD chanCount, PBYTE data, DWORD dataCount);
DaqError apiAdcReadSample(DaqHandleT handle, DWORD chan, DaqAdcGain gain, 
			  DWORD flags, DWORD avgCount, PWORD data);

DaqError adcTestConfig(DaqHandleT handle);

BYTE apiDbk4GetMaxFreq(DaqHandleT handle, DWORD channel);
DaqError apiDbkSendChanOptions(DaqHandleT handle, PDWORD channels,
			       DaqAdcGain * gains, DWORD chanCount);
DaqError apiDbkDacWt(DaqHandleT handle, DWORD chan, PWORD values, DWORD count);

VOID apiDacSetAvail(DaqHandleT handle, DaqDacDeviceType deviceType, DWORD chan, DWORD avail);

DaqError daqAdcGetClockSource(DaqHandleT handle, DaqAdcClockSource * clocksource);

#endif
