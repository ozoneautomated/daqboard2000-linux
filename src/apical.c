////////////////////////////////////////////////////////////////////////////
//
// apical.c                        I    OOO                           h
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

#define MaxGainsPerChan    9

#define DBK4GainsPerChan 13
#define CardsPerSystem  17
#define IDEALGAIN       0x8000
#define IDEALOFFSET     0x8000
#define MaxParams       4

#define NOCHANS         0xFFFE
#define GAINTOL         0x1000
#define OFFSETTOL       0x1000

static VOID freeDataStructMemory(DaqHandleT handle);

typedef struct {
	DcalType calType;
	WORD gain[MaxGainsPerChan];
	SHORT offset[MaxGainsPerChan];
} ChanCalConstT;

typedef struct {
	DcalType calType;
	WORD gainOut;
	SHORT offsetOut;
	WORD gainPga[4];
	SHORT offsetPga[4];
	WORD gainFilter[8];
	SHORT offsetFilter[8];
} Dbk4CalConstT;

typedef struct {
	DWORD nscan;
	DWORD readingsPos;
	DWORD nReadings;
	DcalType calType;
	DWORD chanGain;
	DWORD startChan;
	BOOL bipolar;
	BOOL noOffset;
} CalSetupT;

typedef struct {
	CalSetupT calSetup;
	VOID *chanCal[ChansPerSystem];
	DaqHardwareVersion deviceType;
} ApiCalT;

static ApiCalT session[MaxSessions];

VOID
apiCalMessageHandler(MsgPT msg)
{

	if (msg->errCode == DerrNoError) {
		switch (msg->msg) {
		case MsgAttach:

			{
				DWORD curChan, i;

				for (i = 0; i < MaxSessions; i++) {
					for (curChan = 0; curChan < ChansPerSystem; curChan++)
						session[i].chanCal[curChan] = NULL;

					freeDataStructMemory(i);
				}
			}

			break;

		case MsgDetach:

			{
				DWORD i;

				for (i = 0; i < MaxSessions; i++)
					freeDataStructMemory(i);
			}

			break;

		case MsgInit:
			freeDataStructMemory(msg->handle);
			session[msg->handle].deviceType = msg->deviceType;
			break;

		case MsgClose:
			freeDataStructMemory(msg->handle);
			break;
		}
	}

}

static VOID
freeDataStructMemory(DaqHandleT handle)
{

	DWORD curChan, remainingChans;

	for (curChan = 0; curChan < ChansPerSystem; curChan++) {
		if (session[handle].chanCal[curChan]) {
			iotFarfree(session[handle].chanCal[curChan]);

			for (remainingChans = curChan + 1;
			     remainingChans < ChansPerSystem; remainingChans++) {
				if (session[handle].chanCal[curChan] ==
				    session[handle].chanCal[remainingChans]) {

					session[handle].chanCal[remainingChans] = NULL;
				}
			}

			session[handle].chanCal[curChan] = NULL;
		}
	}

	session[handle].calSetup.nscan = 0;

	return;

}

VOID
setCalStructure(DaqHandleT handle, DWORD chan, VOID * calStruct)
{

	if (session[handle].chanCal[chan])
		iotFarfree(session[handle].chanCal[chan]);

	session[handle].chanCal[chan] = calStruct;

}

static VOID
removeSpaces(CHAR * str)
{
	DWORD i, j;

	for (i = 0, j = 0; i < strlen(str); i++) {

		if (str[i] == '#') {
			break;
		}

		if (str[i] != ' ') {
			str[j++] = str[i];
		}
	}
	str[j] = 0;

}

static BOOL
removeBrackets(CHAR * str)
{

	if (str[0] == '[') {
		DWORD i, j;

		for (i = 1, j = 0; (i < strlen(str)) && (str[i] != ']'); i++) {
			if (str[i] != ' ') {
				str[j++] = str[i];
			}
		}
		str[j] = 0;

		return TRUE;
	}

	return FALSE;

}

static BOOL
blankLine(CHAR * str)
{
	DWORD i;

	for (i = 0; i < strlen(str); i++) {
		if ((str[i] != ' ') && (str[i] != '\n') && (str[i] != '\r') && (str[i] != '\t')
		    ) {
			return FALSE;
		}
	}

	return TRUE;

}

static VOID
getParamStrings(CHAR * str, CHAR * strStart[], DWORD * strCount)
{

	DWORD i, strLen;

	*strCount = 0;

	strStart[*strCount] = str;
	(*strCount)++;

	strLen = strlen(str);
	for (i = 0; (i < strLen) && (*strCount < MaxParams); i++) {
		if (str[i] == ',') {
			str[i] = 0;
			strStart[*strCount] = &str[i + 1];
			(*strCount)++;
		}
	}

}

static int
getParams(CHAR * str, LONG params[])
{
	CHAR *strStart[MaxParams];
	DWORD strCount;
	BOOL bracketFlag;

	strlwr(str);
	removeSpaces(str);
	bracketFlag = removeBrackets(str);

	if (blankLine(str)) {
		return ParamBlankLine;
	}
	getParamStrings(str, strStart, &strCount);

	if (strCount > 0) {

		if (bracketFlag) {
			if (!strcmp(strStart[0], "main")) {

				params[0] = 0;
			} else if (!strncmp(strStart[0], "exp", 3)) {

				params[0] = iotAtol(&strStart[0][3]) + 1;
			} else if (!strncmp(strStart[0], "dbk", 3)) {
				if ((strCount < 4) ||
				    (strncmp(strStart[1], "exp", 3)) ||
				    (strncmp(strStart[2], "sub", 3)) ||
				    (strncmp(strStart[3], "ch", 2))
				    ) {

					return ParamTypeUnknown;
				}

				params[0] = iotAtol(&strStart[0][3]);

				params[1] = iotAtol(&strStart[1][3]) + 1;

				params[2] = iotAtol(&strStart[2][3]);

				params[3] = iotAtol(&strStart[3][2]);

				return ParamDbk4;
			} else {
				return ParamTypeUnknown;
			}

			if (strCount > 1) {
				if (!strncmp(strStart[1], "ch", 2)) {
					params[1] = iotAtol(&strStart[1][2]);

					return ParamTypeChannel;
				}
			} else {

				return ParamTypeBank;
			}
		} else {

			params[0] = iotAtol(strStart[0]);

			if (strCount > 1) {
				params[1] = iotAtol(strStart[1]);

				return ParamTypeGainOffset;
			}
		}
	}

	return ParamTypeUnknown;

}

static DaqError
calConvertStandard(DaqHandleT handle, WORD * counts, DWORD scans)
{

	DWORD i, thisScan, ch, calConstantIndex;
	LONG l;
	ChanCalConstT *calPtr;

	for (thisScan = 0; thisScan < scans; thisScan++) {
		for (i = session[handle].calSetup.readingsPos, ch =
		     session[handle].calSetup.startChan;
		     i <
		     session[handle].calSetup.nReadings +
		     session[handle].calSetup.readingsPos; i++, ch++) {

			if (session[handle].chanCal[ch]) {

				calPtr = (ChanCalConstT *) session[handle].chanCal[ch];
				l = counts[thisScan * session[handle].calSetup.nscan + i];
				l -= (session[handle].calSetup.bipolar ? 0x8000 : 0);

				switch (session[handle].deviceType) {
				case TempBook66:

					switch (session[handle].calSetup.chanGain) {
					case TgainX20:
						calConstantIndex = 4;
						break;
					case TgainX50:
						calConstantIndex = 5;
						break;
					case TgainX100:
						calConstantIndex = 6;
						break;
					case TgainX200:
						calConstantIndex = 7;
						break;
					default:
						calConstantIndex =
						    session[handle].calSetup.chanGain & 0x3;
					}
					break;

				default:

					calConstantIndex = session[handle].calSetup.chanGain & 0x3;
				}

				l *= calPtr->gain[calConstantIndex];

				l = l >> 15;

				if (!session[handle].calSetup.noOffset) {
					l += calPtr->offset[calConstantIndex];

				}

				if (session[handle].calSetup.calType == DcalTypeCJC) {
					if (session[handle].deviceType == TempBook66) {
						l += calPtr->offset[8];

					} else {
						l += calPtr->offset[4];
					}
				}

				l += (session[handle].calSetup.bipolar ? 0x8000 : 0);
				if (l < 0) {
					l = 0;
				}
				if (l > 65535l) {
					l = 65535l;
				}
				counts[thisScan * session[handle].calSetup.nscan + i] = (WORD) l;
			}
		}
	}

	return DerrNoError;

}

static DaqError
calConvertDbk4(DaqHandleT handle, WORD * counts, DWORD scans, BOOL filter)
{

	DWORD i, thisScan, ch, dataIndex;
	Dbk4CalConstT *calPtr;

	double m, b, tmpFloat, bipolarOffset;
	double gOut, gPga, gFil;
	double oOut, oPga, oFil;

	bipolarOffset = (session[handle].calSetup.bipolar ? 32768.0f : 0.0f);
	for (i = session[handle].calSetup.readingsPos, ch =
	     session[handle].calSetup.startChan;
	     i <
	     session[handle].calSetup.nReadings + session[handle].calSetup.readingsPos; i++, ch++) {

		if (session[handle].chanCal[ch]) {

			calPtr = (Dbk4CalConstT *) (session[handle].chanCal[ch]);
			if ((calPtr->calType != DcalDbk4Bypass)
			    && (calPtr->calType != DcalDbk4Filter)) {
				return apiCtrlProcessError(handle, DerrInvCalFile);
			}

			gOut = calPtr->gainOut;
			gPga = calPtr->gainPga[session[handle].calSetup.chanGain & 0x3];
			gFil = calPtr->gainFilter[apiDbk4GetMaxFreq(handle, ch)];
			oOut = calPtr->offsetOut;
			oPga = calPtr->offsetPga[session[handle].calSetup.chanGain & 0x3];
			oFil = calPtr->offsetFilter[apiDbk4GetMaxFreq(handle, ch)];

#ifdef CDQ_BLD
			oOut /= 2;
			oPga /= 2;
			oFil /= 2;
#endif

			if (filter) {
				m = (32768.0 / gPga) * (32768.0 / gOut) * (32768.0 / gFil);
				b = -(oOut + oFil * gOut * 1.583 / 32768.0 +
				      oPga * (gOut / 32768.0) * (gFil / 32768.0));
			} else {
				m = (32768.0 / gPga) * (32768.0 / gOut);
				b = -(oOut + oPga * gOut * 1.583 / 32768.0);
			}

			for (thisScan = 0; thisScan < scans; thisScan++) {

				dataIndex = thisScan * session[handle].calSetup.nscan + i;

				tmpFloat = counts[dataIndex];
				tmpFloat = m * (tmpFloat - bipolarOffset + b) + bipolarOffset;
				if (tmpFloat < 0) {
					tmpFloat = 0;
				}
				if (tmpFloat > 65535u) {
					tmpFloat = 65535u;
				}
				counts[dataIndex] = (WORD) (tmpFloat + 0.5);
			}
		}
	}

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqCalConvert(DaqHandleT handle, PWORD counts, DWORD scans)
{

	DaqError err;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return err;

	if (!session[handle].calSetup.nscan) {
		return apiCtrlProcessError(handle, DerrZCNoSetup);
	}

	switch (session[handle].calSetup.calType) {
	case DcalTypeDefault:
	case DcalTypeCJC:
		err = (DaqError) calConvertStandard(handle, counts, scans);
		break;
	case DcalDbk4Bypass:
		err = (DaqError) calConvertDbk4(handle, counts, scans, FALSE);
		break;
	case DcalDbk4Filter:
		err = (DaqError) calConvertDbk4(handle, counts, scans, TRUE);
		break;
	default:
		err = DerrNoError;
		break;
	}

	return err;

}

DAQAPI DaqError WINAPI
daqCalSetup(DaqHandleT handle, DWORD nscan, DWORD readingsPos,
	    DWORD nReadings, DcalType calType, DaqAdcGain chanGain,
	    DWORD startChan, BOOL bipolar, BOOL noOffset)
{

	DaqError err;
	BOOL invalidParam;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return err;

	switch (session[handle].deviceType) {
	case TempBook66:
		invalidParam = (nscan == 0) ||
		    (nscan > 512) ||
		    (readingsPos >= nscan) ||
		    (nReadings == 0) ||
		    (nReadings + readingsPos > nscan) ||
		    ((chanGain > TgainX100) &&
		     (chanGain != TgainX200)) || (startChan + nReadings > 19);
		break;

	default:

		invalidParam = (nscan == 0) ||
		    (nscan > 512) ||
		    (readingsPos >= nscan) ||
		    (nReadings == 0) ||
		    (nReadings + readingsPos > nscan) ||
		    (chanGain > 0x16) || (startChan + nReadings > 272);
		break;

	}

	if (invalidParam)
		return apiCtrlProcessError(handle, DerrZCInvParam);

	session[handle].calSetup.nscan = nscan;

	session[handle].calSetup.readingsPos = readingsPos;
	session[handle].calSetup.nReadings = nReadings;
	session[handle].calSetup.calType = calType;

	session[handle].calSetup.startChan = startChan;
	session[handle].calSetup.bipolar = bipolar;
	session[handle].calSetup.noOffset = noOffset;

	session[handle].calSetup.chanGain = chanGain;

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqCalSetupConvert(DaqHandleT handle, DWORD nscan, DWORD readingsPos,
		   DWORD nReadings, DcalType calType, DaqAdcGain chanGain,
		   DWORD startChan, BOOL bipolar, BOOL noOffset, PWORD counts, DWORD scans)
{

	DaqError err;

	err =
	    daqCalSetup(handle, nscan, readingsPos, nReadings, calType,
			chanGain, startChan, bipolar, noOffset);

	if (!err) {
		err = daqCalConvert(handle, counts, scans);
	}
	return err;

}

DAQAPI DaqError WINAPI
daqReadCalFile(DaqHandleT handle, CHAR * calfile)
{

	FILE *calfil;
	VOID *tmpCalStruct = NULL;
	CHAR calstr[128], fname[128];
	DWORD bank, chan, chanStart, chanEnd, gain;
	DcalType calType;
	LONG params[MaxParams];
	DWORD mallocSize;
	DaqError err;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return err;

	if ((calfile == NULL) || (calfile[0] == 0)) {
		switch (session[handle].deviceType) {
		case TempBook66:
			strcpy(fname, "tempbook.cal");
			break;
		default:
			strcpy(fname, "daqbook.cal");
		}
	} else {
		strcpy(fname, calfile);
	}

	calfil = (FILE *) iotOpenFile(fname, "rt");
	if (calfil == NULL) {
		return apiCtrlProcessError(handle, DerrInvCalFile);
	}

	freeDataStructMemory(handle);

	bank = NOCHANS;
	chan = NOCHANS;
	gain = 0;

	if (session[handle].deviceType == TempBook66) {
		tmpCalStruct = iotMalloc(sizeof (ChanCalConstT));
		if (!tmpCalStruct) {
			iotCloseFile((VOID *) calfil);
			return apiCtrlProcessError(handle, DerrMemAlloc);
		}
		bank = 0;
		calType = DcalTypeDefault;

		for (chan = 0; chan <= 19; chan++) {
			setCalStructure(handle, chan, tmpCalStruct);
		}

		for (gain = 0; gain < MaxGainsPerChan; gain++) {
			((ChanCalConstT *) tmpCalStruct)->gain[gain] = IDEALGAIN;
			((ChanCalConstT *) tmpCalStruct)->offset[gain] = 0;
		}

		gain = 0;
	}

	while (iotReadLineFile(calstr, sizeof (calstr), (VOID *) calfil)) {
		mallocSize = 0;

		switch (getParams(calstr, params)) {
		case ParamTypeBank:
			switch (session[handle].deviceType) {

			case TempBook66:
				if ((bank = params[0]) != 0) {
					return apiCtrlProcessError(handle, DerrInvCalFile);
				}
				chanStart = 0;
				chanEnd = 19;
				calType = DcalTypeDefault;
				mallocSize = 0;

				break;
			default:
				bank = params[0];
				chanStart = bank * 16;
				chanEnd = chanStart + 16 - 1;
				calType = DcalTypeDefault;
				mallocSize = sizeof (ChanCalConstT);
			}
			break;
		case ParamTypeChannel:
			bank = params[0];
			chanStart = bank * 16 + params[1];
			chanEnd = chanStart;
			calType = DcalTypeDefault;
			mallocSize = sizeof (ChanCalConstT);
			break;
		case ParamDbk4:
			bank = params[1];
			chanStart = bank * 16 + params[2] * 2 + params[3];
			chanEnd = chanStart;
			calType = DcalDbk4Filter;
			mallocSize = sizeof (Dbk4CalConstT);
			break;
		case ParamTypeGainOffset:
			switch (calType) {
			case DcalTypeDefault:
				if ((bank >= CardsPerSystem) ||
				    (gain >= MaxGainsPerChan) ||
				    (params[0] > IDEALGAIN + GAINTOL) ||
				    (params[0] < IDEALGAIN - GAINTOL) ||
				    (params[1] > IDEALOFFSET + OFFSETTOL) ||
				    (params[1] < IDEALOFFSET - OFFSETTOL)
				    ) {
					iotCloseFile((VOID *) calfil);
					return apiCtrlProcessError(handle, DerrInvCalFile);
				}
				((ChanCalConstT *) tmpCalStruct)->calType = DcalTypeDefault;
				((ChanCalConstT *) tmpCalStruct)->gain[gain] = (WORD) params[0];
				((ChanCalConstT *) tmpCalStruct)->offset[gain] =
				    (SHORT) (-params[1] + IDEALOFFSET);
				gain++;
				break;

			case DcalDbk4Bypass:
			case DcalDbk4Filter:
				if ((bank >= CardsPerSystem) ||
				    (gain >= DBK4GainsPerChan) ||
				    (params[0] > IDEALGAIN + GAINTOL) ||
				    (params[0] < IDEALGAIN - GAINTOL) ||
				    (params[1] > OFFSETTOL) || (params[1] < -OFFSETTOL)
				    ) {
					iotCloseFile((VOID *) calfil);
					return apiCtrlProcessError(handle, DerrInvCalFile);
				}
				((Dbk4CalConstT *) tmpCalStruct)->calType = DcalDbk4Filter;
				if (gain < 1) {
					((Dbk4CalConstT *) tmpCalStruct)->
					    gainOut = (WORD) params[0];

					((Dbk4CalConstT *) tmpCalStruct)->
					    offsetOut = (SHORT) params[1];
				} else if (gain < 5) {
					((Dbk4CalConstT *) tmpCalStruct)->
					    gainPga[gain - 1] = (WORD) params[0];
					((Dbk4CalConstT *) tmpCalStruct)->
					    offsetPga[gain - 1] = (SHORT) params[1];
				} else {
					((Dbk4CalConstT *) tmpCalStruct)->
					    gainFilter[gain - 5] = (WORD) params[0];
					((Dbk4CalConstT *) tmpCalStruct)->
					    offsetFilter[gain - 5] = (SHORT) params[1];

				}
				gain++;
				break;

			default:
				break;
			}
			break;

		case ParamBlankLine:
			break;

		default:
			return apiCtrlProcessError(handle, DerrInvCalFile);
		}

		if (mallocSize) {

			tmpCalStruct = iotMalloc(mallocSize);
			if (!tmpCalStruct) {
				iotCloseFile((VOID *) calfil);
				return apiCtrlProcessError(handle, DerrMemAlloc);
			}

			for (chan = chanStart; chan <= chanEnd; chan++) {
				setCalStructure(handle, chan, tmpCalStruct);
			}

			switch (calType) {
			case DcalTypeDefault:
				for (gain = 0; gain < MaxGainsPerChan; gain++) {
					((ChanCalConstT *) tmpCalStruct)->gain[gain] = IDEALGAIN;
					((ChanCalConstT *) tmpCalStruct)->offset[gain] = 0;
				}
				break;

			case DcalDbk4Bypass:
			case DcalDbk4Filter:
				for (gain = 0; gain < DBK4GainsPerChan; gain++) {
					if (gain < 1) {
						((Dbk4CalConstT *)
						 tmpCalStruct)->gainOut = IDEALGAIN;
						((Dbk4CalConstT *)
						 tmpCalStruct)->offsetOut = 0;
					} else if (gain < 5) {
						((Dbk4CalConstT *)
						 tmpCalStruct)->gainPga[gain - 1] = IDEALGAIN;
						((Dbk4CalConstT *)
						 tmpCalStruct)->offsetPga[gain - 1] = 0;
					} else {
						((Dbk4CalConstT *)
						 tmpCalStruct)->gainFilter[gain - 5] = IDEALGAIN;
						((Dbk4CalConstT *)
						 tmpCalStruct)->offsetFilter[gain - 5] = 0;
					}
				}
				break;
			default:
				break;
			}

			gain = 0;
		}
	}

	iotCloseFile((VOID *) calfil);

	return DerrNoError;

}

DAQAPI DaqError WINAPI  daqConfCalConstants(DaqHandleT handle, DWORD chan, DaqAdcGain gain,
                                            DaqAdcRangeT adcRange, PWORD gainConst,
                                            PSHORT offsetConst, DaqCalOperationT operation,
                                            DaqCalTableTypeT tableType, DaqCalInputT input,
                                            DaqCalOptionT option)
{

	DaqError err;
	daqIOT sb;
	CAL_CONFIG_PT calConfig = (CAL_CONFIG_PT) & sb;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return err;

    //MAH: 04.05.04
    if (!IsWaveBook(session[handle].deviceType)&&
             (!(IsDaq2000(session[handle].deviceType))||(IsDaq1000(session[handle].deviceType))))
    {
		return apiCtrlProcessError(handle, DerrNotCapable);
	}

	if (IsWaveBook(session[handle].deviceType)) {
		if (input < DciNormal || input > DciCal500mV)
			return apiCtrlProcessError(handle, DerrInvCalInput);

		if (tableType < DcttFactory || tableType > DcttUser)
			return apiCtrlProcessError(handle, DerrInvCalTableType);

		if (chan < 1 || chan > 72)
			return apiCtrlProcessError(handle, DerrInvChan);

		if (adcRange < DarUni0to10V || adcRange > DarBiMinus5to5V)
			return apiCtrlProcessError(handle, DerrInvAdcRange);
	}

	calConfig->tableType = tableType;
	calConfig->calInput = input;
	calConfig->channel = chan;
	calConfig->gain = gain;
	calConfig->option = (CalOptionT) option;
	calConfig->operation = (CalOperationT) operation;
	calConfig->adcRange = adcRange;
	calConfig->gainConstant = (*gainConst);
	calConfig->offsetConstant = (*offsetConst);

	err = itfExecuteCommand(handle, &sb, CAL_CONFIG);

	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	*gainConst = calConfig->gainConstant;
	*offsetConst = calConfig->offsetConstant;

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqCalSelectInputSignal(DaqHandleT handle, DaqCalInputT input)
{

	DaqError err;
	daqIOT sb;
	CAL_CONFIG_PT calConfig = (CAL_CONFIG_PT) & sb;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return err;

	if (!IsWaveBook(session[handle].deviceType)) {
		return apiCtrlProcessError(handle, DerrNotCapable);
	}
	if (input < DciNormal || input > DciCal500mV)
		return apiCtrlProcessError(handle, DerrInvCalInput);

	calConfig->calInput = input;
	calConfig->operation = CoSelectInput;

	err = itfExecuteCommand(handle, &sb, CAL_CONFIG);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqCalSelectCalTable(DaqHandleT handle, DaqCalTableTypeT tableType)
{

	DaqError err;
	daqIOT sb;
	CAL_CONFIG_PT calConfig = (CAL_CONFIG_PT) & sb;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return err;

	if (IsWaveBook(session[handle].deviceType)) {

		if (tableType < DcttFactory || tableType > DcttUser)
			return apiCtrlProcessError(handle, DerrInvCalTableType);

		calConfig->tableType = tableType;
		calConfig->operation = CoSelectTable;

		err = itfExecuteCommand(handle, &sb, CAL_CONFIG);
		if (err != DerrNoError)
			return apiCtrlProcessError(handle, err);
	} else if ((IsDaq2000(session[handle].deviceType)) || (IsDaq1000(session[handle].deviceType))) {

		if (tableType < DcttFactory || tableType > DcttBaseline)
			return apiCtrlProcessError(handle, DerrInvCalTableType);

		calConfig->tableType = tableType;
		calConfig->operation = CoSelectTable;

		calConfig->calInput = DciNormal;
		calConfig->channel = 0;
		calConfig->gain = DgainX1;
		calConfig->option = (CalOptionT) DcoptMainUnit;
		calConfig->adcRange = DarBiPolarDE;
		calConfig->gainConstant = 0;
		calConfig->offsetConstant = 0;

		err = itfExecuteCommand(handle, &sb, CAL_CONFIG);
		if (err != DerrNoError)
			return apiCtrlProcessError(handle, err);
	} else
		return apiCtrlProcessError(handle, DerrNotCapable);

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqCalSetConstants(DaqHandleT handle, DWORD channel, DaqAdcGain gain,
                           DaqAdcRangeT range, WORD gainConstant, SHORT offsetConstant)
{
	DaqError err;
	daqIOT sb;
	CAL_CONFIG_PT calConfig = (CAL_CONFIG_PT) & sb;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return err;

	if (!IsWaveBook(session[handle].deviceType)) {
		return apiCtrlProcessError(handle, DerrNotCapable);
	}
	if (channel < 1 || channel > 72)
		return apiCtrlProcessError(handle, DerrInvChan);

	if (range < DarUni0to10V || range > DarBiMinus5to5V)
		return apiCtrlProcessError(handle, DerrInvAdcRange);

	calConfig->channel = channel;
	calConfig->gain = gain;
	calConfig->adcRange = range;
	calConfig->gainConstant = gainConstant;
	calConfig->offsetConstant = offsetConstant;

	calConfig->option = CoptUserCal;

	calConfig->operation = CoSetConstants;

	err = itfExecuteCommand(handle, &sb, CAL_CONFIG);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqCalGetConstants(DaqHandleT handle, DWORD channel, DaqAdcGain gain,
                           DaqAdcRangeT range, PWORD gainConstant, PSHORT offsetConstant)
{

	DaqError err;
	daqIOT sb;
	CAL_CONFIG_PT calConfig = (CAL_CONFIG_PT) & sb;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return err;

	if (!IsWaveBook(session[handle].deviceType)) {
		return apiCtrlProcessError(handle, DerrNotCapable);
	}
	if (channel < 1 || channel > 72)
		return apiCtrlProcessError(handle, DerrInvChan);

	if (range < DarUni0to10V || range > DarBiMinus5to5V)
		return apiCtrlProcessError(handle, DerrInvAdcRange);

	calConfig->channel = channel;
	calConfig->gain = gain;
	calConfig->adcRange = range;

	calConfig->option = CoptUserCal;

	calConfig->operation = CoGetConstants;

	err = itfExecuteCommand(handle, &sb, CAL_CONFIG);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	*gainConstant =  calConfig->gainConstant;
	*offsetConstant = calConfig->offsetConstant;

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqCalSaveConstants(DaqHandleT handle, DWORD channel)
{

	DaqError err;
	daqIOT sb;
	CAL_CONFIG_PT calConfig = (CAL_CONFIG_PT) & sb;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return err;

	if (!IsWaveBook(session[handle].deviceType)) {
		return apiCtrlProcessError(handle, DerrNotCapable);
	}
	if (channel < 1 || channel > 72)
		return apiCtrlProcessError(handle, DerrInvChan);

	calConfig->channel = channel;
	calConfig->operation = CoSaveConstants;

	calConfig->option = CoptMainUnit;

	err = itfExecuteCommand(handle, &sb, CAL_CONFIG);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	return DerrNoError;

}
