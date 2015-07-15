////////////////////////////////////////////////////////////////////////////
//
// apidio.c                        I    OOO                           h
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
#include "iottypes.h"

#include "daqx.h"
#include "cmnapi.h"
#include "api.h"

#include "ddapi.h"
#include "itfapi.h"

typedef struct {
	BOOL capableBitIO;
	BOOL capable8255;
	BOOL capable9513;
} ApiDioT;

static ApiDioT session[MaxSessions];

VOID
apiDioMessageHandler(MsgPT msg)
{

	if (msg->errCode == DerrNoError) {
		switch (msg->msg) {
		case MsgAttach:

			break;

		case MsgDetach:

			break;

		case MsgInit:

			switch (msg->deviceType) {

			case DaqBook112:
			case DaqBoard112:
			case Daq112:
			case DaqBook216:
			case DaqBoard216:
			case Daq216:
			case TempBook66:
			case WaveBook512:
			case WaveBook516:
			case WaveBook516_250:
			case WaveBook512_10V:

			case WaveBook512A:
			case WaveBook516A:
				session[msg->handle].capableBitIO = 1;
				session[msg->handle].capable8255 = 0;
				session[msg->handle].capable9513 = 0;
				break;

			case DaqBoard2003:
				session[msg->handle].capableBitIO = 0;
				session[msg->handle].capable8255 = 0;
				session[msg->handle].capable9513 = 0;
				break;

			case DaqBoard2000:
			case DaqBoard2001:
			case DaqBoard2002:

			case DaqBoard2004:
			case DaqBoard2005:
            
            case DaqBoard1000:
            case DaqBoard1005:
				session[msg->handle].capableBitIO = 0;
				session[msg->handle].capable8255 = 1;
				session[msg->handle].capable9513 = 1;
				break;

			case DaqBook100:
			case DaqBook120:
			case DaqBook200:
			case DaqBoard100:
			case DaqBoard200:
			default:
				session[msg->handle].capableBitIO = 1;
				session[msg->handle].capable8255 = 1;
				session[msg->handle].capable9513 = 1;
				daqIOWrite(msg->handle, DiodtLocal9513,
					   Diodp9513Command, 0, DioepP3, 0xFF);
				daq9513MultCtrl(msg->handle, DiodtLocal9513, 0,
						DmccLoad, 1, 1, 1, 1, 1);
				daq9513SetMasterMode(msg->handle,
						     DiodtLocal9513, 0, 0,
						     DcsF1, 0, 0, DtodDisabled);
				break;

			}
			break;

		case MsgClose:
			switch (msg->deviceType) {
			case TempBook66:
				daqIOWrite(msg->handle, Diodt8254Ctr, Diodp8254Trig, 0, DioepP2, 0);
				break;
				
			default:
				break;
			}
			break;

		default:
			break;
		}
	}

}

DaqError
apiDioReadWriteValue(DaqHandleT handle, DaqIODeviceType devType,
		     DaqIODevicePort devPort, DWORD whichDevice,
		     DaqIOExpansionPort whichExpPort, PDWORD value, DWORD bitMask, BOOL writeOp)
{

	DaqError err;
	DaqIODevicePort maxPort;
	DWORD portOffset;
	DWORD shiftCount;
	DWORD itfDevType;
	BOOL ctrFlag;

	showwhere();

	ctrFlag = FALSE;
	shiftCount = 0;
	itfDevType = DioTypeExp;

	switch (devType) {
	case DiodtLocalBitIO:
	case DiodtLocal8254:
		maxPort = DiodpBitIO;
		itfDevType = DioTypeLocalBit;
		break;

	case Diodt8254Dig:
		maxPort = Diodp8254IR;
		itfDevType = (Diodt8254Dig % 3) + 1;
		break;

	case Diodt8254Ctr:
		maxPort = Diodp8254IR;
		itfDevType = (Diodt8254Ctr % 3) + 2;
		break;

	case DiodtLocal8255:
	case DiodtExp8255:
		if (devPort == Diodp8255CHigh) {
			devPort = Diodp8255C;
			bitMask &= 0x0000000F;
			shiftCount = 4;
		} else if (devPort == Diodp8255CLow) {
			devPort = Diodp8255C;
			bitMask &= 0x0000000F;
		} else {
			if (whichExpPort == DioepP3) {
				bitMask &= 0x0000FFFF;
			} else {
				bitMask &= 0x000000FF;
			}
		}
		maxPort = Diodp8255IR;
		if (devType == DiodtLocal8255)
			itfDevType = DioTypeLocal;
		break;

	case DiodtP2Local8:
		bitMask &= 0x000000FF;
		maxPort = DiodpP2LocalIR;
		itfDevType = DioTypeLocal;
		break;

	case DiodtDbk23:
	case DiodtDbk24:
		bitMask &= 0x000000FF;
		maxPort = DiodpDbk23Unused;
		break;
	case DiodtDbk25:
		bitMask &= 0x000000FF;
		maxPort = DiodpDbk25;
		break;
	case DiodtLocal9513:
	case DiodtExp9513:
		ctrFlag = TRUE;
		bitMask &= 0x0000FFFF;

		maxPort = DiodpP3LocalCtr16;
		if (devType == DiodtLocal9513)
			itfDevType = CtrLocal9513;
		else
			itfDevType = CtrExp9513;
		break;
	case DiodtWbk512:

		maxPort = DiodpWbk516_16Bit;

		if (devPort == DiodpWbk516_16Bit)
			bitMask &= 0x0000FFFF;
		else
			bitMask &= 0x000000FF;
		break;

	case DiodtWbk17:
		maxPort = DiodpWbk17_8Bit;
		bitMask &= 0x000000FF;

		break;

	default:

		err = apiCtrlTestHandle(handle, DlmAll);
		if (err != DerrNoError)
			return err;
		return DerrInvDioDeviceType;
	}

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return err;

	if (devType == DiodtLocalBitIO || devType == DiodtWbk512 || devType == DiodtWbk17) {
		if (!session[handle].capableBitIO)
			return apiCtrlProcessError(handle, DerrNotCapable);
	} else if (ctrFlag) {
		if (!session[handle].capable9513)
			return apiCtrlProcessError(handle, DerrNotCapable);
	} else {
		if (!session[handle].capable8255)
			return apiCtrlProcessError(handle, DerrNotCapable);
	}

	if (devPort > maxPort) {
		dbg("before ck maxPort, maxPort = %d", maxPort);
		return DerrInvPort;
	}

	if (devType == DiodtP2Local8) {

		switch (devPort) {
		case DiodpP2Local8:
			{
				if (whichDevice > (DWORD) maxPort)
					return DerrInvPort;
			}
			break;
		case DiodpP2LocalIR:
			{
				whichDevice = 0;
			}
			break;
		default:
			{
				return DerrInvPort;
			}
			break;
		}

		maxPort = (DaqIODevicePort) 0;
	}

	if (devType == DiodtWbk17) {
		if (devPort != DiodpWbk17_8Bit)
			return DerrInvPort;
		if ((whichDevice < 9) || (whichDevice > 72))
			return DerrInvPort;

		itfDevType = DiodtWbk17;
		maxPort = (DaqIODevicePort) 0;
		devPort = (DaqIODevicePort) 0;
	}

	if (!bitMask)
		return DerrInvBitNum;

	portOffset = (whichDevice * (maxPort + 1)) + devPort;

	if (writeOp) {

		*value &= bitMask;

		*value <<= shiftCount;
		bitMask <<= shiftCount;
	}

	if (ctrFlag) {
		DWORD portOffset1, value1, doWrite1, portOffset2;
		portOffset1 = 1;
		portOffset2 = 0;
		doWrite1 = TRUE;
		value1 = 0;

		switch (devPort) {

		case DiodpP3LocalCtr16:
			portOffset2 = whichDevice;
			doWrite1 = FALSE;
			break;

		case Diodp9513Command:
			portOffset2 = 1;
			doWrite1 = FALSE;
			break;

		case Diodp9513Data:
			doWrite1 = FALSE;
			break;

		case Diodp9513MasterMode:
			value1 = 0x17;
			break;

		case Diodp9513Alarm1:
			value1 = 0x07;
			break;

		case Diodp9513Alarm2:
			value1 = 0x0f;
			break;

		case Diodp9513Status:
			value1 = 0x1f;
			break;

		case Diodp9513Mode1:
		case Diodp9513Mode2:
		case Diodp9513Mode3:
		case Diodp9513Mode4:
		case Diodp9513Mode5:
			value1 = (devPort - Diodp9513Mode1) + 0x1;
			break;

		case Diodp9513Load1:
		case Diodp9513Load2:
		case Diodp9513Load3:
		case Diodp9513Load4:
		case Diodp9513Load5:
			value1 = (devPort - Diodp9513Load1) + 0x9;
			break;

		case Diodp9513Hold1:
		case Diodp9513Hold2:
		case Diodp9513Hold3:
		case Diodp9513Hold4:
		case Diodp9513Hold5:
			value1 = (devPort - Diodp9513Hold1) + 0x11;
			break;

		case Diodp9513Hold1HC:
		case Diodp9513Hold2HC:
		case Diodp9513Hold3HC:
		case Diodp9513Hold4HC:
		case Diodp9513Hold5HC:
			value1 = (devPort - Diodp9513Hold1) + 0x19;
			break;

		default:
			break;
		}

		if (writeOp) {
			daqIOT sb;
			CTR_WRITE_PT ctrWrite = (CTR_WRITE_PT) & sb;

			ctrWrite->ctrDevType = itfDevType;
			ctrWrite->ctrWhichExpPort = whichExpPort;
			ctrWrite->ctrPortOffset1 = portOffset1;
			ctrWrite->ctrDoWrite1 = doWrite1;
			ctrWrite->ctrValue1 = value1;
			ctrWrite->ctrPortOffset2 = portOffset2;
			ctrWrite->ctrValue2 = *value;
			ctrWrite->ctrBitMask = bitMask;

			err = itfExecuteCommand(handle, &sb, CTR_WRITE);
		} else {
			daqIOT sb;
			CTR_READ_PT ctrRead = (CTR_READ_PT) & sb;

			ctrRead->ctrDevType = itfDevType;
			ctrRead->ctrWhichExpPort = whichExpPort;
			ctrRead->ctrPortOffset1 = portOffset1;
			ctrRead->ctrDoWrite1 = doWrite1;
			ctrRead->ctrValue1 = value1;
			ctrRead->ctrPortOffset2 = portOffset2;

			err = itfExecuteCommand(handle, &sb, CTR_READ);

			*value = ctrRead->ctrValue2;
		}
	} else {
		if (writeOp) {
			daqIOT sb;
			DIO_WRITE_PT dioWrite = (DIO_WRITE_PT) & sb;
			dioWrite->dioValue = *value;
			dioWrite->dioDevType = itfDevType;
			dioWrite->dioPortOffset = portOffset;
			dioWrite->dioWhichExpPort = whichExpPort;
			dioWrite->dioBitMask = bitMask;

			err = itfExecuteCommand(handle, &sb, DIO_WRITE);
		} else {
			daqIOT sb;

			DIO_READ_PT dioRead = (DIO_READ_PT) & sb;

			dioRead->dioDevType = itfDevType;
			dioRead->dioPortOffset = portOffset;
			dioRead->dioWhichExpPort = whichExpPort;

			err = itfExecuteCommand(handle, &sb, DIO_READ);

			*value = dioRead->dioValue;
		}
	}

	if (err != DerrNoError)
		return err;

	if (!writeOp) {

		*value >>= shiftCount;

		*value &= bitMask;
	}

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqIORead(DaqHandleT handle, DaqIODeviceType devType, DaqIODevicePort devPort,
	  DWORD whichDevice, DaqIOExpansionPort whichExpPort, PDWORD value)
{

	DaqError err;

	err =
	    apiDioReadWriteValue(handle, devType, devPort, whichDevice,
				 whichExpPort, value, 0xFFFFFFFF, FALSE);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqIOReadBit(DaqHandleT handle, DaqIODeviceType devType,
	     DaqIODevicePort devPort, DWORD whichDevice,
	     DaqIOExpansionPort whichExpPort, DWORD bitNum, PBOOL bitValue)
{

	DaqError err;

	err =
	    apiDioReadWriteValue(handle, devType, devPort, whichDevice,
				 whichExpPort, (DWORD *) bitValue, 1L << bitNum, FALSE);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	*bitValue = (*bitValue ? TRUE : FALSE);

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqIOWrite(DaqHandleT handle, DaqIODeviceType devType, DaqIODevicePort devPort,
	   DWORD whichDevice, DaqIOExpansionPort whichExpPort, DWORD value)
{

	DaqError err;

	err =
	    apiDioReadWriteValue(handle, devType, devPort, whichDevice,
				 whichExpPort, &value, 0xFFFFFFFF, TRUE);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqIOWriteBit(DaqHandleT handle, DaqIODeviceType devType,
	      DaqIODevicePort devPort, DWORD whichDevice,
	      DaqIOExpansionPort whichExpPort, DWORD bitNum, BOOL bitValue)
{

	DaqError err;

	bitValue = (bitValue ? 0xFFFFFFFF : 0);

	err =
	    apiDioReadWriteValue(handle, devType, devPort, whichDevice,
				 whichExpPort, (DWORD *) & bitValue, 1L << bitNum, TRUE);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqIOGet8255Conf(DaqHandleT handle, BOOL portA, BOOL portB, BOOL portCHigh,
		 BOOL portCLow, PDWORD config)
{

	DaqError err;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	portA = (portA ? 1 : 0);
	portB = (portB ? 1 : 0);
	portCHigh = (portCHigh ? 1 : 0);
	portCLow = (portCLow ? 1 : 0);

	*config = 0x80 | (portA << 4) | (portB << 1) | (portCHigh << 3) | portCLow;

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqIOTransferReset(DaqHandleT handle)
{
	return DerrNotCapable;
}

DAQAPI DaqError WINAPI
daqIOTransferSetBuffer(DaqHandleT handle, DaqIOEventCode eventCode,
		       DaqIODeviceType devType, DaqIODevicePort devPort,
		       DWORD whichDevice, DaqIOExpansionPort whichExpPort,
		       DaqIOOperationCode opCode, PVOID buf, DWORD bufCount)
{
	return DerrNotCapable;
}

DAQAPI DaqError WINAPI
daqIOTransferStart(DaqHandleT handle)
{
	return DerrNotCapable;
}

DAQAPI DaqError WINAPI
daqIOTransferGetStat(DaqHandleT handle, DaqIODeviceType devType,
		     DaqIODevicePort devPort, DWORD whichDevice,
		     DaqIOExpansionPort whichExpPort, DaqIOActiveFlag * active, PDWORD retCount)
{
	return DerrNotCapable;
}

DAQAPI DaqError WINAPI
daqIOTransferStop(DaqHandleT handle)
{
	return DerrNotCapable;
}
