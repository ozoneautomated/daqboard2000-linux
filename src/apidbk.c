////////////////////////////////////////////////////////////////////////////
//
// apidbk.c                        I    OOO                           h
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
/////////////////////////////////////////////////////////////////////////////
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
#include <memory.h>
#include "iottypes.h"

#include "daqx.h"
#include "cmnapi.h"
#include "api.h"
#include "ddapi.h"
#include "itfapi.h"

static VOID freeDataStructMemory(DaqHandleT handle);
static DaqError initializeWaveBook(DaqHandleT handle);

#define  Wbk512MaxAdChans         73
#define  Wbk512MainUnitMaxAdChans 8

#define Dac12BitLo 0
#define Dac12BitHi 4095

typedef struct {
	BYTE maxFreq;
	BYTE setBaseline;
	BYTE excitation;
	BYTE clock;
	DaqAdcGain gain;
} Dbk4OptionStructT;

typedef struct {
	BYTE slope;
	BYTE debounceTime;
	float minFreq;
	float maxFreq;
} Dbk7OptionStructT;

typedef struct {
	DaqAdcGain gain;
} Dbk50OptionStructT;

typedef struct {
	DaqAdcExpType chanType;
	DaqAdcExpType chanOptionType;
	BOOL changed;
	VOID *optionStruct;

} ChanOptionsT;

typedef struct {
	ChanOptionsT *chanOptions;

	DaqHardwareVersion gnDeviceType;
	DeviceHardwareInfoT gnDevHwareInf;
} ApiDbkT, *ApiDbkPT;

static ApiDbkT session[MaxSessions];

#define Dbk4ControlBit 0x08
#define Dbk4MsgSize 15
#define Dbk4DataLen 12
#define Dbk4ChanMsgSize 6
#define Dbk4ParitySeed 0x0f
#define Dbk4ParityMask 0x0f
#define Dbk4OutStageGain 1.583f
#define Dbk4OutStageCorrect 20700

#define SSHbit 0x40
#define G1bit 0x20
#define G0bit 0x10

static DaqError SetBaseUnitOption(DaqHandleT handle, DaqOptionType optionType, float optionValue);

extern DWORD AdcRawTransferCount(DaqHandleT handle);

VOID
apiDbkMessageHandler(MsgPT msg)
{
	ApiDbkPT pDbk;
	DWORD i, curChan;

	if (msg->errCode == DerrNoError) {
		switch (msg->msg) {
		case MsgAttach:
			for (i = 0; i < MaxSessions; i++)

                
				session[i].chanOptions = NULL;
			break;

		case MsgDetach:
			for (i = 0; i < MaxSessions; i++)
				freeDataStructMemory(i);
			break;

		case MsgInit:
			pDbk = &session[msg->handle];
			if (IsWaveBook(msg->deviceType)) {
				pDbk->chanOptions = (ChanOptionsT *)
				    malloc((Wbk512MaxAdChans + 1) * sizeof (ChanOptionsT));
				if (pDbk->chanOptions == NULL) {
					msg->errCode = DerrMemAlloc;
					return;
				}

				pDbk->gnDevHwareInf.ChannelsPerSys = Wbk512MaxAdChans;
				pDbk->gnDevHwareInf.MainUnitChans = Wbk512MainUnitMaxAdChans;

				for (curChan = 0; curChan < Wbk512MaxAdChans; curChan++)
					pDbk->chanOptions[curChan].optionStruct = NULL;

				freeDataStructMemory(msg->handle);

				for (i = 0; i <= Wbk512MainUnitMaxAdChans; i++) {
					pDbk->chanOptions[i].chanType = DmctWbk512;
					pDbk->chanOptions[i].changed = TRUE;
				}
				msg->errCode = initializeWaveBook(msg->handle);
				{
					const DaqAdcExpType typ = pDbk->chanOptions[1].chanType;
					pDbk->gnDeviceType =
					    (typ == DmctWbk512) ? WaveBook512 :
					    (typ ==
					     DmctWbk512_10V) ? WaveBook512_10V
					    : (typ ==
					       DmctWbk516) ? WaveBook516 : (typ == DmctWbk512A)
					    ? WaveBook512A : (typ ==
							      DmctWbk516A) ?
					    WaveBook516A : (typ ==
							    DmctWbk516_250) ?
					    WaveBook516_250 : WaveBook512;

					msg->deviceType = pDbk->gnDeviceType;
				}

			} else /* !IsWaveBook */ {
				session[msg->handle].gnDeviceType = msg->deviceType;
				if ((IsDaq2000(msg->deviceType)) || (IsDaq1000(msg->deviceType))){
					daqIOT sb;
					ADC_HWINFO_PT infoRequest = (ADC_HWINFO_PT) & sb;
					infoRequest->chan = 0;
					infoRequest->chanType = 0;
					infoRequest->option = 0;
					infoRequest->dtEnum = AhiChanType;
                    
					if (itfExecuteCommand
					    (msg->handle, &sb, ADC_HWINFO) == DerrNoError)
                        {
						if ((DaqBoard2000 <= (DaqHardwareVersion)
						     infoRequest->chanType)
						    && ((DaqHardwareVersion)
							infoRequest->chanType <= DaqBoard2005))
                        {
							session[msg->handle].gnDeviceType =
							    msg->deviceType = (DaqHardwareVersion)
							    infoRequest->chanType;
						}
					}
				}
				session[msg->handle].chanOptions =
				    (ChanOptionsT *) malloc(272 * sizeof (ChanOptionsT));
				session[msg->handle].gnDevHwareInf.ChannelsPerSys = 272L;
				session[msg->handle].gnDevHwareInf.MainUnitChans = 16;
                //printf("1 Setting it all to null\n");
				for (curChan = 0; curChan < 272L; curChan++)
					session[msg->handle].
					    chanOptions[curChan].optionStruct = NULL;
				freeDataStructMemory(msg->handle);
//				break;
			}
			break;

		case MsgClose:
			freeDataStructMemory(msg->handle);
			free(session[msg->handle].chanOptions);
			session[msg->handle].chanOptions = NULL;
			break;

		default:
			break;
		}
	}
	return;
}

static DaqError
initializeWaveBook(DaqHandleT handle)
{

	DaqError err;

	DWORD chan;
	daqIOT sb;
	ADC_HWINFO_PT infoRequest = (ADC_HWINFO_PT) & sb;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	for (chan = 0; chan <= Wbk512MaxAdChans; chan++) {

		infoRequest->chan = chan;
		infoRequest->chanType = 0;
		infoRequest->option = 0;
		infoRequest->dtEnum = AhiChanType;
		err = itfExecuteCommand(handle, &sb, ADC_HWINFO);
		if (err != DerrNoError)
			return err;
		session[handle].chanOptions[chan].chanType = (DaqAdcExpType) infoRequest->chanType;
		session[handle].chanOptions[chan].changed = TRUE;

		if (infoRequest->chanType != DaetNotDefined) {
			infoRequest->option = 1;
			err = itfExecuteCommand(handle, &sb, ADC_HWINFO);

			if (err != DerrNoError)
				return err;
			session[handle].chanOptions[chan].chanOptionType =
			    (DaqAdcExpType) infoRequest->chanType;
		} else {
			session[handle].chanOptions[chan].chanOptionType = DaetNotDefined;
		}
	}
	return err;

}

static DWORD
getChannelsPerCard(DaqAdcExpType chanType)
{
	switch (chanType) {
	case DaetDbk4:
		return 2;
	case DaetDbk7:
	case DaetDbk2:
	case DaetDbk5:
		return 4;
	case DaetDbk50:
		return 8;
	default:
		break;
	}
	return 1;
}

static DaqAdcExpType
apiGetChanType(DaqHandleT handle, DWORD chan)
{
	return (chan >= session[handle].gnDevHwareInf.ChannelsPerSys) 
		? DaetNotDefined 
		: session[handle].chanOptions[chan].chanType;
}

static DWORD
getStartChan(DWORD chan, DaqAdcExpType chanType)
{

	return (chan & ~(getChannelsPerCard(chanType) - 1));

}

static DWORD
getEndChan(DWORD chan, DaqAdcExpType chanType)
{

	return (chan | (getChannelsPerCard(chanType) - 1));

}

static VOID
deallocateBank(DaqHandleT handle, DWORD startChan, DWORD endChan)
{

	DWORD chan;

	if (session[handle].chanOptions[startChan].optionStruct)
		iotFarfree(session[handle].chanOptions[startChan].optionStruct);

	for (chan = startChan; chan <= endChan; chan++) {
		switch (apiGetChanType(handle, chan)) {
		case DaetDbk2:
		case DaetDbk5:
			apiDacSetAvail(handle, DddtDbk, chan, DacAvailNone);
			break;

		default:
			break;
		}
        //printf("2 Setting it all to null\n");
		session[handle].chanOptions[chan].optionStruct = NULL;
		session[handle].chanOptions[chan].changed = FALSE;
		session[handle].chanOptions[chan].chanType = DaetNotDefined;
	}
	return;
}

static DaqError
allocateBank(DaqHandleT handle, DWORD startChan, DWORD endChan, DaqAdcExpType chanType)
{
	BYTE *tmpBuf;
	DWORD chanCount = endChan - startChan + 1;
	DWORD chan;
	DWORD size;

	switch (chanType) {
	case DaetDbk4:
		size = sizeof (Dbk4OptionStructT);
		break;
	case DaetDbk7:
		size = sizeof (Dbk7OptionStructT);
		break;
	case DaetDbk50:
		size = sizeof (Dbk50OptionStructT);
		break;
	case DaetDbk2:
	case DaetDbk5:
		size = 0;
		break;
	case DaetNotDefined:
	default:
		return DerrNoError;
	}

	if (size) {
		if ((tmpBuf = (PBYTE) iotMalloc(chanCount * size)) == NULL)
			return DerrMemAlloc;
	}

	for (chan = startChan; chan <= endChan; chan++) {
		if (size) {
			session[handle].chanOptions[chan].optionStruct =
			    &tmpBuf[(chan - startChan) * size];
		} else {
            printf("3 Setting it all to null\n");
			session[handle].chanOptions[chan].optionStruct = NULL;
		}
		session[handle].chanOptions[chan].changed = FALSE;
		session[handle].chanOptions[chan].chanType = chanType;

		switch (apiGetChanType(handle, chan)) {
		case DaetDbk2:
		case DaetDbk5:
			apiDacSetAvail(handle, DddtDbk, chan, DacAvailVoltage);
			break;

		default:
			break;
		}
	}

	return DerrNoError;

}

static VOID
freeDataStructMemory(DaqHandleT handle)
{

	DaqAdcExpType oldChanType;
	DWORD oldStartChan, oldEndChan;
	DWORD curChan;

	curChan = 0;

	if (session[handle].chanOptions) {

		while (curChan < session[handle].gnDevHwareInf.ChannelsPerSys) {
			oldChanType = apiGetChanType(handle, curChan);
			oldStartChan = getStartChan(curChan, oldChanType);
			oldEndChan = getEndChan(curChan, oldChanType);

			deallocateBank(handle, oldStartChan, oldEndChan);

			curChan = oldEndChan + 1;
		}
	}

}

VOID *
apiGetChanOption(DaqHandleT handle, DWORD chan)
{
    //MAH: WHAT THE HELL IS THIS

/*
typedef struct {
	DaqAdcExpType chanType;
	DaqAdcExpType chanOptionType;
	BOOL changed;
	VOID *optionStruct;

} ChanOptionsT;

typedef struct {
	ChanOptionsT *chanOptions;

	DaqHardwareVersion gnDeviceType;
	DeviceHardwareInfoT gnDevHwareInf;
} ApiDbkT, *ApiDbkPT;
*/
    
        if (&session[handle].chanOptions[chan] == NULL)
        {
            printf("BLOWN POINTER ! GetChanOP handle %i chan %i\n",handle,chan);
        }
        else
        {
        //    printf("Its up Its good!\n");

        }
        
    
	return session[handle].chanOptions[chan].optionStruct;

}

static DaqError
dbk50SendChanOptions(DaqHandleT handle, DWORD chan, Dbk50OptionStructT * optionStruct)
{

	BYTE data[1];
	BYTE DBK50address, DBK50channel;

	DBK50address = (BYTE) ((chan / 16) - 1);
	DBK50channel = (BYTE) (chan % 16);

	data[0] = DBK50channel | (optionStruct->gain << 4);

	apiAdcSendSmartDbkData(handle, DBK50address, 0, data, 1);

	return DerrNoError;

}

DaqError
apiDbkDacWt(DaqHandleT handle, DWORD chan, WORD * values, DWORD count)
{
	BYTE i, data[17], startChan, cardAddr, subAddr, j;
	DWORD dataCount = 0;
	DaqAdcExpType chanType;
	DaqError err;

	chanType = apiGetChanType(handle, chan);
	startChan = (BYTE) (0x03 & chan);
	subAddr = (BYTE) (0x03 & (chan >> 2));
	cardAddr = (BYTE) (0x0F & ((chan - 16) >> 4));

	data[dataCount++] = subAddr;

	for (i = 0, j = 0; i < 4; i++) {
		WORD value;

		if (i >= startChan && j < count) {
			switch (chanType) {
			case DaetDbk2:
				value = values[j++] >> 2;
				break;
			case DaetDbk5:
				value = values[j++] >> 4;
				break;
			default:
				return DerrInvDacChan;
			}
		} else {
			value = 0xFFFF;
		}

		data[dataCount++] = (BYTE) (0x0F & (value >> 12));
		data[dataCount++] = (BYTE) (0x0F & (value >> 8));
		data[dataCount++] = (BYTE) (0x0F & (value >> 4));
		data[dataCount++] = (BYTE) (0x0F & value);

	}

	err = apiAdcSendSmartDbkData(handle, cardAddr, 0, data, dataCount);

	return err;
}

BYTE
apiDbk4GetMaxFreq(DaqHandleT handle, DWORD chan)
{

	return ((Dbk4OptionStructT *) (session[handle].chanOptions[chan].optionStruct))->maxFreq;

}

static DaqError
dbk4SendChanOptions(DaqHandleT handle, DWORD chan, DWORD * delay)
{

	DWORD dataCount = 0, i;
	BYTE data[16];
	DWORD chan0, chan1;
	Dbk4OptionStructT *optionStruct0, *optionStruct1;
	BYTE bankAddress, parity;
	DaqError err;

	bankAddress = SSHbit | (BYTE) ((chan >> 4) - 1);

	chan0 = getStartChan(chan, DaetDbk4);
	chan1 = getEndChan(chan, DaetDbk4);

	optionStruct0 = (Dbk4OptionStructT *) apiGetChanOption(handle, chan0);
	optionStruct1 = (Dbk4OptionStructT *) apiGetChanOption(handle, chan1);

	data[dataCount++] = (BYTE) ((chan0 >> 1) & 0x07);
	data[dataCount++] = Dbk4ControlBit | optionStruct0->maxFreq;
	data[dataCount++] = Dbk4ControlBit | (optionStruct0->gain & 0x03);
	data[dataCount++] = Dbk4ControlBit | optionStruct0->setBaseline;
	data[dataCount++] = Dbk4ControlBit | optionStruct1->maxFreq;
	data[dataCount++] = Dbk4ControlBit | (optionStruct1->gain & 0x03);
	data[dataCount++] = Dbk4ControlBit | optionStruct1->setBaseline;
	data[dataCount++] = Dbk4ControlBit | optionStruct0->excitation;
	data[dataCount++] = Dbk4ControlBit | optionStruct0->clock;
	data[dataCount++] = Dbk4ControlBit;

	parity = Dbk4ParitySeed;
	parity ^= bankAddress;
	for (i = 0; i < dataCount; i++)
		parity ^= data[i];

	data[dataCount++] = parity & Dbk4ParityMask;
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;

	if ((optionStruct0->setBaseline == DcovDbk4BaselineOneShot)
	    || (optionStruct1->setBaseline == DcovDbk4BaselineOneShot))
		if (*delay < 300)
			*delay = 300;

	if (optionStruct0->setBaseline == DcovDbk4BaselineOneShot)
		optionStruct0->setBaseline = DcovDbk4BaselineNever;

	if (optionStruct1->setBaseline == DcovDbk4BaselineOneShot)
		optionStruct1->setBaseline = DcovDbk4BaselineNever;

	err = apiAdcSendSmartDbkData(handle, bankAddress, Dbk4DataLen, data, dataCount);

	return err;

}

static VOID
floatToDbk7Float(float fValue, BYTE bits, PDWORD mantissa, PCHAR exponent)
{
	float norm;
	norm = 1.0f;
	while (bits--){
		norm *= 2.0f;
	}

	*exponent = 0;
	if (fValue != 0.0){
		if (fValue >= norm){
			while (fValue >= norm) {
				fValue /= 2.0f;
				(*exponent)++;
			} 
		}else{
			while (fValue * 2.0f < norm) {
				fValue *= 2.0f;
				(*exponent)--;
			}
		}
	}
	*mantissa = (unsigned long) fValue;
}

static DaqError
dbkReadInput(DaqHandleT handle, DWORD chan, DaqAdcGain gain, PWORD avg)
{
#define        ReadInputCount    256
	DWORD *chanBuf = (PDWORD) malloc(ReadInputCount * sizeof (DWORD));
	DaqAdcGain *gainBuf = (DaqAdcGain *) malloc(ReadInputCount * sizeof (DWORD));
	DWORD *flagsBuf = (PDWORD) malloc(ReadInputCount * sizeof (DWORD));
	WORD *buf = (PWORD) malloc(ReadInputCount * sizeof (WORD));

	DWORD loopCount = 1;
	DaqError err;
	DWORD i, j;
	DWORD sum = 0;
	DWORD active, retCount;
	DaqAdcClockSource savedClockSource;
	float savedFreq;

	err = daqGetInfo(handle, 0, DdiAdcClockSource, &savedClockSource);

	err = daqAdcGetFreq(handle, &savedFreq);

	for (i = 0; i < ReadInputCount; i++) {
		chanBuf[i] = chan;
		gainBuf[i] = gain;
		flagsBuf[i] = DafAnalog | DafBipolar | DafUnsigned | DafSingleEnded;
	}

	err = daqAdcSetScan(handle, chanBuf, gainBuf, flagsBuf, ReadInputCount);

	if (!err)
		err = daqAdcSetAcq(handle, DaamNShot, 0, loopCount);

    //MAH: 04.05.04 added 1000
    if (!(IsDaq2000(session[handle].gnDeviceType) || IsDaq1000(session[handle].gnDeviceType)))
    {
		if (!err)
			err = daqAdcSetClockSource(handle, DacsTriggerSource);

		if (!err)
			err = daqAdcSetTrig(handle, DatsSoftware, 0, 0, 0, 0);
	} else {
		if (!err)
			err = daqAdcSetClockSource(handle, DacsAdcClockDiv2);

		if (!err)
			err = daqAdcSetFreq(handle, 0.1f);

		if (!err)
			err = daqAdcSetTrig(handle, DatsImmediate, 0, 0, 0, 0);
	}

	if (!err)
		err = daqAdcSetDiskFile(handle, "", DaomCreateFile, 0);

	if (!err)
		err = daqAdcTransferSetBuffer(handle, buf, 1, DatmCycleOff | DatmUpdateSingle);
	if (!err) {
		for (j = 0; j < loopCount; j++) {

			if (!err)
				err = daqAdcTransferStart(handle);
			else
				break;

			if (!err)
				err = daqAdcArm(handle);
			else
				break;

			//MAH: 04.05.04 added 1000
            if (!(IsDaq2000(session[handle].gnDeviceType) || IsDaq1000(session[handle].gnDeviceType))) {

				err = daqAdcSoftTrig(handle);
			}

			if (!err)
				err = daqWaitForEvent(handle, DteAdcData);

			else
				break;

			err = daqAdcDisarm(handle);

			err = daqAdcTransferStop(handle);

			if (err != DerrNoError)
				dbg("DAQX:dbkReadInput after daqWaitForEvent with error %d",
				     err);

			if (!err) {
				err = daqAdcTransferGetStat(handle, &active, &retCount);

				if (err != DerrNoError)
					dbg("DAQX:dbkReadInput after daqAdcTransferGetStat with error %d",
					     err);

				if (!err) {
					for (i = 0; i < ReadInputCount; i++) {
						sum += buf[i];
					}
				} else
					break;
			}
		}
	}
	if (!err)
		*avg = (WORD) (sum / ReadInputCount / loopCount);
	free(chanBuf);
	free(gainBuf);
	free(flagsBuf);
	free(buf);

	daqAdcSetClockSource(handle, savedClockSource);

	err = daqAdcSetFreq(handle, savedFreq);

	return err;

}

DaqError
dbk7GetRevision(DaqHandleT handle, DWORD chan, PWORD revision)
{

	DWORD dataCount = 0;
	BYTE data[9];
	float maxADVolts;
	DaqAdcGain gainFlag;

	data[dataCount++] = (BYTE) (chan & 0x0C);
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;
	data[dataCount++] = G0bit | 0x08 | (BYTE) (chan & 0x03);
	data[dataCount++] = G0bit;
	data[dataCount++] = 0x00;

	apiAdcSendSmartDbkData(handle, (chan >> 4) - 1, 0, data, dataCount);

	daqGetInfo(handle, 0, DdiADmaxInfo, &maxADVolts);

	gainFlag = (maxADVolts > 5.0 ? Dbk13X2 : DgainX1);

	return dbkReadInput(handle, chan, gainFlag, revision);

}

VOID
dbk7SetCalibration(DaqHandleT handle, DWORD chan, WORD mainDac, BYTE offsetDac, BYTE gainDac)
{

	DWORD dataCount = 0;
	BYTE data[19];

	data[dataCount++] = (BYTE) (chan & 0x0C);
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;
	data[dataCount++] = G0bit | 0x04 | (BYTE) (chan & 0x03);
	data[dataCount++] = G0bit;
	data[dataCount++] = G0bit | ((mainDac >> 12) & 0x0F);
	data[dataCount++] = G0bit | ((mainDac >> 8) & 0x0F);
	data[dataCount++] = G0bit | ((mainDac >> 4) & 0x0F);
	data[dataCount++] = G0bit | (mainDac & 0x0F);
	data[dataCount++] = G0bit | (offsetDac >> 4);
	data[dataCount++] = G0bit | (offsetDac & 0x0F);
	data[dataCount++] = G0bit | (gainDac >> 4);
	data[dataCount++] = G0bit | (gainDac & 0x0F);
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;

	apiAdcSendSmartDbkData(handle, (chan >> 4) - 1, 0, data, dataCount);

}

static DaqError
dbk7AutoCal(DaqHandleT handle, DWORD startChan, DWORD endChan)
{

	DWORD chan;
	WORD reading;
	WORD mainDac;
	BYTE offsetDac, gainDac;
	float m, b, y1, y2, x1, x2, f;
	DaqError err;
	float maxADVolts;
	DaqAdcGain gainFlag;

	err = daqGetInfo(handle, 0, DdiADmaxInfo, &maxADVolts);

	gainFlag = (maxADVolts > 5.0f ? Dbk13X2 : DgainX1);

	for (chan = startChan; chan <= endChan; chan++) {
		offsetDac = 128;
		gainDac = 128;

		mainDac = 64;
		dbk7SetCalibration(handle, chan, mainDac, offsetDac, gainDac);
		err = dbkReadInput(handle, chan, gainFlag, &reading);
		if (err != DerrNoError)
			return err;
		x1 = (float) mainDac;
		y1 = (float) reading;

		mainDac = 4032;
		dbk7SetCalibration(handle, chan, mainDac, offsetDac, gainDac);
		err = dbkReadInput(handle, chan, gainFlag, &reading);
		if (err != DerrNoError)
			return err;
		x2 = (float) mainDac;
		y2 = (float) reading;

		m = (y1 - y2) / (x1 - x2);
		b = (x1 * y2 - x2 * y1) / (x1 - x2);

		f = -(80.0f * (16.0f - m) / 0.1f);
		if (f < -128) {
			f = -128.0f;
		}
		if (f > 127) {
			f = 127.0f;
		}
		gainDac += (CHAR) f;

		f = -(10.0f * (b / 512.0f + 4.0f * (16.0f - m)) / 0.1f);
		if (f < -128) {
			f = -128.0f;
		}
		if (f > 127) {
			f = 127.0f;
		}
		offsetDac += (CHAR) f;

		mainDac = 0;

		dbk7SetCalibration(handle, chan, mainDac, offsetDac, gainDac);
	}

	return DerrNoError;

}

static DaqError
dbk7SendChanOptions(DaqHandleT handle, DWORD chan, Dbk7OptionStructT * optionStruct)
{

	DWORD integrationTime;
	DWORD dacRangeMantissa, freqOffsetMantissa;
	CHAR dacRangeExponent, freqOffsetExponent;
	DWORD dataCount = 0;
	BYTE data[29];

	DaqError err;

	if ((optionStruct->minFreq == optionStruct->maxFreq)
	    || (optionStruct->minFreq > optionStruct->maxFreq * 0.99))
		return DerrInvOptionValue;

	integrationTime =
	    (DWORD) (4096.0f * optionStruct->maxFreq /
		     (optionStruct->maxFreq - optionStruct->minFreq));
	floatToDbk7Float(8192000000.0f /
			 (optionStruct->maxFreq - optionStruct->minFreq), 16,
			 &dacRangeMantissa, &dacRangeExponent);
	floatToDbk7Float(optionStruct->minFreq / 2000000.0f, 24,
			 &freqOffsetMantissa, &freqOffsetExponent);

	data[dataCount++] = (BYTE) (chan & 0x0C);
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;
	data[dataCount++] = 0x00;
	data[dataCount++] = G0bit | (BYTE) (chan & 0x03);
	data[dataCount++] = G0bit | (optionStruct->slope << 3) | optionStruct->debounceTime;
	data[dataCount++] = G0bit | (BYTE) ((integrationTime >> 20) & 0x0F);
	data[dataCount++] = G0bit | (BYTE) ((integrationTime >> 16) & 0x0F);
	data[dataCount++] = G0bit | (BYTE) ((integrationTime >> 12) & 0x0F);
	data[dataCount++] = G0bit | (BYTE) ((integrationTime >> 8) & 0x0F);
	data[dataCount++] = G0bit | (BYTE) ((integrationTime >> 4) & 0x0F);
	data[dataCount++] = G0bit | (BYTE) (integrationTime & 0x0F);
	data[dataCount++] = G0bit | (BYTE) ((dacRangeMantissa >> 12) & 0x0F);
	data[dataCount++] = G0bit | (BYTE) ((dacRangeMantissa >> 8) & 0x0F);
	data[dataCount++] = G0bit | (BYTE) ((dacRangeMantissa >> 4) & 0x0F);
	data[dataCount++] = G0bit | (BYTE) (dacRangeMantissa & 0x0F);
	data[dataCount++] = G0bit | (BYTE) ((dacRangeExponent >> 4) & 0x0F);
	data[dataCount++] = G0bit | (BYTE) (dacRangeExponent & 0x0F);

	data[dataCount++] = G0bit | (BYTE) ((freqOffsetMantissa >> 20) & 0x0F);
	data[dataCount++] = G0bit | (BYTE) ((freqOffsetMantissa >> 16) & 0x0F);
	data[dataCount++] = G0bit | (BYTE) ((freqOffsetMantissa >> 12) & 0x0F);
	data[dataCount++] = G0bit | (BYTE) ((freqOffsetMantissa >> 8) & 0x0F);
	data[dataCount++] = G0bit | (BYTE) ((freqOffsetMantissa >> 4) & 0x0F);
	data[dataCount++] = G0bit | (BYTE) (freqOffsetMantissa & 0x0F);
	data[dataCount++] = G0bit | ((freqOffsetExponent >> 4) & 0x0F);
	data[dataCount++] = G0bit | (freqOffsetExponent & 0x0F);
	data[dataCount++] = 0x00;

	err = apiAdcSendSmartDbkData(handle, (chan >> 4) - 1, 0, data, dataCount);
	if (err != DerrNoError)
		return err;

	return DerrNoError;

}

static unsigned long
getmSec(VOID)
{
	return GetTickCount() % 86400000L;
}

DaqError
apiDbkSendChanOptions(DaqHandleT handle, PDWORD chans, DaqAdcGain * gains, DWORD count)
{

	DWORD chan;
	VOID *optionStruct;
	DaqAdcGain gain;
	DaqError err;
	DWORD delay = 0;
   
	for (chan = 0; chan < count; chan++) {
      
		optionStruct = apiGetChanOption(handle, chans[chan]);
       
		switch (apiGetChanType(handle, chans[chan])) {
		case DaetDbk4:
			gain = ((Dbk4OptionStructT *) (optionStruct))->gain;
			if (gains[chan] != gain)
				daqAdcExpSetChanOption(handle, chans[chan],
						       DcotDbk4Gain, (float) gains[chan]);
			break;
		case DaetDbk50:
			gain = ((Dbk50OptionStructT *) (optionStruct))->gain;
			if (gains[chan] != gain)
				daqAdcExpSetChanOption(handle, chans[chan],
						       DcotDbk50Gain, (float) gains[chan]);
			break;
			
		default:
			break;
		}
	}
    
	for (chan = 0; chan < session[handle].gnDevHwareInf.ChannelsPerSys; chan++) {
		optionStruct = apiGetChanOption(handle, chan);
		if (session[handle].chanOptions[chan].changed) {
			err = DerrNoError;

			switch (apiGetChanType(handle, chan)) {
			case DaetDbk4:
				err = dbk4SendChanOptions(handle, chan, &delay);
				break;
			case DaetDbk7:
				err = dbk7SendChanOptions(handle, chan, (Dbk7OptionStructT *)
							  optionStruct);
				break;
			case DaetDbk50:
				err = dbk50SendChanOptions(handle, chan, (Dbk50OptionStructT *)
							   optionStruct);
				break;
			default:
				break;
			}

			if (err != DerrNoError)
				return err;

			session[handle].chanOptions[chan].changed = FALSE;
		}
	}

	if (delay) {
		DWORD t1 = getmSec();

		if (t1 < 43200000L) {
			while (getmSec() < t1 + delay) {
			};
		} else {
			while (getmSec() - delay < t1) {
			};
		}
	}
   
	return DerrNoError;                                                                           

}

DaqError
getWbkChanOption(DaqHandleT handle, DWORD chan, DaqOptionType optionType, VOID * optionValue)
{

	DaqError err;
	daqIOT sb;
	ADC_CHAN_OPT_PT adcChanOpt = (ADC_CHAN_OPT_PT) & sb;

	adcChanOpt->startChan = chan;
	adcChanOpt->endChan = chan;
	adcChanOpt->chanOptType = (DWORD) optionType;
	adcChanOpt->optValue = (DWORD) optionValue;
	adcChanOpt->operation = 0;
	err = itfExecuteCommand(handle, &sb, ADC_CHAN_OPT);
	return err;

}

DaqError
setWbk12Options(DaqHandleT handle, DWORD chan, DWORD moduleOption,
		DaqOptionType optionType, float optionValue)
{

	DaqError err;
	DWORD startChan, chansInBank;
	daqIOT sb;
	ADC_CHAN_OPT_PT adcChanOpt = (ADC_CHAN_OPT_PT) & sb;

#define wbk12CheckOptionValue(lowLimit, highLimit)\
       { if ( (optionValue > highLimit) || (optionValue < lowLimit) ) \
            return DerrInvOptionValue; \
       }

	if (!moduleOption) {
		switch (optionType) {
		case DcotWbk12FilterMode:
			wbk12CheckOptionValue(DcovWbk12FilterBypass, DcovWbk12FilterOn);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk12FilterMode;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk12PreFilterMode:
			wbk12CheckOptionValue(DcovWbk12PreFilterDefault, DcovWbk12PreFilterOff);
			chansInBank = 4;

			if (!(chan % chansInBank))
				startChan = chan - chansInBank + 1;
			else
				startChan = chan / chansInBank * chansInBank + 1;

			adcChanOpt->startChan = startChan;
			adcChanOpt->endChan = startChan + 3;
			adcChanOpt->chanOptType = (DWORD) DcotWbk12PreFilterMode;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk12FilterCutOff:
			wbk12CheckOptionValue(400.0f, 100000.0f);
			chansInBank = 4;

			if (!(chan % chansInBank))
				startChan = chan - chansInBank + 1;
			else
				startChan = chan / chansInBank * chansInBank + 1;

			adcChanOpt->startChan = startChan;
			adcChanOpt->endChan = startChan + 3;
			adcChanOpt->chanOptType = (DWORD) DcotWbk12FilterCutOff;
			adcChanOpt->optValue = (DWORD) (optionValue * 1000.0);
			break;

		case DcotWbk12FilterType:
			wbk12CheckOptionValue(DcovWbk12FilterElliptic, DcovWbk12FilterLinear);
			chansInBank = 4;

			if (!(chan % chansInBank))
				startChan = chan - chansInBank + 1;
			else
				startChan = chan / chansInBank * chansInBank + 1;

			adcChanOpt->startChan = startChan;
			adcChanOpt->endChan = startChan + 3;
			adcChanOpt->chanOptType = (DWORD) DcotWbk12FilterType;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk12PreFilterCutOff:
			wbk12CheckOptionValue(DcovWbk12PreFilterCutOffDefault, 1E6f);

			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk12PreFilterCutOff;
			adcChanOpt->optValue = (DWORD) (optionValue * 1000);
			break;

		case DcotWbk12PostFilterCutOff:
			switch (session[handle].chanOptions[chan].chanOptionType) {
			case DoctWbk12A:
			case DoctWbk13A:
				wbk12CheckOptionValue(DcovWbk12PreFilterCutOffDefault, 1E6F);
				chansInBank = 4;

				if (!(chan % chansInBank))
					startChan = chan - chansInBank + 1;
				else
					startChan = chan / chansInBank * chansInBank + 1;

				adcChanOpt->startChan = startChan;
				adcChanOpt->endChan = startChan + 3;
				adcChanOpt->chanOptType = (DWORD) DcotWbk12PostFilterCutOff;
				adcChanOpt->optValue = (DWORD) (optionValue * 1000);
				break;
			default:
				return DerrInvOptionType;
			}
			break;

		default:
			return DerrInvOptionType;
		}

	} else {
		return DerrInvOptionType;
	}
	adcChanOpt->operation = 1;
	err = itfExecuteCommand(handle, &sb, ADC_CHAN_OPT);
	return err;

}

DaqError
setPga516Options(DaqHandleT handle, DWORD chan, DWORD moduleOption,
		 DaqOptionType optionType, float optionValue)
{

	DaqError err;
	daqIOT sb;
	ADC_CHAN_OPT_PT adcChanOpt = (ADC_CHAN_OPT_PT) & sb;

#define wbkPga516CheckOptionValue(lowLimit, highLimit)\
   { if ( (optionValue > highLimit) || (optionValue < lowLimit) ) \
        return DerrInvOptionValue; \
   }

	if (!moduleOption) {
		switch (optionType) {
		case DcotPga516LowPassMode:

			wbkPga516CheckOptionValue(DcovPga516LowPassBypass, DcovPga516LowPassOn);

			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) optionType;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		default:
			return DerrInvOptionType;
		}

	} else {
		return DerrInvOptionType;
	}

	adcChanOpt->operation = 1;
	err = itfExecuteCommand(handle, &sb, ADC_CHAN_OPT);
	return err;

}

DaqError
setWbk14Options(DaqHandleT handle, DWORD chan, DWORD moduleOption,
		DaqOptionType optionType, float optionValue)
{

	DaqError err;
	DWORD startChan, chansInBank;
	DaqChanOptionValue excSrcWaveform;
	daqIOT sb;
	ADC_CHAN_OPT_PT adcChanOpt = (ADC_CHAN_OPT_PT) & sb;

#define wbk14CheckOptionValue(lowLimit, highLimit) \
       { if ( (optionValue > highLimit) || (optionValue < lowLimit) ) \
            return DerrInvOptionValue; \
       }

	if (!moduleOption) {
		switch (optionType) {

		case DcotWbk14LowPassMode:
			wbk14CheckOptionValue(DcovWbk14LowPassBypass, DcovWbk14LowPassExtClk);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk14LowPassMode;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk14LowPassCutOff:
			wbk14CheckOptionValue(30.0f, 100000.0f);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk14LowPassCutOff;

			adcChanOpt->optValue = (DWORD) (optionValue * 1000.0f);
			break;

		case DcotWbk14HighPassCutOff:
			wbk14CheckOptionValue(DcovWbk14HighPass0_1Hz, DcovWbk14HighPass10Hz);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk14HighPassCutOff;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk14PreFilterMode:
			wbk14CheckOptionValue(DcovWbk14PreFilterDefault, DcovWbk14PreFilterOff);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk14PreFilterMode;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk14CurrentSrc:
			wbk14CheckOptionValue(DcovWbk14CurrentSrcOff, DcovWbk14CurrentSrc4mA);
			startChan = chan / 2 * 2 + (chan % 2 ? 1 : 0);
			adcChanOpt->startChan = startChan;
			adcChanOpt->endChan = startChan + 1;
			adcChanOpt->chanOptType = (DWORD) DcotWbk14CurrentSrc;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk14ExtFilterRange:
			wbk14CheckOptionValue(DcovWbk14FilterRange_1K, DcovWbk14FilterRange_20K);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk14ExtFilterRange;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		default:
			return DerrInvOptionType;
		}

	} else {
		switch (optionType) {

		case DmotWbk14ExcSrcWaveform:
			wbk14CheckOptionValue(DmovWbk14ExcSrcRandom, DmovWbk14ExcSrcSine);
			chansInBank = 8;
			if (!(chan % chansInBank))
				startChan = chan - chansInBank + 1;
			else
				startChan = chan / chansInBank * chansInBank + 1;

			adcChanOpt->startChan = startChan;
			adcChanOpt->endChan = startChan + 7;
			adcChanOpt->chanOptType = (DWORD) DmotWbk14ExcSrcWaveform;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DmotWbk14ExcSrcFreq:
			wbk14CheckOptionValue(20.0f, 100000.0f);
			chansInBank = 8;
			if (!(chan % chansInBank))
				startChan = chan - chansInBank + 1;
			else
				startChan = chan / chansInBank * chansInBank + 1;

			adcChanOpt->startChan = startChan;
			adcChanOpt->endChan = startChan + 7;
			adcChanOpt->chanOptType = (DWORD) DmotWbk14ExcSrcFreq;

			adcChanOpt->optValue = (DWORD) (optionValue * 1000.0f);
			break;

		case DmotWbk14ExcSrcAmplitude:

			err =
			    daqGetInfo(handle, chan, DdiWbk14ExcSrcWaveformInfo,
				       (VOID *) & excSrcWaveform);
			if (err != DerrNoError)
				return err;
			if (excSrcWaveform == DmovWbk14ExcSrcRandom) {
				wbk14CheckOptionValue(0.0f, 1.7f);
			} else {
				wbk14CheckOptionValue(0.0f, 10.0f);
			}
			chansInBank = 8;
			if (!(chan % chansInBank))
				startChan = chan - chansInBank + 1;
			else
				startChan = chan / chansInBank * chansInBank + 1;

			adcChanOpt->startChan = startChan;
			adcChanOpt->endChan = startChan + 7;
			adcChanOpt->chanOptType = (DWORD) DmotWbk14ExcSrcAmplitude;
			adcChanOpt->optValue = (DWORD) (optionValue * 1000.0f);
			break;

		case DmotWbk14ExcSrcOffset:
			wbk14CheckOptionValue(-5.0f, 5.0f);
			chansInBank = 8;
			if (!(chan % chansInBank))
				startChan = chan - chansInBank + 1;
			else
				startChan = chan / chansInBank * chansInBank + 1;

			adcChanOpt->startChan = startChan;
			adcChanOpt->endChan = startChan + 7;
			adcChanOpt->chanOptType = (DWORD) DmotWbk14ExcSrcOffset;
			adcChanOpt->optValue = (DWORD) (optionValue * 1000.0f);
			break;

		case DmotWbk14ExcSrcApply:
			chansInBank = 8;
			if (!(chan % chansInBank))
				startChan = chan - chansInBank + 1;
			else
				startChan = chan / chansInBank * chansInBank + 1;

			adcChanOpt->startChan = startChan;
			adcChanOpt->endChan = startChan + 7;
			adcChanOpt->chanOptType = (DWORD) DmotWbk14ExcSrcApply;
			break;

		default:
			return DerrInvOptionType;

		}
	}
	adcChanOpt->operation = 1;
	err = itfExecuteCommand(handle, &sb, ADC_CHAN_OPT);
	return err;

}

DaqError
setWbk16Options(DaqHandleT handle, DWORD chan, DWORD moduleOption,
		DaqOptionType optionType, float optionValue)
{

	DaqError err;
	DWORD startChan, chansInBank;
	daqIOT sb;
	ADC_CHAN_OPT_PT adcChanOpt = (ADC_CHAN_OPT_PT) & sb;

#define wbk16CheckOptionValue(lowLimit, highLimit)\
       { if ( (optionValue > highLimit) || (optionValue < lowLimit) ) \
            return DerrInvOptionValue; \
       }

	if (!moduleOption) {
		switch (optionType) {

		case DcotWbk16Bridge:
			wbk16CheckOptionValue(DcovWbk16ApplyFull, DcovWbk16ApplyHalfQtrNeg);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk16Bridge;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk16ShuntCal:
			wbk16CheckOptionValue(DcovWbk16ApplyNone, DcovWbk16AutoZero);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk16ShuntCal;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk16InDiag:
			wbk16CheckOptionValue(DcovWbk16ReadNone, DcovWbk16ReadPosArm);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk16InDiag;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk16OffsetDac:
			wbk16CheckOptionValue(Dac12BitLo, Dac12BitHi);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk16OffsetDac;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk16OutSource:
			wbk16CheckOptionValue(DcovWbk16ReadSignal, DcovWbk16ReadExcCurr);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk16OutSource;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk16Inv:
			wbk16CheckOptionValue(DcovWbk16Normal, DcovWbk16Inverted);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk16Inv;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk16FilterType:
			wbk16CheckOptionValue(DcovWbk16FltBypass, DcovWbk16Flt1Khz);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk16FilterType;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk16Couple:
			wbk16CheckOptionValue(DcovWbk16CoupleDC, DcovWbk16CoupleAC);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk16Couple;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk16Sample:
			wbk16CheckOptionValue(DcovWbk16Bypassed, DcovWbk16Ssh);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk16Sample;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk16ExcDac:
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk16ExcDac;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk16IAG:
			wbk16CheckOptionValue(DcovWbk16X1, DcovWbk16X1000);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk16IAG;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk16PGA:
			wbk16CheckOptionValue(DcovWbk16X1_00, DcovWbk16X20_00);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk16PGA;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DmotWbk16Immediate:
			wbk16CheckOptionValue(DmovWbk16ExcSrcApply, DmovWbk16FanOff);
			chansInBank = 8;

			if (!(chan % chansInBank))
				startChan = chan - chansInBank + 1;
			else
				startChan = chan / chansInBank * chansInBank + 1;

			adcChanOpt->startChan = startChan;
			adcChanOpt->endChan = startChan + 7;
			adcChanOpt->chanOptType = (DWORD) DmotWbk16Immediate;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		default:
			return DerrInvOptionType;
		}

	} else {
		return DerrInvOptionType;
	}
	adcChanOpt->operation = 1;
	err = itfExecuteCommand(handle, &sb, ADC_CHAN_OPT);
	return err;

}

DaqError
setWbk17Options(DaqHandleT handle, DWORD chan, DWORD moduleOption,
		DaqOptionType optionType, float optionValue)
{

	DaqError err;

	daqIOT sb;
	ADC_CHAN_OPT_PT adcChanOpt = (ADC_CHAN_OPT_PT) & sb;

#define wbk17CheckOptionValue(lowLimit, highLimit)\
       { if ( (optionValue > highLimit) || (optionValue < lowLimit) ) \
            return DerrInvOptionValue; \
       }

#define wbk17CheckSubChannel\
       { if ( !((moduleOption & (DcofSubChannelLow | DcofSubChannelHigh)) && ((moduleOption & 0x1F) <= 0x10)) ) \
            return DerrInvOptionValue; \
       }

#define wbk17SetOptions\
       {adcChanOpt->startChan   = chan;\
        adcChanOpt->endChan     = chan;\
        adcChanOpt->chanOptType = (DWORD)optionType;\
        adcChanOpt->optValue    = (DWORD)optionValue;\
        adcChanOpt->devType     = moduleOption;}

	if (!moduleOption) {

		switch (optionType) {

		case DcotWbk17Level:
			{
				float fWbk17Range = 12.5f;
				wbk17CheckOptionValue(-fWbk17Range, fWbk17Range);
				adcChanOpt->startChan = chan;
				adcChanOpt->endChan = chan;
				adcChanOpt->chanOptType = (DWORD) DcotWbk17Level;

				adcChanOpt->optValue =
				    (DWORD) ((fWbk17Range + optionValue) * 255.0f / 25.0f);
			}
			break;

		case DcotWbk17Coupling:
			wbk17CheckOptionValue(DcovWbk17CoupleOff, DcovWbk17CoupleDC);
			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk17Coupling;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk17FilterType:
			wbk17CheckOptionValue(DcovWbk17FltBypass, DcovWbk17Flt30Hz);

			if (optionValue == 3)
				return DerrInvOptionValue;

			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk17FilterType;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk17DebounceTime:
			wbk17CheckOptionValue(DcovWbk17Debounce500ns, DcovWbk17DebounceNone);

			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk17DebounceTime;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk17DebounceTrigger:
			wbk17CheckOptionValue(DcovWbk17TriggerAfterStable,
					      DcovWbk17TriggerBeforeStable);

			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk17DebounceTrigger;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk17Edge:
			wbk17CheckOptionValue(DcovWbk17RisingEdge, DcovWbk17FallingEdge);

			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk17Edge;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk17MeasurementMode:

			wbk17CheckOptionValue(DcovWbk17Mode_OFF, 0x0500 + 0x007F);

			if (((DWORD) optionValue) & 0x80)
				return DerrInvOptionValue;

			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk17MeasurementMode;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk17TickSize:
			wbk17CheckOptionValue(DcovWbk17Tick20ns, DcovWbk17Tick20000ns);

			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk17TickSize;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk17MapChannel:
			wbk17CheckOptionValue(1, 16);

			adcChanOpt->startChan = chan;
			adcChanOpt->endChan = chan;
			adcChanOpt->chanOptType = (DWORD) DcotWbk17MapChannel;
			adcChanOpt->optValue = (DWORD) optionValue;
			break;

		case DcotWbk17DetectClear:
			wbk17CheckOptionValue(DcovWbk17DetClr_Chan, DcovWbk17DetClr_All);
			wbk17SetOptions;
			break;

		default:
			return DerrInvOptionType;
		}

	} else if ((moduleOption & (DcofSubChannelLow | DcofSubChannelHigh))
		   && ((moduleOption & 0x1F) <= 0x10)) {

		switch (optionType) {

		case DcotWbk17DetectControl:

			wbk17CheckOptionValue(DcovWbk17DetCtrl_Off,
					      (DcovWbk17DetCtrl_Update_Dig * 2) - 1);
			wbk17CheckSubChannel;
			wbk17SetOptions;
			break;

		case DcotWbk17DetectLowLimit:
		case DcotWbk17DetectHighLimit:
			wbk17CheckOptionValue(0, 65535);
			wbk17CheckSubChannel;
			wbk17SetOptions;
			break;

		case DcotWbk17DetectDigComp:
		case DcotWbk17DetectDigMask:
		case DcotWbk17DetectDigOut:
			wbk17CheckOptionValue(0, 255);
			wbk17CheckSubChannel;
			wbk17SetOptions;
			break;

		default:
			return DerrInvOptionType;
		}

	} else {
		return DerrInvOptionType;

	}

	adcChanOpt->operation = 1;
	err = itfExecuteCommand(handle, &sb, ADC_CHAN_OPT);
	return err;

}

DaqError
setDigitalOptions(DaqHandleT handle, DWORD chan, DWORD moduleOption,
		  DaqOptionType optionType, float optionValue)
{

	DaqError err;
	DWORD dwCurrentConf, dwNewConf, dwConfChan;

#define DigitalCheckOptionValue(lowLimit, highLimit)\
   { if ( (optionValue > highLimit) || (optionValue < lowLimit) ) \
        return DerrInvOptionValue; \
   }

#define DigitalCheckPort(lowLimit, highLimit)\
   { if ( (chan > highLimit) || (chan < lowLimit) ) \
        return DerrInvChan; \
   }

	DigitalCheckOptionValue(DcovDigitalInput, DcovDigitalOutput);

	if (!moduleOption) {
		switch (optionType) {

		case DcotP2Local8Mode:

			DigitalCheckPort(0, 2)

			    err =
			    daqIORead(handle, DiodtP2Local8, DiodpP2LocalIR, 0,
				      DioepP2, &dwCurrentConf);
			if (err != DerrNoError)
				return err;

			if (chan == 0) {
				dwNewConf = (dwCurrentConf & 0x0000000B) | ((DWORD)
									    ((BOOL)
									     (optionValue
									      > 0 ? 0 : 1)
									     << 4));
			} else if (chan == 1) {
				dwNewConf = (dwCurrentConf & 0x00000019) | ((DWORD)
									    ((BOOL)
									     (optionValue
									      > 0 ? 0 : 1)
									     << 1));
			} else {
				dwNewConf = (dwCurrentConf & 0x00000012)
				    | ((DWORD)
				       ((BOOL) (optionValue > 0 ? 0 : 1) << 3))
				    | ((DWORD)
				       ((BOOL) (optionValue > 0 ? 0 : 1) << 0));
			}

			dwNewConf |= 0X80;

			err =
			    daqIOWrite(handle, DiodtP2Local8, DiodpP2LocalIR, 0,
				       DioepP2, dwNewConf);
			return err;
			break;

		case DcotP2Exp8Mode:

			DigitalCheckPort(0, 31)

			    if ((chan + 1) % 4 == 0)
				return DerrInvChan;

			dwConfChan = ((chan / 4) * 4) + 3;

			err =
			    daqIORead(handle, DiodtP2Exp8, DiodpP2Exp8,
				      dwConfChan, DioepP2, &dwCurrentConf);
			if (err != DerrNoError)
				return err;

			if ((chan + 1) % 4 == 1) {
				dwNewConf = (dwCurrentConf & 0x0000000B) | ((DWORD)
									    ((BOOL)
									     (optionValue
									      > 0 ? 0 : 1)
									     << 4));

			} else if ((chan + 1) % 4 == 2) {
				dwNewConf = (dwCurrentConf & 0x00000019) | ((DWORD)
									    ((BOOL)
									     (optionValue
									      > 0 ? 0 : 1)
									     << 1));

			} else {
				dwNewConf = (dwCurrentConf & 0x00000012)
				    | ((DWORD)
				       ((BOOL) (optionValue > 0 ? 0 : 1) << 3))
				    | ((DWORD)
				       ((BOOL) (optionValue > 0 ? 0 : 1) << 0));

			}

			dwNewConf |= 0X80;

			err =
			    daqIOWrite(handle, DiodtP2Exp8, DiodpP2Exp8,
				       dwConfChan, DioepP2, dwNewConf);
			return err;
			break;

		case DcotP3Local16Mode:

			DigitalCheckPort(0, 0)
			    dwNewConf = (DWORD) ((BOOL) (optionValue > 0 ? 1 : 0));
			err =
			    daqIOWrite(handle, DiodtP3LocalDig16,
				       DiodpP3LocalDigIR, chan, DioepP3, dwNewConf);
			return err;
			break;

		default:
			return DerrInvOptionType;
		}
	} else {
		return DerrInvOptionType;
	}

}

DaqError
setCounterOptions(DaqHandleT handle, DWORD chan, DWORD moduleOption,
		  DaqOptionType optionType, float optionValue)
{

	DaqError err;
	daqIOT sb;
	ADC_CHAN_OPT_PT adcChanOpt = (ADC_CHAN_OPT_PT) & sb;

#define CounterCheckOptionValue(lowLimit, highLimit)\
   { if ( (optionValue > highLimit) || (optionValue < lowLimit) ) \
        return DerrInvOptionValue; \
   }

	if (!moduleOption) {
		switch (optionType) {

		case DcotCounterCascade:
			CounterCheckOptionValue(DcovCounterSingle, DcovCounterCascade);
			break;

		case DcotCounterMode:
			CounterCheckOptionValue(DcovCounterClearOnRead, DcovCounterTotalize);
			break;

		case DcotCounterControl:
			CounterCheckOptionValue(DcovCounterOff, DcovCounterManualClear);
			break;

		case DcotCounterEdge:
			CounterCheckOptionValue(DcovCounterFallingEdge, DcovCounterRisingEdge);
			break;

		default:
			return DerrInvOptionType;
		}
	} else {

		switch (optionType) {
		case DmotCounterControl:
			CounterCheckOptionValue(DcovCounterOff, DcovCounterManualClear);
			break;

		default:
			return DerrInvOptionType;
		}
	}

	adcChanOpt->startChan = chan;
	adcChanOpt->endChan = chan;
	adcChanOpt->chanOptType = (DWORD) optionType;

	adcChanOpt->optValue = (DWORD) optionValue;

	adcChanOpt->operation = 1;
	err = itfExecuteCommand(handle, &sb, ADC_CHAN_OPT);

	return err;

}

DaqError
setTimerOptions(DaqHandleT handle, DWORD chan, DWORD moduleOption,
		DaqOptionType optionType, float optionValue)
{

	DaqError err;
	daqIOT sb;
	ADC_CHAN_OPT_PT adcChanOpt = (ADC_CHAN_OPT_PT) & sb;

#define TimerCheckOptionValue(lowLimit, highLimit)\
   { if ( (optionValue > highLimit) || (optionValue < lowLimit) ) \
        return DerrInvOptionValue; \
   }

	if (!moduleOption) {
		switch (optionType) {

		case DcotTimerDivisor:

			TimerCheckOptionValue(0, 0xFFFF);
			break;

		case DcotTimerControl:

			TimerCheckOptionValue(DcovTimerOff, DcovTimerOn);
			break;

		default:
			return DerrInvOptionType;
		}

	} else {
		switch (optionType) {

		case DmotTimerControl:

			TimerCheckOptionValue(DcovTimerOff, DcovTimerOn);
			break;

		default:
			return DerrInvOptionType;
		}
	}

	adcChanOpt->startChan = chan;
	adcChanOpt->endChan = chan;
	adcChanOpt->chanOptType = (DWORD) optionType;
	adcChanOpt->optValue = (DWORD) optionValue;

	adcChanOpt->operation = 1;
	err = itfExecuteCommand(handle, &sb, ADC_CHAN_OPT);
	return err;

}

DAQAPI DaqError WINAPI
daqSetOption(DaqHandleT handle, DWORD chan, DWORD moduleOption,
	     DaqOptionType optionType, float optionValue)
{
	VOID *optionStruct;
	DWORD startChan;
	DWORD endChan;
	DaqError err;
	int dasType;
	DWORD ch;
	float fOptionValue = optionValue;
//	daqIOT sb;
//	ADC_CHAN_OPT_PT adcChanOpt = (ADC_CHAN_OPT_PT) & sb;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError) {
		return err;
	}

	switch (optionType & 0x0000F000) {

	case DcotDigitalOption:
		return setDigitalOptions(handle, chan, moduleOption, optionType, optionValue);
		break;

	case DcotCounterOption:
		return setCounterOptions(handle, chan, moduleOption, optionType, optionValue);
		break;

	case DcotTimerOption:
		return setTimerOptions(handle, chan, moduleOption, optionType, optionValue);
		break;

	case DbotBaseUnitOption:
		return SetBaseUnitOption(handle, optionType, optionValue);
		break;
	}

#define SetChanOptionData(optCard, lowLimit, highLimit, structType, valueName, valueType)\
       { \
          if ( (optionValue > highLimit) || (optionValue < lowLimit) ) return (apiCtrlProcessError(handle, DerrInvOptionValue)); \
             ((structType *)(optionStruct))->valueName = (valueType)optionValue;  \
             session[handle].chanOptions[chan].changed = TRUE;  \
       };

#define SetChanGroupOptionData(optCard, groupSize, lowLimit, highLimit, structType, valueName, valueType)\
       {\
             if ( (optionValue > highLimit) || (optionValue < lowLimit) ) return (apiCtrlProcessError(handle, DerrInvOptionValue));\
             startChan = chan - 1;\
             startChan -= startChan % groupSize;\
             for (ch = startChan; ch < (startChan+groupSize); ch++ ) {\
                ((structType *)optionStruct)->valueName = (valueType)optionValue;\
                session[handle].chanOptions[chan].changed = TRUE;\
             }\
       };


	dasType = IsWaveBook(session[handle].gnDeviceType) ? 1 : 0;

	if (chan > session[handle].gnDevHwareInf.ChannelsPerSys + 1) {
		return apiCtrlProcessError(handle, DerrInvChan);
	}

	if (moduleOption) {
		startChan = getStartChan(chan, (apiGetChanType(handle, chan)));
		endChan = getEndChan(chan, (apiGetChanType(handle, chan)));
	}

	optionStruct = apiGetChanOption(handle, chan);

	if (!dasType) {
		switch (apiGetChanType(handle, chan)) {

		case DaetDbk4:

			chan = getStartChan(chan, DaetDbk4);
			switch (optionType) {
			case DcotDbk4Gain:
				{

					DaqAdcGain optionValue = (DaqAdcGain) (DWORD) fOptionValue;
					if (!moduleOption) {
						SetChanOptionData(chan,
								  Dbk4FilterX1,
								  Dbk4FilterX8000,
								  Dbk4OptionStructT,
								  gain, DaqAdcGain);
					} else {
						SetChanGroupOptionData(chan,
								       (endChan
									-
									startChan),
								       Dbk4FilterX1,
								       Dbk4FilterX8000,
								       Dbk4OptionStructT,
								       gain, DaqAdcGain);
					}
				}
				break;
			case DcotDbk4MaxFreq:
				if (!moduleOption) {
					SetChanOptionData(chan,
							  DcovDbk4Freq18000Hz,
							  DcovDbk4Freq141Hz,
							  Dbk4OptionStructT,
							  maxFreq, BYTE);
				} else {
					SetChanGroupOptionData(chan,
							       (endChan -
								startChan),
							       DcovDbk4Freq18000Hz,
							       DcovDbk4Freq141Hz,
							       Dbk4OptionStructT,
							       maxFreq, BYTE);
				}
				break;
			case DcotDbk4SetBaseline:
				if (!moduleOption) {
					SetChanOptionData(chan,
							  DcovDbk4BaselineNever,
							  DcovDbk4BaselineOneShot,
							  Dbk4OptionStructT,
							  setBaseline, BYTE);
				} else {
					SetChanGroupOptionData(chan,
							       (endChan -
								startChan),
							       DcovDbk4BaselineNever,
							       DcovDbk4BaselineOneShot,
							       Dbk4OptionStructT,
							       setBaseline, BYTE);
				}
				break;
			case DcotDbk4Excitation:

				optionStruct = apiGetChanOption(handle, chan);
				((Dbk4OptionStructT *) optionStruct)->
				    excitation = (BYTE) (optionValue == 0);
				break;
			case DcotDbk4Clock:

				optionStruct = apiGetChanOption(handle, chan);
				((Dbk4OptionStructT *) optionStruct)->clock =
				    (BYTE) (optionValue == 0);
				break;
			default:
				return apiCtrlProcessError(handle, DerrInvOptionType);
			}

			break;

		case DaetDbk7:
			switch (optionType) {
			case DcotDbk7Slope:
				((Dbk7OptionStructT *) optionStruct)->slope =
				    (BYTE) (optionValue != 0);
				break;
			case DcotDbk7DebounceTime:
				if (!moduleOption) {
					SetChanOptionData(chan,
							  DcovDbk7DebounceNone,
							  DcovDbk7Debounce10ms,
							  Dbk7OptionStructT,
							  debounceTime, BYTE);
				} else {
					SetChanGroupOptionData(chan,
							       (endChan -
								startChan),
							       DcovDbk7DebounceNone,
							       DcovDbk7Debounce10ms,
							       Dbk7OptionStructT,
							       debounceTime, BYTE);
				}
				break;
			case DcotDbk7MinFreq:
				if (!moduleOption) {
					SetChanOptionData(chan, 0.0, 990000.0,
							  Dbk7OptionStructT, minFreq, float);
				} else {
					SetChanGroupOptionData(chan,
							       (endChan -
								startChan), 0.0,
							       990000.0,
							       Dbk7OptionStructT, minFreq, float);
				}
				break;
			case DcotDbk7MaxFreq:
				if (!moduleOption) {
					SetChanOptionData(chan, 1.0, 1000000.0,
							  Dbk7OptionStructT, maxFreq, float);
				} else {
					SetChanGroupOptionData(chan,
							       (endChan -
								startChan), 1.0,
							       1000000.0,
							       Dbk7OptionStructT, maxFreq, float);
				}
				break;
			default:
				return apiCtrlProcessError(handle, DerrInvOptionType);
			}
			break;

		case DaetDbk50:
			switch (optionType) {
			case DcotDbk50Gain:
				{
					DaqAdcGain optionValue =
					    (DaqAdcGain) (((DWORD) fOptionValue)
							  & 0x03L);
					if (!moduleOption) {
						SetChanOptionData(chan,
								  Dbk50Range0,
								  Dbk50Range300,
								  Dbk50OptionStructT,
								  gain, DaqAdcGain);
					} else {
						SetChanGroupOptionData(chan,
								       (endChan-startChan),
								       Dbk50Range0,
								       Dbk50Range300,
								       Dbk50OptionStructT,
								       gain, DaqAdcGain);
					}
				}
				break;
			default:
				return apiCtrlProcessError(handle, DerrInvOptionType);
			}
			break;
		default:
			break;

		}
	}

	if (dasType) {

		switch (apiGetChanType(handle, chan)) {

		case DmctWbk516:

		case DmctWbk512:
		case DmctWbk512_10V:

		case DmctWbk512A:
		case DmctWbk516A:

			switch (session[handle].chanOptions[chan].chanOptionType) {
			case DoctWbk12:
			case DoctWbk13:
			case DoctWbk12A:
			case DoctWbk13A:
				err =
				    setWbk12Options(handle, chan, moduleOption,
						    optionType, optionValue);
				break;

			case DoctPga516:
				err =
				    setPga516Options(handle, chan, moduleOption,
						     optionType, optionValue);
				break;

			default:
				err = apiCtrlProcessError(handle, DerrInvOptionType);
			}
			if (err != DerrNoError)
				return err;
			break;

		case DmctWbk10:
		case DmctWbk10_10V:
		case DmctWbk10A:

			switch (session[handle].chanOptions[chan].chanOptionType) {
			case DoctWbk12:
			case DoctWbk13:
			case DoctWbk12A:
			case DoctWbk13A:
				err =
				    setWbk12Options(handle, chan, moduleOption,
						    optionType, optionValue);
				break;

			case DoctPga516:
				err =
				    setPga516Options(handle, chan, moduleOption,
						     optionType, optionValue);
				break;

			default:
				return apiCtrlProcessError(handle, DerrInvOptionType);
			}
			if (err != DerrNoError)
				return err;
			break;

		case DmctWbk14:
			err = setWbk14Options(handle, chan, moduleOption, optionType, optionValue);
			break;

		case DmctWbk16:
		case DmctWbk16_SSH:
			dbg("SetOption: (chan,type,val)%d,%d,%f", 
			    chan, optionType, optionValue);

			err = setWbk16Options(handle, chan, moduleOption, optionType, optionValue);
			break;

		case DmctWbk17:
			err = setWbk17Options(handle, chan, moduleOption, optionType, optionValue);

			if (err != DerrNoError)
				return apiCtrlProcessError(handle, err);
			break;

		case DmctWbk15:

		default:
			return apiCtrlProcessError(handle, DerrInvOptionType);

		}
		if (err != DerrNoError)
			return err;
	}

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqAdcExpSetBank(DaqHandleT handle, DWORD chan, DaqAdcExpType chanType)
{

	DaqAdcExpType oldChanType;
	DWORD oldStartChan, oldEndChan;
	DWORD startChan, endChan;
	DaqError err;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return err;

	if ((chan < session[handle].gnDevHwareInf.MainUnitChans) ||
	    (chan > session[handle].gnDevHwareInf.ChannelsPerSys - 1)) {
		return apiCtrlProcessError(handle, DerrInvChan);
	}

	if ((chanType < DaetNotDefined) || (chanType > DaetDbk7)) {
		return apiCtrlProcessError(handle, DerrInvBankType);
	}

	oldChanType = apiGetChanType(handle, chan);
	oldStartChan = getStartChan(chan, oldChanType);
	oldEndChan = getEndChan(chan, oldChanType);
	startChan = getStartChan(chan, chanType);
	endChan = getEndChan(chan, chanType);

	if (chanType == DaetNotDefined) {

		deallocateBank(handle, oldStartChan, oldEndChan);
	} else {

		if (oldChanType != chanType) {

			for (chan = startChan; chan <= endChan; chan++)
				if (apiGetChanType(handle, chan) != DaetNotDefined)
					return apiCtrlProcessError(handle, DerrInvBankType);

			err = allocateBank(handle, startChan, endChan, chanType);
			if (err != DerrNoError)
				return apiCtrlProcessError(handle, err);
		}

		switch (chanType) {
		case DaetDbk7:
			err = dbk7AutoCal(handle, startChan, endChan);
			if (err != DerrNoError) {
				deallocateBank(handle, startChan, endChan);
				return apiCtrlProcessError(handle, err);
			}
			break;

			default:
				break;
		}

		chan = startChan;

		switch (chanType) {
		case DaetDbk2:
			daqDacSetOutputMode(handle, DddtDbk, chan, DdomVoltage);
			daqDacWt(handle, DddtDbk, chan, 0x8000);
			break;
		case DaetDbk5:

			daqDacSetOutputMode(handle, DddtDbk, chan, DdomVoltage);
			daqDacWt(handle, DddtDbk, chan, 0x0000);
			break;
		case DaetDbk4:
			daqSetOption(handle, chan, 1, DdcotDbk4Gain, Dbk4FilterX1);
			daqSetOption(handle, chan, 1, DdcotDbk4MaxFreq, DcovDbk4Freq2250Hz);
			daqSetOption(handle, chan, 1, DdcotDbk4SetBaseline, DcovDbk4BaselineNever);
			daqSetOption(handle, chan, 1, DdcotDbk4Excitation, 1.0f);
			daqSetOption(handle, chan, 1, DdcotDbk4Clock, 1.0f);
			break;
		case DaetDbk7:
			daqSetOption(handle, chan, 1, DdcotDbk7Slope, 1.0f);
			daqSetOption(handle, chan, 1, DdcotDbk7DebounceTime, DcovDbk7DebounceNone);
			daqSetOption(handle, chan, 1, DdcotDbk7MinFreq, 0.0f);
			daqSetOption(handle, chan, 1, DdcotDbk7MaxFreq, 1000000.0f);
			break;
		case DaetDbk50:
			daqSetOption(handle, chan, 1, DdcotDbk50Gain, Dbk50Range0);
			break;
		default:
			break;
		}
	}

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqAdcExpSetChanOption(DaqHandleT handle, DWORD chan,
		       DaqChanOptionType optionType, float optionValue)
{

	DaqError err;
	DaqOptionType tempType;

	switch (apiGetChanType(handle, chan)) {
	case DaetDbk50:
		switch (optionType) {
		case DcotDbk50Gain:
			tempType = DdcotDbk50Gain;
			break;
		default:
			return apiCtrlProcessError(handle, DerrInvOptionType);
		}
		break;
	case DaetDbk4:
		switch (optionType) {
		case DcotDbk4MaxFreq:
			tempType = DdcotDbk4MaxFreq;
			break;
		case DcotDbk4SetBaseline:
			tempType = DdcotDbk4SetBaseline;
			break;
		case DcotDbk4Excitation:
			tempType = DdcotDbk4Excitation;
			break;
		case DcotDbk4Clock:
			tempType = DdcotDbk4Clock;
			break;
		case DcotDbk4Gain:

			tempType = DdcotDbk4Gain;
			break;
		default:
			return apiCtrlProcessError(handle, DerrInvOptionType);
		}
		break;
	case DaetDbk7:
		switch (optionType) {
		case DcotDbk7Slope:
			tempType = DdcotDbk7Slope;
			break;
		case DcotDbk7DebounceTime:
			tempType = DdcotDbk7DebounceTime;
			break;
		case DcotDbk7MinFreq:
			tempType = DdcotDbk7MinFreq;
			break;
		case DcotDbk7MaxFreq:
			tempType = DdcotDbk7MaxFreq;
			break;
		default:
			return apiCtrlProcessError(handle, DerrInvOptionType);
		}
		break;
	default:
		return apiCtrlProcessError(handle, DerrInvBankType);
	}

	err = daqSetOption(handle, chan, 0, tempType, optionValue);

	if (err)
		return err;
	else {
		return DerrNoError;
	}

}

DAQAPI DaqError WINAPI
daqAdcExpSetModuleOption(DaqHandleT handle, DWORD chan,
			 DaqChanOptionType optionType, float optionValue)
{

	DaqError err;
	DaqOptionType tempType;

	switch (apiGetChanType(handle, chan)) {
	case DaetDbk50:
		switch (optionType) {
		case DcotDbk50Gain:
			tempType = DdcotDbk50Gain;
			break;
		default:
			return apiCtrlProcessError(handle, DerrInvOptionType);
		}
	case DaetDbk4:
		switch (optionType) {
		case DcotDbk4MaxFreq:
			tempType = DdcotDbk4MaxFreq;
			break;
		case DcotDbk4SetBaseline:
			tempType = DdcotDbk4SetBaseline;
			break;
		case DcotDbk4Excitation:
			tempType = DdcotDbk4Excitation;
			break;
		case DcotDbk4Clock:
			tempType = DdcotDbk4Clock;
			break;
		case DcotDbk4Gain:
			tempType = DdcotDbk4Gain;
			break;
		default:
			return apiCtrlProcessError(handle, DerrInvOptionType);
		}
	case DaetDbk7:
		switch (optionType) {
		case DcotDbk7Slope:
			tempType = DdcotDbk7Slope;
			break;
		case DcotDbk7DebounceTime:
			tempType = DdcotDbk7DebounceTime;
			break;
		case DcotDbk7MinFreq:
			tempType = DdcotDbk7MinFreq;
			break;
		case DcotDbk7MaxFreq:
			tempType = DdcotDbk7MaxFreq;
			break;
		default:
			return apiCtrlProcessError(handle, DerrInvOptionType);
		}
		break;
	default:
		return apiCtrlProcessError(handle, DerrInvBankType);
	}

	err = daqSetOption(handle, chan, 1, tempType, optionValue);

	if (err)
		return err;
	else {
		return DerrNoError;
	}

}

DAQAPI DaqError WINAPI
daqGetInfo(DaqHandleT handle, DWORD chan, DaqInfo whichInfo, VOID * info)
{
	DaqError err;
	daqIOT sb;
//	int chStat = 0;
//	int radix = 0;
	double dblVal = 0;
	const double Million = 1000000.0;
	const double Thousand = 1000.0;
	DaqAdcExpType chType;
	ADC_HWINFO_PT infoRequest = (ADC_HWINFO_PT) & sb;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	chType = apiGetChanType(handle, chan);

	switch (whichInfo) {

	case DdiHardwareVersionInfo:
	case DdiProtocolInfo:
		daqGetHardwareInfo(handle, (DaqHardwareInfo) whichInfo, info);
		break;
	case DdiADminInfo:
		daqGetHardwareInfo(handle, DhiADmin, info);
		break;
	case DdiADmaxInfo:
		daqGetHardwareInfo(handle, DhiADmax, info);
		break;

	case DdiChTypeInfo:
		*(PDWORD) info = apiGetChanType(handle, chan);
		break;
	case DdiChOptionTypeInfo:
		*(PDWORD) info = session[handle].chanOptions[chan].chanOptionType;
		break;

	case DdiChanCountInfo:
		*(PDWORD) info = session[handle].gnDevHwareInf.MainUnitChans;
		break;

	case DdiWgcX1Info:
		switch (apiGetChanType(handle, chan)) {
		case DmctWbk512:
		case DmctWbk512_10V:
		case DmctWbk516:

		case DmctWbk512A:
		case DmctWbk516A:

		case DmctWbk10:
		case DmctWbk10_10V:
		case DmctWbk10A:
		case DoctWbk11:
		case DoctWbk12:
		case DoctWbk12A:
		case DoctWbk13:
		case DoctWbk13A:

		case DmctWbk14:
		case DmctWbk15:
		case DmctWbk16:
		case DmctWbk16_SSH:
		case DmctWbk17:
		case DmctResponseDac:
			*(float *) info = 1.0F;
			return (DerrNoError);
		default:
			return apiCtrlProcessError(handle, DerrInvGain);
		}
		break;

	case DdiWgcX2Info:
		switch (apiGetChanType(handle, chan)) {
		case DmctWbk512:
		case DmctWbk512_10V:
		case DmctWbk516:

		case DmctWbk512A:
		case DmctWbk516A:

		case DmctWbk10:
		case DmctWbk10_10V:
		case DmctWbk10A:
		case DoctWbk11:
		case DoctWbk12:
		case DoctWbk12A:
		case DoctWbk13:
		case DoctWbk13A:

		case DmctWbk14:
			*(float *) info = 2.0F;
			return (DerrNoError);
		default:
			return apiCtrlProcessError(handle, DerrInvGain);
		}
		break;
	case DdiWgcX5Info:
		switch (apiGetChanType(handle, chan)) {
		case DmctWbk512:
		case DmctWbk512_10V:
		case DmctWbk516:

		case DmctWbk512A:
		case DmctWbk516A:

		case DmctWbk10:
		case DmctWbk10_10V:
		case DmctWbk10A:
		case DoctWbk11:
		case DoctWbk12:
		case DoctWbk12A:
		case DoctWbk13:
		case DoctWbk13A:

		case DmctWbk14:
			*(float *) info = 5.0F;
			return (DerrNoError);
		default:
			return apiCtrlProcessError(handle, DerrInvGain);
		}
		break;
	case DdiWgcX10Info:
		switch (apiGetChanType(handle, chan)) {
		case DmctWbk512:
		case DmctWbk512_10V:
		case DmctWbk516:

		case DmctWbk512A:
		case DmctWbk516A:

		case DmctWbk10:
		case DmctWbk10_10V:
		case DmctWbk10A:
		case DoctWbk11:
		case DoctWbk12:
		case DoctWbk12A:
		case DoctWbk13:
		case DoctWbk13A:

		case DmctWbk14:
			*(float *) info = 10.0F;
			return (DerrNoError);
		default:
			return apiCtrlProcessError(handle, DerrInvGain);
		}
		break;
	case DdiWgcX20Info:
		switch (apiGetChanType(handle, chan)) {
		case DmctWbk512:
		case DmctWbk512_10V:
		case DmctWbk516:

		case DmctWbk512A:
		case DmctWbk516A:

		case DmctWbk10:
		case DmctWbk10_10V:
		case DmctWbk10A:
			switch (session[handle].chanOptions[chan].chanOptionType) {
			case DoctWbk11:
			case DoctWbk12:
			case DoctWbk12A:
			case DoctWbk13:

			case DoctWbk13A:
				*(float *) info = 20.0F;
				return (DerrNoError);
			case DoctPga516:
				if (apiGetChanType(handle, chan) == DmctWbk10A) {
					*(float *) info = 20.0F;
					return (DerrNoError);
				}
			default:
				return apiCtrlProcessError(handle, DerrInvGain);
			}
		case DmctWbk14:
			*(float *) info = 20.0F;
			return (DerrNoError);
		default:
			return apiCtrlProcessError(handle, DerrInvGain);
		}
		break;

	case DdiWgcX50Info:
		switch (apiGetChanType(handle, chan)) {
		case DmctWbk512:
		case DmctWbk512_10V:
		case DmctWbk516:

		case DmctWbk512A:
		case DmctWbk516A:

		case DmctWbk10:
		case DmctWbk10_10V:
		case DmctWbk10A:

			switch (session[handle].chanOptions[chan].chanOptionType) {
			case DoctWbk11:
			case DoctWbk12:
			case DoctWbk12A:
			case DoctWbk13:
			case DoctWbk13A:
				*(float *) info = 50.0F;
				return (DerrNoError);
			default:
				return apiCtrlProcessError(handle, DerrInvGain);
			}
		case DmctWbk14:
			*(float *) info = 50.0F;
			return (DerrNoError);
		default:
			return apiCtrlProcessError(handle, DerrInvGain);
		}
		break;
	case DdiWgcX100Info:
		switch (apiGetChanType(handle, chan)) {
		case DmctWbk512:
		case DmctWbk512_10V:
		case DmctWbk516:

		case DmctWbk512A:
		case DmctWbk516A:

		case DmctWbk10:
		case DmctWbk10_10V:
		case DmctWbk10A:
			switch (session[handle].chanOptions[chan].chanOptionType) {
			case DoctWbk11:
			case DoctWbk12:
			case DoctWbk12A:
			case DoctWbk13:
			case DoctWbk13A:
				*(float *) info = 100.0F;
				return (DerrNoError);
			default:
				return apiCtrlProcessError(handle, DerrInvGain);
			}
		case DmctWbk14:
			*(float *) info = 100.0F;
			return (DerrNoError);
		default:
			return apiCtrlProcessError(handle, DerrInvGain);
		}
		break;
	case DdiWgcX200Info:
		switch (apiGetChanType(handle, chan)) {
		case DmctWbk14:
			*(float *) info = 200.0F;
			return (DerrNoError);

		case DmctWbk10A:
			switch (session[handle].chanOptions[chan].chanOptionType) {
			case DoctWbk11:
			case DoctWbk12:
			case DoctWbk12A:
			case DoctWbk13:
			case DoctWbk13A:
				*(float *) info = 100.0F;
				return (DerrNoError);
			default:
				return apiCtrlProcessError(handle, DerrInvGain);
			}
		default:
			return apiCtrlProcessError(handle, DerrInvGain);
		}
		break;

	case DdiNVRAMDateInfo:
		infoRequest->dtEnum = AhiDate;
		infoRequest->option = 0;
		infoRequest->usrPtr = (DWORD) info;
		infoRequest->chan = chan;
		err = itfExecuteCommand(handle, &sb, ADC_HWINFO);
		if (err != DerrNoError)
			return apiCtrlProcessError(handle, err);
		break;

	case DdiNVRAMTimeInfo:
		infoRequest->dtEnum = AhiTime;
		infoRequest->option = 0;
		infoRequest->usrPtr = (DWORD) info;
		infoRequest->chan = chan;
		err = itfExecuteCommand(handle, &sb, ADC_HWINFO);
		if (err != DerrNoError)
			return apiCtrlProcessError(handle, err);
		break;

	case DdiOptNVRAMDateInfo:
		infoRequest->dtEnum = AhiDate;
		infoRequest->option = 1;
		infoRequest->usrPtr = (DWORD) info;
		infoRequest->chan = chan;
		err = itfExecuteCommand(handle, &sb, ADC_HWINFO);
		if (err != DerrNoError)
			return apiCtrlProcessError(handle, err);
		break;

	case DdiOptNVRAMTimeInfo:
		infoRequest->dtEnum = AhiTime;
		infoRequest->option = 1;
		infoRequest->usrPtr = (DWORD) info;
		infoRequest->chan = chan;
		err = itfExecuteCommand(handle, &sb, ADC_HWINFO);
		if (err != DerrNoError)
			return apiCtrlProcessError(handle, err);
		break;

	case DdiSerialNumber:
		infoRequest->dtEnum = AhiSerialNumber;
		infoRequest->option = 0;
		infoRequest->usrPtr = (DWORD) info;
		infoRequest->chan = chan;
		err = itfExecuteCommand(handle, &sb, ADC_HWINFO);
		if (err != DerrNoError)
			return apiCtrlProcessError(handle, err);
		break;

	case DdiFirmwareVersion:
		infoRequest->dtEnum = AhiFirmwareVer;
		infoRequest->option = 0;
		infoRequest->usrPtr = (DWORD) info;
		infoRequest->chan = chan;
		err = itfExecuteCommand(handle, &sb, ADC_HWINFO);
		if (err != DerrNoError)
			return apiCtrlProcessError(handle, err);
		break;

	case DdiHardwareVersion:
		infoRequest->dtEnum = AhiHardwareVer;
		infoRequest->option = 0;
		infoRequest->usrPtr = (DWORD) info;
		infoRequest->chan = chan;
		err = itfExecuteCommand(handle, &sb, ADC_HWINFO);
		if (err != DerrNoError)
			return apiCtrlProcessError(handle, err);
		break;

	case DdiDriverVersion:
		{
			daqIOT vsb;
			DDVER_PT versionInfo = (DDVER_PT) & vsb;

			CHAR idStr[255] = { 0 };

			err = apiCtrlTestHandle(handle, DlmNone);
			if (err != DerrNoError)
				return apiCtrlProcessError(handle, err);

			versionInfo->errorCode = 0;
			versionInfo->idStr = &idStr[0];
			versionInfo->idStrLen = 200;
			versionInfo->idWritten = 0;
			versionInfo->version = 0;

			err = itfExecuteCommand(handle, &vsb, DDVER);
			if (err != DerrNoError)
				return apiCtrlProcessError(handle, err);

			sprintf((PCHAR) info, "%.2d.%.2d.%.2d",
				((versionInfo->version & 0x00FF0000) >> 16),
				((versionInfo->version & 0x0000FF00) >> 8),
				( versionInfo->version & 0x000000FF));
		}
		break;

	case DdiDbk4SetBaselineInfo:
	case DdiDbk4ExcitationInfo:
	case DdiDbk4ClockInfo:
	case DdiDbk4GainInfo:
	case DdiDbk4MaxFreqInfo:
		switch (apiGetChanType(handle, chan)) {
		case DaetDbk4:
			switch (whichInfo) {
			case DdiDbk4SetBaselineInfo:
				*(PBYTE) info =
				    ((Dbk4OptionStructT *) (session[handle].
							    chanOptions[chan].
							    optionStruct))->setBaseline;
				break;
			case DdiDbk4ExcitationInfo:
				*(PBYTE) info =
				    ((Dbk4OptionStructT *) (session[handle].
							    chanOptions[chan].
							    optionStruct))->excitation;

				break;
			case DdiDbk4ClockInfo:
				*(PBYTE) info =
				    ((Dbk4OptionStructT *) (session[handle].
							    chanOptions[chan].optionStruct))->clock;
				break;
			case DdiDbk4GainInfo:
				*(DaqAdcGain *) info =
				    ((Dbk4OptionStructT *) (session[handle].
							    chanOptions[chan].optionStruct))->gain;
				break;
			case DdiDbk4MaxFreqInfo:

				*(PBYTE) info =
				    ((Dbk4OptionStructT *) (session[handle].
							    chanOptions[chan].
							    optionStruct))->maxFreq;
				break;
			default:
				break;
			}
			break;
		default:
			return apiCtrlProcessError(handle, DerrInvOptionType);
		}
		break;

	case DdiDbk7SlopeInfo:
	case DdiDbk7DebounceTimeInfo:
	case DdiDbk7MinFreqInfo:
	case DdiDbk7MaxFreqInfo:
		switch (apiGetChanType(handle, chan)) {
		case DaetDbk7:
			switch (whichInfo) {
			case DdiDbk7SlopeInfo:
				*(PBYTE) info =
				    ((Dbk7OptionStructT *) (session[handle].
							    chanOptions[chan].optionStruct))->slope;
				break;
			case DdiDbk7DebounceTimeInfo:
				*(PBYTE) info =
				    ((Dbk7OptionStructT *) (session[handle].
							    chanOptions[chan].
							    optionStruct))->debounceTime;
				break;
			case DdiDbk7MinFreqInfo:
				*(float *) info =
				    ((Dbk7OptionStructT *) (session[handle].
							    chanOptions[chan].
							    optionStruct))->minFreq;
				break;
			case DdiDbk7MaxFreqInfo:
				*(float *) info =
				    ((Dbk7OptionStructT *) (session[handle].
							    chanOptions[chan].
							    optionStruct))->maxFreq;
				break;
			default:
				break;
			}
			break;
		default:
			return apiCtrlProcessError(handle, DerrInvOptionType);
		}
		break;

	case DdiWbk12FilterCutOffInfo:
	case DdiWbk12FilterTypeInfo:
	case DdiWbk12FilterModeInfo:
	case DdiWbk12PreFilterModeInfo:
	case DdiWbk12PreFilterCutOffInfo:
	case DdiWbk12PostFilterCutOffInfo:
	case DdiWbk13FilterCutOffInfo:

	case DdiWbk13FilterTypeInfo:
	case DdiWbk13FilterModeInfo:
	case DdiWbk13PreFilterModeInfo:
	case DdiWbk13PreFilterCutOffInfo:
	case DdiWbk13PostFilterCutOffInfo:
		switch (session[handle].chanOptions[chan].chanOptionType) {
		case DoctWbk12:
		case DoctWbk13:
		case DoctWbk12A:
		case DoctWbk13A:

			switch (whichInfo) {

			case DdiWbk12PreFilterCutOffInfo:
			case DdiWbk13PreFilterCutOffInfo:
				err =
				    getWbkChanOption(handle, chan, DcotWbk12PreFilterCutOff, info);
				*(float *) info /= 1000.0f;
				break;

			case DdiWbk12PostFilterCutOffInfo:
			case DdiWbk13PostFilterCutOffInfo:
				err =
				    getWbkChanOption(handle, chan, DcotWbk12PostFilterCutOff, info);
				*(float *) info /= 1000.0f;

				break;

			case DdiWbk12FilterCutOffInfo:
			case DdiWbk13FilterCutOffInfo:
				err = getWbkChanOption(handle, chan, DcotWbk12FilterCutOff, info);
				*(float *) info /= 1000.0f;
				break;

			case DdiWbk12FilterTypeInfo:
			case DdiWbk13FilterTypeInfo:
				err = getWbkChanOption(handle, chan, DcotWbk12FilterType, info);
				break;

			case DdiWbk12FilterModeInfo:
			case DdiWbk13FilterModeInfo:
				err = getWbkChanOption(handle, chan, DcotWbk12FilterMode, info);
				break;

			case DdiWbk12PreFilterModeInfo:
			case DdiWbk13PreFilterModeInfo:
				err = getWbkChanOption(handle, chan, DcotWbk12PreFilterMode, info);
				break;
			default:
				break;
			}

			if (err != DerrNoError)
				return apiCtrlProcessError(handle, err);

			break;
		default:
			return apiCtrlProcessError(handle, DerrInvOptionType);
		}
		break;

	case DdiWbk14LowPassModeInfo:
	case DdiWbk14LowPassCutOffInfo:
	case DdiWbk14HighPassCutOffInfo:
	case DdiWbk14CurrentSrcInfo:
	case DdiWbk14PreFilterModeInfo:
	case DdiWbk14ExcSrcWaveformInfo:
	case DdiWbk14ExcSrcFreqInfo:
	case DdiWbk14ExcSrcAmplitudeInfo:
	case DdiWbk14ExcSrcOffsetInfo:
		switch (apiGetChanType(handle, chan)) {
		case DmctWbk14:
			switch (whichInfo) {
			case DdiWbk14LowPassModeInfo:
				err = getWbkChanOption(handle, chan, DcotWbk14LowPassMode, info);
				break;
			case DdiWbk14LowPassCutOffInfo:
				err = getWbkChanOption(handle, chan, DcotWbk14LowPassCutOff, info);
				*(float *) info /= 1000.0f;
				break;
			case DdiWbk14HighPassCutOffInfo:
				err = getWbkChanOption(handle, chan, DcotWbk14HighPassCutOff, info);
				*(float *) info /= 1000.0f;
				break;
			case DdiWbk14CurrentSrcInfo:
				err = getWbkChanOption(handle, chan, DcotWbk14CurrentSrc, info);
				break;
			case DdiWbk14PreFilterModeInfo:
				err = getWbkChanOption(handle, chan, DcotWbk14PreFilterMode, info);
				break;
			case DdiWbk14ExcSrcWaveformInfo:
				err = getWbkChanOption(handle, chan, DmotWbk14ExcSrcWaveform, info);
				break;
			case DdiWbk14ExcSrcFreqInfo:
				err = getWbkChanOption(handle, chan, DmotWbk14ExcSrcFreq, info);
				*(float *) info /= 1000.0f;
				break;
			case DdiWbk14ExcSrcAmplitudeInfo:
				err =
				    getWbkChanOption(handle, chan, DmotWbk14ExcSrcAmplitude, info);
				*(float *) info /= 1000.0f;
				break;
			case DdiWbk14ExcSrcOffsetInfo:
				err = getWbkChanOption(handle, chan, DmotWbk14ExcSrcOffset, info);
				*(float *) info /= 1000.0f;
				break;
			default:
				break;
			}
			break;
		default:
			return apiCtrlProcessError(handle, DerrInvOptionType);
		}
		break;

	case DdiDetectSensor:
		switch (apiGetChanType(handle, chan)) {
		case DmctWbk14:
			err = getWbkChanOption(handle, chan, DcotWbk14DetectSensor, info);
			break;
		default:
			return apiCtrlProcessError(handle, DerrInvOptionType);
		}
		break;

	case DdiDbk50GainInfo:
		switch (apiGetChanType(handle, chan)) {
		case DaetDbk50:
			*(DaqAdcGain *) info =
			    ((Dbk50OptionStructT *) (session[handle].
						     chanOptions[chan].optionStruct))->gain;
			break;
		default:
			return apiCtrlProcessError(handle, DerrInvChan);

		}
		break;

	case DdiPreTrigFreqInfo:
	case DdiPostTrigFreqInfo:
	case DdiPreTrigPeriodInfo:
	case DdiPostTrigPeriodInfo:

		err = adcTestConfig(handle);
		if (err != DerrNoError)
			return err;

		infoRequest->dtEnum = AhiCycles;

		err = itfExecuteCommand(handle, &sb, ADC_HWINFO);
		if (err != DerrNoError)
			return apiCtrlProcessError(handle, err);

		switch (whichInfo) {

		case DdiPreTrigFreqInfo:
			dblVal = Million / (double) sb.p1;
			break;
		case DdiPostTrigFreqInfo:
			dblVal = Million / (double) sb.p2;
			break;
		case DdiPreTrigPeriodInfo:
			dblVal = Thousand * (double) sb.p1;
			break;
		case DdiPostTrigPeriodInfo:
			dblVal = Thousand * (double) sb.p2;
			break;
		default:
			break;
		}
		memcpy(info, &dblVal, sizeof (dblVal));
		break;

	case DdiExtFeatures:{
			PDWORD pdwExtFeatures = (PDWORD) info;

			infoRequest->dtEnum = AhiExtFeatures;
			if (DerrNoError != (err = itfExecuteCommand(handle, &sb, ADC_HWINFO))) {
				return apiCtrlProcessError(handle, err);
			}

			*pdwExtFeatures = 0;

			if (BIT_TST(infoRequest->chanType, AhefWbk30)) {
				BIT_SET(*pdwExtFeatures,
					DhefFifoOverflowMode | DhefFifoCycleMode
					| DhefFifoDataCount);
			}

			if (IsWBK516X((DaqHardwareVersion) session[handle].chanOptions[1].chanType)) {
				BIT_SET(*pdwExtFeatures,
					DhefTrigDigPattern | DhefTrigPulseInput
					| DhefAcqClkExternal);
			}

			if (IsWBK51_A((DaqHardwareVersion) session[handle].chanOptions[1].chanType)) {
				BIT_SET(*pdwExtFeatures, DhefSyncMaster | DhefSyncSlave);
			}

		}
		break;

	case DdiFifoSize:{
			PDWORD pdwSize = (PDWORD) info;
			infoRequest->dtEnum = AhiFifoSize;
			if (DerrNoError != (err = itfExecuteCommand(handle, &sb, ADC_HWINFO))) {
				return apiCtrlProcessError(handle, err);
			}
			*pdwSize = infoRequest->chanType;
		}
		break;

	case DdiFifoCount:{
			PDWORD pdwCount = (PDWORD) info;
			*pdwCount = 0;
			infoRequest->dtEnum = AhiFifoCount;
			if (DerrNoError != (err = itfExecuteCommand(handle, &sb, ADC_HWINFO))) {
				return apiCtrlProcessError(handle, err);
			}
			*pdwCount = infoRequest->chanType;

		}
		break;

	case DdiAdcClockSource:

		daqAdcGetClockSource(handle, (DaqAdcClockSource *) info);

		break;

	case DdiAdcTriggerScan:

		{
			ADC_STATUS_PT adcStat = (ADC_STATUS_PT) & sb;
			if (DerrNoError != (err = itfExecuteCommand(handle, &sb, ADC_STATUS))) {
				return apiCtrlProcessError(handle, err);
			}
			*(PDWORD) info = adcStat->adcTriggerScan;
		}
		break;

	case DdiAdcPreTriggerCount:

		{
			ADC_STATUS_PT adcStat = (ADC_STATUS_PT) & sb;
			if (DerrNoError != (err = itfExecuteCommand(handle, &sb, ADC_STATUS))) {
				return apiCtrlProcessError(handle, err);
			}
			*(PDWORD) info = adcStat->adcPreTriggerCount;
		}
		break;

	case DdiAdcPostTriggerCount:

		{
			ADC_STATUS_PT adcStat = (ADC_STATUS_PT) & sb;
			if (DerrNoError != (err = itfExecuteCommand(handle, &sb, ADC_STATUS))) {
				return apiCtrlProcessError(handle, err);
			}
			*(PDWORD) info = adcStat->adcPostTriggerCount;
		}
		break;

	case DdiAdcLastRawTransferCount:
		{
			*(PDWORD) info = AdcRawTransferCount(handle);
		}
		break;

	default:
		return apiCtrlProcessError(handle, DerrNotCapable);
	}

	return DerrNoError;
}

static DaqError
SetBaseUnitOption(DaqHandleT handle, DaqOptionType optionType, float optionValue)
{
	DaqError err;

	ID_LINK_T HwOptMap[] = {
		{DbotFifoOverflowMode, DoptFifoOverflowMode},
		{DbotFifoCycleMode, DoptFifoCycleMode},
		{DbotFifoCycleSize, DoptFifoCycleSize},
		{DbotFifoFlush, DoptFifoFlush},
		{DbotFifoNoFlushOnDisarm, DoptFifoNoFlushOnDisarm}
	};

	daqIOT sb;
	SET_OPTION_PT pb = (SET_OPTION_PT) & sb;
	pb->errorCode = 0;
	pb->dwValue = (DWORD) optionValue;

	if (!MapApiToIoctl(optionType, (PINT) & pb->doptType, HwOptMap, NUM(HwOptMap))) {
		return apiCtrlProcessError(handle, DerrInvOptionType);
	}

	dbg("SetOption: (type,value) = %d, %d", pb->doptType, pb->dwValue);

	err = itfExecuteCommand(handle, &sb, SET_OPTION);
	if (DerrNoError != err) {
		return apiCtrlProcessError(handle, err);
	}

	return DerrNoError;
}
