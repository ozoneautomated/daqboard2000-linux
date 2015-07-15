////////////////////////////////////////////////////////////////////////////
//
// apictr.c                        I    OOO                           h
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


#include "iottypes.h"
#include "daqx.h"
#include "cmnapi.h"
#include "api.h"

VOID
apiCtrMessageHandler(MsgPT msg)
{

	if (msg->errCode == DerrNoError) {
		return; 
	}
	switch (msg->msg) {
	case MsgAttach:

		break;

	case MsgDetach:

		break;

	case MsgInit:

		break;

	case MsgClose:

		break;
	}

	return ;
}


DAQAPI DaqError WINAPI
daq9513GetHold(DaqHandleT handle, DaqIODeviceType deviceType,
	       DWORD whichDevice, DWORD ctrNum, PWORD ctrVal)
{

	DWORD dwctrVal;
	DaqError err;

	if ((ctrNum < 1) || (ctrNum > 5)) {
		return (apiCtrlProcessError(handle, DerrInvCtrNum));
	}

	err = daqIORead(handle, deviceType,
			(DaqIODevicePort) ((Diodp9513Hold1 + ctrNum) - 1),
			whichDevice, DioepP3, &dwctrVal);
	*ctrVal = (WORD) dwctrVal;
	return err;
}


DAQAPI DaqError WINAPI
daq9513MultCtrl(DaqHandleT handle, DaqIODeviceType deviceType,
		DWORD whichDevice, Daq9513MultCtrCommand ctrCmd,
		BOOL ctr1, BOOL ctr2, BOOL ctr3, BOOL ctr4, BOOL ctr5)
{
	DaqError err;

	if ((ctrCmd < DmccArm) || (ctrCmd > DmccDisarm)) {
		return (apiCtrlProcessError(handle, DerrInvCtrCmd));
	}

	/* PEK:weird
	 * loads of spooky magic numbers here...
	 */
	if (ctrCmd == DmccLoadArm) {
		if (ctr1) {
			err = daqRegWrite(handle, Dreg9513IR, 0xe8 | 1);
			err = daqRegWrite(handle, Dreg9513IR, 0xe0 | 1);
		}
		if (ctr2) {
			err = daqRegWrite(handle, Dreg9513IR, 0xe8 | 2);
			err = daqRegWrite(handle, Dreg9513IR, 0xe0 | 2);
		}
		if (ctr3) {
			err = daqRegWrite(handle, Dreg9513IR, 0xe8 | 3);
			err = daqRegWrite(handle, Dreg9513IR, 0xe0 | 3);
		}
		if (ctr4) {
			err = daqRegWrite(handle, Dreg9513IR, 0xe8 | 4);
			err = daqRegWrite(handle, Dreg9513IR, 0xe0 | 4);
		}
		if (ctr5) {
			err = daqRegWrite(handle, Dreg9513IR, 0xe8 | 5);
			err = daqRegWrite(handle, Dreg9513IR, 0xe0 | 5);
		}
	}

	err = daqIOWrite(handle, deviceType, Diodp9513Command, whichDevice,
			 DioepP3,
			 ((ctrCmd + 1) << 5) | ((ctr5 != 0) << 4) | ((ctr4 !=
								      0) << 3) |
			 ((ctr3 != 0) << 2) | ((ctr2 != 0) << 1) | (ctr1 != 0));

	return err;

}


DAQAPI DaqError WINAPI
daq9513RdFreq(DaqHandleT handle, DaqIODeviceType deviceType,
	      DWORD whichDevice, DWORD interval, Daq9513CountSource cntSource, PDWORD count)
{
	DaqError err;

	if ((interval < 1) || (interval > 32767)) {
		return (apiCtrlProcessError(handle, DerrInvInterval));
	}

	if ((cntSource < 1) || (cntSource > 9)) {
		return (apiCtrlProcessError(handle, DerrInvCntSource));
	}

	err = daqIOWrite(handle, deviceType, Diodp9513Mode4, whichDevice, DioepP3, 0x0e22);

	if (!err)
		err = daqIOWrite(handle, deviceType, Diodp9513Mode4,
				 whichDevice, DioepP3, interval);

	if (!err)
		err = daqIOWrite(handle, deviceType, Diodp9513Command, whichDevice, DioepP3, 0x68);

	if (!err)
		err = daqIOWrite(handle, deviceType, Diodp9513Mode5,
				 whichDevice, DioepP3, 0x8029 | (cntSource << 8));

	if (!err)
		err = daqIOWrite(handle, deviceType, Diodp9513Mode5, whichDevice, DioepP3, 0x00);

	if (!err)
		err = daqIOWrite(handle, deviceType, Diodp9513Command, whichDevice, DioepP3, 0x70);

	if (!err)
		err = daqIOWrite(handle, deviceType, Diodp9513Command, whichDevice, DioepP3, 0xB0);

	if (!err)
		err = daqIORead(handle, deviceType, Diodp9513Mode5, whichDevice, DioepP3, count);
	return err;

}


DAQAPI DaqError WINAPI
daq9513SetAlarm(DaqHandleT handle, DaqIODeviceType deviceType,
		DWORD whichDevice, DWORD alarmNum, DWORD alarmVal)
{
	DaqError err;

	if ((alarmNum < 1) || (alarmNum > 2)) {
		return (apiCtrlProcessError(handle, DerrInvCtrNum));
	}

	err = daqIOWrite(handle, deviceType,
			 (DaqIODevicePort) ((Diodp9513Alarm1 + alarmNum) - 1),
			 whichDevice, DioepP3, alarmVal);

	return err;
}


DAQAPI DaqError WINAPI
daq9513SetCtrMode(DaqHandleT handle, DaqIODeviceType deviceType,
		  DWORD whichDevice, DWORD ctrNum,
		  Daq9513GatingControl gateCtrl, BOOL cntEdge,
		  Daq9513CountSource cntSource, BOOL specGate, BOOL reload,
		  BOOL cntRepeat, BOOL cntType, BOOL cntDir, Daq9513OutputControl outputCtrl)
{
	DaqError err;
	WORD outputCtrlArray[5] = { 0x00, 0x01, 0x02, 0x04, 0x05 };

	if ((ctrNum < 1) || (ctrNum > 5)) {
		return (apiCtrlProcessError(handle, DerrInvCtrNum));
	}
	if ((gateCtrl < DgcNoGating) || (gateCtrl > DgcLowEdgeGateN)) {
		return (apiCtrlProcessError(handle, DerrInvGateCtrl));
	}
	if ((cntSource < DcsTcnM1) || (cntSource > DcsF5)) {
		return (apiCtrlProcessError(handle, DerrInvCntSource));
	}

	if ((outputCtrl < DocInactiveLow) || (outputCtrl > DocLowTermCntPulse)) {
		return (apiCtrlProcessError(handle, DerrInvOutputCtrl));
	}

	err = daqIOWrite(handle, deviceType,
			 (DaqIODevicePort) ((Diodp9513Mode1 + ctrNum) - 1),
			 whichDevice, DioepP3,
		         (gateCtrl << 13) | ((cntEdge != 0) << 12) | 
			 (cntSource << 8) | ((specGate != 0) << 7) | 
			 ((reload !=0) << 6) | ((cntRepeat != 0) << 5) |
			 ((cntType != 0) << 4) | ((cntDir != 0) << 3) | 
			 outputCtrlArray[outputCtrl]);
	return err;
}


DAQAPI DaqError WINAPI
daq9513SetHold(DaqHandleT handle, DaqIODeviceType deviceType,
	       DWORD whichDevice, DWORD ctrNum, WORD ctrVal)
{
	DaqError err;

	if ((ctrNum < 1) || (ctrNum > 5)) {
		return (apiCtrlProcessError(handle, DerrInvCtrNum));
	}

	err = daqIOWrite(handle, deviceType,
			 (DaqIODevicePort) ((Diodp9513Hold1 + ctrNum) - 1),
			 whichDevice, DioepP3, ctrVal);

	return err;
}


DAQAPI DaqError WINAPI
daq9513SetLoad(DaqHandleT handle, DaqIODeviceType deviceType,
	       DWORD whichDevice, DWORD ctrNum, WORD ctrVal)
{
	DaqError err;

	if ((ctrNum < 1) || (ctrNum > 5)) {
		return (apiCtrlProcessError(handle, DerrInvCtrNum));
	}

	err = daqIOWrite(handle, deviceType,
			 (DaqIODevicePort) ((Diodp9513Load1 + ctrNum) - 1),
			 whichDevice, DioepP3, ctrVal);

	return err;
}



DAQAPI DaqError WINAPI
daq9513SetMasterMode(DaqHandleT handle, DaqIODeviceType deviceType,
		     DWORD whichDevice, DWORD foutDiv,
		     Daq9513CountSource cntSource, BOOL comp1, BOOL comp2, Daq9513TimeOfDay tod)
{
	DaqError err;

	if ((cntSource < DcsTcnM1) || (cntSource > DcsF5)) {
		return (apiCtrlProcessError(handle, DerrInvCntSource));
	}

	if ((tod < DtodDisabled) || (tod > DtodDivideBy10)) {
		return (apiCtrlProcessError(handle, DerrInvTod));
	}

	if (foutDiv > 15) {
		return (apiCtrlProcessError(handle, DerrInvDiv));
	}

	err = daqIOWrite(handle, deviceType, Diodp9513MasterMode,
			 whichDevice, DioepP3,
			 0xC000 | ((cntSource == 0) << 12) | 
			 (foutDiv << 8) | (cntSource << 4) | 
			 ((comp2 != 0) << 3) | ((comp1 != 0) << 2) | tod);

	return err;
}
