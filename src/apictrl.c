////////////////////////////////////////////////////////////////////////////
//
// apictrl.c                       I    OOO                           h
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


#include <stdio.h>
#include <stdlib.h>
#include "iottypes.h"

#include "version.h"
#include "daqx.h"
#include "cmnapi.h"
#include "api.h"
#include "ddapi.h"
#include "itfapi.h"

#define MaxLocks (MaxSessions * 2)
#define MaxAliasLen 64
#define MaxDevices 4
#define MaxDeviceDrivers 6
#define DaqNone -1
#define MaxErrCodes 129

typedef struct {
	DaqProtocol gnProtocol;
	DaqHardwareVersion gnDeviceType;
	DWORD mSecTimeout;
	DaqError errCode;
	DaqErrorHandlerFPT errorHandler;
} ApiCtrlT, *ApiCtrlPT;

typedef struct {
	DWORD processId;
	CHAR daqName[64];
	LONG lockMask;
	DaqHandleT handle;
} ApiCtrlLockT;

typedef struct {
	DaqHardwareVersion deviceType;
	DWORD basePortAddress;
	DWORD dmaChannel;
	DWORD socket;
	DWORD interruptLevel;
	DaqProtocol protocol;
	CHAR daqName[64];
} ApiCtrlDeviceRegInfoT;

static ApiCtrlT session[MaxSessions];
static DaqError daqErrCode;
static DaqErrorHandlerFPT daqErrorHandler = daqDefaultErrorHandler;
static DaqHandleT transferEventR3;

static DaqHandleT transferEventR0;

static VOID freeHandle(DaqHandleT handle, LONG lockMask);

extern DWORD waitForStatusChgEvent(PVOID handle, DWORD timeout, DaqHandleT transEventR3);

//#pragma  data_seg(".apiCtrlSharedDS")
static DWORD lockCount = 0;
static ApiCtrlLockT lockTable[MaxLocks] = { {0, "", DlmNone, 0}, };
//#pragma  data_seg()

//MAH: 04.22.04 HACK
	static int BeenHereDoneThat =0;

#ifdef DEBUG
static VOID
printLockTable(VOID)
{
	DWORD i;

	printf("| i | processId |       daqName       | lockMask | handle |\n");
	printf("-----------------------------------------------------------\n");

	for (i = 0; i < lockCount; i++) {
		printf("| %1u | %9x | %19s | %8lx | %6d |\n", i,
		       lockTable[i].processId, lockTable[i].daqName,
		       lockTable[i].lockMask, lockTable[i].handle);
	}

	return ;
}
#endif

VOID
apiCtrlBroadcastMessage(MsgPT msg)
{
	apiDbkMessageHandler(msg);
	apiCvtMessageHandler(msg);
	apiCalMessageHandler(msg);
	apiAdcMessageHandler(msg);
	apiDioMessageHandler(msg);

	if (!IsWaveBook(msg->deviceType)) {
		apiDacMessageHandler(msg);
		apiCtrMessageHandler(msg);
	}

	apiCtrlMessageHandler(msg);
	return ;
}

VOID
apiCtrlMessageHandler(MsgPT msg)
{
	if (msg->errCode != DerrNoError) {
		return;
	}

	switch (msg->msg) {
	case MsgAttach:
#if 0
		if (OS.IsW9X()) {
			if (!cmnCreateCommonEvent (&transferEventR3, &transferEventR0, FALSE, FALSE)) {
				msg->errCode = DerrBadAddress;
			}
		}
#endif

		break;

	case MsgDetach:
#if 0
		if (OS.IsW9X()) {
			CloseHandle(transferEventR3);
		}
#endif
		break;

	case MsgInit:
        
		session[msg->handle].errCode = DerrNoError;
		daqSetErrorHandler(msg->handle, daqErrorHandler);
		daqSetTimeout(msg->handle, 10000);
		session[msg->handle].gnDeviceType = msg->deviceType;
		session[msg->handle].gnProtocol = msg->protocol;
		break;

	case MsgClose:
		freeHandle(msg->handle, msg->lockMask);
		break;
	}
	return;

}

DaqError
apiCtrlProcessError(DaqHandleT handle, DaqError errCode)
{

	if (apiCtrlTestHandle(handle, DlmNone) != DerrNoError) {
		daqErrCode = errCode;
		if (daqErrorHandler)
			daqErrorHandler(handle, errCode);
	} else {
		session[handle].errCode = errCode;
		if (session[handle].errorHandler)
			session[handle].errorHandler(handle, errCode);
	}

	if (errCode)
		dbg("returning error %d\n", errCode);

	return errCode;
}

DaqError
apiCtrlTestHandle(DaqHandleT handle, LONG lockMask)
{
	DWORD i;
	DWORD processId;
	BOOL valid;

	cmnWaitForMutex(LockTableMutex);

	processId = GetCurrentProcessId();
	valid = FALSE;

	/*
	 * PEK:weird: we return the `valid' value TRUE if
	 * any one locktable passes the test.  Why do we loop over
	 * all i values?  hell, it can be true for the first entry,
	 * and false for the rest... why test them??  what is this
	 * test really supposed to do?
	 */

	for (i = 0; i < lockCount; i++) {
		if ((processId == lockTable[i].processId) &&
		    (handle == lockTable[i].handle) &&
		    ((lockMask & lockTable[i].lockMask) == lockMask)) {
			valid = TRUE;
		}
	}

	cmnReleaseMutex(LockTableMutex);

	if(!valid){
		dbg("invalid handle found %d", (int) handle);
#ifdef DEBUG
	    printLockTable();
#endif
	}

	return (valid ? DerrNoError : DerrInvHandle);
}

static DaqHandleT
getHandleFromName(PCHAR daqName)
{
	DWORD i;
	DaqHandleT handle = NO_GRIP;

	for (i = 0; i < lockCount; i++) {
		if (!stricmp(daqName, lockTable[i].daqName)) {
			if (GetCurrentProcessId() == lockTable[i].processId) {
				handle = lockTable[i].handle;
			}
		}
	}
	return handle;
}

static DaqError
openHandle(DaqHandleT * handle, PCHAR daqName, LONG lockMask)
{
	DWORD i;
	DWORD processId;
	DaqError status = DerrNoError;

	cmnWaitForMutex(LockTableMutex);
	processId = GetCurrentProcessId();

	for (i = 0; i < lockCount; i++) {
		if (!strcmp(daqName, lockTable[i].daqName)) {
			if (processId == lockTable[i].processId) {
				*handle = lockTable[i].handle;
				lockTable[i].lockMask |= lockMask;
				status = DerrNoError;
				goto unprotect;
			} 

			if (lockMask & lockTable[i].lockMask) {
				*handle = NO_GRIP;
				status = DerrAlreadyLocked;
				dbg("error: DerrAlreadyLocked!");
				goto unprotect;
			}
		}
	}

	dbg("No locks on device %s",daqName);

	if (lockCount == MaxLocks) {
		*handle = NO_GRIP;
		status = DerrTooManyHandles;
		dbg("DerrTooManyHandles!");
		goto unprotect;
	}

	for (*handle = 0; *handle < MaxSessions; (*handle)++) {
		for (i = 0; i < lockCount; i++) {
			if ((processId == lockTable[i].processId)
			    && (*handle == lockTable[i].handle)) {
				break;
			}
		}
		if (i == lockCount) {
			break;
		}
	}

	if (*handle == MaxSessions) {
		*handle = NO_GRIP;
		status = DerrTooManyHandles;
		dbg("Too many handles!");
		goto unprotect;
	}

	lockTable[lockCount].lockMask = lockMask;
	lockTable[lockCount].processId = processId;
	strcpy(lockTable[lockCount].daqName, daqName);
	lockTable[lockCount].handle = *handle;
	lockCount++;

unprotect:
	cmnReleaseMutex(LockTableMutex);

#ifdef DEBUG
	printLockTable();
#endif
	return status;
}


static VOID
freeHandle(DaqHandleT handle, LONG lockMask)
{
	DWORD i, iInc;
	DWORD processId;

	cmnWaitForMutex(LockTableMutex);

	processId = GetCurrentProcessId();
	for (i = 0; i < lockCount; i += iInc) {

		iInc = 1;

		if ((processId == lockTable[i].processId) && 
		    (handle == lockTable[i].handle)) {

			lockTable[i].lockMask &= ~lockMask;

			if (lockTable[i].lockMask == 0) {
				lockCount--;
				if (lockCount) {
					lockTable[i].lockMask = lockTable[lockCount].lockMask;
					lockTable[i].processId = lockTable[lockCount].processId;
					strcpy(lockTable[i].daqName, lockTable[lockCount].daqName);
					lockTable[i].handle = lockTable[lockCount].handle;
					iInc = 0;
				}
			}
		}
	}

	cmnReleaseMutex(LockTableMutex);
	return ;
}

VOID
apiCtrlSetTransferEvent(VOID)
{
	SetEvent(transferEventR3);
	return ;
}

DaqHandleT
apiCtrlGetRing0TransferEventHandle(VOID)
{
	return transferEventR0;
}

DAQAPI DaqError WINAPI
daqRegWrite(DaqHandleT handle, DWORD reg, DWORD value)
{

	DaqError err;
	daqIOT sb;
	REG_WRITE_PT regWrite = (REG_WRITE_PT) & sb;
	DWORD regType = 0;

	err = apiCtrlTestHandle(handle, DlmNone);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	regType = (reg & 0xFFFF0000) >> 16;
	if ((regType < RegTypeDeviceRegs) || (regType > RegTypeDspPM))
		return apiCtrlProcessError(handle, DerrNotCapable);

	regWrite->regType = regType;
	regWrite->reg = reg & 0x0000FFFF;
	regWrite->value = value;

	err = itfExecuteCommand(handle, &sb, REG_WRITE);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqRegRead(DaqHandleT handle, DWORD reg, PDWORD value)
{

	DaqError err;
	daqIOT sb;
	REG_READ_PT regRead = (REG_READ_PT) & sb;
	DWORD regType = 0;

	err = apiCtrlTestHandle(handle, DlmNone);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	regType = (reg & 0xFFFF0000) >> 16;

	if ((regType < RegTypeDeviceRegs) || (regType > RegTypeDspPM))
		return apiCtrlProcessError(handle, DerrNotCapable);

	regRead->regType = regType;

	regRead->reg = reg & 0x0000FFFF;

	err = itfExecuteCommand(handle, &sb, REG_READ);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	*value = regRead->value;

	return DerrNoError;

}

INT
daq_selftest(PCHAR device_name, PCHAR error_msg)
{
	BOOL available;
	DWORD result;
	DaqHandleT handle;

	daqSetDefaultErrorHandler((DaqErrorHandlerFPT) 0);

	handle = daqOpen(device_name);

	if (handle == NO_GRIP) {
		sprintf(error_msg, "Error - Device not on-line\n");
		return 0;
	}

	daqTest(handle, DtstBaseAddressValid, 10, &available, &result);

	if (available && result) {
		sprintf(error_msg, "Error - Base Address Test Failed\n");
		return 0;
	}

	daqTest(handle, DtstInterruptLevelValid, 1, &available, &result);
	if (available && result) {
		sprintf(error_msg, "Error - Interrupt Level Test Failed\n");
		return 0;
	}

	daqTest(handle, DtstDmaChannelValid, 1, &available, &result);

	if (available && result ) {
		sprintf(error_msg, "Error - DMA Channel Test Failed\n");
		return 0;
	}

	sprintf(error_msg, "SelfTest - Passed\n");
	return 1;

}

DAQAPI DaqError WINAPI
daqTest(DaqHandleT handle, DaqTestCommand command, DWORD count, PBOOL cmdAvailable, PDWORD result)
{

	DaqError err;
	daqIOT sb;
	DAQ_TEST_PT daqTest = (DAQ_TEST_PT) & sb;

	err = apiCtrlTestHandle(handle, DlmNone);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	if ((command < Dtst_ParamRangeMin) || (Dtst_ParamRangeMax < command))
		return apiCtrlProcessError(handle, DerrInvCommand);

	daqTest->command = command;
	daqTest->count = count;
	err = itfExecuteCommand(handle, &sb, DAQ_TEST);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	*cmdAvailable = daqTest->available;
	*result = daqTest->result;
	return DerrNoError;

}

void
apiCtrlSetDeviceProperties(DaqDevicePropsT * devProps, DWORD deviceType)
{

	if (devProps == NULL) {
		return;
	}

	switch (deviceType) {

	case DaqBook100:
		devProps->maxAdChannels = 272;
		devProps->maxDaChannels = 258;
		devProps->maxDigInputBits = 192;
		devProps->maxDigOutputBits = 192;
		devProps->maxCtrChannels = 5;
		devProps->mainUnitAdChannels = 16;
		devProps->mainUnitDaChannels = 2;
		devProps->mainUnitDigInputBits = 24;
		devProps->mainUnitDigOutputBits = 24;
		devProps->mainUnitCtrChannels = 5;
		devProps->adFifoSize = 4096;
		devProps->daFifoSize = 0;
		devProps->adResolution = 12;
		devProps->daResolution = 12;
		devProps->adMinFreq = 0.00002315F;
		devProps->adMaxFreq = 100000.0F;
		devProps->daMinFreq = 0.0F;
		devProps->daMaxFreq = 0.0F;
		break;

	case DaqBook112:
		devProps->maxAdChannels = 272;
		devProps->maxDaChannels = 258;
		devProps->maxDigInputBits = 192;
		devProps->maxDigOutputBits = 192;
		devProps->maxCtrChannels = 5;
		devProps->mainUnitAdChannels = 16;
		devProps->mainUnitDaChannels = 2;
		devProps->mainUnitDigInputBits = 24;
		devProps->mainUnitDigOutputBits = 24;
		devProps->mainUnitCtrChannels = 5;
		devProps->adFifoSize = 4096;
		devProps->daFifoSize = 0;
		devProps->adResolution = 12;
		devProps->daResolution = 12;
		devProps->adMinFreq = 0.00002315F;
		devProps->adMaxFreq = 100000.0F;
		devProps->daMinFreq = 0.0F;
		devProps->daMaxFreq = 0.0F;
		break;

	case DaqBook120:
		devProps->maxAdChannels = 272;
		devProps->maxDaChannels = 258;
		devProps->maxDigInputBits = 192;
		devProps->maxDigOutputBits = 192;
		devProps->maxCtrChannels = 5;
		devProps->mainUnitAdChannels = 16;
		devProps->mainUnitDaChannels = 2;
		devProps->mainUnitDigInputBits = 24;
		devProps->mainUnitDigOutputBits = 24;
		devProps->mainUnitCtrChannels = 5;
		devProps->adFifoSize = 4096;
		devProps->daFifoSize = 0;
		devProps->adResolution = 12;
		devProps->daResolution = 12;
		devProps->adMinFreq = 0.00002315F;
		devProps->adMaxFreq = 100000.0F;
		devProps->daMinFreq = 0.0F;
		devProps->daMaxFreq = 0.0F;
		break;

	case DaqBook200:
		devProps->maxAdChannels = 272;
		devProps->maxDaChannels = 258;
		devProps->maxDigInputBits = 192;
		devProps->maxDigOutputBits = 192;
		devProps->maxCtrChannels = 5;
		devProps->mainUnitAdChannels = 16;
		devProps->mainUnitDaChannels = 2;
		devProps->mainUnitDigInputBits = 24;
		devProps->mainUnitDigOutputBits = 24;
		devProps->mainUnitCtrChannels = 5;
		devProps->adFifoSize = 4096;
		devProps->daFifoSize = 0;
		devProps->adResolution = 16;
		devProps->daResolution = 16;
		devProps->adMinFreq = 0.00002315F;
		devProps->adMaxFreq = 100000.0F;
		devProps->daMinFreq = 0.0F;
		devProps->daMaxFreq = 0.0F;
		break;

	case DaqBook216:
		devProps->maxAdChannels = 272;
		devProps->maxDaChannels = 258;
		devProps->maxDigInputBits = 192;
		devProps->maxDigOutputBits = 192;
		devProps->maxCtrChannels = 5;
		devProps->mainUnitAdChannels = 16;
		devProps->mainUnitDaChannels = 2;
		devProps->mainUnitDigInputBits = 24;
		devProps->mainUnitDigOutputBits = 24;
		devProps->mainUnitCtrChannels = 5;
		devProps->adFifoSize = 4096;
		devProps->daFifoSize = 0;
		devProps->adResolution = 16;
		devProps->daResolution = 16;
		devProps->adMinFreq = 0.00002315F;
		devProps->adMaxFreq = 100000.0F;
		devProps->daMinFreq = 0.0F;
		devProps->daMaxFreq = 0.0F;
		break;

	case DaqBoard100:
		devProps->maxAdChannels = 272;
		devProps->maxDaChannels = 258;
		devProps->maxDigInputBits = 192;
		devProps->maxDigOutputBits = 192;
		devProps->maxCtrChannels = 5;
		devProps->mainUnitAdChannels = 16;
		devProps->mainUnitDaChannels = 2;
		devProps->mainUnitDigInputBits = 24;
		devProps->mainUnitDigOutputBits = 24;
		devProps->mainUnitCtrChannels = 5;
		devProps->adFifoSize = 4096;
		devProps->daFifoSize = 4096;
		devProps->adResolution = 12;
		devProps->daResolution = 12;
		devProps->adMinFreq = 0.00002315F;
		devProps->adMaxFreq = 100000.0F;
		devProps->daMinFreq = 0.0F;
		devProps->daMaxFreq = 1000000.0F;
		break;

	case DaqBoard112:
		devProps->maxAdChannels = 272;
		devProps->maxDaChannels = 258;
		devProps->maxDigInputBits = 192;
		devProps->maxDigOutputBits = 192;
		devProps->maxCtrChannels = 5;
		devProps->mainUnitAdChannels = 16;
		devProps->mainUnitDaChannels = 2;
		devProps->mainUnitDigInputBits = 24;
		devProps->mainUnitDigOutputBits = 24;
		devProps->mainUnitCtrChannels = 5;
		devProps->adFifoSize = 4096;
		devProps->daFifoSize = 4096;
		devProps->adResolution = 12;
		devProps->daResolution = 12;
		devProps->adMinFreq = 0.00002315F;
		devProps->adMaxFreq = 100000.0F;
		devProps->daMinFreq = 0.0F;
		devProps->daMaxFreq = 1000000.0F;
		break;

	case DaqBoard200:
		devProps->maxAdChannels = 272;
		devProps->maxDaChannels = 258;
		devProps->maxDigInputBits = 192;
		devProps->maxDigOutputBits = 192;
		devProps->maxCtrChannels = 5;
		devProps->mainUnitAdChannels = 16;
		devProps->mainUnitDaChannels = 2;
		devProps->mainUnitDigInputBits = 24;
		devProps->mainUnitDigOutputBits = 24;
		devProps->mainUnitCtrChannels = 5;
		devProps->adFifoSize = 4096;
		devProps->daFifoSize = 4096;
		devProps->adResolution = 16;
		devProps->daResolution = 16;
		devProps->adMinFreq = 0.00002315F;
		devProps->adMaxFreq = 100000.0F;
		devProps->daMinFreq = 0.0F;
		devProps->daMaxFreq = 1000000.0F;
		break;

	case DaqBoard216:
		devProps->maxAdChannels = 272;
		devProps->maxDaChannels = 258;
		devProps->maxDigInputBits = 192;
		devProps->maxDigOutputBits = 192;
		devProps->maxCtrChannels = 5;
		devProps->mainUnitAdChannels = 16;
		devProps->mainUnitDaChannels = 2;
		devProps->mainUnitDigInputBits = 24;
		devProps->mainUnitDigOutputBits = 24;
		devProps->mainUnitCtrChannels = 5;
		devProps->adFifoSize = 4096;
		devProps->daFifoSize = 4096;
		devProps->adResolution = 16;
		devProps->daResolution = 16;
		devProps->adMinFreq = 0.00002315F;
		devProps->adMaxFreq = 100000.0F;
		devProps->daMinFreq = 0.0F;
		devProps->daMaxFreq = 1000000.0F;
		break;

	case DaqBoard2000:
		devProps->maxAdChannels = 272;
		devProps->maxDaChannels = 258;
		devProps->maxDigInputBits = 208;
		devProps->maxDigOutputBits = 208;
		devProps->maxCtrChannels = 4;
		devProps->mainUnitAdChannels = 16;
		devProps->mainUnitDaChannels = 2;
		devProps->mainUnitDigInputBits = 40;
		devProps->mainUnitDigOutputBits = 40;
		devProps->mainUnitCtrChannels = 4;
		devProps->adFifoSize = 131072;
		devProps->daFifoSize = 131072;
		devProps->adResolution = 16;
		devProps->daResolution = 16;
		devProps->adMinFreq = 0.00002315F;
		devProps->adMaxFreq = 200000.0F;
		devProps->daMinFreq = 1.526F;
		devProps->daMaxFreq = 100000.0F;
		break;

	case DaqBoard2001:
		devProps->maxAdChannels = 272;
		devProps->maxDaChannels = 260;
		devProps->maxDigInputBits = 208;
		devProps->maxDigOutputBits = 208;
		devProps->maxCtrChannels = 4;
		devProps->mainUnitAdChannels = 16;
		devProps->mainUnitDaChannels = 4;
		devProps->mainUnitDigInputBits = 40;
		devProps->mainUnitDigOutputBits = 40;
		devProps->mainUnitCtrChannels = 4;
		devProps->adFifoSize = 131072;
		devProps->daFifoSize = 131072;
		devProps->adResolution = 16;
		devProps->daResolution = 16;
		devProps->adMinFreq = 0.00002315F;
		devProps->adMaxFreq = 200000.0F;
		devProps->daMinFreq = 1.526F;
		devProps->daMaxFreq = 100000.0F;
		break;

	case DaqBoard2002:
		devProps->maxAdChannels = 0;
		devProps->maxDaChannels = 0;
		devProps->maxDigInputBits = 208;
		devProps->maxDigOutputBits = 208;
		devProps->maxCtrChannels = 4;
		devProps->mainUnitAdChannels = 0;
		devProps->mainUnitDaChannels = 0;
		devProps->mainUnitDigInputBits = 40;
		devProps->mainUnitDigOutputBits = 40;
		devProps->mainUnitCtrChannels = 4;
		devProps->adFifoSize = 131072;
		devProps->daFifoSize = 131072;
		devProps->adResolution = 0;
		devProps->daResolution = 0;
		devProps->adMinFreq = 0.00002315F;
		devProps->adMaxFreq = 200000.0F;
		devProps->daMinFreq = 1.526F;
		devProps->daMaxFreq = 100000.0F;
		break;

	case DaqBoard2003:
		devProps->maxAdChannels = 0;
		devProps->maxDaChannels = 4;
		devProps->maxDigInputBits = 0;
		devProps->maxDigOutputBits = 0;
		devProps->maxCtrChannels = 0;
		devProps->mainUnitAdChannels = 0;
		devProps->mainUnitDaChannels = 4;
		devProps->mainUnitDigInputBits = 0;
		devProps->mainUnitDigOutputBits = 0;
		devProps->mainUnitCtrChannels = 0;
		devProps->adFifoSize = 0;
		devProps->daFifoSize = 131072;
		devProps->adResolution = 0;
		devProps->daResolution = 16;
		devProps->adMinFreq = 0.0F;
		devProps->adMaxFreq = 0.0F;
		devProps->daMinFreq = 1.526F;
		devProps->daMaxFreq = 100000.0F;
		break;

	case DaqBoard2004:
		devProps->maxAdChannels = 0;
		devProps->maxDaChannels = 260;
		devProps->maxDigInputBits = 208;
		devProps->maxDigOutputBits = 208;
		devProps->maxCtrChannels = 4;
		devProps->mainUnitAdChannels = 0;
		devProps->mainUnitDaChannels = 4;
		devProps->mainUnitDigInputBits = 40;
		devProps->mainUnitDigOutputBits = 40;
		devProps->mainUnitCtrChannels = 4;
		devProps->adFifoSize = 131072;
		devProps->daFifoSize = 131072;
		devProps->adResolution = 0;
		devProps->daResolution = 16;
		devProps->adMinFreq = 0.00002315F;
		devProps->adMaxFreq = 200000.0F;
		devProps->daMinFreq = 1.526F;
		devProps->daMaxFreq = 100000.0F;
		break;

	case DaqBoard2005:
		devProps->maxAdChannels = 272;
		devProps->maxDaChannels = 256;
		devProps->maxDigInputBits = 208;
		devProps->maxDigOutputBits = 208;
		devProps->maxCtrChannels = 4;
		devProps->mainUnitAdChannels = 16;
		devProps->mainUnitDaChannels = 0;
		devProps->mainUnitDigInputBits = 40;
		devProps->mainUnitDigOutputBits = 40;
		devProps->mainUnitCtrChannels = 4;
		devProps->adFifoSize = 131072;
		devProps->daFifoSize = 131072;
		devProps->adResolution = 16;
		devProps->daResolution = 0;
		devProps->adMinFreq = 0.00002315F;
		devProps->adMaxFreq = 200000.0F;
		devProps->daMinFreq = 1.526F;
		devProps->daMaxFreq = 100000.0F;
		break;

	case Daq112:
		devProps->maxAdChannels = 272;
		devProps->maxDaChannels = 256;
		devProps->maxDigInputBits = 4;
		devProps->maxDigOutputBits = 4;
		devProps->maxCtrChannels = 0;
		devProps->mainUnitAdChannels = 16;
		devProps->mainUnitDaChannels = 0;
		devProps->mainUnitDigInputBits = 4;
		devProps->mainUnitDigOutputBits = 4;
		devProps->mainUnitCtrChannels = 0;
		devProps->adFifoSize = 4096;
		devProps->daFifoSize = 0;
		devProps->adResolution = 12;
		devProps->daResolution = 0;
		devProps->adMinFreq = 0.00002315F;
		devProps->adMaxFreq = 100000.0F;
		devProps->daMinFreq = 0.0F;
		devProps->daMaxFreq = 0.0F;
		break;

	case Daq216:
		devProps->maxAdChannels = 272;
		devProps->maxDaChannels = 256;
		devProps->maxDigInputBits = 4;
		devProps->maxDigOutputBits = 4;
		devProps->maxCtrChannels = 0;
		devProps->mainUnitAdChannels = 16;
		devProps->mainUnitDaChannels = 0;
		devProps->mainUnitDigInputBits = 4;
		devProps->mainUnitDigOutputBits = 4;
		devProps->mainUnitCtrChannels = 0;
		devProps->adFifoSize = 4096;
		devProps->daFifoSize = 0;
		devProps->adResolution = 16;
		devProps->daResolution = 0;
		devProps->adMinFreq = 0.00002315F;
		devProps->adMaxFreq = 100000.0F;
		devProps->daMinFreq = 0.0F;
		devProps->daMaxFreq = 0.0F;
		break;

	case WaveBook512:
	case WaveBook512_10V:
	case WaveBook516:
	case WaveBook516_250:
	case WaveBook512A:
	case WaveBook516A:
		devProps->maxAdChannels = 72;
		devProps->maxDaChannels = 0;
		devProps->maxDigInputBits = 256;
		devProps->maxDigOutputBits = 256;
		devProps->maxCtrChannels = 1;
		devProps->mainUnitAdChannels = 8;
		devProps->mainUnitDaChannels = 0;
		devProps->mainUnitDigInputBits = 16;
		devProps->mainUnitDigOutputBits = 16;
		devProps->mainUnitCtrChannels = 1;
		devProps->daFifoSize = 0;
		devProps->daResolution = 0;
		devProps->adMinFreq = 0.01F;
		devProps->adMaxFreq = 1000000.0F;
		devProps->daMinFreq = 0.0F;
		devProps->daMaxFreq = 0.0F;
		devProps->adResolution = 16;

		if (!Is16Bit((DaqHardwareVersion) deviceType))
			devProps->adResolution = 12;

		if (!IsWBK516X((DaqHardwareVersion) deviceType)) {
			devProps->maxCtrChannels = 0;
			devProps->mainUnitDigInputBits = 8;
			devProps->mainUnitDigOutputBits = 8;
			devProps->mainUnitCtrChannels = 0;
		}
		devProps->adFifoSize = 65536;
		break;

	case TempBook66:
		devProps->maxAdChannels = 16;
		devProps->maxDaChannels = 0;
		devProps->maxDigInputBits = 8;
		devProps->maxDigOutputBits = 8;
		devProps->maxCtrChannels = 1;
		devProps->mainUnitAdChannels = 16;
		devProps->mainUnitDaChannels = 0;
		devProps->mainUnitDigInputBits = 8;
		devProps->mainUnitDigOutputBits = 8;
		devProps->mainUnitCtrChannels = 1;
		devProps->adFifoSize = 4096;
		devProps->daFifoSize = 0;
		devProps->adResolution = 12;
		devProps->daResolution = 12;
		devProps->adMinFreq = 0.00002778F;
		devProps->adMaxFreq = 100000.0F;
		devProps->daMinFreq = 0.0F;
		devProps->daMaxFreq = 0.0F;
		break;
	}
	return ;
}



void
apiCtrlGetRegDeviceInfo(DWORD * deviceCount, ApiCtrlDeviceRegInfoT * info)
{
	HKEY key;
	LONG result;
	DWORD keySize, value, maxAliasLen;
	WORD thisDevice;
//	BOOL flag = TRUE;
	CHAR searchPath[80];
	CHAR ddName[20];
	CHAR aliasName[MaxAliasLen];
	ApiCtrlDeviceRegInfoT *inf;
	PCHAR driverType[MaxDeviceDrivers] =
		{ "Daqbk", "WaveBk", "TempBk", 
		  "DaqBrd", "DaqPCC", "DaqBrd2K" };
	DWORD driverTypeCnt;

	/*
	 * make a private copy of info, leave the passed one alone 
	 * in case we are called with an (& val) pointer...
	 */
	inf = info;
	*deviceCount = 0;

	for (driverTypeCnt = 0; driverTypeCnt < MaxDeviceDrivers; driverTypeCnt++) {
		for (thisDevice = 0; thisDevice < MaxDevices; thisDevice++) {
			/* What the #$%& is this for??? */
            // @*&! from the windows world - to be obliterated later MAH
			strcpy(searchPath, "SYSTEM\\CurrentControlSet\\Services\\Daqx\\");
			sprintf(ddName, "%s%1d", driverType[driverTypeCnt], thisDevice);
			strcat(searchPath, ddName);

			result = RegOpenKey(HKEY_LOCAL_MACHINE, searchPath, &key);
			maxAliasLen = MaxAliasLen;
			/*
			 * Why is this? We can have devicecount incremented 
			 * for a device that has no info structure? why?
			 */
			if (result == ERROR_SUCCESS)
				(*deviceCount)++;
			/* 
			 * This section had up to 12 levels of `if' indentation 
			 * What a skanky mess! 
			 * By changing to negative tests, with a cleanup goto,
			 * we eliminate the insane levels of indentation...
			 */

			if ((result != ERROR_SUCCESS) && !inf) {
				/*
				 * RegOpenKey failed, try next possible device.
				 */
				continue;
			}

			result = RegQueryValueEx(key, "AliasName", NULL,
						 NULL, (PBYTE) aliasName, &maxAliasLen);
			if (result != ERROR_SUCCESS) {
				/*
				 * RegOpenKey succeeded, but access failed, 
				 * close this key, then try next possible device.
				 */
				goto thisDeviceDead;
			}
			memcpy(inf->daqName, aliasName, NAMESIZE);
			keySize = sizeof (DWORD);

			result = RegQueryValueEx(key,"BasePortAddress",
						 NULL, NULL, (PBYTE) & value, &keySize);
			if (result != ERROR_SUCCESS) {
				goto thisDeviceDead;
			}
			inf->basePortAddress = value;

			result= RegQueryValueEx(key, "InterruptLevel",
						NULL, NULL, (PBYTE) & value, &keySize);
			if (result != ERROR_SUCCESS) {
				goto thisDeviceDead;
			}
			inf->interruptLevel = value;

			result = RegQueryValueEx(key, "DeviceType",
						  NULL, NULL, (PBYTE) & value, &keySize);
			if (result != ERROR_SUCCESS) {
				goto thisDeviceDead;
			}
			inf->deviceType = (DaqHardwareVersion) value;

			switch (inf->deviceType) {
			case (Daq112):
			case (Daq216):
				inf->dmaChannel = 0;
				inf->protocol = DaqProtocolPcCard;
				result = RegQueryValueEx(key, "Socket",
							 NULL, NULL, (PBYTE) & value, &keySize);
				if (result == ERROR_SUCCESS)
					inf->socket = value + 1;
				break;
			default:
				inf->socket = 0;
				result = RegQueryValueEx(key, "DMAChannel",
							 NULL, NULL, (PBYTE) & value, &keySize);
				if (result == ERROR_SUCCESS) {
					inf->dmaChannel = value;
					result = RegQueryValueEx (key, "Protocol",
								  NULL, NULL, (PBYTE) & value, &keySize);
					if (result == ERROR_SUCCESS)
						inf->protocol=(DaqProtocol)value;
				}
				break;
			} /* switch */

			inf++;

		thisDeviceDead:
			RegCloseKey(key);
		}         /* thisDevice */
	}                 /* DriverTypeCnt */

	return ;
}

DAQAPI DaqError WINAPI
daqGetDeviceCount(DWORD * deviceCount)
{
	apiCtrlGetRegDeviceInfo(deviceCount, NULL);
	return DerrNoError;
}


DAQAPI DaqError WINAPI
daqGetDeviceList(DaqDeviceListT * deviceList, DWORD * deviceCount)
{

	DWORD i;
	DaqDeviceListT *deviceListSave;
	ApiCtrlDeviceRegInfoT info[MaxDevices * MaxDeviceDrivers];

	deviceListSave = deviceList;

	apiCtrlGetRegDeviceInfo(deviceCount, info);

	for (i = 0; i < (*deviceCount); i++) {
		memcpy(deviceList->daqName, info[i].daqName, 64);
		deviceList++;
	}

	deviceList = deviceListSave;

	return DerrNoError;

}


DAQAPI DaqError WINAPI
daqGetDeviceProperties(PCHAR daqName, DaqDevicePropsT * deviceProps)
{
	DWORD i, deviceCount;
	ApiCtrlDeviceRegInfoT info[MaxDevices * MaxDeviceDrivers];
	DaqHandleT tHandle = NO_GRIP;
	apiCtrlGetRegDeviceInfo(&deviceCount, info);

	for (i = 0; i < deviceCount; i++) {
		if (!stricmp(info[i].daqName, daqName)) {
			dbg("devtype of %s =%d\n", 
			    info[i].daqName,info[i].deviceType);

			deviceProps->deviceType = info[i].deviceType;
			deviceProps->basePortAddress = info[i].basePortAddress;
			deviceProps->dmaChannel = info[i].dmaChannel;
			deviceProps->socket = info[i].socket;
			deviceProps->interruptLevel = info[i].interruptLevel;
			deviceProps->protocol = info[i].protocol;
			memcpy(deviceProps->daqName, info[i].daqName, 64);

			tHandle = getHandleFromName(daqName);
			break;
		}
	}

	if (tHandle == NO_GRIP) {
		/*PEK for want of better error handling at the moment */
		abort();
	}

	deviceProps->deviceType = session[tHandle].gnDeviceType;
	apiCtrlSetDeviceProperties(deviceProps, deviceProps->deviceType);
	/*
	 * PEK: This is stupid! 
	 * deviceProps->daqName is at present an array [64] of CHAR.
	 * This brain damaged piece of code wanders to the end of it
	 * and, hoping that a DaqHandleT is 4 bytes long, copies the 
	 * contents of the handle over the end of the string.
	 */
	memcpy(deviceProps->daqName + 60, &tHandle, 4);
	
	/*
	 * Assuming that the string doesn't need those 64 bytes
	 * you should at least do
	   memcpy( deviceProps->daqName 
	           + sizeof(deviceProps->daqName) 
		   - sizeof(DaqHandleT), &tHandle, sizeof(DaqHandleT));
	 *
	 * but if this is really the case, just add another entry to 
	 * the DaqDevicePropsT structure to hold this handle.
	 */

	return DerrNoError;
}


DAQAPI DaqError WINAPI
daqOnline(DaqHandleT handle, PBOOL online)
{

	DaqError err;
	daqIOT sb;
	DAQ_ONLINE_PT daqOnlineTest = (DAQ_ONLINE_PT) & sb;

	err = apiCtrlTestHandle(handle, DlmNone);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	err = itfExecuteCommand(handle, &sb, DAQ_ONLINE);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	*online = daqOnlineTest->online;

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqGetHardwareInfo(DaqHandleT handle, DaqHardwareInfo whichInfo, VOID * info)
{
	ApiCtrlPT ps;
	DaqError err;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	ps = &session[handle];

	switch (whichInfo) {

	case DhiHardwareVersion:
		*(DaqHardwareVersion *) info = ps->gnDeviceType;        
		break;

	case DhiADmin:

		if (Is10V(ps->gnDeviceType))
			*(float *) info = -10.0f;
		else
			*(float *) info = -5.0f;
		break;

	case DhiADmax:

		if (Is10V(ps->gnDeviceType))
			*(float *) info = 10.0f;
		else
			*(float *) info = 5.0f;
		break;

	case DhiProtocol:
		*(DaqProtocol *) info = ps->gnProtocol;
		break;
	}

	return DerrNoError;

}


DAQAPI DaqError WINAPI
daqGetDriverVersion(PDWORD version)
{

	*version = VER_MAJOR * 100 + VER_MINOR;

	return DerrNoError;
}


DAQAPI DaqError WINAPI
daqSetTimeout(DaqHandleT handle, DWORD mSecTimeout)
{

	DaqError err;

	err = apiCtrlTestHandle(handle, DlmNone);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	session[handle].mSecTimeout = (mSecTimeout ? mSecTimeout : INFINITE);

	return DerrNoError;

}


DAQAPI VOID WINAPI
daqDefaultErrorHandler(DaqHandleT handle, DaqError errCode)
{
    int result;
    CHAR errMsg[64];
    CHAR errStr[255];

    daqFormatError(errCode, errMsg);

    sprintf(errStr,
	    "Daq Error #%u (0x%x),  handle = %ld (0x%ld)\n'%s'\n",
	    errCode, errCode, (ULONG) handle, (ULONG) handle, errMsg);

    strcat(errStr, "\nDo you wish to continue?\n");

    result = MessageBox((HWND) NULL, errStr, "Default Error Handler",
	    MB_YESNO | MB_DEFBUTTON1 | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
    if (IDNO == result) {
	fprintf(stderr, "\nExiting Process\n");
	daqSetDefaultErrorHandler((DaqErrorHandlerFPT) 0);
	daqSetErrorHandler(handle, (DaqErrorHandlerFPT) 0);
	daqClose(handle);
	ExitProcess(1);
    }

    return ;
}


DAQAPI DaqError WINAPI
daqSetDefaultErrorHandler(DaqErrorHandlerFPT handler)
{
	daqErrorHandler = handler;
	return DerrNoError;
}


DAQAPI DaqError WINAPI
daqSetErrorHandler(DaqHandleT handle, DaqErrorHandlerFPT handler)
{
	DaqError err;
	err = apiCtrlTestHandle(handle, DlmNone);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);
	session[handle].errorHandler = handler;
	return DerrNoError;
}


DAQAPI DaqError WINAPI
daqProcessError(DaqHandleT handle, DaqError errCode)
{
	return apiCtrlProcessError(handle, errCode);
}


DAQAPI DaqError WINAPI
daqGetLastError(DaqHandleT handle, DaqError * errCode)
{
	DaqError err;

	err = apiCtrlTestHandle(handle, DlmNone);
	if (err != DerrNoError) {

		*errCode = daqErrCode;

		daqErrCode = DerrNoError;
	} else {

		*errCode = session[handle].errCode;
		session[handle].errCode = DerrNoError;
	}

	return DerrNoError;
}


DAQAPI DaqHandleT WINAPI
daqOpen(PCHAR daqName)
{
	DaqHandleT handle;
	MsgT initMsg;
	DaqError err;
	LONG lockMask;

	lockMask = DlmAll;
        memset((void *) &initMsg, 0, sizeof(MsgT));  // avoid Valgrind complains

	err = openHandle(&handle, daqName, lockMask);
	if (err == DerrNoError) {

		//MAH:09.26.03
		if(BeenHereDoneThat == 0)
		{
			initMsg.msg = MsgAttach;        // specify the message type
			initMsg.errCode = DerrNoError;	// clear the error field
			cmnBroadcastMessage(&initMsg);
			BeenHereDoneThat =1;
		}

		initMsg.msg = MsgInit;
		initMsg.handle = handle;
		initMsg.daqName = daqName;
		initMsg.lockMask = lockMask;
		initMsg.errCode = DerrNoError;

		cmnBroadcastMessage(&initMsg);

		err = initMsg.errCode;
		if (err != DerrNoError) {
			dbg("help daqclose called  err=%i", err);
			daqClose(handle);
			handle = NO_GRIP;
		}
	}

	if (err != DerrNoError) {
		apiCtrlProcessError(handle, err);
	}

	return handle;

}

DAQAPI DaqError WINAPI
daqClose(DaqHandleT handle)
{
	MsgT closeMsg;
	DaqError err;
	LONG lockMask;

	lockMask = DlmAll;
        memset((void *) &closeMsg, 0, sizeof(MsgT));  // avoid Valgrind complains
	err = apiCtrlTestHandle(handle, DlmNone);

	if (err == DerrNoError) {
		closeMsg.msg = MsgClose;
		closeMsg.handle = handle;
		closeMsg.lockMask = lockMask;
		closeMsg.errCode = DerrNoError;
		closeMsg.deviceType = session[handle].gnDeviceType;
		cmnBroadcastMessage(&closeMsg);
		err = closeMsg.errCode;
	}

	if (err != DerrNoError) {
		apiCtrlProcessError(handle, err);
	}

	return err;
}


DAQAPI DaqError WINAPI
daqWaitForEvents(DaqHandleT * handles, DaqTransferEvent * events,
		 DWORD eventCount, PBOOL eventSet, DaqWaitMode waitMode)
{
	DaqError err;
	DWORD i;
	BOOL doneWaiting, thisEventSet;
	DWORD startTick, lastTick, currentTick, localmSecTimeout;

	if (eventCount == 0)
		return apiCtrlProcessError(handles[0], DerrInvCount);

	if ((waitMode < DwmNoWait) || (waitMode > DwmWaitForAll))
		return apiCtrlProcessError(handles[0], DerrInvMode);

	for (i = 0; i < eventCount; i++) {
		err = apiCtrlTestHandle(handles[i], DlmNone);
		if (err != DerrNoError)
			return apiCtrlProcessError(handles[i], err);

		if ((events[i] < DteAdcData) || (events[i] > DteIODone))
			return apiCtrlProcessError(handles[i], DerrInvEvent);

	}

	startTick = GetTickCount();
	lastTick = 0;
	localmSecTimeout = session[handles[0]].mSecTimeout;

	do {
		switch (waitMode) {
		case DwmNoWait:
		case DwmWaitForAll:
			doneWaiting = TRUE;
			break;

		case DwmWaitForAny:
			doneWaiting = FALSE;
			break;
		}

		for (i = 0; i < eventCount; i++) {
			switch (events[i]) {
			case DteAdcData:
			case DteAdcDone:
				err = apiAdcTransferGetFlag(handles[i], events[i], &thisEventSet);
				break;

			case DteDacData:
			case DteDacDone:

				err = DerrNotImplemented;
				break;

			case DteIOData:
			case DteIODone:

				err = DerrNotImplemented;
				break;
			}

			if (err != DerrNoError)
				return apiCtrlProcessError(handles[i], err);

			if (eventSet)
				eventSet[i] = thisEventSet;

			switch (waitMode) {
			case DwmNoWait:

				break;

			case DwmWaitForAny:

				if (thisEventSet)
					doneWaiting = TRUE;
				break;

			case DwmWaitForAll:

				if (!thisEventSet)
					doneWaiting = FALSE;
				break;
			}
		}

		if (!doneWaiting) {

			currentTick = GetTickCount();
			if (lastTick > currentTick) {

				localmSecTimeout =
				    max( (LONG) (localmSecTimeout -(lastTick - startTick) - currentTick), 0L);
				startTick = 0;
			}

			if ((currentTick - startTick) > localmSecTimeout) {
				return apiCtrlProcessError(handles[0], DerrTimeout);
			}
			lastTick = currentTick;

		}
	} while (!doneWaiting);

	return DerrNoError;
}


DAQAPI DaqError WINAPI
daqWaitForEvent(DaqHandleT handle, DaqTransferEvent event)
{
	return daqWaitForEvents(&handle, &event, 1, NULL, DwmWaitForAll);
}


#if 0
DaqError WINAPI
daqGetLockMask(DaqHandleT handle, DaqPorts devPort, PLONG lockMask)
{
	DWORD i;
	DWORD processId;

	cmnWaitForMutex(LockTableMutex);

	processId = GetCurrentProcessId();
	*lockMask = DlmNone;
	for (i = 0; i < lockCount; i++) {
		if (devPort == lockTable[i].devPort) {

			if ((processId == lockTable[i].processId)
			    && (handle == lockTable[i].handle)) {

				*lockMask = lockTable[i].lockMask;
			} else {

				*lockMask |= lockTable[i].lockMask;
			}
		}
	}

	cmnReleaseMutex(LockTableMutex);

	return DerrNoError;

}
#endif


DAQAPI DaqError WINAPI
daqFormatError(DaqError errorNum, PCHAR msg)
{
	DWORD i;
//	DaqError errCode = DerrNoError;

	typedef struct {
		DaqError errCode;
		PCHAR errMsg;
	} daqErrMsgArrayT;

	daqErrMsgArrayT errMsgArray[MaxErrCodes] = {
		{DerrNoError, "No error"}, 
		{DerrBadChannel, "Specified LPT channel is out of range"}, 
		{DerrNotOnLine, "Requested device is not on line"},
		{DerrNoDaqbook, "DaqBook is not on the requested channel"},
		{DerrBadAddress, "Bad function address"},
		{DerrFIFOFull, "FIFO Full detected, possible data corruption"},
		{DerrBadDma, "Bad or illegal DMA channel or mode specified for device"},
		{DerrBadInterrupt, "Bad or illegal INTERRUPT level specified for device"},
		{DerrDmaBusy, "DMA is currently being used."}, 
		{DerrInvChan, "Invalid analog input channel"}, 
		{DerrInvCount, "Invalid count parameter"},
		{DerrInvTrigSource, "Invalid trigger source parameter"}, 
		{DerrInvLevel, "Invalid trigger level parameter"},
		{DerrInvGain, "Invalid channel gain parameter"}, 
		{DerrInvDacVal, "Invalid DAC output parameter"}, 
		{DerrInvExpCard, "Invalid expansion card parameter"},
		{DerrInvPort, "Invalid port parameter"}, 
		{DerrInvChip, "Invalid chip parameter"}, 
		{DerrInvDigVal, "Invalid digital output parameter"},
		{DerrInvBitNum, "Invalid bit number parameter"}, 
		{DerrInvClock, "Invalid clock parameter"}, 
		{DerrInvTod, "Invalid time-of-day parameter"},
		{DerrInvCtrNum, "Invalid counter number"}, 
		{DerrInvCntSource, "Invalid counter source parameter"}, 
		{DerrInvCtrCmd, "Invalid counter command parameter"},
		{DerrInvGateCtrl, "Invalid gate control parameter"}, 
		{DerrInvOutputCtrl, "Invalid output control parameter"},
		{DerrInvInterval, "Invalid interval parameter"}, 
		{DerrTypeConflict, "An integer was passed to a function requiring a character"},
		{DerrMultBackXfer, "A second transfer was requested"}, 
		{DerrInvDiv, "Invalid Fout divisor"}, 
		{DerrTCE_TYPE, "Thermocouple type out of range"},
		{DerrTCE_TRANGE, "Temperature out of CJC range"}, 
		{DerrTCE_VRANGE, "Voltage out of thermocouple range"},
		{DerrTCE_PARAM, "Unspecified TC parameter value error"}, 
		{DerrTCE_NOSETUP, "daqCvtTCConvert called before daqCvtTCSetup"},
		{DerrNotCapable, "Device is incapable of function"}, 
		{DerrOverrun, "A buffer overrun occurred"},
		{DerrZCInvParam, "Unspecified Cal parameter value error"}, 
		{DerrZCNoSetup, "daqCalConvert called before daqCalSetup"},
		{DerrInvCalFile, "Cannot open the specified calibration file"},
		{DerrMemLock, "Cannot lock allocated memory from operating system"},
		{DerrMemHandle, "Cannot get a memory handle from operating system"},
		{DerrNoPreTActive, "No pre-trigger configured"}, 
		{DerrInvDacChan, "DAC channel does not exist"}, 
		{DerrInvDacParam, "DAC parameter is invalid"},
		{DerrInvBuf, "Buffer points to NULL or buffer size is zero"},
		{DerrMemAlloc, "Could not allocate the needed memory"}, 
		{DerrUpdateRate, "Could not achieve the specified update rate"},
		{DerrInvDacWave, "Could not start DAC waveforms. Parameters missing or invalid"},
		{DerrInvBackDac, "Could not start DAC waveforms with background transfers"},
		{DerrInvPredWave, "Predefined DAC waveform not supported"}, 
		{DerrRtdValue, "rtdValue out of range"},
		{DerrRtdNoSetup, "rtdConvert called before rtdSetup"}, 
		{DerrRtdTArraySize, "RTD Temperature array not large enough"},
		{DerrRtdParam, "Incorrect RTD parameter"}, 
		{DerrInvBankType, "Invalid bank type specified"}, 
		{DerrBankBoundary, "Simultaneous writes to DBK cards in different banks not allowed"},
		{DerrInvFreq, "Invalid scan frequency specified"}, 
		{DerrNoDaq, "No Daq 112B/216B installed"}, 
		{DerrInvOptionType, "Invalid option type parameter"},
		{DerrInvOptionValue, "Invalid option value parameter"}, 
		{DerrTooManyHandles, "No more handles available to open"},
		{DerrInvLockMask, "Only part of the resource is locked, must be all or none"},
		{DerrAlreadyLocked, "All or part of the resource was locked by another application"},
		{DerrAcqArmed, "Operation not available while an acquisition is armed"},
		{DerrParamConflict, "Each parameter is valid, but the combination is invalid"},
		{DerrInvMode, "Invalid acquisition mode"}, 
		{DerrInvOpenMode, "Invalid file open mode"}, 
		{DerrFileOpenError, "Unable to open file"},
		{DerrFileWriteError, "Unable to write file"}, 
		{DerrFileReadError, "Unable to read file"}, 
		{DerrInvClockSource, "Invalid clock source"},
		{DerrInvEvent, "Invalid transfer event"}, 
		{DerrTimeout, "Timeout on wait"}, 
		{DerrInitFailure, "Unexpected result occurred while initializing the hardware"},
		{DerrBufTooSmall, "Array size too small"}, 
		{DerrInvType, "Invalid Dac Device type"}, 
		{DerrArraySize, "Array size not large enough"},
		{DerrBadAlias, "Invalid alias name for Vxd lookup"}, 
		{DerrInvCommand, "Invalid comamnd"}, 
		{DerrInvHandle, "Invalid handle"},
		{DerrNoTransferActive, "Transfer not active"}, 
		{DerrNoAcqActive, "Acquisition not active"}, 
		{DerrInvOpstr, "Invalid operation string used for enhanced triggering"},
		{DerrDspCommFailure, "Device with DSP failed communication"},
		{DerrEepromCommFailure, "Device with Eeprom failed communication"}, 
		{DerrInvEnhTrig, "Device using enhanced trigger detected invalid trigger type"},
		{DerrInvCalConstant, "User calibration constant out-of-range"},
		{DerrInvErrorCode, "Invalid error code"}, 
		{DerrInvAdcRange, "Invalid analog input voltage range parameter"}, 
		{DerrInvCalTableType, "Invalid calibration table type"},
		{DerrInvCalInput, "Invalid calibration input signal selection"},
		{DerrInvRawDataFormat, "Invalid raw data format selection"},
		{DerrNotImplemented, "Feature/function not implemented yet"},
		{DerrInvDioDeviceType, "Invalid digital I/O device type"}, 
		{DerrInvPostDataFormat, "Invalid post-processing data format selection"}, 
		{DerrDaqStalled, "Low level driver stalled"},
		{DerrDaqLostPower, "Daq Device has lost power"}, 
		{DerrDaqMissing, "Daq Device is missing"}, 
		{DerrScanConfig, "Invalid channel scan config"},
		{DerrInvTrigSense, "Invalid trigger sense parameter"}, 
		{DerrInvTrigEvent, "Invalid trigger event parameter"},
		{DerrInvTrigChannel, "Invalid Trigger channel"}, 
		{DerrDacWaveformNotActive, "DAC waveform output not active"},
		{DerrDacWaveformActive, "DAC waveform output already active"},
		{DerrDacNotEnoughMemory, "DAC static waveforms exceed maximum length"}, 
		{DerrDacBuffersNotEqual, "DAC static waveforms unequal length"},
		{DerrDacBufferTooSmall, "DAC dynamic waveform buffer too small"},
		{DerrDacBufferUnderrun, "DAC dynamic waveform buffer underrun"},
		{DerrDacPacerOverrun, "DAC pacer overrun"}, 
		{DerrAdcPacerOverrun, "ADC pacer overrun"}, 
		{DerrAdcNotReady, "ADC not ready"},
		{DerrArbitrationFailure, "Internal bus arbitration error"}, 
		{DerrDacWaveFileTooSmall, "DAC waveform file too small for requested use"}, 
		{DerrDacWaveFileUnderrun, "DAC waveform file buffer underrun"},
		{DerrDacWaveModeConflict, "DAC waveform mode, buffer, or source conflict"}, 
		{(DaqError) 0xFFFFFFFF, "Highest possible error code"}
	};

	i = 0;
	while (errMsgArray[i].errCode != (DaqError) 0xFFFFFFFF) {
		if (errMsgArray[i].errCode == errorNum) {

			strcpy(msg, errMsgArray[i].errMsg);
			return DerrNoError;
		}
		i++;
	}

	strncpy(msg, "Invalid error code", strlen("Invalid error code"));
	return DerrInvErrorCode;
}


#ifdef OBSOLETE
static DaqHardwareVersions
getHardwareVersion(DaqHandleT handle)
{

	if (apiAdc16Bit(handle)) {
		return Daq216;
	} else {
		return Daq112;
	}

}
#endif
