////////////////////////////////////////////////////////////////////////////
//
// apiadc.c                        I    OOO                           h
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
#include <unistd.h>
#include <linux/limits.h>
#include <sys/mman.h>
#endif

#include <stdio.h>
#include <assert.h>
#define DEBUG
#include "iottypes.h"

#include "daqx.h"
#include "cmnapi.h"
#include "api.h"

#include "ddapi.h"
#include "itfapi.h"
#include "apicvt.h"

#define  MaxScanSeqSize          512
#define  MaxAdcFifoHalfFullSize  8192
#define  MinAdcFifoHalfFullSize  256
#define  MaxTrigChannels         256

#define API_ADC_DEBUG 1


typedef struct {
	ADC_CONFIG_T adcConfig;
	SCAN_SEQ_T adcScanSeq[MaxScanSeqSize];
	BOOL adcChanged;

	ADC_START_T adcXfer;
	ADC_STATUS_T adcStat;

	BOOL adcXferActive;
	BOOL adcAcqActive;

	CHAR adcFileName[PATH_MAX];
	DaqAdcOpenMode adcFileOpenMode;
	DWORD adcFilePreWrite;
	DaqHandleT adcFileHandle;

	DWORD adcLastRetCount;
	DWORD adcSamplesProcessed;
	DaqHandleT adcProcessingThreadHandle;
	BOOL perChannelProcessing;
	BOOL HW2sComplement;

	DaqHandleT adcEventR3;
	HGLOBAL tmpMemory;
	WORD *tmpMemoryP;
	DWORD headBufPtr;
	DWORD tailBufPtr;
	DWORD distHeadToTail;

	PWORD adcXferDrvrBuf;
	DaqHardwareVersion deviceType;
	ADC_ACQSTAT_T adcAcqStat;
	DaqAdcPostProcDataFormatT postProcFormat;
	TCType postProcTcType;
	DaqEnhTrigSensT trigSensitivity;
	TRIG_EVENT_T triggerEventList[MaxTrigChannels];
} ApiAdcT, *ApiAdcPT;

static ApiAdcT session[MaxSessions];

static DWORD adcTrigLevel[MaxTrigChannels];
static DWORD  adcTrigHyst[MaxTrigChannels];

#define MaxTrigEnhChans 72

typedef struct {
	DaqAdcTriggerSource triggerSources[MaxTrigEnhChans];
	DaqAdcGain gains[MaxTrigEnhChans];
	DaqAdcRangeT adcRanges[MaxTrigEnhChans];
	DaqEnhTrigSensT trigSensitivity[MaxTrigEnhChans];
	DWORD level[MaxTrigEnhChans];
	DWORD hysteresis[MaxTrigEnhChans];
	DWORD channels[MaxTrigEnhChans];
	CHAR opStr[MaxTrigEnhChans];
} ApiTrigEnhT, *ApiTrigEnhPT;

static ApiTrigEnhT trigEnh[MaxSessions];

float FAR DaqGainArrayAnalogLocal[] = { 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 0.0f };
float FAR DaqGainArrayDigitalLocal[] = { 1.0f, 0.0f };
float FAR DaqGainArrayDigitalExp[] = { 1.0f, 0.0f };
float FAR DaqGainArrayCounterLocal[] = { 1.0f, 0.0f };
float FAR DaqGainArrayDBK1[] = { 1.0f, 0.0f };
float FAR DaqGainArrayDBK4[] = { 1.0f, 10.0f, 100.0f, 1000.0f, 0.0f };
float FAR DaqGainArrayDBK6[] = { 1.0f, 4.0f, 16.0f, 64.0f, 0.0f };
float FAR DaqGainArrayDBK7[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK8[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK9[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK12[] = { 1.0f, 2.0f, 4.0f, 8.0f, 0.0f };
float FAR DaqGainArrayDBK13[] = { 1.0f, 10.0f, 100.0f, 1000.0f, 0.0f };
float FAR DaqGainArrayDBK14[] = { 1.0f, 10.0f, 100.0f, 1000.0f, 0.0f };
float FAR DaqGainArrayDBK15[] = { 1.0f, 2.0f, 1.0f, 2.0f, 0.0f };
float FAR DaqGainArrayDBK16[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK17[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK18[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK19[] = { 60.0f, 90.0f, 180.0f, 240.0f, 0.0f };
float FAR DaqGainArrayDBK20[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK21[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK23[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK24[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK25[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK42[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK43[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK44[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK45[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK50[] = { 1.0f, 0.5f, 0.05f, 0.0166667f, 0.0f };
float FAR DaqGainArrayDBK51[] = { 1.0f, 50.0f, 5.0f, 0.5f, 0.0f };
float FAR DaqGainArrayDBK52[] = { 60.0f, 90.0f, 180.0f, 240.0f, 0.0f };
float FAR DaqGainArrayDBK53[] = { 1.0f, 2.0f, 4.0f, 8.0f, 0.0f };
float FAR DaqGainArrayDBK54[] = { 1.0f, 10.0f, 100.0f, 1000.0f, 0.0f };
float FAR DaqGainArrayDBK56[] = { 1.0f, 10.0f, 100.0f, 1000.0f, 0.0f };
float FAR DaqGainArrayDBK70[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK80[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK81[] = { 99.915f, 99.915f, 99.915f, 99.915f, 0.0f };
float FAR DaqGainArrayDBK82[] = { 99.915f, 99.915f, 99.915f, 99.915f, 0.0f };
float FAR DaqGainArrayDBK207[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayDBK208[] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
float FAR DaqGainArrayNONE[] = { 0.0f, 0.0f, 0.0f, 0.0f };

DaqChannelType DaqTypeDBKMin = DaqTypeDBK1;
DaqChannelType DaqTypeDBKMax = DaqTypeDBK208;

GainInfoT FAR GainInfoStruct[] = {
	{DaqTypeAnalogLocal, DaqGainArrayAnalogLocal},
	{DaqTypeDigitalLocal, DaqGainArrayDigitalLocal},
	{DaqTypeDigitalExp, DaqGainArrayDigitalExp},
	{DaqTypeCounterLocal, DaqGainArrayCounterLocal},
	{DaqTypeDBK1, DaqGainArrayDBK1},
	{DaqTypeDBK4, DaqGainArrayDBK4},
	{DaqTypeDBK6, DaqGainArrayDBK6},
	{DaqTypeDBK7, DaqGainArrayDBK7},
	{DaqTypeDBK8, DaqGainArrayDBK8},
	{DaqTypeDBK9, DaqGainArrayDBK9},
	{DaqTypeDBK12, DaqGainArrayDBK12},
	{DaqTypeDBK13, DaqGainArrayDBK13},
	{DaqTypeDBK14, DaqGainArrayDBK14},
	{DaqTypeDBK15, DaqGainArrayDBK15},
	{DaqTypeDBK16, DaqGainArrayDBK16},
	{DaqTypeDBK17, DaqGainArrayDBK17},
	{DaqTypeDBK18, DaqGainArrayDBK18},
	{DaqTypeDBK19, DaqGainArrayDBK19},
	{DaqTypeDBK20, DaqGainArrayDBK20},
	{DaqTypeDBK21, DaqGainArrayDBK21},
	{DaqTypeDBK23, DaqGainArrayDBK23},
	{DaqTypeDBK24, DaqGainArrayDBK24},
	{DaqTypeDBK25, DaqGainArrayDBK25},
	{DaqTypeDBK42, DaqGainArrayDBK42},
	{DaqTypeDBK43, DaqGainArrayDBK43},
	{DaqTypeDBK44, DaqGainArrayDBK44},
	{DaqTypeDBK45, DaqGainArrayDBK45},
	{DaqTypeDBK50, DaqGainArrayDBK50},
	{DaqTypeDBK51, DaqGainArrayDBK51},
	{DaqTypeDBK52, DaqGainArrayDBK52},
	{DaqTypeDBK53, DaqGainArrayDBK53},
	{DaqTypeDBK54, DaqGainArrayDBK54},
	{DaqTypeDBK56, DaqGainArrayDBK56},
	{DaqTypeDBK70, DaqGainArrayDBK70},
	{DaqTypeDBK80, DaqGainArrayDBK80},
	{DaqTypeDBK81, DaqGainArrayDBK81},
	{DaqTypeDBK82, DaqGainArrayDBK82},
	{DaqTypeDBK207, DaqGainArrayDBK207},
	{DaqTypeDBK208, DaqGainArrayDBK208},
	{DaqTypeNONE, DaqGainArrayNONE},
};

static DaqError adcOpenDiskFile(DaqHandleT handle);

static DWORD XferToScanCount(DWORD dwXferCount, DWORD dwScanLen);
static DWORD ScanToXferCount(DWORD dwScanCount, DWORD dwScanLen);

DWORD AdcRawTransferCount(DaqHandleT handle);

//static DaqError getGainVal(DaqHandleT handle, DaqAdcGain gainCode,
//			   DaqChannelType chanType, float * val);

//static DaqError WaitForAcqActivePolled(DaqHandleT handle, DWORD dwTimeout,
//				       const BOOL bActive, PBOOL pbTimedOut);

static DaqError daqCalcTriggerVals(DaqHandleT handle,
				   DaqAdcTriggerSource trigSource,
				   DaqEnhTrigSensT trigSensitivity,
				   DWORD channel,
				   DaqAdcGain gainCode,
				   DWORD flags,
				   DaqChannelType channelType,
				   float level,
				   float variance, PDWORD pCalcLevel, PDWORD pCalcVariance);

static DaqError daqCalcTCTriggerVals(DaqHandleT handle,
				     DaqEnhTrigSensT trigSensitivity,
				     DWORD channel,
				     DaqAdcGain gainCode,
				     DWORD flags,
				     float gainVal,
				     DaqChannelType channelType,
				     float level,
				     float variance, float * pfCalcLevel, float * pfCalcVariance);

VOID
apiAdcMessageHandler(MsgPT msg)
{
	DWORD i;

        assert(sizeof(ADC_STATUS_PT) <= sizeof(daqIOT) );

	if (msg->errCode == DerrNoError) {
		switch (msg->msg) {
		case MsgAttach:
			dbg("In MsgAttach");
                        
			for (i = 0; i < MaxSessions; i++) {
				session[i].adcConfig.adcScanSeq = session[i].adcScanSeq;
				session[i].adcConfig.triggerEventList =
					session[i].triggerEventList;


				session[i].adcProcessingThreadHandle = NO_GRIP;
				session[i].adcFileHandle = NO_GRIP;
				session[i].adcXferActive = FALSE;
				session[i].adcAcqActive = FALSE;
				session[i].adcConfig.adcDataPack = DardfNative;
				session[i].postProcFormat = DappdfRaw;
                                session[i].adcConfig.adcTrigLevel = adcTrigLevel;
                                session[i].adcConfig.adcTrigHyst =  adcTrigHyst;
			}
			break;


		case MsgDetach:
			dbg("In MsgDetach");
			for (i = 0; i < MaxSessions; i++) {


				if (session[i].adcProcessingThreadHandle != NO_GRIP)
					CloseHandle(session[i].adcProcessingThreadHandle);
				if (session[i].adcFileHandle != NO_GRIP)
					CloseHandle(session[i].adcFileHandle);
			}
			if (session[i].tmpMemory) {

				(VOID) GlobalUnlock(session[i].tmpMemory);
				(VOID) GlobalFree(session[i].tmpMemory);
				session[i].tmpMemoryP = NULL;
			}
			break;

		case MsgInit:
			dbg("In MsgInit");
            
			session[msg->handle].deviceType = msg->deviceType;
			session[msg->handle].adcStat.errorCode = DerrNoError;

			switch (session[msg->handle].deviceType) {
			case DaqBoard2003:
				return;
				break;
			default:
				break;
			}

			daqAdcDisarm(msg->handle);
			daqAdcTransferStop(msg->handle);
			daqAdcSetDataFormat(msg->handle, DardfNative, DappdfRaw);

			daqAdcSetDiskFile(msg->handle, "", DaomCreateFile, 0);

			daqAdcSetClockSource(msg->handle, DacsAdcClock);
			daqAdcSetAcq(msg->handle, DaamInfinitePost, 0, 1);
			daqAdcSetTrig(msg->handle, DatsSoftware, TRUE, 0, 0, 0);

			if (IsWaveBook(msg->deviceType))
            {
				daqAdcSetMux(msg->handle, 1, 1, DgainX1, DafBipolar);
			}
            //MAH: 04.05.04 1000 added
            else if ((IsDaq2000(msg->deviceType) || IsDaq1000(msg->deviceType)))
            {
				daqAdcSetMux(msg->handle, 0, 0, DgainX1, DafP3Local16);
			}
            else
				daqAdcSetMux(msg->handle, 0, 0, DgainX1, DafBipolar);
                
			daqAdcSetFreq(msg->handle, 1000.0f);
			daqAdcArm(msg->handle);

			daqAdcDisarm(msg->handle);

			switch (session[msg->handle].deviceType) {
			case DaqBook100:
			case DaqBook112:
			case DaqBook120:
			case Daq112:
			case Daq216:
			case TempBook66:
				session[msg->handle].HW2sComplement = FALSE;
				break;

			case DaqBook200:
			case DaqBook216:
			case DaqBoard100:
			case DaqBoard112:
			case DaqBoard200:
			case DaqBoard216:
			case WaveBook512:
			case WaveBook516:
			case WaveBook512A:
			case WaveBook516A:
			case PersonalDaq56:
			case WaveBook516_250:
			case WaveBook512_10V:
			case DaqBoard2000:
			case DaqBoard2001:
			case DaqBoard2002:
			case DaqBoard2004:
			case DaqBoard2005:
            case DaqBoard1000:
			case DaqBoard1005:
			default:
				session[msg->handle].HW2sComplement = TRUE;
				break;
			}
			break;

		case MsgClose:
			dbg("In MsgClose");
			switch (session[msg->handle].deviceType) {
			case DaqBoard2003:
				return;
				break;
			default:
				break;
			}
			daqAdcDisarm(msg->handle);
			daqAdcTransferStop(msg->handle);
			break;
		}
	}
	return;
}

static DaqError
adcOpenDiskFile(DaqHandleT handle)
{

	if (session[handle].adcFileHandle != NO_GRIP)
		CloseHandle(session[handle].adcFileHandle);

	if (session[handle].adcFileName[0] != 0) {
		DWORD createFlag;
		DWORD fileSize, fileSizeHigh;

		switch (session[handle].adcFileOpenMode) {
		case DaomCreateFile:
			createFlag = CREATE_NEW;
			break;

		case DaomWriteFile:
			createFlag = CREATE_ALWAYS;
			break;

		case DaomAppendFile:
			createFlag = OPEN_ALWAYS;
			break;
		}

		session[handle].adcFileHandle =
		    CreateFile(session[handle].adcFileName, GENERIC_WRITE, 0,
			       0, createFlag, FILE_ATTRIBUTE_NORMAL, 0);

		if (session[handle].adcFileHandle == NO_GRIP)
			return DerrFileOpenError;

		fileSize = GetFileSize(session[handle].adcFileHandle, &fileSizeHigh);

		if (session[handle].adcFilePreWrite) {
			static CHAR buf[512];
			DWORD preWrite, count, written;

			preWrite = session[handle].adcFilePreWrite *
			    session[handle].adcConfig.adcScanLength * sizeof (WORD);

			count = sizeof (buf);

			while (preWrite) {
				if (preWrite < count)
					count = preWrite;

				if (!WriteFile
				    (session[handle].adcFileHandle, buf, count, &written, NULL))
					return DerrFileWriteError;

				preWrite -= count;
			}

			FlushFileBuffers(session[handle].adcFileHandle);
		}

		SetFilePointer(session[handle].adcFileHandle, fileSize,
			       (PLONG) & fileSizeHigh, FILE_BEGIN);
	} else {
		session[handle].adcFileHandle = NO_GRIP;
	}

	return DerrNoError;

}

static DaqError
adcWriteDiskFile(DaqHandleT handle, PWORD buf, DWORD sampleCount)
{

	if (session[handle].adcFileHandle != NO_GRIP) {
		DWORD written;

		if (!WriteFile
		    (session[handle].adcFileHandle, buf,
		     sampleCount * sizeof (WORD), &written, NULL))

			return DerrFileWriteError;
	}

	return DerrNoError;

}

static DaqError
adcCloseDiskFile(DaqHandleT handle)
{
	if (session[handle].adcFileHandle != NO_GRIP) {

		SetEndOfFile(session[handle].adcFileHandle);
		FlushFileBuffers(session[handle].adcFileHandle);

		CloseHandle(session[handle].adcFileHandle);
		session[handle].adcFileHandle = NO_GRIP;
	}

	return DerrNoError;

}

static DaqError
processNewDataBlock(DaqHandleT handle, PWORD buf, DWORD samplesToProcess)
{
	DaqError err;
	DWORD i, chanIndex;
	DWORD iTemp;

	chanIndex = 0;

	if (session[handle].perChannelProcessing) {
		for (i = 0; i < samplesToProcess; i++, chanIndex++) {
			if (chanIndex == session[handle].adcConfig.adcScanLength)
				chanIndex = 0;

			if ((session[handle].adcScanSeq[chanIndex].flags & DafSigned)
			    && (session[handle].HW2sComplement == FALSE)) {
				iTemp = buf[i];
				iTemp -= 32768;
				buf[i] = (SHORT) iTemp;
			}

			if ((session[handle].adcScanSeq[chanIndex].flags & DafShiftLSNibble)
			    && (session[handle].adcScanSeq[chanIndex].flags & DafHighSpeedDigital)) {
				buf[i] >>= 8;
			}

			else if ((session[handle].adcScanSeq[chanIndex].flags & DafClearLSNibble)
				 && (session[handle].adcScanSeq[chanIndex].
				     flags & DafHighSpeedDigital))
				buf[i] &= 0xFF00;

			else if (session[handle].adcScanSeq[chanIndex].flags & DafShiftLSNibble)
				buf[i] >>= 4;

			else if (session[handle].adcScanSeq[chanIndex].flags & DafClearLSNibble)
				buf[i] &= 0xFFF0;
		}

	}

	err = adcWriteDiskFile(handle, buf, samplesToProcess);
	if (err != DerrNoError)
		return err;

	return DerrNoError;
}

static DaqError
processNewData(DaqHandleT handle, DWORD adcXferCount)
{

	DaqError err;
	DWORD oldXferCount = session[handle].adcStat.adcXferCount;
	DWORD samplesToProcess = adcXferCount - oldXferCount;
	DWORD realSamplesToProcess;
	DWORD scanHead;
	DWORD scanTail = session[handle].adcAcqStat.adcAcqStatTail;
	DWORD scanLen = session[handle].adcConfig.adcScanLength;
	DWORD realBufLenScans = session[handle].adcXfer.adcXferBufLen;
	DWORD realBufLenSamples =
	    (session[handle].adcXfer.adcXferBufLen * session[handle].adcConfig.adcScanLength);
	DWORD bufLen;
	DWORD OverRunOK = FALSE;

	if (session[handle].adcStat.adcXferCount > adcXferCount) {
		adcXferCount +=
		    (0xFFFFFFFF / session[handle].adcConfig.adcScanLength) -
		    session[handle].adcStat.adcXferCount;
		samplesToProcess = adcXferCount;
	}

	if (samplesToProcess < 1) {
		return DerrNoError;
	}

	if (session[handle].adcConfig.adcDataPack == DardfPacked) {
		bufLen =
		    (session[handle].adcXfer.adcXferBufLen *
		     session[handle].adcConfig.adcScanLength * 3) / 4;
		realSamplesToProcess = samplesToProcess * 4 / 3;
	} else {
		bufLen =
		    session[handle].adcXfer.adcXferBufLen * session[handle].adcConfig.adcScanLength;
	}

	if ((samplesToProcess > bufLen)
	    && !(session[handle].adcXfer.adcXferMode & DatmIgnoreOverruns)) {

		if (session[handle].perChannelProcessing
		    || (session[handle].adcFileHandle != NO_GRIP)) {

			return DerrOverrun;
		}

	} else if (samplesToProcess) {

		PWORD buf = (PWORD) session[handle].adcXfer.adcXferBuf;

		oldXferCount %= bufLen;

		if (oldXferCount + samplesToProcess > bufLen) {

			err =
			    processNewDataBlock(handle, buf + oldXferCount, bufLen - oldXferCount);
			if (err != DerrNoError)
				return err;

			err =
			    processNewDataBlock(handle, buf,
						oldXferCount + samplesToProcess - bufLen);
			if (err != DerrNoError)
				return err;
		} else {
			err = processNewDataBlock(handle, buf + oldXferCount, samplesToProcess);
			if (err != DerrNoError)
				return err;
		}

		if (((realSamplesToProcess +
		      (session[handle].adcAcqStat.adcAcqStatScansAvail *
		       scanLen)) > realBufLenSamples)
		    && (session[handle].adcXfer.adcXferMode & DatmDriverBuf)) {
			if (session[handle].adcXfer.adcXferMode & DatmIgnoreOverruns) {
				OverRunOK = TRUE;
			} else {
				return DerrOverrun;
			}
		}

		scanHead =
		    (session[handle].adcAcqStat.adcAcqStatHead +
		     (realSamplesToProcess / scanLen)) % realBufLenScans;

		session[handle].adcAcqStat.adcAcqStatHead = scanHead;

		if ((scanTail == scanHead)
		    || (OverRunOK == TRUE)
		    || (session[handle].adcAcqStat.adcAcqStatScansAvail == realBufLenScans)) {
			session[handle].adcAcqStat.adcAcqStatScansAvail = realBufLenScans;
			session[handle].adcAcqStat.adcAcqStatSamplesAvail = realBufLenSamples;
		} else {
			if (scanTail > scanHead) {

				session[handle].adcAcqStat.
				    adcAcqStatScansAvail = (realBufLenScans - scanTail) + scanHead;

			} else {
				session[handle].adcAcqStat.
				    adcAcqStatScansAvail = scanHead - scanTail;

			}
		}
	}
	session[handle].adcSamplesProcessed += samplesToProcess;

	return DerrNoError;
}                    

static VOID
adcGetStatAndProcessData(DaqHandleT handle, PDWORD pActive, PDWORD pScanCount)
{

	daqIOT sb;
        memset( (void*) &sb, 0, sizeof(sb));
	ADC_STATUS_PT adcStat = (ADC_STATUS_PT) & sb;
	ApiAdcPT ps = &session[handle];
	DaqError err;
	BOOL bDataPacked;
	DWORD dwSampleCount;

        dbg("WELCOME TO adcGetStatAndProcessData\n");

	bDataPacked = (DardfPacked == ps->adcConfig.adcDataPack);

	if (NULL != pActive) {
		adcStat->adcAcqStatus = *pActive;
	} else {
                adcStat->adcAcqStatus = 0;
        }
	if (NULL != pScanCount) {
		adcStat->adcXferCount = ScanToXferCount(*pScanCount, ps->adcConfig.adcScanLength);
	} else {
               adcStat->adcXferCount = 0;
        }

	dbg("(OUT) %d",adcStat->adcXferCount);

	err = itfExecuteCommand(handle, &sb, ADC_STATUS);
    
	dbg("(IN) %d, 0x%X", adcStat->adcXferCount, adcStat->adcAcqStatus);

	adcStat->adcXferCount /= ps->adcConfig.adcScanLength;
	adcStat->adcXferCount *= ps->adcConfig.adcScanLength;

	dwSampleCount = adcStat->adcXferCount;

	if (!err) {

		if (bDataPacked) {
			adcStat->adcXferCount = adcStat->adcXferCount * 3 / 4;
		}
		err = processNewData(handle, adcStat->adcXferCount);

	}

	if (ps->adcStat.errorCode == DerrNoError && err != DerrNoError) {
		ps->adcStat.errorCode = err;
		adcStat->errorCode = err;
	}

#ifdef DEBUG
	if (ps->adcStat.errorCode != DerrNoError)
		dbg("adcGetStatAndProcessData got error %d from the driver's adcGetStatus",
		     ps->adcStat.errorCode);
#endif

	if (ps->adcXferActive && !(adcStat->adcAcqStatus & DaafTransferActive)) {
		ps->adcXferActive = FALSE;
	}

	if (ps->adcAcqActive) {
		DWORD sta = adcStat->adcAcqStatus;
		if (!BIT_TST(sta, DaafAcqActive)){
			err = adcCloseDiskFile(handle);
			if (err != DerrNoError) {
				adcStat->errorCode = err;
			}

			ps->adcAcqActive = FALSE;
		}
	}

	ps->adcStat.adcXferCount = adcStat->adcXferCount;
	ps->adcStat.adcAcqStatus = adcStat->adcAcqStatus;

	if (NULL != pActive) {
		*pActive = adcStat->adcAcqStatus;
	}
	if (NULL != pScanCount) {
		*pScanCount = XferToScanCount(dwSampleCount, ps->adcConfig.adcScanLength);

	}
}

DWORD
waitForStatusChgEvent(VOID * pHandle, DWORD timeout, DaqHandleT eventHandle)
{
	return DerrNoError;
}

static DaqError
adcTransferGetStat(DaqHandleT handle, PDWORD pdwActive, PDWORD pdwScanCount)
{
	DaqError err;

	ApiAdcPT ps = &session[handle];


//    MAH:06.11.03 THIS IS WHERE THE BOMB LIVES 
//	if (ps->adcProcessingThreadHandle != NO_GRIP) {
    
		adcGetStatAndProcessData(handle, pdwActive, pdwScanCount);
   
//    }
//    else
//    {
//    fprintf(stderr,"NO_GRIP \n");
//    }


	err = (DaqError) ps->adcStat.errorCode;
	ps->adcStat.errorCode = DerrNoError;

	if ((NULL != pdwScanCount) && (DatmDriverBuf & ps->adcXfer.adcXferMode)) {

		*pdwScanCount = ps->adcAcqStat.adcAcqStatScansAvail;
	}

	return err;
}

DaqError
apiAdcTransferGetFlag(DaqHandleT handle, DaqTransferEvent event, PBOOL eventSet)
{

	DaqError err;
	DWORD acqStatus= 0; 
        DWORD xferCount=0;

	cmnWaitForMutex(AdcStatMutex);

	err = adcTransferGetStat(handle, &acqStatus, &xferCount);
	cmnReleaseMutex(AdcStatMutex);

	*eventSet = ((acqStatus & DaafTransferActive) == 0) &&
	    (session[handle].adcProcessingThreadHandle == NO_GRIP);

	if (event == DteAdcData) {
		*eventSet |= (xferCount > session[handle].adcLastRetCount);
	}
	return err;
}

DaqError
apiAdcSendSmartDbkData(DaqHandleT handle, DWORD address, DWORD chanCount,
		       PBYTE data, DWORD dataCount)
{

	DaqError err;

	daqIOT sb;
        memset( (void *) &sb, 0, sizeof(sb));
	ADC_CONFIG_PT adcConfig = (ADC_CONFIG_PT) & sb;
	SCAN_SEQ_T *adcScanSeq;
	TRIG_EVENT_T *triggerEventList;
	WORD buf[2 * MaxScanSeqSize] = { 10, 20, 30, 40, 50, 60, 70, 80, 90 };

	DWORD i;
	DWORD active, retCount;
	CHAR tmpFileName[PATH_MAX];

	daqIOT ssb;
        memset( (void *) &ssb, 0, sizeof(ssb));
	ADC_START_PT adcXfer = (ADC_START_PT) & ssb;

	if (session[handle].adcAcqActive){
		dbg("DerrAcqArmed error");
		return DerrAcqArmed;
	}

	if (session[handle].adcXferActive)
		return DerrMultBackXfer;

	session[handle].adcStat.errorCode = DerrNoError;

	if ((dataCount == 0) || (dataCount + 9 > MaxScanSeqSize)){
		dbg("dataCount invalid: 0 < %d <= (MaxScanSeqSize-9) (=%i)",
		    dataCount, MaxScanSeqSize-9);
		return DerrInvCount;
	}

	adcScanSeq = (SCAN_SEQ_T *) malloc(MaxScanSeqSize * sizeof (SCAN_SEQ_T));

	triggerEventList = (TRIG_EVENT_T *) malloc(MaxTrigChannels * sizeof (TRIG_EVENT_T));

	for (i = 0; i < 9; i++) {
		adcScanSeq[i].channel = 16;
		adcScanSeq[i].gain = 0;

		adcScanSeq[i].flags = DafSSHSample | DafBipolar;
		adcScanSeq[i].reserved = 0;
	}

	adcScanSeq[1].flags = DafSSHHold | DafBipolar;
	adcScanSeq[3].flags = DafSSHHold | DafBipolar;
	adcScanSeq[6].gain = 0x01;
	adcScanSeq[7].channel = (16 + (chanCount & 0x0f));
	adcScanSeq[8].channel = (16 + (address & 0x0f));

	for (i = 0; i < dataCount; i++) {
		adcScanSeq[i + 9].channel = 16 + (data[i] & 0x0f);
		adcScanSeq[i + 9].gain = ((data[i] >> 4) & 0x03);
		adcScanSeq[i + 9].flags = DafSSHSample | DafBipolar;
		adcScanSeq[i + 9].reserved = 0;
	}

	if ((dataCount + 9) % 2) {

		adcScanSeq[dataCount + 9].channel = adcScanSeq[dataCount + 8].channel;
		adcScanSeq[dataCount + 9].gain = adcScanSeq[dataCount + 8].gain;
		adcScanSeq[dataCount + 9].flags = adcScanSeq[dataCount + 8].flags;
		adcScanSeq[dataCount + 9].reserved = adcScanSeq[dataCount + 8].reserved;

		dataCount++;
	}

	adcConfig->adcScanSeq = adcScanSeq;
	adcConfig->adcScanLength = dataCount + 9;

	triggerEventList[0].TrigCount = 0;
	triggerEventList[0].TrigNumber = 0;
	triggerEventList[0].TrigEvent = DaqStartEvent;

	triggerEventList[0].TrigSens = (DaqEnhTrigSensT) 0;
	triggerEventList[0].TrigLevel = 0;
	triggerEventList[0].TrigVariance = 0;
	triggerEventList[0].TrigLocation = 0;

	triggerEventList[0].TrigChannel = 0;
	triggerEventList[0].TrigGain = (DaqAdcGain) 0;
	triggerEventList[0].Trigflags = 0;

	adcConfig->adcAcqPreCount = 0;
	adcConfig->adcAcqPostCount = 1 * adcConfig->adcScanLength;
	adcConfig->adcAcqMode = DaamNShot;

	adcConfig->adcTrigLevel = 0;
        dbg("adcConfig->adcTrigLevel = NULL");
	adcConfig->adcTrigHyst = 0;
	adcConfig->adcTrigChannel = 0;
	adcConfig->adcEnhTrigCount = 0;
	adcConfig->adcDataPack = 0;

    //MAH: 04.05.04 added 1000
    if (!(IsDaq2000(session[handle].deviceType) || IsDaq1000(session[handle].deviceType)))
    {

		adcConfig->adcPeriodmsec = session[handle].adcConfig.adcPeriodmsec;
		adcConfig->adcPeriodnsec = session[handle].adcConfig.adcPeriodnsec;

		adcConfig->adcTrigSource = DatsSoftware;
		triggerEventList[0].TrigSource = DatsSoftware;
		adcConfig->adcClockSource = DacsTriggerSource;
	} else {

		adcConfig->adcPeriodmsec = 10000;

		adcConfig->adcPeriodnsec = 0;

		adcConfig->adcTrigSource = DatsImmediate;
		triggerEventList[0].TrigSource = DatsImmediate;
		adcConfig->adcClockSource = DacsAdcClockDiv2;
	}

	adcConfig->triggerEventList = triggerEventList;

	err = itfExecuteCommand(handle, &sb, ADC_CONFIG);

	if (!err) {
		session[handle].adcLastRetCount = 0;
		session[handle].adcStat.errorCode = DerrNoError;
		session[handle].adcStat.adcXferCount = 0;
		session[handle].adcAcqStat.adcAcqStatHead = 0;
		session[handle].adcAcqStat.adcAcqStatTail = 0;
		session[handle].adcAcqStat.adcAcqStatScansAvail = 0;
		session[handle].adcAcqStat.adcAcqStatSamplesAvail = 0;

		session[handle].adcXferActive = TRUE;
		session[handle].adcProcessingThreadHandle = NO_GRIP;

		adcXfer->adcXferBufLen = adcConfig->adcScanLength;
		adcXfer->adcXferMode = DatmCycleOff | DatmUpdateSingle;
		adcXfer->adcXferBuf = buf;


		err = itfExecuteCommand(handle, &ssb, ADC_START);
	}

	if (!err) {

		strcpy(tmpFileName, session[handle].adcFileName);
		strcpy(session[handle].adcFileName, "");

		session[handle].adcChanged = FALSE;

		err = daqAdcArm(handle);

		session[handle].adcChanged = TRUE;

		strcpy(session[handle].adcFileName, tmpFileName);
	}

	if (!err)
        //MAH: 04.05.04 added 1000
		if (!(IsDaq2000(session[handle].deviceType) || IsDaq1000(session[handle].deviceType)))
        {

			err = daqAdcSoftTrig(handle);
		}

	if (!err)

		err = daqWaitForEvent(handle, DteAdcDone);

	if (err != DerrNoError)
		dbg("apiAdcSendSmartDbkData after daqWaitForEvent with error %d.", err);

	daqAdcTransferGetStat(handle, &active, &retCount);

	daqAdcDisarm(handle);

	daqAdcTransferStop(handle);

	free(adcScanSeq);
	free(triggerEventList);

	return err;
}

DaqError
apiAdcReadSample(DaqHandleT handle, DWORD chan, DaqAdcGain gain, DWORD flags,
		 DWORD avgCount, PWORD data)
{

	DaqError err;
	daqIOT sb, ssb;

        memset( (void *) &sb, 0, sizeof(sb));
        memset( (void *) &ssb, 0, sizeof(ssb));
	ADC_CONFIG_PT adcConfig = (ADC_CONFIG_PT) & sb;
	ADC_START_PT adcXfer = (ADC_START_PT) & ssb;

	SCAN_SEQ_T *adcScanSeq;
	TRIG_EVENT_T *triggerEventList;
	WORD *buf;
	DWORD active, retCount, x;
	CHAR tmpFileName[PATH_MAX];
	float fTemp = 0.0f;

	if (data != NULL)
		*data = 0;
	else
		return DerrInvBuf;

	if (session[handle].adcAcqActive){
		dbg("DerrAcqArmed error");
		return DerrAcqArmed;
	}

	if (session[handle].adcXferActive)
		return DerrMultBackXfer;

	session[handle].adcStat.errorCode = DerrNoError;

	if ((avgCount < 1) || avgCount > 1000)
		avgCount = 1;

	buf = (WORD *) malloc((avgCount + 4096) * sizeof (WORD));

	adcScanSeq = (SCAN_SEQ_T *) malloc(MaxScanSeqSize * sizeof (SCAN_SEQ_T));

	triggerEventList = (TRIG_EVENT_T *) malloc(MaxTrigChannels * sizeof (TRIG_EVENT_T));

	adcScanSeq[0].channel = chan;

	adcScanSeq[0].gain = gain;
	adcScanSeq[0].flags = flags;
	adcScanSeq[0].reserved = 0;

	adcConfig->adcScanSeq = adcScanSeq;
	adcConfig->adcScanLength = 1;

	triggerEventList[0].TrigCount = 0;
	triggerEventList[0].TrigNumber = 0;
	triggerEventList[0].TrigEvent = DaqStartEvent;

	triggerEventList[0].TrigSens = (DaqEnhTrigSensT) 0;
	triggerEventList[0].TrigLevel = 0;
	triggerEventList[0].TrigVariance = 0;
	triggerEventList[0].TrigLocation = 0;

	triggerEventList[0].TrigChannel = 0;
	triggerEventList[0].TrigGain = (DaqAdcGain) 0;
	triggerEventList[0].Trigflags = 0;

	triggerEventList[0].TrigSource = DatsImmediate;
	adcConfig->adcTrigSource = DatsImmediate;

	adcConfig->triggerEventList = triggerEventList;

	adcConfig->adcAcqPreCount = 0;
	adcConfig->adcAcqPostCount = avgCount * adcConfig->adcScanLength;
	adcConfig->adcAcqMode = DaamNShot;

	adcConfig->adcTrigLevel = 0;
        dbg("adcConfig->adcTrigLevel = NULL");
	adcConfig->adcTrigHyst = 0;
	adcConfig->adcTrigChannel = 0;
	adcConfig->adcEnhTrigCount = 0;
	adcConfig->adcDataPack = 0;

	adcConfig->adcPeriodmsec = 1;
	adcConfig->adcPeriodnsec = 0;

	if (!(IsDaq2000(session[handle].deviceType)|| IsDaq1000(session[handle].deviceType)))
    {
		adcConfig->adcClockSource = DacsAdcClock;
	} else {

		adcConfig->adcClockSource = DacsAdcClockDiv2;
	}

	err = itfExecuteCommand(handle, &sb, ADC_CONFIG);

	if (!err) {
		session[handle].adcLastRetCount = 0;
		session[handle].adcStat.errorCode = DerrNoError;
		session[handle].adcStat.adcXferCount = 0;
		session[handle].adcAcqStat.adcAcqStatHead = 0;
		session[handle].adcAcqStat.adcAcqStatTail = 0;
		session[handle].adcAcqStat.adcAcqStatScansAvail = 0;
		session[handle].adcAcqStat.adcAcqStatSamplesAvail = 0;
		session[handle].adcXferActive = TRUE;
		session[handle].adcProcessingThreadHandle = NO_GRIP;

		adcXfer->adcXferBufLen = avgCount * adcConfig->adcScanLength;
		adcXfer->adcXferMode = DatmCycleOff | DatmUpdateSingle;
		adcXfer->adcXferBuf = buf;


		err = itfExecuteCommand(handle, &ssb, ADC_START);
	}

	if (!err) {

		strcpy(tmpFileName, session[handle].adcFileName);
		strcpy(session[handle].adcFileName, "");
		session[handle].adcChanged = FALSE;
		err = daqAdcArm(handle);
		session[handle].adcChanged = TRUE;
		strcpy(session[handle].adcFileName, tmpFileName);
	}

	if (!err)

		err = daqWaitForEvent(handle, DteAdcDone);
	daqAdcTransferGetStat(handle, &active, &retCount);
	daqAdcDisarm(handle);
	daqAdcTransferStop(handle);

	for (x = 0; x < avgCount; x++) {
		fTemp += buf[x];
	}

	*data = (WORD) (fTemp / avgCount);

	free(adcScanSeq);
	free(triggerEventList);
	free(buf);

	return err;
}

DAQAPI DaqError WINAPI
daqAdcRd(DaqHandleT handle, DWORD chan, PWORD sample, DaqAdcGain gain, DWORD flags)
{
	if ((IsDaq2000(session[handle].deviceType)) || (IsDaq1000(session[handle].deviceType)))
    {
		return daqAdcRdScanN(handle, chan, chan, sample, 1,
				     DatsImmediate, 0, 0, 200.0E3f, gain, flags);
	} else

		return daqAdcRdScanN(handle, chan, chan, sample, 1,
				     DatsImmediate, 0, 0, 1000.0f, gain, flags);
}

DAQAPI DaqError WINAPI
daqAdcRdScan(DaqHandleT handle, DWORD startChan, DWORD endChan, PWORD buf,
	     DaqAdcGain gain, DWORD flags)
{
	if ((IsDaq2000(session[handle].deviceType)) || (IsDaq1000(session[handle].deviceType)))
    {
		return daqAdcRdScanN(handle, startChan, endChan, buf, 1,
				     DatsImmediate, 0, 0,
				     (float) (200.0E3f /
					      max(1.0f,
						  (float) (endChan - startChan))), gain, flags);
	} else
		return daqAdcRdScanN(handle, startChan, endChan, buf, 1,
				     DatsImmediate, 0, 0, 1000.0f, gain, flags);

}

DAQAPI DaqError WINAPI
daqAdcRdN(DaqHandleT handle, DWORD chan, PWORD buf, DWORD scanCount,
	  DaqAdcTriggerSource triggerSource, BOOL rising, WORD level,
	  float freq, DaqAdcGain gain, DWORD flags)
{

	return daqAdcRdScanN(handle, chan, chan, buf, scanCount,
			     triggerSource, rising, level, freq, gain, flags);

}

DAQAPI DaqError WINAPI
daqAdcRdScanN(DaqHandleT handle, DWORD startChan, DWORD endChan, PWORD buf,
	      DWORD scanCount, DaqAdcTriggerSource triggerSource, BOOL rising,
	      WORD level, float freq, DaqAdcGain gain, DWORD flags)
{

#define  USE_DRIVER_BUF DatmDriverBuf

	DaqError err;
	PWORD rawBuf;
	DWORD updateSingle;

	err = daqAdcSetMux(handle, startChan, endChan, gain, flags);
	if (err != DerrNoError)
		return err;

	if (triggerSource == DatsSoftware)
		triggerSource = DatsImmediate;

	if ((triggerSource < DatsImmediate)
	    || (triggerSource == DatsSoftware)
	    || (triggerSource > DatsHardwareAnalog))
		return apiCtrlProcessError(handle, DerrInvTrigSource);

	err = daqAdcSetTrig(handle, triggerSource, rising, level, level, 0);
	if (err != DerrNoError)
		return err;

	err = daqAdcSetAcq(handle, DaamNShot, 0, scanCount);
	if (err != DerrNoError)
		return err;

	err = daqAdcSetFreq(handle, freq);
	if (err != DerrNoError)
		return err;

	updateSingle = 0;

	if ((scanCount == 1)
	    || (freq < (2048 / session[handle].adcConfig.adcScanLength)))
		updateSingle = DatmUpdateSingle;

	dbg("updateSingle = %d, scanCount = %d.", updateSingle, scanCount);

	switch (session[handle].postProcFormat) {
	case DappdfTenthsDegC:
		rawBuf =
		    (PWORD) malloc(session[handle].adcConfig.adcAcqPostCount *
				   session[handle].adcConfig.adcScanLength * sizeof (WORD));
		if (!rawBuf)
			return apiCtrlProcessError(handle, DerrMemAlloc);
		err =
		    daqAdcTransferSetBuffer(handle, rawBuf, scanCount,
					    updateSingle | USE_DRIVER_BUF);
		break;
	default:

		err =
		    daqAdcTransferSetBuffer(handle, buf, scanCount, updateSingle | USE_DRIVER_BUF);
	}
	if (err != DerrNoError)
		return err;

	err = daqAdcTransferStart(handle);
	if (err != DerrNoError)
		return err;

	err = daqAdcArm(handle);
	if (err != DerrNoError) {
		daqAdcDisarm(handle);

		daqAdcTransferStop(handle);
		return err;
	}

	err = daqWaitForEvent(handle, DteAdcDone);

	if (err != DerrNoError) {
		dbg("DAQX:daqAdcRdScanN after daqWaitForEvent with error %d.", err);
	} else {

		if (USE_DRIVER_BUF) {

			DWORD retCount = 0;
			switch (session[handle].postProcFormat) {
			case DappdfTenthsDegC:

				err =
				    daqAdcTransferBufData(handle, rawBuf,
							  scanCount, DabtmNoWait, &retCount);
				break;
			default:

				err =
				    daqAdcTransferBufData(handle, buf,
							  scanCount, DabtmNoWait, &retCount);
			}
		}
	}
	daqAdcDisarm(handle);
	daqAdcTransferStop(handle);

	if (err != DerrNoError) {
		return err;
	}

	switch (session[handle].postProcFormat) {
	case DappdfTenthsDegC:
		daqZeroDbk19(TRUE);
		err =  daqCvtTCSetupConvert(session[handle].adcConfig.
					 adcScanLength, 2,
					 session[handle].adcConfig.
					 adcScanLength - 3,
					 session[handle].postProcTcType,
					 flags & DafBipolar, 1, rawBuf,
					 scanCount, (PSHORT) buf,
					 scanCount * session[handle].adcConfig.adcScanLength - 3);
		free(rawBuf);
		break;
	default:
		break;
	}

	return err;
}



static VOID
copyScan(PWORD dest, PWORD source, DWORD scanSize)
{
	while (scanSize--)
		*dest++ = *source++;

}

DAQAPI DaqError WINAPI
daqAdcBufferRotate(DaqHandleT handle, PWORD buf, DWORD scanCount, DWORD chanCount, DWORD retCount)
{

	DWORD passes, gcf, temp, pass;
	DWORD get, put, end, pivot;
	WORD tempScan[128];

	if (retCount > scanCount) {

		pivot = retCount % scanCount;

		if (pivot) {

			passes = scanCount;
			gcf = pivot;

			while (gcf) {
				temp = gcf;
				gcf = passes % gcf;
				passes = temp;
			}

			end = scanCount - pivot;

			for (pass = 0; pass < passes; ++pass) {

				copyScan(tempScan, &buf[pass * chanCount], chanCount);
				put = pass;
				get = pass + pivot;

				do {
					copyScan(&buf[put * chanCount],
						 &buf[get * chanCount], chanCount);
					put = get;

					if (get >= end) {
						get -= end;
					} else {
						get += pivot;
					}
				}
				while (get != pass);
				copyScan(&buf[put * chanCount], tempScan, chanCount);
			}
		}
	}
	return DerrNoError;
}

DAQAPI DaqError WINAPI
daqAdcCalcTrig(DaqHandleT handle, BOOL bipolar, float gainVal,
	       float voltageLevel, PWORD triggerLevel)
{
	DaqError err;
	float ADmin, ADmax, info, level, Vhyst;
	DWORD optType;
	BOOL rising;
	DaqHardwareVersion devType;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	daqGetHardwareInfo(handle, DhiADmin, &info);
	ADmin = info;

	daqGetHardwareInfo(handle, DhiADmax, &info);
	ADmax = info;

	daqGetInfo(handle, 1, DdiChOptionTypeInfo, (PVOID) & optType);

	devType = session[handle].deviceType;

	switch (devType) {

	case WaveBook512:
	case WaveBook512_10V:
		{
			int nScaleFactor = (devType == WaveBook512) ? 1 : 2;

			float DAmin, DAmax;
			SHORT sLevel;
			switch (optType) {
			case DoctWbk11:
			case DoctWbk12:
			case DoctWbk12A:
			case DoctWbk13:
			case DoctWbk13A:

				DAmin = -5.046f / gainVal * nScaleFactor;
				DAmax = 10.136f / gainVal * nScaleFactor;
				Vhyst = .025f / gainVal * nScaleFactor;
				break;

			case DoctPga516:
			default:
				DAmin = -5.046f * nScaleFactor;
				DAmax = 10.119f * nScaleFactor;
				Vhyst = .025f * nScaleFactor;
			}

			rising = session[handle].adcConfig.adcTrigSource & RisingEdgeFlag;
			voltageLevel += (rising ? -Vhyst : Vhyst);
			sLevel =
			    (SHORT) ((voltageLevel - DAmin) / (DAmax -
							       DAmin) * 4095.0f - 2048.0f + 0.5f);

			if ((sLevel < -2048.0f) || (sLevel > 2047.0f))
				return apiCtrlProcessError(handle, DerrInvLevel);

			*triggerLevel = (WORD) sLevel;
		}
		break;

	case WaveBook516:
	case WaveBook516_250:

	case WaveBook512A:
	case WaveBook516A:
		{
			float DAmin, DAmax;
			SHORT sLevel;
			switch (optType) {
			case DoctWbk11:

			case DoctWbk12:
			case DoctWbk12A:
			case DoctWbk13:
			case DoctWbk13A:

				DAmin = -12.638f / gainVal;

				DAmax = 17.662f / gainVal;
				Vhyst = 0.025f / gainVal;
				break;

			case DoctPga516:
			default:

				DAmin = -10.0f;
				DAmax = 10.0f;
				Vhyst = 0.025f;
				break;
			}

			rising = session[handle].adcConfig.adcTrigSource & RisingEdgeFlag;
			voltageLevel += (rising ? -Vhyst : Vhyst);
			sLevel =
			    (SHORT) ((voltageLevel - DAmin) / (DAmax -
							       DAmin) * 4095.0f - 2048.0f + 0.5f);

			if ((sLevel < -2048.0f) || (sLevel > 2047.0f))

				return apiCtrlProcessError(handle, DerrInvLevel);

			*triggerLevel = (WORD) sLevel;
		}
		break;

	default:
		level = voltageLevel * gainVal;
		if (bipolar)

			level += (ADmax - ADmin) / 2.0f;
		level *= 65536.0f / (ADmax - ADmin);

		if ((level < 0.0f) || (level > 65535.0f))
			return apiCtrlProcessError(handle, DerrInvLevel);

		*triggerLevel = (WORD) level;
	}
	return DerrNoError;
}

DAQAPI DaqError WINAPI
daqAdcGetFreq(DaqHandleT handle, float * freq)
{

	DaqError err;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	*freq = 1.0f / (session[handle].adcConfig.adcPeriodmsec / 1000.0f +
			session[handle].adcConfig.adcPeriodnsec / 1000000000.0f);

	return DerrNoError;
}

DAQAPI DaqError WINAPI
daqAdcSetFreq(DaqHandleT handle, float freq)
{

	DaqError err;
	double period;
	DWORD nsec, msec;
	float actualValue;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	if (session[handle].adcAcqActive){
		dbg("DerrAcqArmed error");
		return apiCtrlProcessError(handle, DerrAcqArmed);
	}

	if ((freq <= 0) || (freq > 1000000000.0f))
		return apiCtrlProcessError(handle, DerrInvFreq);

	period = 1.0 / freq;
	period += 0.0000000005;
	period *= 1000.0;
	msec = (DWORD) period;
	period -= (double) msec;
	period *= 1000000.0;
	nsec = (DWORD) period;

	session[handle].adcConfig.adcPeriodnsec = nsec;
	session[handle].adcConfig.adcPeriodmsec = msec;

	session[handle].adcChanged = TRUE;

	daqAdcSetRate(handle, DarmFrequency, DaasPostTrig, freq, &actualValue);

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqAdcSetRate(DaqHandleT handle, DaqAdcRateMode mode, DaqAdcAcqState state,
	      float reqValue, float * actualValue)
{
	daqIOT sb;
	DaqError err;
        memset((void *) &sb, 0, sizeof(sb));
	ADC_CONFIG_PT adcConfig = (ADC_CONFIG_PT) & sb;
        dbg("sizeof(adcConfig) = %ld, sizeof(sb) = %ld", sizeof(*adcConfig),  sizeof(sb));

	double period;
	DWORD nsec, msec;

	DaqHardwareVersion info;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	daqGetHardwareInfo(handle, DhiHardwareVersion, (PINT) & info);

	if ((DaasPreTrig == state) && !IsWaveBook(info)) {
		return apiCtrlProcessError(handle, DerrParamConflict);
	}

	if (session[handle].adcAcqActive){
		dbg("DerrAcqArmed error");
		return apiCtrlProcessError(handle, DerrAcqArmed);
	}

	if (mode == DarmFrequency) {
		if ((reqValue <= 0.0f) || (reqValue > 1000000.0f))
			return apiCtrlProcessError(handle, DerrInvFreq);

		period = 1.0 / reqValue;
		period += 0.0000000005;
		period *= 1000.0;
		msec = (DWORD) period;
		period -= (double) msec;
		period *= 1000000.0;
		nsec = (DWORD) period;
	} else if (mode == DarmPeriod) {

		if (reqValue == 0.0f || (reqValue < 1000.0f || reqValue > 100000000000.0f))
			return apiCtrlProcessError(handle, DerrInvFreq);

		period = reqValue / 1000000.0f;
		msec = (DWORD) period;

		nsec = (DWORD) reqValue % 1000000;
	} else if (mode == DarmExtClockPacer) {
		if ((reqValue <= 0.0f) || (reqValue > 255))
			return apiCtrlProcessError(handle, DerrInvFreq);

		msec = (DWORD) 0;
		nsec = (DWORD) reqValue;
	} else {
		return apiCtrlProcessError(handle, DerrParamConflict);
	}

	if (state == DaasPreTrig) {
		adcConfig->adcPreTPeriodns = nsec;
		adcConfig->adcPreTPeriodms = msec;
	} else {
		adcConfig->adcPeriodnsec = nsec;
		adcConfig->adcPeriodmsec = msec;
	}

	session[handle].adcChanged = TRUE;

	adcConfig->adcClockSource = session[handle].adcConfig.adcClockSource;
	adcConfig->adcAcqMode = session[handle].adcConfig.adcAcqMode;
	adcConfig->adcAcqPreCount = session[handle].adcConfig.adcAcqPreCount *
	    session[handle].adcConfig.adcScanLength;
	adcConfig->adcAcqPostCount = session[handle].adcConfig.adcAcqPostCount *
	    session[handle].adcConfig.adcScanLength;
	adcConfig->adcTrigSource = session[handle].adcConfig.adcTrigSource;
	adcConfig->adcTrigLevel = session[handle].adcConfig.adcTrigLevel;
        dbg("adcConfig->adcTrigLevel = session[%ld] ", handle);
	adcConfig->adcTrigHyst = session[handle].adcConfig.adcTrigHyst;

	adcConfig->adcTrigChannel = session[handle].adcConfig.adcTrigChannel;
	adcConfig->adcScanSeq = session[handle].adcConfig.adcScanSeq;
	adcConfig->adcScanLength = session[handle].adcConfig.adcScanLength;
	adcConfig->adcEnhTrigCount = 0;
	adcConfig->adcDataPack = 0;

	adcConfig->adcEnhTrigSens = session[handle].adcConfig.adcEnhTrigSens;

	adcConfig->triggerEventList = session[handle].adcConfig.triggerEventList;

	if (!IsWaveBook(session[handle].deviceType)) {
		adcConfig->adcAcqMode = DaamNShot;
		adcConfig->adcAcqPreCount = 0;

		adcConfig->adcAcqPostCount = session[handle].adcConfig.adcScanLength;
		adcConfig->adcTrigSource = DatsImmediate;
		adcConfig->adcTrigLevel = 0;
		adcConfig->adcTrigHyst = 0;
		adcConfig->adcTrigChannel = 0;
	}
#if APIADC_DEBUG
	DWORD i;
	dbg("adcClockSource  = %d", adcConfig->adcClockSource);
	dbg("adcAcqMode      = %d", adcConfig->adcAcqMode);
	dbg("adcAcqPreCount  = %d", adcConfig->adcAcqPreCount);
	dbg("adcAcqPostCount = %d", adcConfig->adcAcqPostCount);
	dbg("adcPeriodmsec   = %d", adcConfig->adcPeriodmsec);
	dbg("adcPeriodnsec   = %d", adcConfig->adcPeriodnsec);
	dbg("adcPreTPeriodms = %d", adcConfig->adcPreTPeriodms);
	dbg("adcPreTPeriodns = %d", adcConfig->adcPreTPeriodns);
	dbg("adcTrigSource   = %d", adcConfig->adcTrigSource);
	dbg("adcTrigLevel    = %d", (SHORT) adcConfig->adcTrigLevel);
	dbg("adcTrigHyst     = %d", adcConfig->adcTrigHyst);
	dbg("adcTrigChannel  = %d", adcConfig->adcTrigChannel);
	dbg("adcDataPack     = %d", adcConfig->adcDataPack);
	dbg("adcScanLength   = %d", adcConfig->adcScanLength);
	for (i = 0; i < adcConfig->adcScanLength; i++) {
		dbg("adcScanSeq      = %d",
			session[handle].adcScanSeq[i]);
	}
	DWORD *enhTrigChan = (DWORD *) session[handle].adcConfig.adcEnhTrigChan;
	DWORD *enhTrigGain = (DWORD *) session[handle].adcConfig.adcEnhTrigGain;
	DWORD *enhTrigBipolar = (DWORD *) session[handle].adcConfig.adcEnhTrigBipolar;
	DWORD *enhTrigSens = (DWORD *) session[handle].adcConfig.adcEnhTrigSens;
	DWORD *trigLevel = (DWORD *) session[handle].adcConfig.adcTrigLevel;
	DWORD *trigHyst = (DWORD *) session[handle].adcConfig.adcTrigHyst;
	dbg("adcEnhTrigCount = %d", adcConfig->adcEnhTrigCount);
	for (i = 0; i < adcConfig->adcEnhTrigCount; i++) {
		dbg("   adcEnhTrigChan    = %d", enhTrigChan[i]);
		dbg("   adcEnhTrigGain    = %d", enhTrigGain[i]);
		dbg("   adcEnhTrigBipolar = %d", enhTrigBipolar[i]);
		dbg("   adcEnhTrigSens    = %d", enhTrigSens[i]);
		dbg("   adcTrigLevel      = %d", trigLevel[i]);
		dbg("   adcTrigHyst       = %d", trigHyst[i]);
	}
	dbg("adcEnhTrigOpcode     = %s",
	    (PCHAR) adcConfig->adcEnhTrigOpcode);
#endif

	err = itfExecuteCommand(handle, &sb, ADC_CONFIG);

	if (!err) {

		if (state == DaasPreTrig) {
			session[handle].adcConfig.adcPreTPeriodns = adcConfig->adcPreTPeriodns;
			session[handle].adcConfig.adcPreTPeriodms = adcConfig->adcPreTPeriodms;
			if (mode & DarmFrequency) {
				period =
				    adcConfig->adcPreTPeriodms * 1000000.0f +
				    adcConfig->adcPreTPeriodns;
				*actualValue = (float) (1.0e9 / period);
			} else {

				*actualValue = (float) adcConfig->adcPreTPeriodns;

				*actualValue += (float) (adcConfig->adcPreTPeriodms * 1000000.0f);
			}
		} else {
			session[handle].adcConfig.adcPeriodnsec = adcConfig->adcPeriodnsec;
			session[handle].adcConfig.adcPeriodmsec = adcConfig->adcPeriodmsec;
			if (mode & DarmFrequency) {

				period =
				    adcConfig->adcPeriodmsec * 1000000.0f +
				    adcConfig->adcPeriodnsec;
				*actualValue = (float) (1.0e9 / period);
			} else {

				*actualValue = (float) adcConfig->adcPeriodnsec;
				*actualValue += (float) (adcConfig->adcPeriodmsec * 1000000.0f);
			}
		}

	} else {

		return apiCtrlProcessError(handle, err);
	}
	return DerrNoError;
}

DAQAPI DaqError WINAPI
daqAdcSetAcq(DaqHandleT handle, DaqAdcAcqMode mode, DWORD preTrigCount, DWORD postTrigCount)
{

	DaqError err;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	if (session[handle].adcAcqActive){
		dbg("DerrAcqArmed error");
		return apiCtrlProcessError(handle, DerrAcqArmed);
	}

	if ((mode < DaamNShot) || (mode > DaamPrePost))
		return apiCtrlProcessError(handle, DerrInvMode);

	if ((mode == DaamNShotRearm) && (postTrigCount == 0)){
		dbg("bad DaamNShotRearm/postTrigCount combination");
		return apiCtrlProcessError(handle, DerrInvCount);
	}

	if (!(IsDaq2000(session[handle].deviceType) || IsDaq1000(session[handle].deviceType)))
    {
		if ((mode == DaamPrePost)
		    && ((preTrigCount == 0) || (postTrigCount == 0))){
			dbg("bad DaamNShotRearm/pre/postTrigCount combination");
			return apiCtrlProcessError(handle, DerrInvCount);
		}
	} else {
		if (mode != DaamPrePost)
			preTrigCount = 0;
	}
	session[handle].adcConfig.adcAcqMode = mode;

	session[handle].adcConfig.adcAcqPreCount = preTrigCount;
	session[handle].adcConfig.adcAcqPostCount = postTrigCount;
	session[handle].adcChanged = TRUE;

	return DerrNoError;
}

DAQAPI DaqError WINAPI
daqAdcSetClockSource(DaqHandleT handle, DaqAdcClockSource clockSource)
{
	DaqError err;    

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	if (session[handle].adcAcqActive){
		dbg("DerrAcqArmed error");
		return apiCtrlProcessError(handle, DerrAcqArmed);
	}

	if (IsWaveBook(session[handle].deviceType)) {
		switch (clockSource) {
		case (DacsAdcClock):
		case (DacsExternalTTL):
			break;
		case (DacsAdcClock | DacsSyncMaster):
		case (DacsExternalTTL | DacsSyncMaster):
		case (DacsExternalTTL | DacsSyncSlave):

			if (!IsWBK51_A(session[handle].deviceType))
				return apiCtrlProcessError(handle, DerrInvClockSource);
			break;
		default:
			return apiCtrlProcessError(handle, DerrInvClockSource);
		}

	}
    else if ((IsDaq2000(session[handle].deviceType)) || (IsDaq1000(session[handle].deviceType)))
    {
		DWORD flagMask = DacsOutputEnable | DacsFallingEdge;
		if (((clockSource & ~flagMask) == DacsGatedAdcClock)
		    || ((clockSource & ~flagMask) == DacsTriggerSource))
			return apiCtrlProcessError(handle, DerrInvClockSource);
	} else {
		if ((clockSource < DacsAdcClock)
		    || (clockSource > DacsTriggerSource))
			return apiCtrlProcessError(handle, DerrInvClockSource);
	}

	session[handle].adcConfig.adcClockSource = clockSource;

	session[handle].adcChanged = TRUE;

	return DerrNoError;
}

DaqError
daqAdcGetClockSource(DaqHandleT handle, DaqAdcClockSource * clockSource)
{

	DaqError err;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	*clockSource = (DaqAdcClockSource) session[handle].adcConfig.adcClockSource;

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqAdcSetTrig(DaqHandleT handle, DaqAdcTriggerSource triggerSource, BOOL rising,
	      WORD level, WORD hysteresis, DWORD channel)
{

	DaqError err;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	if (session[handle].adcAcqActive){
		dbg("DerrAcqArmed error");
		return apiCtrlProcessError(handle, DerrAcqArmed);
	}

	if ((triggerSource < DatsImmediate)
	    || (triggerSource > DatsSoftwareAnalog))
		return apiCtrlProcessError(handle, DerrInvTrigSource);

	session[handle].adcConfig.adcTrigSource = triggerSource;
	if (rising)
		session[handle].adcConfig.adcTrigSource |= RisingEdgeFlag;

	session[handle].adcConfig.adcTrigLevel[0] = level;
	session[handle].adcConfig.adcTrigHyst[0] = hysteresis;
	session[handle].adcConfig.adcTrigChannel = channel;
	session[handle].adcConfig.adcEnhTrigCount = 0;

	session[handle].adcConfig.adcEnhTrigSens = NULL;

	session[handle].triggerEventList[0].TrigCount = 0;
	session[handle].triggerEventList[0].TrigNumber = 0;

	session[handle].triggerEventList[0].TrigEvent = DaqStartEvent;
	session[handle].triggerEventList[0].TrigSource = triggerSource;
	session[handle].triggerEventList[0].TrigSens =
	    (DaqEnhTrigSensT) ((DWORD) (!rising) * DetsFallingEdge);
	session[handle].triggerEventList[0].TrigLevel = level;
	session[handle].triggerEventList[0].TrigVariance = hysteresis;
	session[handle].triggerEventList[0].TrigLocation = channel;
	session[handle].triggerEventList[0].TrigChannel = 0;
	session[handle].triggerEventList[0].TrigGain = (DaqAdcGain) 0;
	session[handle].triggerEventList[0].Trigflags = 0;

	session[handle].adcChanged = TRUE;

	return DerrNoError;
}

DAQAPI DaqError WINAPI
daqAdcSetTrigEnhanced(DaqHandleT handle, DaqAdcTriggerSource * triggerSources,
		      DaqAdcGain * gains, DaqAdcRangeT * adcRanges,
		      DaqEnhTrigSensT * trigSensitivity, float * level,
		      float * hysteresis, PDWORD channels, DWORD chanCount, PCHAR opStr)
{
	DaqError err;
	DWORD i;
	float fGainVal;
	WORD wTriggerLevel, wHysteresisLevel;
	DaqInfo gainInfo;
	BOOL bBipolar;
//	DWORD num_level = 1;
//	DWORD num_hyst = 1;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	if (!IsWaveBook(session[handle].deviceType)) {
		return apiCtrlProcessError(handle, DerrNotCapable);
	}

	if (session[handle].adcAcqActive){
		dbg("DerrAcqArmed error");
		return apiCtrlProcessError(handle, DerrAcqArmed);
	}

	session[handle].triggerEventList[0].TrigCount = 0;

	switch (*triggerSources) {
	case (DatsImmediate):
	case (DatsSoftware):
	case (DatsAdcClock):
	case (DatsGatedAdcClock):
		session[handle].adcConfig.adcTrigSource = *triggerSources;
		session[handle].adcConfig.adcEnhTrigCount = 0;
		break;

	case (DatsExternalTTL):
		session[handle].adcConfig.adcTrigSource = *triggerSources;
		if (*trigSensitivity > DetsFallingEdge)
			return apiCtrlProcessError(handle, DerrParamConflict);

		if (*trigSensitivity == DetsRisingEdge)
			session[handle].adcConfig.adcTrigSource = *triggerSources | RisingEdgeFlag;
		else
			session[handle].adcConfig.adcTrigSource = *triggerSources;

		session[handle].adcConfig.adcEnhTrigCount = 0;
		break;

	case (DatsHardwareAnalog):
		if (IsWBK516X(session[handle].deviceType)) {
			if (!(((*trigSensitivity >= DetsWindowAboveLevelBeforeTime)
			       && (*trigSensitivity <= DetsWindowBelowLevelAfterTime))
			      || (*trigSensitivity <= DetsFallingEdge)))
				return apiCtrlProcessError(handle, DerrParamConflict);
		} else {
			if (*trigSensitivity > DetsFallingEdge)
				return apiCtrlProcessError(handle, DerrParamConflict);
		}
		if (*trigSensitivity == DetsRisingEdge)
			session[handle].adcConfig.adcTrigSource = *triggerSources | RisingEdgeFlag;
		else
			session[handle].adcConfig.adcTrigSource = *triggerSources;

		if (IsWaveBook(session[handle].deviceType)) {

			switch (*gains) {
			case (WgcX1):
				gainInfo = DdiWgcX1Info;
				break;
			case (WgcX2):
				gainInfo = DdiWgcX2Info;
				break;
			case (WgcX5):
				gainInfo = DdiWgcX5Info;
				break;
			case (WgcX10):
				gainInfo = DdiWgcX10Info;
				break;
			case (WgcX20):
				gainInfo = DdiWgcX20Info;
				break;
			case (WgcX50):
				gainInfo = DdiWgcX50Info;
				break;

			case (WgcX100):
				gainInfo = DdiWgcX100Info;
				break;
			case (WgcX200):
				gainInfo = DdiWgcX200Info;
				break;
			default:
				return daqProcessError(handle, DerrInvGain);
			}
			daqGetInfo(handle, *channels, gainInfo, &fGainVal);
		} else {

			switch (*gains) {
			case 0:
				fGainVal = 1.0f;
				break;
			case 1:
				fGainVal = 2.0f;
				break;
			case 2:
				fGainVal = 4.0f;
				break;
			case 3:
				fGainVal = 8.0f;
				break;

			default:
				return daqProcessError(handle, DerrInvGain);
			}
		}

		switch (*adcRanges) {
		case (DarUni0to10V):

			bBipolar = TRUE;
			break;
		case (DarBiMinus5to5V):
			bBipolar = FALSE;
			break;
		default:
			return daqProcessError(handle, DerrInvAdcRange);
		}

		daqAdcCalcTrig(handle, bBipolar, fGainVal, *level, &wTriggerLevel);
		session[handle].adcConfig.adcTrigLevel[0] = wTriggerLevel;

		session[handle].adcConfig.adcEnhTrigSens = trigSensitivity;

		if ((*trigSensitivity >= DetsWindowAboveLevelBeforeTime)
		    && (*trigSensitivity <= DetsWindowBelowLevelAfterTime)) {
			if ((hysteresis[0] < 50.0)
			    || (hysteresis[0] > 800000000.0))
				return apiCtrlProcessError(handle, DerrInvLevel);
			session[handle].adcConfig.adcTrigHyst[0] = (DWORD) (hysteresis[0]);
		} else {

			daqAdcCalcTrig(handle, bBipolar, fGainVal, *hysteresis, &wHysteresisLevel);
			session[handle].adcConfig.adcTrigHyst[0] = wHysteresisLevel;
		}

		session[handle].adcConfig.adcEnhTrigCount = 0;
		break;

	case (DatsSoftwareAnalog):
		chanCount = 1;
	case (DatsEnhancedTrig):
		if (chanCount == 1)
			opStr[0] = '+';

		if (opStr[0] != '*' && opStr[0] != '+')
			return apiCtrlProcessError(handle, DerrParamConflict);

		session[handle].adcConfig.adcEnhTrigCount = chanCount;

		if (*triggerSources == DatsSoftwareAnalog)
			session[handle].adcConfig.adcTrigSource = DatsEnhancedTrig;
		else
			session[handle].adcConfig.adcTrigSource = *triggerSources;

		session[handle].adcConfig.adcTrigLevel = (PDWORD) &adcTrigLevel[0];

		session[handle].adcConfig.adcTrigHyst = (PDWORD) &adcTrigHyst[0];

		for (i = 0; i < chanCount; i++) {
			adcTrigLevel[i] = (DWORD) (level[i] * 1000000.0);

			if ((trigSensitivity[i] >= DetsWindowAboveLevelBeforeTime)
			    && (trigSensitivity[i] <= DetsWindowBelowLevelAfterTime)) {
				if ((hysteresis[0] < 50.0)
				    || (hysteresis[0] > 800000000.0))
					return apiCtrlProcessError(handle, DerrInvLevel);
				adcTrigHyst[i] = (DWORD) (hysteresis[i]);
			} else {
				adcTrigHyst[i] = (DWORD) (hysteresis[i] * 1000000.0);
			}
		}

		{
			DWORD optType;
			for (i = 0; i < chanCount; i++) {
				daqGetInfo(handle, channels[i], DdiChTypeInfo, (PVOID) & optType);
				if (optType == (DWORD) DmctWbk17) {
					if ((WgcDigital8 <= gains[i])
					    && (gains[i] <= WgcCtr32High)) {

						adcTrigLevel[i] =
						    (DWORD) (level[i] * (5000000.0f / 32767.666f));
						if (level[i] < 500)
							adcTrigLevel[i]++;
						adcTrigHyst[i] =
						    (DWORD) (hysteresis[i] *
							     (5000000.0f / 32767.666f));

					} else {
						adcTrigLevel[i] =
						    (DWORD) ((5000000.0f / 75.0f) * level[i]);
						adcTrigHyst[i] =
						    (DWORD) ((5000000.0f / 75.0f) * hysteresis[i]);

					}
				}
			}
		}
		session[handle].adcConfig.adcEnhTrigSens = trigSensitivity;
		session[handle].adcConfig.adcEnhTrigChan = channels;
		session[handle].adcConfig.adcEnhTrigGain = gains;
		session[handle].adcConfig.adcEnhTrigBipolar = (PDWORD) adcRanges;
		session[handle].adcConfig.adcEnhTrigOpcode =  opStr;
		break;

	case (DatsDigPattern):

		if (!IsWBK516X(session[handle].deviceType))

			return apiCtrlProcessError(handle, DerrInvTrigSource);

		if (trigSensitivity[0] > DetsNELevel)
			return apiCtrlProcessError(handle, DerrInvEnhTrig);

		session[handle].adcConfig.adcEnhTrigSens = trigSensitivity;

		session[handle].adcConfig.adcTrigLevel = adcTrigLevel;

		adcTrigLevel[0] = (DWORD) level[0];
		adcTrigLevel[1] = (DWORD) level[1];

		session[handle].adcConfig.adcTrigSource = *triggerSources;
		break;

	case (DatsPulse):

		if (!IsWBK516X(session[handle].deviceType))
			return apiCtrlProcessError(handle, DerrInvTrigSource);

		if ((trigSensitivity[0] < DetsWindowAboveLevelBeforeTime)
		    || (trigSensitivity[0] > DetsWindowBelowLevelAfterTime))
			return apiCtrlProcessError(handle, DerrInvEnhTrig);

		session[handle].adcConfig.adcTrigSource = *triggerSources;
		session[handle].adcConfig.adcEnhTrigCount = 0;

		session[handle].adcConfig.adcEnhTrigSens = trigSensitivity;

		session[handle].adcConfig.adcEnhTrigBipolar = (PDWORD) adcRanges;

		if ((level[0] < -5.0) || (level[0] > 5.0))
			return apiCtrlProcessError(handle, DerrInvLevel);

		adcTrigLevel[0] = (DWORD) (level[0] * 2048 / 5);

		if ((hysteresis[0] < 50.0) || (hysteresis[0] > 800000000.0))
			return apiCtrlProcessError(handle, DerrInvLevel);
		adcTrigHyst[0] = (DWORD) (hysteresis[0]);

		session[handle].adcConfig.adcTrigLevel =  adcTrigLevel;
		session[handle].adcConfig.adcTrigHyst =  adcTrigHyst;
		break;

	default:

		return apiCtrlProcessError(handle, DerrInvTrigSource);

	}

	switch (*triggerSources) {

	case (DatsImmediate):
	case (DatsSoftware):

	case (DatsAdcClock):
	case (DatsGatedAdcClock):
	case (DatsExternalTTL):
	case (DatsHardwareAnalog):

		break;

	case (DatsSoftwareAnalog):
	case (DatsEnhancedTrig):

		for (i = 0; i < session[handle].adcConfig.adcEnhTrigCount; i++) {

			trigEnh[handle].trigSensitivity[i] =
			    session[handle].adcConfig.adcEnhTrigSens[i];
			trigEnh[handle].channels[i] = session[handle].adcConfig.adcEnhTrigChan[i];
			trigEnh[handle].gains[i] = session[handle].adcConfig.adcEnhTrigGain[i];
			trigEnh[handle].adcRanges[i] =
			    (DaqAdcRangeT) session[handle].adcConfig.adcEnhTrigBipolar[i];
			trigEnh[handle].level[i] =
			    *(PDWORD) (session[handle].adcConfig.adcTrigLevel +
				       (i * sizeof (DWORD)));
			trigEnh[handle].hysteresis[i] =
			    *(PDWORD) (session[handle].adcConfig.adcTrigHyst +
				       (i * sizeof (DWORD)));
		}
		trigEnh[handle].opStr[0] = *(PCHAR) session[handle].adcConfig.adcEnhTrigOpcode;

		session[handle].adcConfig.adcEnhTrigSens = trigEnh[handle].trigSensitivity;
		session[handle].adcConfig.adcEnhTrigChan = trigEnh[handle].channels;
		session[handle].adcConfig.adcEnhTrigGain = trigEnh[handle].gains;

		session[handle].adcConfig.adcEnhTrigBipolar = (PDWORD) trigEnh[handle].adcRanges;
		session[handle].adcConfig.adcEnhTrigOpcode =  trigEnh[handle].opStr;
		session[handle].adcConfig.adcTrigLevel =  trigEnh[handle].level;
		session[handle].adcConfig.adcTrigHyst =  trigEnh[handle].hysteresis;
		break;

	case (DatsDigPattern):

		trigEnh[handle].trigSensitivity[0] = session[handle].adcConfig.adcEnhTrigSens[0];
		trigEnh[handle].level[0] = *(PDWORD) (session[handle].adcConfig.adcTrigLevel);
		trigEnh[handle].level[1] =
		    *(PDWORD) (session[handle].adcConfig.adcTrigLevel + sizeof (DWORD));

		session[handle].adcConfig.adcEnhTrigSens = trigEnh[handle].trigSensitivity;
		session[handle].adcConfig.adcTrigLevel =  trigEnh[handle].level;

		break;

	case (DatsPulse):

		trigEnh[handle].trigSensitivity[0] = session[handle].adcConfig.adcEnhTrigSens[0];
		trigEnh[handle].adcRanges[0] =
		    (DaqAdcRangeT) session[handle].adcConfig.adcEnhTrigBipolar[0];
		trigEnh[handle].level[0] = *(PDWORD) session[handle].adcConfig.adcTrigLevel;
		trigEnh[handle].hysteresis[0] = *(PDWORD) session[handle].adcConfig.adcTrigHyst;

		session[handle].adcConfig.adcEnhTrigSens = trigEnh[handle].trigSensitivity;
		session[handle].adcConfig.adcEnhTrigBipolar = (PDWORD) trigEnh[handle].adcRanges;
		session[handle].adcConfig.adcTrigLevel =  trigEnh[handle].level;
		session[handle].adcConfig.adcTrigHyst =  trigEnh[handle].hysteresis;
		break;

	default:

		break;
	}

	session[handle].adcChanged = TRUE;
	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqSetTriggerEvent(DaqHandleT handle,
		   DaqAdcTriggerSource trigSource,
		   DaqEnhTrigSensT trigSensitivity,
		   DWORD channel,
		   DaqAdcGain gainCode,
		   DWORD flags,
		   DaqChannelType channelType, float level, float variance, DaqTriggerEvent event)
{

	DaqError err;
	DWORD dwLevel;
	DWORD chanCount;
	DWORD dwVariance;
	DWORD listNum;

	DaqAdcTriggerSource globalTriggerSources[1];
	DaqAdcGain globalGains[1];
	DaqEnhTrigSensT globalTrigSenses[1];
	DaqAdcRangeT globalAdcRanges[1];
	float globalLevels[1];
	float globalHysteresises[1];
	DWORD globalChannels[1];
	CHAR globalOpStr[1];

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)

		return apiCtrlProcessError(handle, err);

	if (session[handle].adcAcqActive){
		dbg("DerrAcqArmed error");
		return apiCtrlProcessError(handle, DerrAcqArmed);
	}

	if ((event < DaqStartEvent) || (event > DaqStopEvent))
		return apiCtrlProcessError(handle, DerrInvTrigEvent);

	if ((IsDaq2000(session[handle].deviceType)) || (IsDaq1000(session[handle].deviceType))) {
		if ((event < DaqStartEvent) || (event > DaqStopEvent))
			return apiCtrlProcessError(handle, DerrInvTrigEvent);
	}

	else {

		if ((event == DaqStopEvent) && (trigSource != DatsScanCount))
			return apiCtrlProcessError(handle, DerrInvTrigEvent);
	}

	if (IsWaveBook(session[handle].deviceType)) {

		if ((event == DaqStopEvent) && (trigSource == DatsScanCount))
			return DerrNoError;

		globalTriggerSources[0] = trigSource;
		globalGains[0] = gainCode;
		globalAdcRanges[0] =
		    (DaqAdcRangeT) (flags & DafBipolar) ? DarBiPolarDE : DarUniPolarDE;
		globalTrigSenses[0] = trigSensitivity;
		globalLevels[0] = level;
		globalHysteresises[0] = variance;
		globalChannels[0] = channel;

		globalOpStr[0] = 43;

		chanCount = 1;

		return daqAdcSetTrigEnhanced(handle, globalTriggerSources,
					     globalGains, globalAdcRanges,
					     globalTrigSenses, globalLevels,
					     globalHysteresises, globalChannels,
					     chanCount, globalOpStr);
	}

	if ((IsDaq2000(session[handle].deviceType)) || (IsDaq1000(session[handle].deviceType))){
    //MAH:04.05.04 right here
		if ((trigSource < DatsImmediate)

		    || (trigSource == DatsGatedAdcClock)
		    || (trigSource == DatsEnhancedTrig)

		    || (trigSource == DatsPulse)
		    || (trigSource > DatsScanCount))
			return apiCtrlProcessError(handle, DerrInvTrigSource);
	} else {

		if ((event == DaqStartEvent)
		    && ((trigSource < DatsImmediate)
			|| (trigSource > DatsSoftwareAnalog)))
			return apiCtrlProcessError(handle, DerrInvTrigSource);
	}

	if ((IsDaq2000(session[handle].deviceType)) || (IsDaq1000(session[handle].deviceType))) {

		if ((trigSensitivity < DetsRisingEdge)
		    || (trigSensitivity == DetsAfterRisingEdge)
		    || (trigSensitivity == DetsAfterFallingEdge)
		    || (trigSensitivity == DetsAfterAboveLevel)
		    || (trigSensitivity == DetsAfterBelowLevel)
		    || (trigSensitivity > DetsNELevel))
			return apiCtrlProcessError(handle, DerrInvTrigSense);
	} else {
		if ((trigSensitivity < DetsRisingEdge)
		    || (trigSensitivity > DetsFallingEdge))
			return apiCtrlProcessError(handle, DerrInvTrigSense);

	}

	switch (trigSource) {
	    case DatsHardwareAnalog:    
	    case DatsSoftwareAnalog:
		{
            //MAH: 04.05.04
            //if its a 1000 and hardware analog - bail
            
            if( IsDaq1000(session[handle].deviceType))
            {
                if(trigSource == DatsHardwareAnalog)
                {
                    return apiCtrlProcessError(handle, DerrInvTrigSource);
                }    
            }            
			err = daqCalcTriggerVals(handle,
						 trigSource,
						 trigSensitivity,
						 channel,
						 gainCode,
						 flags,
						 channelType,
						 level, variance, &dwLevel, &dwVariance);
			if (err != DerrNoError)
				return apiCtrlProcessError(handle, err);
			break;
		}
	    case DatsDigPattern:
		{
			dwLevel = (DWORD) level;
			dwVariance = (DWORD) variance;
			break;
		}

	default:
		{
			dwLevel = 0;
			dwVariance = 0;
			break;
		}

	}

	if (event == DaqStartEvent) {
		session[handle].adcConfig.adcTrigLevel[0] = dwLevel;
		session[handle].adcConfig.adcTrigHyst[0] = dwVariance;

		session[handle].adcConfig.adcTrigSource = trigSource;

		if (trigSensitivity == DetsRisingEdge)
			session[handle].adcConfig.adcTrigSource |= RisingEdgeFlag;

		session[handle].adcConfig.adcEnhTrigCount = 0;

		session[handle].trigSensitivity = trigSensitivity;
		session[handle].adcConfig.adcEnhTrigSens = &session[handle].trigSensitivity;

		session[handle].adcConfig.adcTrigChannel = 0;
	}

	listNum = event + 1;

	if (session[handle].triggerEventList[0].TrigCount < listNum)
		session[handle].triggerEventList[0].TrigCount = listNum;

	session[handle].triggerEventList[listNum].TrigCount = listNum;
	session[handle].triggerEventList[listNum].TrigNumber = listNum;
	session[handle].triggerEventList[listNum].TrigEvent = event;

	session[handle].triggerEventList[listNum].TrigSource = trigSource;
	session[handle].triggerEventList[listNum].TrigSens = trigSensitivity;
	session[handle].triggerEventList[listNum].TrigLevel = dwLevel;
	session[handle].triggerEventList[listNum].TrigVariance = dwVariance;
	session[handle].triggerEventList[listNum].TrigLocation = 0;

	session[handle].triggerEventList[listNum].TrigChannel = channel;
	session[handle].triggerEventList[listNum].TrigGain = gainCode;
	session[handle].triggerEventList[listNum].Trigflags = flags;

	session[handle].adcChanged = TRUE;

	return DerrNoError;
}

DAQAPI DaqError WINAPI
daqAdcSoftTrig(DaqHandleT handle)
{

	DaqError err;
	daqIOT sb;
        memset((void *) &sb, 0, sizeof(sb));

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	err = itfExecuteCommand(handle, &sb, ADC_SOFTTRIG);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqAdcSetScan(DaqHandleT handle, PDWORD channels, DaqAdcGain * gains, PDWORD flags, DWORD chanCount)
{

	DaqError err;
	DWORD optType, gainInfo;
	DWORD i;
	SCAN_SEQ_PT seq;
//	BOOL ctrTest = FALSE;
    
	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	if (session[handle].adcAcqActive){
		dbg("DerrAcqArmed error");
		return apiCtrlProcessError(handle, DerrAcqArmed);
	}

	if ((chanCount == 0) || (chanCount > MaxScanSeqSize)){
		dbg("dataCount invalid: 0 < %d <= (MaxScanSeqSize) (=%i)",
		    chanCount, MaxScanSeqSize);
		return apiCtrlProcessError(handle, DerrInvCount);
	}
    
	err = apiDbkSendChanOptions(handle, channels, gains, chanCount);    
	if (err != DerrNoError)

		return apiCtrlProcessError(handle, err);

	session[handle].perChannelProcessing = FALSE;
    
	for (i = 0, seq = session[handle].adcScanSeq; i < chanCount; i++, seq++) {
		seq->channel = channels[i];
		seq->gain = gains[i];
		seq->flags = flags[i];
		seq->reserved = 0;

		if ((IsWaveBook(session[handle].deviceType))
		    && ((seq->flags & DafIgnoreType) != DafIgnoreType)) {

			optType = DaetNotDefined;
			if (seq->channel > 8) {
				daqGetInfo(handle, seq->channel, 
					   DdiChTypeInfo, (PVOID) & optType);
				dbg("optType = %d for chan %d",
						optType, seq->channel);
				if (optType == DaetNotDefined) {

					return apiCtrlProcessError(handle, DerrInvChan);

				}
			}

			if (seq->flags & DafHighSpeedDigital) {
				if ((WgcDigital8 <= seq->gain)
				    && (seq->gain <= WgcCtr32High)) {
					seq->gain = WgcX1;
				}
			}

			switch (seq->gain) {
			case (WgcX1):
				gainInfo = DdiWgcX1Info;
				break;
			case (WgcX2):
				gainInfo = DdiWgcX2Info;
				break;
			case (WgcX5):
				gainInfo = DdiWgcX5Info;
				break;
			case (WgcX10):
				gainInfo = DdiWgcX10Info;
				break;

			case (WgcX20):
				gainInfo = DdiWgcX20Info;
				break;
			case (WgcX50):
				gainInfo = DdiWgcX50Info;
				break;
			case (WgcX100):
				gainInfo = DdiWgcX100Info;
				break;
			case (WgcX200):
				gainInfo = DdiWgcX200Info;
				break;

			default:
				return daqProcessError(handle, DerrInvGain);

			}
			daqGetInfo(handle, seq->channel, (DaqInfo) gainInfo, (PVOID) & optType);

			if (optType == DaetNotDefined) {
				return apiCtrlProcessError(handle, DerrInvGain);
			}
		}

		if (i == 0)
			seq->flags &= ~DafSSHHold;
		else
			seq->flags |= DafSSHHold;

		if ((seq->flags & DafClearLSNibble)
		    || (seq->flags & DafShiftLSNibble))
			session[handle].perChannelProcessing = TRUE;

		if ((seq->flags & DafSigned)
		    && (session[handle].HW2sComplement == FALSE))
			session[handle].perChannelProcessing = TRUE;
	}
  
	session[handle].adcConfig.adcScanLength = chanCount;

	session[handle].adcChanged = TRUE;

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqAdcGetScan(DaqHandleT handle, PDWORD channels, DaqAdcGain * gains,
	      PDWORD flags, PDWORD chanCount)
{

	DaqError err;
	DWORD i;
	SCAN_SEQ_PT seq;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	*chanCount = session[handle].adcConfig.adcScanLength;

	for (i = 0, seq = session[handle].adcScanSeq; i < *chanCount; i++, seq++) {
		if (channels)
			channels[i] = seq->channel;
		if (gains)
			gains[i] = (DaqAdcGain) seq->gain;
		if (flags)
			flags[i] = seq->flags;
	}

	return DerrNoError;

}

DaqError
adcTbk66FlagsToTcTypeAndGain(DWORD flags, TCType * tcType, DaqAdcGain * tcGain)
{

	BOOL bipolar = (flags & DafBipolar);

	switch (flags & 0x780) {
	case DafTcTypeJ:
		*tcType = TbkTCTypeJ;
		*tcGain = (bipolar ? TbkBiTypeJ : TbkUniTypeJ);
		break;
	case DafTcTypeK:
		*tcType = TbkTCTypeK;
		*tcGain = (bipolar ? TbkBiTypeK : TbkUniTypeK);

		break;
	case DafTcTypeT:
		*tcType = TbkTCTypeT;
		*tcGain = (bipolar ? TbkBiTypeT : TbkUniTypeT);
		break;
	case DafTcTypeE:
		*tcType = TbkTCTypeE;
		*tcGain = (bipolar ? TbkBiTypeE : TbkUniTypeE);
		break;
	case DafTcTypeN28:
		*tcType = TbkTCTypeN28;
		*tcGain = (bipolar ? TbkBiTypeN28 : TbkUniTypeN28);
		break;

	case DafTcTypeN14:
		*tcType = TbkTCTypeN14;
		*tcGain = (bipolar ? TbkBiTypeN14 : TbkUniTypeN14);
		break;
	case DafTcTypeS:
		*tcType = TbkTCTypeS;
		*tcGain = (bipolar ? TbkBiTypeS : TbkUniTypeS);
		break;
	case DafTcTypeR:
		*tcType = TbkTCTypeR;
		*tcGain = (bipolar ? TbkBiTypeR : TbkUniTypeR);
		break;
	case DafTcTypeB:
		*tcType = TbkTCTypeB;

		*tcGain = (bipolar ? TbkBiTypeB : TbkUniTypeB);
		break;
	default:
		return DerrTCE_TYPE;
	}

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqAdcSetMux(DaqHandleT handle, DWORD startChan, DWORD endChan, DaqAdcGain gain, DWORD flags)
{

	DWORD *channelArray;
	DaqAdcGain *gainArray;
	DWORD *flagsArray;
	DWORD seq, chanCount, chanIncrement;
	DaqError ret;

	if ((session[handle].deviceType != TempBook66) &&
	    (session[handle].postProcFormat == DappdfTenthsDegC))
		return apiCtrlProcessError(handle, DerrInvPostDataFormat);

	if ((startChan > endChan) || (endChan - startChan + 1 >= MaxScanSeqSize)
	    || (startChan >= MaxScanSeqSize))
		return apiCtrlProcessError(handle, DerrInvChan);

	chanCount = endChan - startChan + 1;

	if (chanCount > MaxScanSeqSize)
		return apiCtrlProcessError(handle, DerrInvChan);

	switch (session[handle].postProcFormat) {
	case DappdfTenthsDegC:
		{
			DaqAdcGain tcGain;
			TCType tcType;

			chanCount += 3;
			channelArray = (PDWORD) malloc(chanCount * sizeof (DWORD));
			gainArray = (DaqAdcGain *) malloc(chanCount * sizeof (DWORD));

			flagsArray = (PDWORD) malloc(chanCount * sizeof (DWORD));

			channelArray[0] = 18;
			channelArray[1] = 18;
			channelArray[2] = 16;

			flagsArray[0] = flags;
			flagsArray[1] = flags;
			flagsArray[2] = flags;

			ret = adcTbk66FlagsToTcTypeAndGain(flags, &tcType, &tcGain);
			if (ret != DerrNoError)

				return apiCtrlProcessError(handle, ret);

			if (tcGain != gain)
				return apiCtrlProcessError(handle, DerrInvGain);

			session[handle].postProcTcType = tcType;

			gainArray[0] = ((flags & DafBipolar) ? TbkBiCJC : TbkUniCJC);
			gainArray[1] = tcGain;
			gainArray[2] = ((flags & DafBipolar) ? TbkBiCJC : TbkUniCJC);

			seq = 3;
		}
		break;

	default:

		channelArray = (PDWORD) malloc(MaxScanSeqSize * sizeof (DWORD));
		gainArray = (DaqAdcGain *) malloc(MaxScanSeqSize * sizeof (DWORD));
		flagsArray = (PDWORD) malloc(MaxScanSeqSize * sizeof (DWORD));

		seq = 0;

	}

	chanIncrement = 0;
	while (seq < chanCount) {
		channelArray[seq] = startChan + chanIncrement;
		gainArray[seq] = gain;
		flagsArray[seq] = flags;
		seq++;

		chanIncrement++;
	}
	ret = daqAdcSetScan(handle, channelArray, gainArray, flagsArray, chanCount);
	free(channelArray);
	free(gainArray);
	free(flagsArray);

	return ret;

}

DAQAPI DaqError WINAPI
daqAdcSetDiskFile(DaqHandleT handle, PCHAR filename, DaqAdcOpenMode openMode, DWORD preWrite)
{

	DaqError err;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError){
		return apiCtrlProcessError(handle, err);
	}

	if (session[handle].adcAcqActive){
		dbg("DerrAcqArmed error");
		return apiCtrlProcessError(handle, DerrAcqArmed);
	}

	if ((openMode > DaomCreateFile) || (openMode < DaomAppendFile))
		return apiCtrlProcessError(handle, DerrInvOpenMode);

	strcpy(session[handle].adcFileName, filename);
	session[handle].adcFileOpenMode = openMode;

	session[handle].adcFilePreWrite = preWrite;

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqAdcArm(DaqHandleT handle)
{
	DaqError err;
	daqIOT sb;
	ADC_CONFIG_PT adcConfig;

        memset( (void *) &sb, 0, sizeof(sb));

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError){
		dbg("handle error err=%i",err);
		return apiCtrlProcessError(handle, err);
	}

	if (session[handle].adcAcqActive){
		dbg("DerrAcqArmed error");
		return apiCtrlProcessError(handle, DerrAcqArmed);
	}

	if (session[handle].adcChanged) {
		adcConfig = (ADC_CONFIG_PT) & sb;

		if (((session[handle].adcConfig.adcTrigSource & 0x00000007) == DatsImmediate)
		    && (session[handle].adcConfig.adcClockSource == DacsTriggerSource))
			return apiCtrlProcessError(handle, DerrInvClockSource);

		if (((session[handle].adcConfig.adcAcqMode == DaamNShotRearm) ||
		     (session[handle].adcConfig.adcAcqMode == DaamPrePost)) &&
		    (((session[handle].adcConfig.adcTrigSource & ~RisingEdgeFlag) == DatsImmediate)
		     ||
		     ((session[handle].adcConfig.adcTrigSource & ~RisingEdgeFlag) == DatsAdcClock)
		     ||
		     ((session[handle].adcConfig.
		       adcTrigSource & ~RisingEdgeFlag) == DatsGatedAdcClock))) {
			return apiCtrlProcessError(handle, DerrInvMode);
		}

		if (IsDaqDevice(session[handle].deviceType)) {
			DWORD x, listNum = 0;
			if (session[handle].triggerEventList[0].TrigCount > 0) {

				for (listNum = 1;
				     listNum <
				     (session[handle].triggerEventList[0].
				      TrigCount + 1); listNum++) {
					switch (session[handle].
						triggerEventList[listNum].TrigSource) {

					case DatsHardwareAnalog:
						{
							if (!
							    ((session[handle].
							      triggerEventList
							      [listNum].
							      TrigChannel ==
							      session[handle].adcScanSeq[0].channel)
							     &&
							     (session[handle].
							      triggerEventList
							      [listNum].TrigGain == (DaqAdcGain)
							      session[handle].adcScanSeq[0].gain)
							     &&
							     (session[handle].
							      triggerEventList
							      [listNum].
							      Trigflags ==
							      (session[handle].
							       adcScanSeq[0].
							       flags & ~DafSSHHold)))) {
								return
								    apiCtrlProcessError
								    (handle, DerrInvTrigChannel);
							}
							break;
						}
					case DatsSoftwareAnalog:
					case DatsDigPattern:
						{
							for (x = 0;
							     x <
							     session[handle].
							     adcConfig.adcScanLength; x++) {
								if ((session
								     [handle].
								     triggerEventList
								     [listNum].
								     TrigChannel
								     ==
								     session
								     [handle].adcScanSeq[x].channel)
								    &&
								    (session
								     [handle].
								     triggerEventList
								     [listNum].
								     TrigGain == (DaqAdcGain)
								     session
								     [handle].adcScanSeq[x].gain)
								    &&
								    (session
								     [handle].
								     triggerEventList
								     [listNum].
								     Trigflags
								     ==
								     (session
								      [handle].
								      adcScanSeq
								      [x].flags & ~DafSSHHold))) {

									adcConfig->
									    adcTrigChannel = x;
									session
									    [handle].
									    triggerEventList
									    [listNum].
									    TrigLocation = x;
									break;
								}
							}
							if (x ==
							    session[handle].adcConfig.adcScanLength)
								return
								    apiCtrlProcessError
								    (handle, DerrInvTrigChannel);
							break;
						}
					default:
						{
							break;
						}
					}
				}
			}
		} else
			adcConfig->adcTrigChannel = session[handle].adcConfig.adcTrigChannel;

		adcConfig->triggerEventList = session[handle].adcConfig.triggerEventList;

		adcConfig->adcClockSource = session[handle].adcConfig.adcClockSource;
		adcConfig->adcAcqMode = session[handle].adcConfig.adcAcqMode;
		adcConfig->adcAcqPreCount =
		    session[handle].adcConfig.adcAcqPreCount *
		    session[handle].adcConfig.adcScanLength;
		adcConfig->adcAcqPostCount =
		    session[handle].adcConfig.adcAcqPostCount *
		    session[handle].adcConfig.adcScanLength;

		adcConfig->adcPeriodmsec = session[handle].adcConfig.adcPeriodmsec;
		adcConfig->adcPeriodnsec = session[handle].adcConfig.adcPeriodnsec;
		adcConfig->adcPreTPeriodms = session[handle].adcConfig.adcPreTPeriodms;
		adcConfig->adcPreTPeriodns = session[handle].adcConfig.adcPreTPeriodns;

		adcConfig->adcTrigSource = session[handle].adcConfig.adcTrigSource;

		adcConfig->adcScanSeq = session[handle].adcConfig.adcScanSeq;
		adcConfig->adcScanLength = session[handle].adcConfig.adcScanLength;
		adcConfig->adcDataPack = session[handle].adcConfig.adcDataPack;

		adcConfig->adcEnhTrigCount = session[handle].adcConfig.adcEnhTrigCount;

		adcConfig->adcEnhTrigChan = session[handle].adcConfig.adcEnhTrigChan;
		adcConfig->adcEnhTrigGain = session[handle].adcConfig.adcEnhTrigGain;
		adcConfig->adcEnhTrigBipolar = session[handle].adcConfig.adcEnhTrigBipolar;

		adcConfig->adcEnhTrigSens = session[handle].adcConfig.adcEnhTrigSens;
		adcConfig->adcTrigLevel = session[handle].adcConfig.adcTrigLevel;
		adcConfig->adcTrigHyst = session[handle].adcConfig.adcTrigHyst;
		adcConfig->adcEnhTrigOpcode = session[handle].adcConfig.adcEnhTrigOpcode;
		adcConfig->adcTrigChannel = 0;

#if API_ADC_DEBUG
		DWORD i;
		dbg("daqAdcArm: adcClockSource  = %d", adcConfig->adcClockSource);
		dbg("daqAdcArm: adcAcqMode      = %d", adcConfig->adcAcqMode);
		dbg("daqAdcArm: adcAcqPreCount  = %d", adcConfig->adcAcqPreCount);
		dbg("daqAdcArm: adcAcqPostCount = %d", adcConfig->adcAcqPostCount);
		dbg("daqAdcArm: adcPeriodmsec   = %d", adcConfig->adcPeriodmsec);
		dbg("daqAdcArm: adcPeriodnsec   = %d", adcConfig->adcPeriodnsec);
		dbg("daqAdcArm: adcPreTPeriodms = %d", adcConfig->adcPreTPeriodms);
		dbg("daqAdcArm: adcPreTPeriodns = %d", adcConfig->adcPreTPeriodns);
		dbg("daqAdcArm: adcTrigSource   = %d", adcConfig->adcTrigSource);
		dbg("daqAdcArm: adcTrigLevel    = %ld", (ULONG) adcConfig->adcTrigLevel);
		dbg("daqAdcArm: adcTrigHyst     = %ld", (ULONG)adcConfig->adcTrigHyst);
		dbg("daqAdcArm: adcTrigChannel  = %d", adcConfig->adcTrigChannel);
		dbg("daqAdcArm: adcDataPack     = %d", adcConfig->adcDataPack);
		dbg("daqAdcArm: adcScanLength   = %d", adcConfig->adcScanLength);
#ifdef NOTDEF
		for (i = 0; i < adcConfig->adcScanLength; i++) {
			dbg("daqAdcArm:    adcScanSeq      = %ld",
                            (ULONG) session[handle].adcScanSeq[i]);
		}
#endif
		DWORD *enhTrigChan = (DWORD *) session[handle].adcConfig.adcEnhTrigChan;
		DWORD *enhTrigGain = (DWORD *) session[handle].adcConfig.adcEnhTrigGain;
		DWORD *enhTrigBipolar = (DWORD *) session[handle].adcConfig.adcEnhTrigBipolar;
		DWORD *enhTrigSens = (DWORD *) session[handle].adcConfig.adcEnhTrigSens;
		DWORD *trigLevel =  &session[handle].adcConfig.adcTrigLevel[0];
		DWORD *trigHyst = (DWORD *) session[handle].adcConfig.adcTrigHyst;
		dbg("daqAdcArm: adcEnhTrigCount = %d", adcConfig->adcEnhTrigCount);
		for (i = 0; i < adcConfig->adcEnhTrigCount; i++) {
			dbg("daqAdcArm:    adcEnhTrigChan    = %d", enhTrigChan[i]);
			dbg("daqAdcArm:    adcEnhTrigGain    = %d", enhTrigGain[i]);
			dbg("daqAdcArm:    adcEnhTrigBipolar = %d", enhTrigBipolar[i]);
			dbg("daqAdcArm:    adcEnhTrigSens    = %d", enhTrigSens[i]);
			dbg("daqAdcArm:    adcTrigLevel      = %d", trigLevel[i]);
			dbg("daqAdcArm:    adcTrigHyst       = %d", trigHyst[i]);
		}
		dbg("daqAdcArm: adcEnhTrigOpcode     = %s",
				(PCHAR) adcConfig->adcEnhTrigOpcode);
#endif

		err = itfExecuteCommand(handle, &sb, ADC_CONFIG);
		if (err != DerrNoError)

			return apiCtrlProcessError(handle, err);
		session[handle].adcConfig.adcPeriodmsec = adcConfig->adcPeriodmsec;
		session[handle].adcConfig.adcPeriodnsec = adcConfig->adcPeriodnsec;

		session[handle].adcChanged = FALSE;
	}

	session[handle].adcSamplesProcessed = 0;

	session[handle].adcAcqActive = TRUE;

	err = adcOpenDiskFile(handle);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	dbg("Arming");

	err = itfExecuteCommand(handle, &sb, ADC_ARM);
	if (err != DerrNoError) {
		dbg("Arming failed with error = %d", err);
		return apiCtrlProcessError(handle, err);
	}

	return DerrNoError;
}

/*
DaqError
WaitForAcqActivePolled(DaqHandleT handle, DWORD dwTimeout, const BOOL bActive, PBOOL pbTimedOut)
{
	DaqError ret = DerrNoError;
	BOOL bTimedOut = FALSE;
	DWORD dwMsCurr = GetTickCount();
	DWORD dwMsTerm = dwMsCurr + dwTimeout;
	ApiAdcPT ps = &session[handle];
	DWORD active = 0;
	DWORD retCount = 0;

	while (TRUE) {
		ret = daqAdcTransferGetStat(handle, &active, &retCount);
		dbg("WaitingForAcqActivePolled: active (ticks): %d (%d)",
		     active, dwMsTerm - dwMsCurr);

		if (DerrNoError != ret) {
			dbg("ARMING: daqAdcTransferGetStat failed with error = %d", ret);
			ret = apiCtrlProcessError(handle, ret);
			break;
		}

		if (bActive == ps->adcAcqActive) {
			break;
		}

		dwMsCurr = GetTickCount();
		if ((dwMsTerm <= dwMsCurr)
		    && ((dwMsCurr - dwMsTerm) < 0x80000000)) {
			bTimedOut = TRUE;
			break;
		}
		usleep(10);
	}

	if (NULL != pbTimedOut) {
		*pbTimedOut = bTimedOut;
	}

	return ret;
}
*/

DAQAPI DaqError WINAPI
daqAdcDisarm(DaqHandleT handle)
{

	DaqError err = apiCtrlTestHandle(handle, DlmAll);
	dbg("Entered Disarm, test handle returns %d",err);
	if (err != DerrNoError) {
		dbg("Error on Disarm, %d",err);
		return apiCtrlProcessError(handle, err);
	}

	if (session[handle].adcAcqActive) {
		daqIOT sb;
                memset((void *) &sb, 0, sizeof(sb));
		if (DerrNoError != (err = itfExecuteCommand(handle, &sb, ADC_DISARM))) {
			dbg("Error on Disarm, %d",err);
			return apiCtrlProcessError(handle, err);
		}
		cmnWaitForMutex(AdcStatMutex);
		err = adcTransferGetStat(handle, NULL, NULL);
		cmnReleaseMutex(AdcStatMutex);
		if (err != DerrNoError) {
			dbg("Error on Disarm, %d",err);
			return apiCtrlProcessError(handle, err);
		}
		//PEK:patched this in here...
		session[handle].adcAcqActive = FALSE;
	}
	dbg("Correctly Disarmed, exit code=%i", err);

	return err;
}

DAQAPI DaqError WINAPI
daqAdcTransferSetBuffer(DaqHandleT handle, PWORD buf, DWORD scanCount, DWORD transferMask)
{
	DaqHandleT linhandle;
	ApiAdcPT ps;
	int size;

	DaqError err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	ps = &session[handle];

	if (ps->adcXferActive)
		return apiCtrlProcessError(handle, DerrMultBackXfer);

	if (scanCount == 0){
		dbg("scanCount is zero!");
		return apiCtrlProcessError(handle, DerrInvCount);
	}

	ps->adcXfer.adcXferBufLen = scanCount;
	ps->adcXfer.adcXferMode = transferMask;

	/* provide a buffer for the user */
	if (transferMask & DatmDriverBuf) {
		if (ps->adcXferDrvrBuf) {
			/* we have to assume that since DatmDriverBuf
			 * is set, that we are the ones who malloc'ed this...
			 */
			free(ps->adcXferDrvrBuf);
			ps->adcXferDrvrBuf = NULL;
		}

		linhandle = itfGetDriverHandle(handle);
		if (linhandle == NO_GRIP) {
			dbg("itfGetDriverHandle FAILED  %li", linhandle);
			return apiCtrlProcessError(handle, DerrMemAlloc);
		}
		
		size = sizeof (WORD) * scanCount * ps->adcConfig.adcScanLength;

		ps->adcXferDrvrBuf = linMap(size, handle, linhandle);

		if (ps->adcXferDrvrBuf == NULL) {
			dbg("Driver provided Buffer Allocation FAILED!");
			return apiCtrlProcessError(handle, DerrMemAlloc);
		}
		
		ps->adcXfer.adcXferBuf = ps->adcXferDrvrBuf;
		ps->adcAcqStat.adcAcqStatHead = 0;
		ps->adcAcqStat.adcAcqStatTail = 0;
		ps->adcAcqStat.adcAcqStatScansAvail = 0;
		ps->adcAcqStat.adcAcqStatSamplesAvail = 0;
		ps->adcStat.adcXferCount = 0;

	} else {
		if(buf == NULL){
			dbg("User provided Buffer is NULL!");
			return apiCtrlProcessError(handle, DerrMemAlloc);
		}
		dbg("Using provided user buffer");
		ps->adcXfer.adcXferBuf = buf;
	}

	return DerrNoError;
}

DaqError
adcLoadBuffer(DaqHandleT handle, PWORD buf, DWORD loadScanCount)
{
	DWORD firstLoadAmount, secondLoadAmount, totalLoadAmount, distToEndOfBuf;
	BOOL bufferWrapped = FALSE;
	DWORD bufLenScans, bufLen;

	if (session[handle].adcConfig.adcDataPack == DardfPacked) {
		bufLen =
		    (session[handle].adcXfer.adcXferBufLen *
		     session[handle].adcConfig.adcScanLength * 3) / 4;
		bufLenScans = (session[handle].adcXfer.adcXferBufLen * 3) / 4;
	} else {
		bufLen =
		    session[handle].adcXfer.adcXferBufLen * session[handle].adcConfig.adcScanLength;
		bufLenScans = session[handle].adcXfer.adcXferBufLen;
	}

	if (session[handle].adcAcqStat.adcAcqStatTail <= session[handle].adcAcqStat.adcAcqStatHead) {

		totalLoadAmount = loadScanCount;

		if (session[handle].adcConfig.adcDataPack == DardfPacked) {
			memcpy(buf,
			       &session[handle].adcXferDrvrBuf[session[handle].
							       adcAcqStat.
							       adcAcqStatTail *
							       session[handle].
							       adcConfig.
							       adcScanLength *
							       3 / 4],
			       totalLoadAmount *
			       session[handle].adcConfig.adcScanLength * sizeof (WORD) * 3 / 4);
		} else {
			memcpy(buf,
			       &session[handle].adcXferDrvrBuf[session[handle].
							       adcAcqStat.
							       adcAcqStatTail *
							       session[handle].
							       adcConfig.
							       adcScanLength],
			       totalLoadAmount *
			       session[handle].adcConfig.adcScanLength * sizeof (WORD));
		}

		if (session[handle].adcXfer.adcXferBufLen == totalLoadAmount) {

		} else {

			session[handle].adcAcqStat.adcAcqStatTail += totalLoadAmount;
		}

	} else {

		if (session[handle].adcAcqStat.adcAcqStatTail >
		    session[handle].adcAcqStat.adcAcqStatHead) {

			distToEndOfBuf = firstLoadAmount =
			    session[handle].adcXfer.adcXferBufLen -
			    session[handle].adcAcqStat.adcAcqStatTail;

			if (firstLoadAmount >= loadScanCount) {
				firstLoadAmount = loadScanCount;
			} else {
				bufferWrapped = TRUE;
			}

			if (session[handle].adcConfig.adcDataPack == DardfPacked) {
				memcpy(buf,
				       &session[handle].
				       adcXferDrvrBuf[session[handle].
						      adcAcqStat.
						      adcAcqStatTail *
						      session[handle].adcConfig.
						      adcScanLength * 3 / 4],
				       firstLoadAmount *
				       session[handle].adcConfig.adcScanLength *
				       sizeof (WORD) * 3 / 4);

			} else {
				memcpy(buf,
				       &session[handle].
				       adcXferDrvrBuf[session[handle].
						      adcAcqStat.
						      adcAcqStatTail *
						      session[handle].adcConfig.
						      adcScanLength],
				       firstLoadAmount *
				       session[handle].adcConfig.adcScanLength * sizeof (WORD));
			}

			if (!bufferWrapped) {

				if (firstLoadAmount == distToEndOfBuf) {
					session[handle].adcAcqStat.adcAcqStatTail = 0;
				} else {
					session[handle].adcAcqStat.
					    adcAcqStatTail += firstLoadAmount;
				}

			} else {

				secondLoadAmount = loadScanCount - firstLoadAmount;

				if (session[handle].adcConfig.adcDataPack == DardfPacked) {
					memcpy(&buf
					       [firstLoadAmount *
						session[handle].adcConfig.
						adcScanLength * 3 / 4],
					       &session[handle].
					       adcXferDrvrBuf[0],
					       secondLoadAmount *
					       session[handle].adcConfig.
					       adcScanLength * sizeof (WORD) * 3 / 4);
				} else {
					memcpy(&buf
					       [firstLoadAmount *
						session[handle].adcConfig.
						adcScanLength],
					       &session[handle].
					       adcXferDrvrBuf[0],
					       secondLoadAmount *
					       session[handle].adcConfig.
					       adcScanLength * sizeof (WORD));
				}

				session[handle].adcAcqStat.adcAcqStatTail = secondLoadAmount;

			}

		}
	}

	if (session[handle].adcAcqStat.adcAcqStatTail > session[handle].adcAcqStat.adcAcqStatHead) {

		session[handle].adcAcqStat.adcAcqStatScansAvail = (bufLenScans -
								   session
								   [handle].
								   adcAcqStat.adcAcqStatTail)
		    + session[handle].adcAcqStat.adcAcqStatHead;

	} else {
		session[handle].adcAcqStat.adcAcqStatScansAvail =
		    session[handle].adcAcqStat.adcAcqStatHead -
		    session[handle].adcAcqStat.adcAcqStatTail;
	}

	return DerrNoError;

}

DaqError
adcLoadNewest(DaqHandleT handle, PWORD buf, DWORD loadScanCount)
{
	DWORD firstLoadAmount, secondLoadAmount, totalLoadAmount, startCopyLocation;
//	BOOL bufferWrapped = FALSE;
	DWORD bufLenScans, bufLen;

	DWORD AdjustedAcqStatHead;

	if (session[handle].adcConfig.adcDataPack == DardfPacked) {
		bufLen =
		    (session[handle].adcXfer.adcXferBufLen *
		     session[handle].adcConfig.adcScanLength * 3) / 4;
		bufLenScans = (session[handle].adcXfer.adcXferBufLen * 3) / 4;
	} else {
		bufLen =
		    session[handle].adcXfer.adcXferBufLen * session[handle].adcConfig.adcScanLength;
		bufLenScans = session[handle].adcXfer.adcXferBufLen;

	}

	if (session[handle].adcConfig.adcDataPack == DardfPacked) {
		AdjustedAcqStatHead =
		    ((session[handle].adcAcqStat.adcAcqStatHead
		      * session[handle].adcConfig.adcScanLength)
		     / 4)
		    * 4 / session[handle].adcConfig.adcScanLength;
	} else {
		AdjustedAcqStatHead = session[handle].adcAcqStat.adcAcqStatHead;
	}

	if (loadScanCount <= AdjustedAcqStatHead) {

		totalLoadAmount = loadScanCount;

		startCopyLocation = AdjustedAcqStatHead - totalLoadAmount;

		if (session[handle].adcConfig.adcDataPack == DardfPacked) {

			memcpy(buf,
			       &session[handle].
			       adcXferDrvrBuf[startCopyLocation *
					      session[handle].adcConfig.
					      adcScanLength * 3 / 4],
			       totalLoadAmount *
			       session[handle].adcConfig.adcScanLength * sizeof (WORD) * 3 / 4);
		} else {
			memcpy(buf,
			       &session[handle].
			       adcXferDrvrBuf[startCopyLocation *
					      session[handle].adcConfig.
					      adcScanLength],
			       totalLoadAmount *
			       session[handle].adcConfig.adcScanLength * sizeof (WORD));
		}

	} else {

		firstLoadAmount = loadScanCount - AdjustedAcqStatHead;

		secondLoadAmount = AdjustedAcqStatHead;

		startCopyLocation = session[handle].adcXfer.adcXferBufLen - firstLoadAmount;

		if (session[handle].adcConfig.adcDataPack == DardfPacked) {
			memcpy(buf,
			       &session[handle].
			       adcXferDrvrBuf[startCopyLocation *
					      session[handle].adcConfig.
					      adcScanLength * 3 / 4],
			       firstLoadAmount *
			       session[handle].adcConfig.adcScanLength * sizeof (WORD) * 3 / 4);

		} else {
			memcpy(buf,
			       &session[handle].
			       adcXferDrvrBuf[startCopyLocation *
					      session[handle].adcConfig.
					      adcScanLength],
			       firstLoadAmount *
			       session[handle].adcConfig.adcScanLength * sizeof (WORD));
		}

		if (session[handle].adcConfig.adcDataPack == DardfPacked) {
			memcpy(&buf
			       [firstLoadAmount *
				session[handle].adcConfig.adcScanLength * 3 /
				4], &session[handle].adcXferDrvrBuf[0],
			       secondLoadAmount *
			       session[handle].adcConfig.adcScanLength * sizeof (WORD) * 3 / 4);

		} else {
			memcpy(&buf
			       [firstLoadAmount *
				session[handle].adcConfig.adcScanLength],
			       &session[handle].adcXferDrvrBuf[0],
			       secondLoadAmount *
			       session[handle].adcConfig.adcScanLength * sizeof (WORD));

		}

	}

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqAdcTransferBufData(DaqHandleT handle, PWORD buf, DWORD scanCount,
		      DaqAdcBufferXferMask bufMask, PDWORD retCount)
{
	DaqError err = DerrNoError;
	DWORD active;
	DWORD scansAvailable;
	DWORD loadScans, iteration, oldScansAvail;
	DWORD bufferMode;
	BOOL enoughData = FALSE;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err)
		return apiCtrlProcessError(handle, err);

	if (!(session[handle].adcXfer.adcXferMode & DatmDriverBuf))
		return apiCtrlProcessError(handle, DerrNotCapable);

	if ((buf == NULL) || (scanCount == 0)) {
		return apiCtrlProcessError(handle, DerrInvBuf);
	}

	if (session[handle].adcXferDrvrBuf == NULL) {
		return apiCtrlProcessError(handle, DerrInvBuf);
	}

	if (session[handle].adcXfer.adcXferBufLen < scanCount) {
		dbg("buffer less than scanCount: buf:%i, scan:%i",
		    session[handle].adcXfer.adcXferBufLen, scanCount);
		return apiCtrlProcessError(handle, DerrInvCount);
	}

	bufferMode = bufMask & (DabtmWait | DabtmRetAvail | DabtmNoWait | DabtmRetNotDone);
	switch (bufferMode) {
	case DabtmWait:
	case DabtmRetAvail:
	case DabtmNoWait:
	case DabtmRetNotDone:
		break;
	default:
		return apiCtrlProcessError(handle, DerrParamConflict);
	}

	cmnWaitForMutex(AdcStatMutex);

	err = adcTransferGetStat(handle, &active, &scansAvailable);

	cmnReleaseMutex(AdcStatMutex);

	if (err)

		return apiCtrlProcessError(handle, err);

	if (!session[handle].adcXferActive && !scansAvailable)
		return apiCtrlProcessError(handle, DerrNoTransferActive);

	loadScans = scanCount;

	if (bufMask & DabtmRetNotDone) {

		if (session[handle].adcXferActive) {
			*retCount = 0;
			return DerrNoError;
		}

		if (scansAvailable < scanCount) {
			*retCount = 0;
			dbg("scans available< scancount: available=%i,  scanCount=%i",
			    scansAvailable,scanCount);
			return apiCtrlProcessError(handle, DerrInvCount);
		}
	}

	if ((bufMask & DabtmNoWait) && (loadScans > scansAvailable)) {

		*retCount = 0;
		return DerrNoError;
	}

	if ((bufMask & DabtmRetAvail) && (loadScans > scansAvailable)) {
		loadScans = scansAvailable;
	}

	if (scansAvailable >= loadScans) {

		cmnWaitForMutex(AdcStatMutex);

		if (bufMask & DabtmNewest) {
			adcLoadNewest(handle, buf, loadScans);
		} else {
			adcLoadBuffer(handle, buf, loadScans);
		}

		cmnReleaseMutex(AdcStatMutex);

		*retCount = loadScans;

		return DerrNoError;
	}

	iteration = oldScansAvail = 0;
	do {

		err = daqWaitForEvent(handle, DteAdcData);
		if (err)
			break;

		cmnWaitForMutex(AdcStatMutex);

		err = adcTransferGetStat(handle, &active, &scansAvailable);

		cmnReleaseMutex(AdcStatMutex);

		if (err)
			break;

		if (scansAvailable >= loadScans) {
			enoughData = TRUE;
		}

		iteration++;

	} while (active && !enoughData);

	if (err != DerrNoError) {
		*retCount = 0;

		return apiCtrlProcessError(handle, err);
	}

	if (!enoughData) {
		*retCount = 0;
		dbg("Not enough data");
		return apiCtrlProcessError(handle, DerrInvCount);
	}

	cmnWaitForMutex(AdcStatMutex);

	if (bufMask & DabtmNewest) {

		adcLoadNewest(handle, buf, loadScans);
	} else {
		adcLoadBuffer(handle, buf, loadScans);
	}

	cmnReleaseMutex(AdcStatMutex);

	*retCount = loadScans;

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqAdcSetDataFormat(DaqHandleT handle, DaqAdcRawDataFormatT rawFormat,
		    DaqAdcPostProcDataFormatT postProcFormat)
{

	if (session[handle].adcAcqActive){
		dbg("DerrAcqArmed error");
		return apiCtrlProcessError(handle, DerrAcqArmed);
	}

	switch (rawFormat) {
	case DardfNative:
	case DardfPacked:
		session[handle].adcConfig.adcDataPack = rawFormat;
		break;

	default:
		return apiCtrlProcessError(handle, DerrInvRawDataFormat);
	}

	switch (postProcFormat) {
	case DappdfRaw:
	case DappdfTenthsDegC:
		session[handle].postProcFormat = postProcFormat;
		break;

	default:
		return apiCtrlProcessError(handle, DerrInvPostDataFormat);
	}

	return DerrNoError;
}

#define nBlockLenUnpk 4
#define nBlockLenPack 3

typedef WORD BLOCKUNPK[nBlockLenUnpk];
typedef BLOCKUNPK *PBLOCKUNPK;

typedef WORD BLOCKPACK[nBlockLenPack];
typedef BLOCKPACK *PBLOCKPACK;

void
PackData(PBLOCKPACK pPacked, PBLOCKUNPK pUnpked, int nBlocks)
{
	int i;
	nBlocks = 1;
	for (i = 0; i < nBlocks; i++) {
		WORD first = (*pUnpked)[0];
		(*pPacked)[0] = ((*pUnpked)[1] >> 4) | ((first & 0x00F0) << 8);
		(*pPacked)[1] = ((*pUnpked)[2] >> 4) | ((first & 0x0F00) << 4);
		(*pPacked)[2] = ((*pUnpked)[3] >> 4) | ((first & 0xF000) << 0);
		pPacked++;
		pUnpked++;
	}
}

void
UnpackData(PBLOCKUNPK pUnpked, PBLOCKPACK pPacked, int nBlocks)
{
	int i;
	nBlocks = 1;
	for (i = 0; i < nBlocks; i++) {
		WORD first = (((*pPacked)[2] & 0xF000))
		    | (((*pPacked)[1] & 0xF000) >> 4)
		    | (((*pPacked)[0] & 0xF000) >> 8);

		(*pUnpked)[3] = ((*pPacked)[2] & 0x0FFF) << 4;
		(*pUnpked)[2] = ((*pPacked)[1] & 0x0FFF) << 4;
		(*pUnpked)[1] = ((*pPacked)[0] & 0x0FFF) << 4;

		(*pUnpked)[0] = first;
		pUnpked--;
		pPacked--;
	}
}

DAQAPI DaqError WINAPI
daqCvtRawDataFormat(PWORD buf, DaqAdcCvtAction action, DWORD lastRetCount,
		    DWORD scanCount, DWORD chanCount)
{
	int nBlocks;
	PBLOCKUNPK pBlockUnpk;
	PBLOCKPACK pBlockPack;

	switch (action) {

	case DacaUnpack:{
//			DWORD nScansActual = min(lastRetCount, scanCount);
			nBlocks = (scanCount * chanCount) / nBlockLenUnpk;
			pBlockUnpk = ((PBLOCKUNPK) buf) + nBlocks - 1;
			pBlockPack = ((PBLOCKPACK) buf) + nBlocks - 1;
			UnpackData(pBlockUnpk, pBlockPack, nBlocks);
		} break;

	case DacaPack:{
//			DWORD nScansActual = min(lastRetCount, scanCount);
			nBlocks = (scanCount * chanCount) / nBlockLenUnpk;
			pBlockUnpk = (PBLOCKUNPK) buf;
			pBlockPack = (PBLOCKPACK) buf;
			PackData(pBlockPack, pBlockUnpk, nBlocks);
		} break;

	case DacaRotate:
		daqAdcBufferRotate(NO_GRIP, buf, scanCount, chanCount, lastRetCount);
		break;

	default:
		return apiCtrlProcessError(NO_GRIP, DerrNotCapable);
	}

	return DerrNoError;
}


DAQAPI DaqError WINAPI
daqAdcTransferSetBufferAllocMem(DaqHandleT handle, DWORD scanCount, DWORD transferMask)
{

	DaqError err;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	if (session[handle].adcXferActive)
		return apiCtrlProcessError(handle, DerrMultBackXfer);

	if (scanCount == 0){
		dbg("scanCount zero, bad juju!");
		return apiCtrlProcessError(handle, DerrInvCount);
	}

	if ( (session[handle].tmpMemory = GlobalAlloc(GMEM_FIXED,
						      (sizeof (DWORD) * scanCount *
						       session[handle].adcConfig.adcScanLength)))) {
		if ((session[handle].tmpMemoryP = (PWORD) GlobalLock(session[handle].tmpMemory))) {
			err = daqAdcTransferSetBuffer(handle,
						    session[handle].tmpMemoryP,
						    scanCount, transferMask);
		} else
			return apiCtrlProcessError(handle, DerrMemLock);
	} else
		return apiCtrlProcessError(handle, DerrMemHandle);
	if (err)
		return apiCtrlProcessError(handle, err);
	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqAdcTransferStart(DaqHandleT handle)
{

	DaqError err;
	daqIOT sb;
	ADC_START_PT adcXfer = (ADC_START_PT) & sb;

        memset((void *) &sb, 0, sizeof(sb));
	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	if (session[handle].adcXferActive)
		return apiCtrlProcessError(handle, DerrMultBackXfer);

	session[handle].adcLastRetCount = 0;
	session[handle].adcStat.errorCode = DerrNoError;
	session[handle].adcStat.adcXferCount = 0;
	session[handle].adcAcqStat.adcAcqStatHead = 0;
	session[handle].adcAcqStat.adcAcqStatTail = 0;
	session[handle].adcAcqStat.adcAcqStatScansAvail = 0;
	session[handle].adcAcqStat.adcAcqStatSamplesAvail = 0;
	session[handle].adcXferActive = TRUE;
	session[handle].adcProcessingThreadHandle = NO_GRIP;

	if (session[handle].adcConfig.adcDataPack == DardfPacked) {
		adcXfer->adcXferBufLen =
		    (session[handle].adcXfer.adcXferBufLen *
		     session[handle].adcConfig.adcScanLength * 3) / 4;
	} else {
		adcXfer->adcXferBufLen =
		    session[handle].adcXfer.adcXferBufLen * session[handle].adcConfig.adcScanLength;
	}
	dbg("daqAdcTransferStart: adcXferBufLen = %d", adcXfer->adcXferBufLen);

	adcXfer->adcXferMode = session[handle].adcXfer.adcXferMode;
	adcXfer->adcXferBuf = session[handle].adcXfer.adcXferBuf;


	dbg("STARTING TRANSFER with xferMode = %d", adcXfer->adcXferMode);

	err = itfExecuteCommand(handle, &sb, ADC_START);
	if (err != DerrNoError) {
		dbg("FAILING TO START TRANSFER with error = %d", err);
		return apiCtrlProcessError(handle, err);
	} else {
		dbg("itfExecute(ADC_START) succeeded.");
	}

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqAdcTransferStop(DaqHandleT handle)
{
	DaqError err;
	daqIOT sb;

	DaqHandleT linhandle;

        memset((void *) &sb, 0, sizeof(sb));
	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	if (session[handle].adcXferActive) {

		err = itfExecuteCommand(handle, &sb, ADC_STOP);

		if (err != DerrNoError)
			return apiCtrlProcessError(handle, err);

		err = daqWaitForEvent(handle, DteAdcDone);

		if (err != DerrNoError)

			return err;

	}

	if (session[handle].adcXfer.adcXferMode & DatmDriverBuf) {

		if (session[handle].adcXferDrvrBuf) {

#ifndef DLINUX
			free(session[handle].adcXferDrvrBuf);
			session[handle].adcXferDrvrBuf = NULL;

#endif
#ifdef DLINUX

			linhandle = itfGetDriverHandle(handle);
			linUnMap(handle, linhandle);
			session[handle].adcXferDrvrBuf = NULL;

#endif
		}

	}

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqAdcTransferStopAllocMem(DaqHandleT handle, PWORD buf, DWORD scanCount)
{

	DaqError err;
	daqIOT sb;

        memset((void *) &sb, 0, sizeof(sb));
	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	if (session[handle].adcXferActive) {
		dbg("STOPPING TRANSFER (daqAdcTransferStopAllocMem)");

		err = itfExecuteCommand(handle, &sb, ADC_STOP);
		if (err != DerrNoError)
			return apiCtrlProcessError(handle, err);

		err = daqWaitForEvent(handle, DteAdcDone);

		dbg(":daqAdcTransferStopAllocMem after daqWaitForEvent with error %d",
		     err);
	}
	if (session[handle].tmpMemory) {
		scanCount *= session[handle].adcConfig.adcScanLength;
		while (scanCount--)
			*buf++ = *(session[handle].tmpMemoryP)++;
		(VOID) GlobalUnlock(session[handle].tmpMemory);
		(VOID) GlobalFree(session[handle].tmpMemory);
		session[handle].tmpMemoryP = NULL;
		if (err != DerrNoError)
			return err;

	}
	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqAdcTransferGetStat(DaqHandleT handle, PDWORD pActive, PDWORD pScanCount)
{
	DWORD dwAcqStatus = 0;
	DWORD dwScanCount = 0;
	ApiAdcPT ps;    

	DaqError err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	ps = &session[handle];

	if (DatmPacingMode & ps->adcXfer.adcXferMode) {
		if ((NULL != pActive)
		    && ((0 == *pActive) || (NULL != pScanCount))
		    ) {
			dwAcqStatus = *pActive;
			if (0 != dwAcqStatus) {
				dwScanCount = *pScanCount;
			}
		} else {
			return apiCtrlProcessError(handle, DerrInvBuf);
		}
	}

	cmnWaitForMutex(AdcStatMutex);
    
	err = adcTransferGetStat(handle, &dwAcqStatus, &dwScanCount);

	cmnReleaseMutex(AdcStatMutex);

	if (NULL != pScanCount) {
		ps->adcLastRetCount = *pScanCount = dwScanCount;
	}

	if (NULL != pActive) {

		*pActive = dwAcqStatus;
	}

	if (err != DerrNoError) {
		dbg("DAQX:daqAdcTransferGetStat calling apiCtrlProcessError with error %d",
		     err);
		return apiCtrlProcessError(handle, err);
	}

	return DerrNoError;
}

DaqError
adcTestConfig(DaqHandleT handle)
{
	DaqError err;
	daqIOT sb;

        memset((void *) &sb, 0, sizeof(sb));
	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	if (session[handle].adcAcqActive){
		dbg("DerrAcqArmed error");
		return apiCtrlProcessError(handle, DerrAcqArmed);
	}

	if (session[handle].adcChanged) {
		ADC_CONFIG_PT adcConfig = (ADC_CONFIG_PT) & sb;

		if ((session[handle].adcConfig.adcTrigSource == DatsImmediate)
		    && (session[handle].adcConfig.adcClockSource == DacsTriggerSource))
			return apiCtrlProcessError(handle, DerrInvClockSource);

		adcConfig->triggerEventList = session[handle].adcConfig.triggerEventList;

		adcConfig->adcClockSource = session[handle].adcConfig.adcClockSource;

		adcConfig->adcAcqMode = session[handle].adcConfig.adcAcqMode;
		adcConfig->adcAcqPreCount =
		    session[handle].adcConfig.adcAcqPreCount *
		    session[handle].adcConfig.adcScanLength;
		adcConfig->adcAcqPostCount =
		    session[handle].adcConfig.adcAcqPostCount *
		    session[handle].adcConfig.adcScanLength;
		adcConfig->adcPeriodmsec = session[handle].adcConfig.adcPeriodmsec;

		adcConfig->adcPeriodnsec = session[handle].adcConfig.adcPeriodnsec;
		adcConfig->adcPreTPeriodms = session[handle].adcConfig.adcPreTPeriodms;
		adcConfig->adcPreTPeriodns = session[handle].adcConfig.adcPreTPeriodns;
		adcConfig->adcTrigSource = session[handle].adcConfig.adcTrigSource;
		adcConfig->adcTrigLevel = session[handle].adcConfig.adcTrigLevel;
		adcConfig->adcTrigHyst = session[handle].adcConfig.adcTrigHyst;
		adcConfig->adcTrigChannel = session[handle].adcConfig.adcTrigChannel;
		adcConfig->adcScanSeq = session[handle].adcConfig.adcScanSeq;

		adcConfig->adcScanLength = session[handle].adcConfig.adcScanLength;
		adcConfig->adcEnhTrigCount = session[handle].adcConfig.adcEnhTrigCount;
		adcConfig->adcEnhTrigGain = session[handle].adcConfig.adcEnhTrigGain;
		adcConfig->adcEnhTrigChan = session[handle].adcConfig.adcEnhTrigChan;
		adcConfig->adcEnhTrigBipolar = session[handle].adcConfig.adcEnhTrigBipolar;
		adcConfig->adcEnhTrigOpcode = session[handle].adcConfig.adcEnhTrigOpcode;
		adcConfig->adcEnhTrigSens = session[handle].adcConfig.adcEnhTrigSens;
		adcConfig->adcDataPack = session[handle].adcConfig.adcDataPack;

		err = itfExecuteCommand(handle, &sb, ADC_CONFIG);

		if (err != DerrNoError)
			return apiCtrlProcessError(handle, err);

		session[handle].adcConfig.adcPeriodmsec = adcConfig->adcPeriodmsec;
		session[handle].adcConfig.adcPeriodnsec = adcConfig->adcPeriodnsec;
	}
	return DerrNoError;
}

/*
DaqError
getGainVal(DaqHandleT handle, DaqAdcGain gainCode, DaqChannelType chanType, float * val)
{
	*val = (float) 1.0;
	return DerrNoError;
}
*/

DaqError
daqCalcTriggerVals(DaqHandleT handle,
		   DaqAdcTriggerSource trigSource,
		   DaqEnhTrigSensT trigSensitivity,
		   DWORD channel,
		   DaqAdcGain gainCode,
		   DWORD flags,
		   DaqChannelType channelType,
		   float level, 
		   float variance, 
		   PDWORD pdwCalcLevel, 
		   PDWORD pdwCalcVariance)
{

	DaqError err;
	float fADmin, fADmax, fOffset, fMainGain, fExpGain;
	float fMaxVal, fMinVal;
	float fLevel, fVariance;
	BOOL bSigned, bBipolar, bDigital;
	DWORD x;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	if (session[handle].adcAcqActive){
		dbg("DerrAcqArmed error");
		return apiCtrlProcessError(handle, DerrAcqArmed);
	}

	if (!IsDaqDevice(session[handle].deviceType))
		return apiCtrlProcessError(handle, DerrNotImplemented);

	daqGetInfo(handle, channel, DdiADminInfo, &fADmin);
	daqGetInfo(handle, channel, DdiADmaxInfo, &fADmax);

	bBipolar = (flags & DafBipolar) ? TRUE : FALSE;
	fOffset = (float) ((flags & DafBipolar) ? 0.0 : 1.0) * fADmax;
	bSigned = (flags & DafSigned) ? TRUE : FALSE;
	bDigital = FALSE;

    //MAH: 04.05.04 added 1000
    if ( (!(IsDaq2000(session[handle].deviceType))|| (IsDaq2000(session[handle].deviceType)))
	    && bSigned && (trigSource == DatsSoftwareAnalog)
	    && (session[handle].HW2sComplement == FALSE))
		bSigned = FALSE;

	if ((channelType == DaqTypeDigitalLocal)

	    || (channelType == DaqTypeDigitalExp)
	    || (channelType == DaqTypeCounterLocal)) {

		fMainGain = 1.0f;
		fExpGain = 1.0f;
		bDigital = TRUE;
	} else if (channelType == DaqTypeAnalogLocal) {

		fMainGain = DaqGainArrayAnalogLocal[((gainCode) & 0x7)];
		fExpGain = 1.0f;
	} else if ((channelType >= DaqTypeDBKMin)
		   && (channelType <= DaqTypeDBKMax)) {

		x = 0;
		while (GainInfoStruct[x].DBKType != (DaqChannelType) DaqTypeNONE) {
			if (GainInfoStruct[x].DBKType == channelType)
				break;
			x++;
		}
		if (GainInfoStruct[x].DBKType == (DaqChannelType) DaqTypeNONE)
			return apiCtrlProcessError(handle, DerrInvExpCard);

		if (((channelType >= DaqTypeDBK20)
		     && (channelType <= DaqTypeDBK25))
		    || (channelType == DaqTypeDBK208))
			bDigital = TRUE;

		fMainGain = DaqGainArrayAnalogLocal[((gainCode >> 2) & 0x7)];
		fExpGain = GainInfoStruct[x].GainArray[((gainCode) & 0x3)];

	} else {
		return apiCtrlProcessError(handle, DerrInvExpCard);
	}

	if ((fMainGain == 0) || (fExpGain == 0))
		return apiCtrlProcessError(handle, DerrInvGain);

	switch (channelType) {
	case DaqTypeDBK9:
		{

			return apiCtrlProcessError(handle, DerrInvExpCard);
			break;
		}
	case DaqTypeDBK19:
	case DaqTypeDBK52:
	case DaqTypeDBK81:
	case DaqTypeDBK82:
		{
			float pfCalcLevel;
			float pfCalcVariance;
			DWORD TCTypeFlag;

			if ((flags & DafTcCJC) == DafTcCJC)
				return apiCtrlProcessError(handle, DerrTCE_PARAM);

			TCTypeFlag = flags & 0x00000780;
			if ((TCTypeFlag < 0x080) || (0x480 < TCTypeFlag)) {
				break;
			}

			err = daqCalcTCTriggerVals(handle,
						   trigSensitivity,
						   channel,
						   gainCode,
						   flags,
						   fMainGain * fExpGain,
						   channelType,
						   level, variance, &pfCalcLevel, &pfCalcVariance);

			if (err != DerrNoError)
				return apiCtrlProcessError(handle, err);
			else {
				level = pfCalcLevel;
				variance = pfCalcVariance;
			}
			break;
		}
	default:
		{
			break;
		}
	}

	if ((bDigital == TRUE) && (bSigned == TRUE))
		return apiCtrlProcessError(handle, DerrInvRawDataFormat);

	if (bDigital == TRUE) {
		fMinVal = fADmin = (float) 0x0;
		fMaxVal = fADmax = (float) 0xFFFF;

	} else {
		fMinVal = (fADmin + fOffset) / fMainGain / fExpGain;
		fMaxVal = (fADmax + fOffset) / fMainGain / fExpGain;
	}

	if ((level < fMinVal) || (level > fMaxVal))
		return apiCtrlProcessError(handle, DerrInvLevel);

	if (trigSensitivity == DetsRisingEdge)
		variance = level - (float) fabs(variance);
	if (trigSensitivity == DetsFallingEdge)
		variance = level + (float) fabs(variance);

	if ((variance < fMinVal) || (variance > fMaxVal))
		return apiCtrlProcessError(handle, DerrInvLevel);

	if (((trigSensitivity == DetsEQLevel)
	     || (trigSensitivity == DetsNELevel)
	    ) && (trigSource == DatsSoftwareAnalog)) {
		variance = (float) fabs(variance);
		if (((level - variance) < fMinVal)
		    || ((level + variance) > fMaxVal))
			return apiCtrlProcessError(handle, DerrInvLevel);
	}

	fLevel = level * fMainGain * fExpGain;
	if (bBipolar)
		fLevel += (fADmax - fADmin) / 2.0f;
	fLevel *= 65535.0f / (fADmax - fADmin);
	if ((fLevel < 0.0f) || (fLevel > 65535.0f))

		return apiCtrlProcessError(handle, DerrInvLevel);

	if (bSigned && (trigSource == DatsSoftwareAnalog)) {
		fLevel -= 32767.0f;
	}

	*pdwCalcLevel = (DWORD) fLevel;

	if (trigSource == DatsSoftwareAnalog) {
		switch (trigSensitivity) {
		case DetsEQLevel:
		case DetsNELevel:

			{

				fVariance = variance * fMainGain * fExpGain;
				fVariance *= 65535.0f / (fADmax - fADmin);
				if ((fVariance < 0.0f)
				    || (fVariance > 65535.0f))

					return apiCtrlProcessError(handle, DerrInvLevel);
			}
			break;

		case DetsRisingEdge:
		case DetsFallingEdge:
			{

				fVariance = variance * fMainGain * fExpGain;
				if (bBipolar)
					fVariance += (fADmax - fADmin) / 2.0f;
				fVariance *= 65535.0f / (fADmax - fADmin);
				if ((fVariance < 0.0f)
				    || (fVariance > 65535.0f))
					return apiCtrlProcessError(handle, DerrInvLevel);

				if (bSigned && (trigSource == DatsSoftwareAnalog)) {
					fVariance -= 32767.0f;
				}

			}
			break;

		case DetsAboveLevel:
		case DetsBelowLevel:
		default:

			fVariance = fLevel;
			break;
		}

	}

	else if (trigSource == DatsHardwareAnalog) {

		fVariance = variance * fMainGain * fExpGain;
		if (bBipolar)
			fVariance += (fADmax - fADmin) / 2.0f;
		fVariance *= 65535.0f / (fADmax - fADmin);
		if ((fVariance < 0.0f) || (fVariance > 65535.0f))
			return apiCtrlProcessError(handle, DerrInvLevel);
	}

	*pdwCalcVariance = (DWORD) fVariance;

	return DerrNoError;

}

DaqError
daqCalcTCTriggerVals(DaqHandleT handle,
		     DaqEnhTrigSensT trigSensitivity,
		     DWORD channel,
		     DaqAdcGain gainCode,
		     DWORD flags,
		     float gainVal,
		     DaqChannelType channelType,
		     float level, float variance, float * pfCalcLevel, float * pfCalcVariance)
{

	DaqError err;
	double levelVoltage, VarianceVoltage;
	double cjcTempVoltage;
	float maxADVolts;
	double cjcTemp;
	DWORD CJCchan;

	DaqAdcGain CJCgain;
	DWORD CJCflags, TCTypeFlag;
	TCType tcType;
	WORD wData;
	BOOL bBipolar;

	ID_LINK_T TCMap[] = {
		{DafTcTypeJ, Dbk19TCTypeJ},
		{DafTcTypeK, Dbk19TCTypeK},
		{DafTcTypeT, Dbk19TCTypeT},
		{DafTcTypeE, Dbk19TCTypeE},
		{DafTcTypeN28, Dbk19TCTypeN28},
		{DafTcTypeN14, Dbk19TCTypeN14},
		{DafTcTypeS, Dbk19TCTypeS},
		{DafTcTypeR, Dbk19TCTypeR},
		{DafTcTypeB, Dbk19TCTypeB},
	};

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError)
		return apiCtrlProcessError(handle, err);

	if (session[handle].adcAcqActive){
		dbg("DerrAcqArmed error");
		return apiCtrlProcessError(handle, DerrAcqArmed);
	}

	if (!IsDaqDevice(session[handle].deviceType))
		return apiCtrlProcessError(handle, DerrNotImplemented);

	if (channel < 16)
		return apiCtrlProcessError(handle, DerrInvChan);

	daqGetInfo(handle, 0, DdiADmaxInfo, &maxADVolts);

	switch (channelType) {
	case DaqTypeDBK19:
	case DaqTypeDBK52:
	case DaqTypeDBK81:
	case DaqTypeDBK82:
		{

			if ((trigSensitivity == DetsEQLevel)
			    || (trigSensitivity == DetsNELevel)) {
				return apiCtrlProcessError(handle, DerrNotImplemented);
			}

			if ((channelType == DaqTypeDBK81)
			    || (channelType == DaqTypeDBK82)) {

				if (channel % 8 == 0)
					return apiCtrlProcessError(handle, DerrInvChan);

				CJCchan = (DWORD) (channel / 8) * 8;

			} else {

				if (channel % 16 == 0)
					return apiCtrlProcessError(handle, DerrInvChan);

				CJCchan = (DWORD) (channel / 16) * 16;
			}

			TCTypeFlag = flags & 0x00000780;
			if ((TCTypeFlag < 0x080) || (0x480 < TCTypeFlag))
				return apiCtrlProcessError(handle, DerrTCE_PARAM);

			if (!MapApiToIoctl(TCTypeFlag, (PINT) & tcType, TCMap, NUM(TCMap)))
				return apiCtrlProcessError(handle, DerrTCE_PARAM);

			CJCgain = (maxADVolts > 5.0 ? Dbk13X2 : Dbk13X1);
			CJCflags = DafBipolar | DafUnsigned | DafSingleEnded | DafAnalog;

			err = apiAdcReadSample(handle, CJCchan, CJCgain, CJCflags, 1, &wData);

			if (err != DerrNoError)
				return apiCtrlProcessError(handle, err);

			bBipolar = (flags & DafBipolar) ? TRUE : FALSE;

			switch (channelType) {
			case DaqTypeDBK19:
			case DaqTypeDBK52:
				{
					cjcTemp = ((wData - 32767.0)) / 655.350;
					break;
				}

			case DaqTypeDBK81:
			case DaqTypeDBK82:
				{

					err = (DaqError) thermistor2Temp(wData, bBipolar, &cjcTemp);
					if (err != DerrNoError)
						return apiCtrlProcessError(handle, err);

					break;

				}
			default:
				break;
			}

			err = (DaqError) ExtTemp2Voltage(&cjcTempVoltage, cjcTemp, tcType);
			if (err) {
				return err;
			}

			if (trigSensitivity == DetsRisingEdge)
				variance = level - (float) fabs(variance);
			if (trigSensitivity == DetsFallingEdge)

				variance = level + (float) fabs(variance);

			err = (DaqError) ExtTemp2Voltage(&levelVoltage, (double) (level), tcType);
			if (err) {
				return err;
			}
			err =
			    (DaqError) ExtTemp2Voltage(&VarianceVoltage,
						       (double) (variance), tcType);
			if (err) {
				return err;
			}

			levelVoltage -= cjcTempVoltage;
			VarianceVoltage -= cjcTempVoltage;

			if (trigSensitivity == DetsRisingEdge)

				VarianceVoltage = levelVoltage - VarianceVoltage;

			if (trigSensitivity == DetsFallingEdge)
				VarianceVoltage = VarianceVoltage - levelVoltage;

			break;
		}
	case DaqTypeDBK9:
		{

			return apiCtrlProcessError(handle, DerrNotImplemented);
			break;
		}
	default:
		{
			return apiCtrlProcessError(handle, DerrInvExpCard);
			break;

		}
	}

	*pfCalcLevel = (float) levelVoltage;
	*pfCalcVariance = (float) VarianceVoltage;

	return DerrNoError;

}

DWORD
AdcRawTransferCount(DaqHandleT handle)
{
	return XferToScanCount(session[handle].adcStat.adcXferCount,
			       session[handle].adcConfig.adcScanLength);
}

static DWORD
XferToScanCount(DWORD dwXferCount, DWORD dwScanLen)
{
	if (0 == dwScanLen)
		return 0;
	return dwXferCount / dwScanLen;
}

static DWORD
ScanToXferCount(DWORD dwScanCount, DWORD dwScanLen)
{
	return dwScanCount * dwScanLen;
}

DAQAPI DaqError WINAPI
daqDumpDriverDebugBuffer(DaqHandleT handle, PCHAR buf, DWORD charCount)
{
	DaqError err;
	daqIOT sb;
	ADC_START_PT adcStart;


        memset((void *) &sb, 0, sizeof(sb));
	adcStart = (ADC_START_PT) & sb;
	adcStart->errorCode = DerrNoError;
	adcStart->adcXferBufLen = charCount / 2;
	adcStart->adcXferBuf = (SHORT *) buf;
	adcStart->adcXferMode = 0x80;

	err = itfExecuteCommand(handle, &sb, ADC_START);
	dbg("daqDumpDriverDebugBuffer: driver returned %d", err);

	return err;
}
