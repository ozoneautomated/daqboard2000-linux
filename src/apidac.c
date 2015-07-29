////////////////////////////////////////////////////////////////////////////
//
// apidac.c                        I    OOO                           h
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

#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include "iottypes.h"

#include "daqx.h"
#include "cmnapi.h"
#include "api.h"

#include "ddapi.h"
#include "itfapi.h"

#define  MaxDacDeviceTypes             3

typedef enum {
	DdbsNone = 0,
	DdbsPreDefWave = 1,
	DdbsDiskFile = 2,

	DdbsUserWave = 3,
} DaqDacBufferSource;

typedef struct {
	WORD dacOutput;

	DaqDacOutputMode dacSelectedFormat;
	DWORD dacHWFormatCapable;
} ApiDacVoltageT;

typedef struct {
	DAC_CONFIG_T dacConfig;
	BOOL dacConfigChanged;

	CHAR dacFileName[PATH_MAX];

	DaqDacWaveType dacPreDefWaveType;
	DWORD dacPreDefAmplitude;
	DWORD dacPreDefOffset;
	DWORD dacPreDefDutyCycle;
	DWORD dacPreDefPhaseShift;

	DAC_START_T dacStart;

	DaqDacBufferSource dacBufferSource;
	BOOL dacBufferChanged;

	DWORD dacFileOffsetBytes;
	DWORD dacFileOffsetUpdateCycle;

	DWORD dacFileTotalUpdates;
	DWORD dacFileDataFormat;

	FILE *dacFileHandle;
	DWORD dacFileCurrentScan;

    
// fpos_t dacFileStartPosition;
    DWORD dacFileStartPosition;

    
	DWORD dacFileUpdatesInFile;
} ApiDacWaveT;

typedef struct {
	DWORD dacChannelCount;
	DWORD dacFirstChannel;
	DWORD *dacAvail;
	DaqDacOutputMode *dacOutputMode;
	ApiDacVoltageT *dacVoltage;
	BOOL dacWaveActive;
	ApiDacWaveT *dacWave;
	DWORD dacNumWaveActive;
	BOOL dacWaveFromFileActive;
	DWORD dacOldReadCount;

} ApiDacDeviceT;

typedef struct {
	ApiDacDeviceT *dacDevice[MaxDacDeviceTypes];
	DaqHardwareVersion deviceType;
	PWORD dacXferDrvrBuf;
} ApiDacT;

static ApiDacT session[MaxSessions];

static VOID dacDeallocateAndDisarm(DaqHandleT handle);

DWORD
dacFindFirstFileChan(DaqHandleT handle, DaqDacDeviceType * deviceType, DWORD * chan)
{
	BOOL bDetected = FALSE;
	ApiDacDeviceT *dacDevice;
	ApiDacWaveT *dacWave;
	DWORD dacChannel = 0;
	DaqDacDeviceType vDeviceType = DddtLocal;

	if (session[handle].dacDevice[DddtLocal] != NULL) {
		vDeviceType = DddtLocal;
		dacDevice = session[handle].dacDevice[vDeviceType];
		for (dacChannel = dacDevice->dacFirstChannel;
		     dacChannel <
		     dacDevice->dacFirstChannel + dacDevice->dacChannelCount; dacChannel++) {
			if (dacDevice->dacOutputMode[dacChannel] != DdomVoltage) {
				dacWave = &dacDevice->dacWave[dacChannel];
				if (dacWave->dacBufferSource == DdbsDiskFile) {
					bDetected = TRUE;
					break;
				}
			}
		}
	}
	if (bDetected == FALSE) {
		if (session[handle].dacDevice[DddtLocalDigital] != NULL) {
			vDeviceType = DddtLocalDigital;
			dacDevice = session[handle].dacDevice[vDeviceType];
			for (dacChannel = dacDevice->dacFirstChannel;
			     dacChannel <
			     dacDevice->dacFirstChannel +
			     dacDevice->dacChannelCount; dacChannel++) {
				if (dacDevice->dacOutputMode[dacChannel] != DdomVoltage) {
					dacWave = &dacDevice->dacWave[dacChannel];
					if (dacWave->dacBufferSource == DdbsDiskFile) {
						bDetected = TRUE;
						break;
					}
				}
			}
		}
	}

	*deviceType = vDeviceType;
	*chan = dacChannel;

	return bDetected;

}

DaqError
openAndConvertFile(DaqHandleT handle, ApiDacWaveT * dacWave, FILE ** outputFilePtr)
{
#ifndef DLINUX
#if 0
	FILE *sourceFile = NULL;
	double doublesample;
	float floatsample;
	__int64_t dbllongsample;
	DWORD bitCount;
	WORD wordsample, lowword, highword;
	DWORD digitalChannelNumber, totalWaveFileChannels = 0, currentSampleCount = 0;
	BOOL digitalChannelEnabled = FALSE;
	DWORD totalWaveChannels = 0;

	DWORD skipSamples = 0;
	DWORD sampleSize = sizeof (WORD);
	fpos_t filePointer;
	size_t readSize, writeSize;
	CHAR formatstr[3] = "%d";
	FILE *outputFileHandle = NULL;
	DWORD sampleCount = 0;
	BOOL offSetByteError = FALSE;

	if (session[handle].dacDevice[DddtLocalDigital] != NULL) {
		digitalChannelEnabled =
		    (session[handle].dacDevice[DddtLocalDigital]->
		     dacOutputMode[0] != DdomVoltage) ? TRUE : FALSE;
		totalWaveFileChannels =
		    session[handle].dacDevice[DddtLocalDigital]->dacNumWaveActive;
	} else {
		digitalChannelEnabled = FALSE;
	}

	digitalChannelNumber = 0;

	if (session[handle].dacDevice[DddtLocal] != NULL) {
		totalWaveFileChannels = session[handle].dacDevice[DddtLocal]->dacNumWaveActive;
	}

	if (dacWave->dacFileOffsetUpdateCycle) {
		skipSamples = dacWave->dacFileOffsetUpdateCycle * totalWaveFileChannels;
	}

	switch (dacWave->dacFileDataFormat) {
	default:
	case DdwdfBinaryCounts:

		outputFileHandle = fopen(dacWave->dacFileName, "rb");
		if (outputFileHandle == NULL) {
			dacWave->dacFileHandle = NULL;
			dacWave->dacFileUpdatesInFile = (DWORD) 0;
			return DerrFileOpenError;
		}

		sampleCount = (DWORD) (filelength(fileno(outputFileHandle)) / sizeof (WORD));

		break;

	case DdwdfBinaryCountsHL:

		outputFileHandle = fopen("temp.bin", "w+b");
		sourceFile = fopen(dacWave->dacFileName, "rb");
		sampleSize = sizeof (WORD);

		if (sourceFile != NULL) {
			if (dacWave->dacFileOffsetBytes >= (DWORD) filelength(fileno(sourceFile))) {
				offSetByteError = TRUE;
			}
		}

		if ((sourceFile == NULL) || (outputFileHandle == NULL)
		    || offSetByteError) {

			if (sourceFile != NULL) {
				fclose(sourceFile);
			}
			if (outputFileHandle != NULL) {
				fclose(outputFileHandle);
			}
			dacWave->dacFileHandle = NULL;
			dacWave->dacFileUpdatesInFile = (DWORD) 0;

			if (offSetByteError) {
				return DerrDacWaveFileTooSmall;
			} else {
				return DerrFileOpenError;
			}
		}

		filePointer = dacWave->dacFileOffsetBytes;
		fsetpos(sourceFile, &filePointer);

		readSize = fread(&wordsample, sampleSize, 1, sourceFile);
		while (readSize == 1) {

			if (currentSampleCount >= skipSamples) {

				lowword = wordsample >> 8;
				highword = wordsample << 8;
				wordsample = highword + lowword;

				writeSize = fwrite(&wordsample, sizeof (WORD), 1, outputFileHandle);
				if (writeSize != 1) {
					return DerrFileWriteError;
				}

				sampleCount++;
			}

			readSize = fread(&wordsample, sampleSize, 1, sourceFile);
			currentSampleCount++;
		}

		fclose(sourceFile);

		break;

	case DdwdfBinaryDouble:
		sampleSize = sizeof (double);

		outputFileHandle = fopen("temp.bin", "w+b");
		sourceFile = fopen(dacWave->dacFileName, "rb");

		if (sourceFile != NULL) {
			if (dacWave->dacFileOffsetBytes >= (DWORD) filelength(fileno(sourceFile))) {
				offSetByteError = TRUE;
			}
		}

		if ((sourceFile == NULL) || (outputFileHandle == NULL)
		    || offSetByteError) {

			if (sourceFile != NULL) {
				fclose(sourceFile);
			}
			if (outputFileHandle != NULL) {
				fclose(outputFileHandle);
			}
			dacWave->dacFileHandle = NULL;
			dacWave->dacFileUpdatesInFile = (DWORD) 0;

			if (offSetByteError) {
				return DerrDacWaveFileTooSmall;
			} else {
				return DerrFileOpenError;
			}
		}

		filePointer = dacWave->dacFileOffsetBytes;
		fsetpos(sourceFile, &filePointer);

		readSize = fread(&doublesample, sampleSize, 1, sourceFile);
		while (readSize == 1) {

			if (currentSampleCount >= skipSamples) {

				wordsample = (WORD) (((doublesample + 10.0) / 20.0) * 65535.0);
				if (digitalChannelEnabled) {
					if ((currentSampleCount %
					     totalWaveFileChannels) == digitalChannelNumber) {
						wordsample = (WORD) doublesample;
					}
				}

				writeSize = fwrite(&wordsample, sizeof (WORD), 1, outputFileHandle);
				if (writeSize != 1) {
					return DerrFileWriteError;
				}
				sampleCount++;
			}

			readSize = fread(&doublesample, sampleSize, 1, sourceFile);
			currentSampleCount++;
		}

		fclose(sourceFile);

		break;

	case DdwdfBinaryFloat:

		sampleSize = sizeof (float);

		outputFileHandle = fopen("temp.bin", "w+b");
		sourceFile = fopen(dacWave->dacFileName, "rb");

		if (sourceFile != NULL) {
			if (dacWave->dacFileOffsetBytes >= (DWORD) filelength(fileno(sourceFile))) {
				offSetByteError = TRUE;
			}
		}

		if ((sourceFile == NULL) || (outputFileHandle == NULL)
		    || offSetByteError) {

			if (sourceFile != NULL) {
				fclose(sourceFile);
			}
			if (outputFileHandle != NULL) {
				fclose(outputFileHandle);
			}
			dacWave->dacFileHandle = NULL;
			dacWave->dacFileUpdatesInFile = (DWORD) 0;

			if (offSetByteError) {
				return DerrDacWaveFileTooSmall;
			} else {
				return DerrFileOpenError;
			}
		}

		filePointer = dacWave->dacFileOffsetBytes;
		fsetpos(sourceFile, &filePointer);

		readSize = fread(&floatsample, sampleSize, 1, sourceFile);
		while (readSize == 1) {

			if (currentSampleCount >= skipSamples) {

				wordsample = (WORD) ((double)
						     (((floatsample + 10.0f) / 20.0) * 65535.0));
				if (digitalChannelEnabled) {

					if ((currentSampleCount %
					     totalWaveFileChannels) == digitalChannelNumber) {
						wordsample = (WORD) floatsample;
					}
				}

				writeSize = fwrite(&wordsample, sizeof (WORD), 1, outputFileHandle);
				if (writeSize != 1) {
					return DerrFileWriteError;
				}
				sampleCount++;
			}

			readSize = fread(&floatsample, sampleSize, 1, sourceFile);
			currentSampleCount++;
		}

		fclose(sourceFile);

		break;

	case DdwdfAsciiCountsBin:
		{

			outputFileHandle = fopen("temp.bin", "w+b");
			sourceFile = fopen(dacWave->dacFileName, "r");

			if (sourceFile != NULL) {
				if (dacWave->dacFileOffsetBytes >=
				    (DWORD) filelength(fileno(sourceFile))) {
					offSetByteError = TRUE;
				}
			}

			if ((sourceFile == NULL) || (outputFileHandle == NULL)
			    || offSetByteError) {

				if (sourceFile != NULL) {
					fclose(sourceFile);
				}
				if (outputFileHandle != NULL) {
					fclose(outputFileHandle);

				}
				dacWave->dacFileHandle = NULL;
				dacWave->dacFileUpdatesInFile = (DWORD) 0;

				if (offSetByteError) {
					return DerrDacWaveFileTooSmall;
				} else {
					return DerrFileOpenError;
				}
			}

			strcpy(formatstr, "%I64d");

			filePointer = dacWave->dacFileOffsetBytes;
			fsetpos(sourceFile, &filePointer);

			while (!feof(sourceFile)) {
				readSize = fscanf(sourceFile, formatstr, &dbllongsample);
				if (readSize == 0) {
					fgetpos(sourceFile, &filePointer);
					fsetpos(sourceFile, &(++filePointer));
				} else if (readSize == EOF) {
					break;
				} else {
					currentSampleCount++;

					if (currentSampleCount >= skipSamples) {

						bitCount = 0;
						wordsample = 0;
						while (dbllongsample > 0) {
							wordsample +=
							    (WORD) ((dbllongsample % 10 ==
								     1) ? 1 : 0) << bitCount;
							bitCount++;
							dbllongsample /= 10;
						}

						writeSize =
						    fwrite(&wordsample,
							   sizeof (WORD), 1, outputFileHandle);
						if (writeSize != 1) {
							return DerrFileWriteError;
						}
						sampleCount++;
					}
				}
			}

			fclose(sourceFile);

		}
		break;
	case DdwdfAsciiCountsDec:
	case DdwdfAsciiCountsHex:
	case DdwdfAsciiCountsOct:

		outputFileHandle = fopen("temp.bin", "w+b");
		sourceFile = fopen(dacWave->dacFileName, "r");

		if (sourceFile != NULL) {
			if (dacWave->dacFileOffsetBytes >= (DWORD) filelength(fileno(sourceFile))) {
				offSetByteError = TRUE;
			}
		}

		if ((sourceFile == NULL) || (outputFileHandle == NULL)
		    || offSetByteError) {

			if (sourceFile != NULL) {
				fclose(sourceFile);
			}
			if (outputFileHandle != NULL) {
				fclose(outputFileHandle);
			}
			dacWave->dacFileHandle = NULL;
			dacWave->dacFileUpdatesInFile = (DWORD) 0;

			if (offSetByteError) {
				return DerrDacWaveFileTooSmall;
			} else {
				return DerrFileOpenError;

			}
		}

		switch (dacWave->dacFileDataFormat) {
		case DdwdfAsciiCountsDec:
			strcpy(formatstr, "%hd");
			break;
		case DdwdfAsciiCountsHex:
			strcpy(formatstr, "%hx");
			break;

		case DdwdfAsciiCountsOct:
			strcpy(formatstr, "%ho");
			break;
		}

		filePointer = dacWave->dacFileOffsetBytes;
		fsetpos(sourceFile, &filePointer);

		while (!feof(sourceFile)) {
			readSize = fscanf(sourceFile, formatstr, &wordsample);
			if (readSize == 0) {
				fgetpos(sourceFile, &filePointer);
				fsetpos(sourceFile, &(++filePointer));
			} else if (readSize == EOF) {
				break;
			} else {
				currentSampleCount++;

				if (currentSampleCount >= skipSamples) {

					writeSize =
					    fwrite(&wordsample, sizeof (WORD), 1, outputFileHandle);
					if (writeSize != 1) {
						return DerrFileWriteError;
					}
					sampleCount++;
				}
			}
		}

		fclose(sourceFile);

		break;

	case DdwdfAsciiFloat:

		outputFileHandle = fopen("temp.bin", "w+b");
		sourceFile = fopen(dacWave->dacFileName, "r");

		if (sourceFile != NULL) {
			if (dacWave->dacFileOffsetBytes >= (DWORD) filelength(fileno(sourceFile))) {
				offSetByteError = TRUE;

			}
		}

		if ((sourceFile == NULL) || (outputFileHandle == NULL)
		    || offSetByteError) {

			if (sourceFile != NULL) {
				fclose(sourceFile);
			}
			if (outputFileHandle != NULL) {
				fclose(outputFileHandle);
			}
			dacWave->dacFileHandle = NULL;
			dacWave->dacFileUpdatesInFile = (DWORD) 0;

			if (offSetByteError) {
				return DerrDacWaveFileTooSmall;
			} else {
				return DerrFileOpenError;
			}
		}

		filePointer = dacWave->dacFileOffsetBytes;
		fsetpos(sourceFile, &filePointer);

		while (!feof(sourceFile)) {

			readSize = fscanf(sourceFile, "%f", &floatsample);
			if (readSize == 0) {
				fgetpos(sourceFile, &filePointer);
				fsetpos(sourceFile, &(++filePointer));
			} else if (readSize == EOF) {
				break;
			} else {

				currentSampleCount++;

				if (currentSampleCount > skipSamples) {
					if ((digitalChannelEnabled) &&
					    ((currentSampleCount %
					      totalWaveFileChannels) == digitalChannelNumber)) {
						wordsample = (WORD) floatsample;
					} else {

						floatsample += 10.0;
						wordsample = (WORD) ((floatsample / 20.0) * 65535);
					}

					writeSize =
					    fwrite(&wordsample, sizeof (WORD), 1, outputFileHandle);
					if (writeSize != 1) {
						return DerrFileWriteError;
					}
					sampleCount++;
				}
			}
		}

		fclose(sourceFile);

		break;
	}

	if (sampleCount < totalWaveFileChannels) {

		if (outputFileHandle != NULL) {
			fclose(outputFileHandle);
		}
		dacWave->dacFileHandle = NULL;
		dacWave->dacFileUpdatesInFile = 0;

		return apiCtrlProcessError(handle, DerrDacWaveFileTooSmall);
	}

	dacWave->dacFileHandle = outputFileHandle;

	dacWave->dacFileUpdatesInFile = (DWORD) (sampleCount / totalWaveFileChannels);

	return DerrNoError;

#endif
#else
	return apiCtrlProcessError(handle, DerrNotImplemented);
#endif
}

DaqError
dacShutdownWaveFromFile(DaqHandleT handle, DWORD dacChannel)
{

	if (session[handle].dacDevice[dacChannel]->dacWave != NULL) {

		if (session[handle].dacDevice[dacChannel]->dacWave->dacFileName != NULL) {
			fclose(session[handle].dacDevice[dacChannel]->dacWave->dacFileHandle);
			session[handle].dacDevice[dacChannel]->dacWave->dacFileName[0] = 0x00;
			session[handle].dacDevice[dacChannel]->dacWave->dacBufferSource = DdbsNone;
			session[handle].dacDevice[dacChannel]->dacWave->dacBufferChanged = FALSE;
		}
	}

	return DerrNoError;

}

DaqError
dacXferFileToBuffer(DaqHandleT handle, DaqDacDeviceType deviceType, DWORD chan, DWORD updateRequest)
{
#ifndef DLINUX
#if 0

	ApiDacWaveT *dacWave = &session[handle].dacDevice[deviceType]->dacWave[chan];

	PVOID pXferBuf = dacWave->dacStart.dacXferBuf;
	DWORD xferBufLoc = dacWave->dacStart.dacXferBufHead;
	DWORD xferBufSize = dacWave->dacStart.dacXferBufLen;

	fpos_t xferFileStart = dacWave->dacFileStartPosition;
	DWORD xferFileLoc = dacWave->dacFileCurrentScan;
	DWORD xferFileSize = dacWave->dacFileUpdatesInFile;
	FILE *dacFileHandle = dacWave->dacFileHandle;

	DWORD sampleSize = sizeof (WORD);
	DWORD scanLength = session[handle].dacDevice[deviceType]->dacNumWaveActive;
	DWORD sizeAdj = sampleSize * scanLength;

	DWORD readAmount, readRoom, amountActuallyRead;
	fpos_t filePointer;
	PVOID bufPointer;

	if (updateRequest > xferBufSize) {
		return DerrDacWaveFileUnderrun;
	}

	while (updateRequest > 0) {

		readRoom = min(xferBufSize - xferBufLoc, xferFileSize - xferFileLoc);
		readAmount = min(readRoom, updateRequest);
		updateRequest -= readAmount;

		filePointer = xferFileStart + (xferFileLoc * sizeAdj);
		fsetpos(dacFileHandle, &filePointer);

		bufPointer = (PVOID) ((DWORD) pXferBuf + (xferBufLoc * sizeAdj));

		amountActuallyRead = fread((VOID *) bufPointer, sizeAdj, readAmount, dacFileHandle);
		if (amountActuallyRead != readAmount) {
			return DerrFileReadError;
		}

		xferBufLoc = (xferBufLoc + readAmount) % xferBufSize;
		xferFileLoc = (xferFileLoc + readAmount) % xferFileSize;

	}

	dacWave->dacStart.dacXferBufHead = xferBufLoc;
	dacWave->dacFileCurrentScan = xferFileLoc;

	return DerrNoError;
#endif
#else
	return apiCtrlProcessError(handle, DerrNotImplemented);
#endif
}

VOID
apiDacMessageHandler(MsgPT msg)
{
	DWORD i, deviceType;
	
	if (msg->errCode != DerrNoError) {
		return;
	}

	switch (msg->msg) {
	case MsgAttach:
		for (i = 0; i < MaxSessions; i++)
			for (deviceType = DddtLocal;
			     deviceType < MaxDacDeviceTypes; deviceType++)
				session[i].dacDevice[deviceType] = NULL;
		break;

	case MsgDetach:
		for (i = 0; i < MaxSessions; i++)
			dacDeallocateAndDisarm(i);
		break;

	case MsgInit:
	{
		ApiDacDeviceT *dacDevice;
		DWORD chan;
		DWORD mallocCount;
		dacDeallocateAndDisarm(msg->handle);
		session[msg->handle].deviceType = msg->deviceType;
		session[msg->handle].dacXferDrvrBuf = NULL;

		switch (msg->deviceType) {
		case TempBook66:
			break;

		case DaqBoard100:
		case DaqBoard112:
		case DaqBoard200:
		case DaqBoard216:
			dacDevice = (ApiDacDeviceT *)
				iotMalloc(sizeof (ApiDacDeviceT));
			session[msg->handle].dacDevice[DddtLocal] = dacDevice;
			dacDevice->dacChannelCount = 2;
			dacDevice->dacNumWaveActive = 0;
			dacDevice->dacOldReadCount = 0;
			dacDevice->dacFirstChannel = 0;
			mallocCount =
				dacDevice->dacChannelCount + dacDevice->dacFirstChannel;
			dacDevice->dacAvail =
				(DWORD *) iotMalloc(mallocCount * sizeof (DWORD));
			dacDevice->dacOutputMode = (DaqDacOutputMode *)
				iotMalloc(mallocCount * sizeof (DaqDacOutputMode));
			dacDevice->dacVoltage = (ApiDacVoltageT *)
				iotMalloc(mallocCount * sizeof (ApiDacVoltageT));
			dacDevice->dacWave = (ApiDacWaveT *)
				iotMalloc(mallocCount * sizeof (ApiDacWaveT));

			dacDevice->dacWaveActive = FALSE;
			for (chan = dacDevice->dacFirstChannel;
			     chan < dacDevice->dacFirstChannel +
				     dacDevice->dacChannelCount; chan++) {

				apiDacSetAvail(msg->handle, DddtLocal, chan,
					       DacAvailVoltage 
					       | DacAvailStaticWave
					       | DacAvailDynamicWave);

				dacDevice->dacVoltage[chan].
					dacHWFormatCapable = FALSE;

				daqDacSetOutputMode(msg->handle, DddtLocal,
						    chan, DdomStaticWave);
				daqDacWaveSetTrig(msg->handle, DddtLocal,
						  chan, DdtsImmediate, TRUE);
				daqDacWaveSetClockSource(msg->handle,
							 DddtLocal,
							 chan, DdcsDacClock);
				daqDacWaveSetFreq(msg->handle,
						  DddtLocal, chan, 10.0f);
				daqDacWaveSetMode(msg->handle,
						  DddtLocal, chan, DdwmInfinite, 0);
				daqDacWaveSetUserWave(msg->handle, DddtLocal, chan);
				daqDacWaveSetBuffer(msg->handle, DddtLocal,
						    chan, NULL, 1, DdtmUpdateBlock);

				dacDevice->dacWave[chan].dacFileHandle = NULL;

				daqDacSetOutputMode(msg->handle,
						    DddtLocal, chan, DdomVoltage);

				daqDacWt(msg->handle, DddtLocal, chan, 0);
			}

			dacDevice = (ApiDacDeviceT *)
				iotMalloc(sizeof (ApiDacDeviceT));
			session[msg->handle].dacDevice[DddtDbk] = dacDevice;
			dacDevice->dacChannelCount = 256;
			dacDevice->dacFirstChannel = 16;
			dacDevice->dacNumWaveActive = 0;
			mallocCount = dacDevice->dacChannelCount + dacDevice->dacFirstChannel;
			dacDevice->dacAvail =
				(DWORD *) iotMalloc(mallocCount * sizeof (DWORD));
			dacDevice->dacOutputMode = (DaqDacOutputMode *)
				iotMalloc(mallocCount * sizeof (DaqDacOutputMode));
			dacDevice->dacVoltage = (ApiDacVoltageT *)
				iotMalloc(mallocCount * sizeof (ApiDacVoltageT));
			dacDevice->dacWave = NULL;
			dacDevice->dacWaveActive = FALSE;

			for (chan = dacDevice->dacFirstChannel;
			     chan < dacDevice->dacFirstChannel +
				     dacDevice->dacChannelCount; chan++) {

				apiDacSetAvail(msg->handle,
					       DddtDbk, chan, DacAvailNone);
				dacDevice->dacVoltage[chan].
					dacHWFormatCapable = FALSE;
			}
			break;

		case DaqBoard2000:
		case DaqBoard2001:
		case DaqBoard2002:
		case DaqBoard2003:
		case DaqBoard2004:
		case DaqBoard1000:
        case DaqBoard1005:
			case DaqBoard2005:
			if ((msg->deviceType != DaqBoard2002)
			    && (msg->deviceType != DaqBoard2005)) {

				dacDevice = (ApiDacDeviceT *)
					iotMalloc(sizeof (ApiDacDeviceT));
				session[msg->handle].
					dacDevice[DddtLocal] = dacDevice;

				if (msg->deviceType == DaqBoard2000) {
					dacDevice->dacChannelCount = 2;
				} else {
					dacDevice->dacChannelCount = 4;
				}

				dacDevice->dacNumWaveActive = 0;
				dacDevice->dacOldReadCount = 0;
				dacDevice->dacFirstChannel = 0;
				mallocCount = dacDevice->dacChannelCount +
					dacDevice->dacFirstChannel;
				dacDevice->dacAvail = (DWORD *)
					iotMalloc(mallocCount * sizeof (DWORD));
				dacDevice->dacOutputMode = (DaqDacOutputMode *)
					iotMalloc(mallocCount *
						  sizeof (DaqDacOutputMode));
				dacDevice->dacVoltage = (ApiDacVoltageT *)
					iotMalloc(mallocCount *
						  sizeof (ApiDacVoltageT));
				dacDevice->dacWave = (ApiDacWaveT *)
					iotMalloc(mallocCount * sizeof (ApiDacWaveT));

				dacDevice->dacWaveActive = FALSE;
				for (chan = dacDevice->dacFirstChannel;
				     chan < dacDevice->dacFirstChannel +
					     dacDevice->dacChannelCount; chan++) {

					apiDacSetAvail(msg->handle, DddtLocal,
						       chan, DacAvailVoltage
						       | DacAvailStaticWave
						       | DacAvailDynamicWave);

					dacDevice->dacVoltage[chan].
						dacHWFormatCapable = TRUE;

					daqDacSetOutputMode
						(msg->handle,
						 DddtLocal, chan, DdomStaticWave);
					daqDacWaveSetTrig(msg->handle,
							  DddtLocal, chan,
							  DdtsImmediate, TRUE);
					daqDacWaveSetClockSource
						(msg->handle,
						 DddtLocal, chan, DdcsDacClock);
					daqDacWaveSetFreq(msg->handle,
							  DddtLocal, chan, 10.0f);
					daqDacWaveSetMode(msg->handle,
							  DddtLocal,
							  chan, DdwmInfinite, 0);
					daqDacWaveSetUserWave
						(msg->handle, DddtLocal, chan);
					daqDacWaveSetBuffer
						(msg->handle, DddtLocal, chan,
						 NULL, 1, DdtmUpdateBlock);

					dacDevice->
						dacWave[chan].dacFileHandle = NULL;

					daqDacSetOutputMode
						(msg->handle,
						 DddtLocal, chan, DdomVoltage);

					daqDacWt(msg->handle,
						 DddtLocal, chan, 32768);
				}
			}

			if ((msg->deviceType < DaqBoard2002)
			    || (msg->deviceType > DaqBoard2004)) {

				dacDevice = (ApiDacDeviceT *)
					iotMalloc(sizeof (ApiDacDeviceT));
				session[msg->handle].dacDevice[DddtDbk] = dacDevice;
				dacDevice->dacChannelCount = 256;
				dacDevice->dacFirstChannel = 16;
				dacDevice->dacNumWaveActive = 0;
				mallocCount = dacDevice->dacChannelCount +
					dacDevice->dacFirstChannel;
				dacDevice->dacAvail = (DWORD *)
					iotMalloc(mallocCount * sizeof (DWORD));
				dacDevice->dacOutputMode = (DaqDacOutputMode *)
					iotMalloc(mallocCount *
						  sizeof (DaqDacOutputMode));
				dacDevice->dacVoltage = (ApiDacVoltageT *)
					iotMalloc(mallocCount *
						  sizeof (ApiDacVoltageT));
				dacDevice->dacWave = NULL;
				dacDevice->dacWaveActive = FALSE;

				for (chan = dacDevice->dacFirstChannel;
				     chan < dacDevice->dacFirstChannel +
					     dacDevice->dacChannelCount; chan++) {

					apiDacSetAvail(msg->handle,
						       DddtDbk, chan, DacAvailNone);

					dacDevice->dacVoltage[chan].
						dacHWFormatCapable = FALSE;
				}
			}

			if (msg->deviceType != DaqBoard2003) {

				dacDevice = (ApiDacDeviceT *)
					iotMalloc(sizeof (ApiDacDeviceT));
				session[msg->handle].dacDevice[DddtLocalDigital]
					= dacDevice;

				dacDevice->dacChannelCount = 1;
				dacDevice->dacFirstChannel = 0;

				dacDevice->dacNumWaveActive = 0;
				mallocCount = dacDevice->dacChannelCount +
					dacDevice->dacFirstChannel;
				dacDevice->dacAvail = (DWORD *)
					iotMalloc(mallocCount * sizeof (DWORD));
				dacDevice->dacOutputMode = (DaqDacOutputMode *)
					iotMalloc(mallocCount *
						  sizeof (DaqDacOutputMode));
				dacDevice->dacVoltage = (ApiDacVoltageT *)
					iotMalloc(mallocCount *
						  sizeof (ApiDacVoltageT));
				dacDevice->dacWave = (ApiDacWaveT *)
					iotMalloc(mallocCount * sizeof (ApiDacWaveT));

				dacDevice->dacWaveActive = FALSE;
				for (chan = dacDevice->dacFirstChannel;
				     chan < dacDevice->dacFirstChannel +
					     dacDevice->dacChannelCount; chan++) {

					apiDacSetAvail(msg->handle,
						       DddtLocalDigital, chan,
						       DacAvailVoltage
						       | DacAvailStaticWave
						       | DacAvailDynamicWave);

					dacDevice->dacVoltage[chan].
						dacHWFormatCapable = TRUE;

					dacDevice->dacOutputMode[chan]
						= DdomStaticWave;

					daqDacWaveSetTrig(msg->handle,
							  DddtLocalDigital,chan,
							  DdtsImmediate, TRUE);
					daqDacWaveSetClockSource
						(msg->handle,
						 DddtLocalDigital, chan, DdcsDacClock);
					daqDacWaveSetFreq(msg->handle,
							  DddtLocalDigital,
							  chan, 10.0f);
					daqDacWaveSetMode(msg->handle,
							  DddtLocalDigital,
							  chan, DdwmInfinite, 0);
					daqDacWaveSetUserWave
						(msg->handle, DddtLocalDigital, chan);
					daqDacWaveSetBuffer
						(msg->handle, DddtLocalDigital,
						 chan, NULL, 1, DdtmUpdateBlock);

					dacDevice->
						dacWave[chan].dacFileHandle = NULL;

					dacDevice->dacOutputMode[chan] = DdomDigitalDirect;
					dacDevice-> dacVoltage[chan].
						dacSelectedFormat = DdomUnsigned;
				}
			}

			break;

		case DaqBook100:
		case DaqBook112:
		case DaqBook120:
		case DaqBook200:
		case DaqBook216:
			dacDevice = (ApiDacDeviceT *)
				iotMalloc(sizeof (ApiDacDeviceT));
			session[msg->handle].dacDevice[DddtLocal] = dacDevice;
			dacDevice->dacChannelCount = 2;
			dacDevice->dacFirstChannel = 0;

			dacDevice->dacNumWaveActive = 0;
			mallocCount =
				dacDevice->dacChannelCount + dacDevice->dacFirstChannel;
			dacDevice->dacAvail =
				(DWORD *) iotMalloc(mallocCount * sizeof (DWORD));
			dacDevice->dacOutputMode = (DaqDacOutputMode *)
				iotMalloc(mallocCount * sizeof (DaqDacOutputMode));
			dacDevice->dacVoltage = (ApiDacVoltageT *)
				iotMalloc(mallocCount * sizeof (ApiDacVoltageT));
			dacDevice->dacWave = NULL;
			dacDevice->dacWaveActive = FALSE;

			for (chan = dacDevice->dacFirstChannel;
			     chan < dacDevice->dacFirstChannel +
				     dacDevice->dacChannelCount; chan++) {

				apiDacSetAvail(msg->handle,
					       DddtLocal, chan, DacAvailVoltage);

				dacDevice->dacVoltage[chan].
					dacHWFormatCapable = FALSE;

				daqDacSetOutputMode(msg->handle,
						    DddtLocal, chan, DdomVoltage);
				daqDacWt(msg->handle, DddtLocal, chan, 0);
			}

			dacDevice = (ApiDacDeviceT *)
				iotMalloc(sizeof (ApiDacDeviceT));
			session[msg->handle].dacDevice[DddtDbk] = dacDevice;
			dacDevice->dacChannelCount = 256;
			dacDevice->dacFirstChannel = 16;
			dacDevice->dacNumWaveActive = 0;
			mallocCount =
				dacDevice->dacChannelCount + dacDevice->dacFirstChannel;
			dacDevice->dacAvail =
				(DWORD *) iotMalloc(mallocCount * sizeof (DWORD));
			dacDevice->dacOutputMode = (DaqDacOutputMode *)
				iotMalloc(mallocCount * sizeof (DaqDacOutputMode));
			dacDevice->dacVoltage = (ApiDacVoltageT *)
				iotMalloc(mallocCount * sizeof (ApiDacVoltageT));
			dacDevice->dacWave = NULL;
			dacDevice->dacWaveActive = FALSE;

			for (chan = dacDevice->dacFirstChannel;
			     chan < dacDevice->dacFirstChannel +
				     dacDevice->dacChannelCount; chan++) {

				apiDacSetAvail(msg->handle,
					       DddtDbk, chan, DacAvailNone);

				dacDevice->dacVoltage[chan].
					dacHWFormatCapable = FALSE;
			}
			break;

		case Daq112:
		case Daq216:
			dacDevice = (ApiDacDeviceT *)
				iotMalloc(sizeof (ApiDacDeviceT));
			session[msg->handle].dacDevice[DddtDbk] = dacDevice;
			dacDevice->dacChannelCount = 256;
			dacDevice->dacFirstChannel = 16;
			dacDevice->dacNumWaveActive = 0;
			mallocCount =
				dacDevice->dacChannelCount + dacDevice->dacFirstChannel;
			dacDevice->dacAvail =
				(DWORD *) iotMalloc(mallocCount * sizeof (DWORD));
			dacDevice->dacOutputMode = (DaqDacOutputMode *)
				iotMalloc(mallocCount * sizeof (DaqDacOutputMode));
			dacDevice->dacVoltage = (ApiDacVoltageT *)
				iotMalloc(mallocCount * sizeof (ApiDacVoltageT));
			dacDevice->dacWave = NULL;
			dacDevice->dacWaveActive = FALSE;

			for (chan = dacDevice->dacFirstChannel;
			     chan < dacDevice->dacFirstChannel +
				     dacDevice->dacChannelCount; chan++) {

				apiDacSetAvail(msg->handle,
					       DddtDbk, chan, DacAvailNone);
				dacDevice->dacVoltage[chan].
					dacHWFormatCapable = FALSE;
			}
			break;

		default:
			break;
		}
	}
	break;

	case MsgClose:
		dacDeallocateAndDisarm(msg->handle);
		break;
	}

	return;
}


static VOID
dacDeallocateAndDisarm(DaqHandleT handle)
{

	DWORD deviceType;
	ApiDacDeviceT *dacDevice;

	for (deviceType = DddtLocal; deviceType < MaxDacDeviceTypes; deviceType++) {

		dacDevice = session[handle].dacDevice[deviceType];

		if (dacDevice) {
			daqDacWaveDisarm(handle, (DaqDacDeviceType) deviceType);

			if (dacDevice->dacAvail) {
				iotFarfree(dacDevice->dacAvail);
				dacDevice->dacAvail = NULL;
			}

			if (dacDevice->dacOutputMode) {
				iotFarfree(dacDevice->dacOutputMode);
				dacDevice->dacOutputMode = NULL;
			}

			if (dacDevice->dacVoltage) {
				iotFarfree(dacDevice->dacVoltage);
				dacDevice->dacVoltage = NULL;
			}

			if (dacDevice->dacWave) {
				iotFarfree(dacDevice->dacWave);
				dacDevice->dacWave = NULL;
			}

			iotFarfree(dacDevice);
			session[handle].dacDevice[deviceType] = NULL;
		}
	}

}

static DaqError
dacTestDeviceType(DaqHandleT handle, DaqDacDeviceType deviceType)
{

	DaqError err;
	ApiDacDeviceT *dacDevice;

	err = apiCtrlTestHandle(handle, DlmAll);
	if (err != DerrNoError) {
		return err;
	}

	if ((deviceType < DddtLocal) || (deviceType > (MaxDacDeviceTypes - 1))) {

		return DerrInvType;
	}

	dacDevice = session[handle].dacDevice[deviceType];

	if (!dacDevice) {
		return DerrNotCapable;
	}

	return DerrNoError;

}

static DaqError
dacTestChannel(DaqHandleT handle, DaqDacDeviceType deviceType, DWORD chan, DWORD avail)
{

	DaqError err;
	ApiDacDeviceT *dacDevice;

	err = dacTestDeviceType(handle, deviceType);
	if (err != DerrNoError) {
		return err;
	}

	dacDevice = session[handle].dacDevice[deviceType];

	if (chan < dacDevice->dacFirstChannel) {
		return DerrInvChan;
	}

	if (chan >= dacDevice->dacFirstChannel + dacDevice->dacChannelCount) {
		return DerrInvChan;
	}

	if (avail) {
		DacAvailT outputModeAvail[] = { DacAvailVoltage,
			DacAvailStaticWave,
			DacAvailDynamicWave
		};

		if (!(avail & outputModeAvail[dacDevice->dacOutputMode[chan]])) {
			return DerrInvMode;
		}
	}

	return DerrNoError;

}

VOID
apiDacSetAvail(DaqHandleT handle, DaqDacDeviceType deviceType, DWORD chan, DWORD avail)
{

	if (dacTestChannel(handle, deviceType, chan, DacAvailNone) == DerrNoError) {
		session[handle].dacDevice[deviceType]->dacAvail[chan] = avail;
	}

}

static VOID
dacBufferFillPredefWave(PWORD buf, DWORD samples,
			DaqDacWaveType waveType, DWORD amplitude, DWORD offset,
			DWORD dutyCycle, DWORD phaseShift)
{
	DWORD i;
	DWORD shiftPoint, midCyclePoint, secondHalfSize, interval;
	LONG minLevel, maxLevel, actualLevel, clippedLevel;
	double halfAmplitude;
//	double pi = 3.141592653589793238462643383279502884197;

	phaseShift %= 360;

	shiftPoint = phaseShift * samples / 360;
	midCyclePoint = dutyCycle * samples / 100;
	secondHalfSize = samples - midCyclePoint;

	switch (waveType) {
	case DdwtSine:
		halfAmplitude = amplitude / 2;

		for (i = shiftPoint; i < shiftPoint + midCyclePoint; i++) {

			if (clippedLevel < 0) {
				clippedLevel = 0;
			}
			if (clippedLevel > 0xFFFF) {
				clippedLevel = 0xFFFF;
			}
			buf[i % samples] = (WORD) clippedLevel;
		}

		for (i = shiftPoint + midCyclePoint; i < shiftPoint + samples; i++) {

			if (clippedLevel < 0) {
				clippedLevel = 0;
			}
			if (clippedLevel > 0xFFFF) {
				clippedLevel = 0xFFFF;
			}
			buf[i % samples] = (WORD) clippedLevel;
		}

		break;

	case DdwtSquare:
		minLevel = (LONG) offset - (LONG) (amplitude / 2);
		if (minLevel < 0) {
			minLevel = 0;
		}
		maxLevel = (LONG) offset + (LONG) (amplitude / 2);
		if (maxLevel > 0xFFFF) {
			maxLevel = 0xFFFF;
		}

		for (i = shiftPoint; i < shiftPoint + midCyclePoint; i++) {
			buf[i % samples] = (WORD) maxLevel;

		}

		for (i = shiftPoint + midCyclePoint; i < shiftPoint + samples; i++) {
			buf[i % samples] = (WORD) minLevel;
		}

		break;

	case DdwtTriangle:
		minLevel = (LONG) offset - (amplitude / 2);
		actualLevel = minLevel;

		interval = amplitude / midCyclePoint;
		for (i = shiftPoint; i < shiftPoint + midCyclePoint; i++) {
			actualLevel += interval;
			clippedLevel = actualLevel;
			if (clippedLevel < 0) {
				clippedLevel = 0;
			}
			if (clippedLevel > 0xFFFF) {
				clippedLevel = 0xFFFF;
			}
			buf[i % samples] = (WORD) clippedLevel;
		}

		interval = amplitude / secondHalfSize;
		for (i = shiftPoint + midCyclePoint; i < shiftPoint + samples; i++) {
			actualLevel -= interval;
			clippedLevel = actualLevel;
			if (clippedLevel < 0) {
				clippedLevel = 0;
			}
			if (clippedLevel > 0xFFFF) {
				clippedLevel = 0xFFFF;
			}
			buf[i % samples] = (WORD) clippedLevel;
		}

		break;
	}
}

DAQAPI DaqError WINAPI
daqDacSetOutputMode(DaqHandleT handle, DaqDacDeviceType deviceType, DWORD chan,
		    DaqDacOutputMode outputMode)
{

	DaqError err;
	daqIOT sb;
	DAC_MODE_PT dacMode = (DAC_MODE_PT) & sb;
	ApiDacDeviceT *dacDevice;
	DacAvailT outputModeAvail[] = {
		DacAvailVoltage,
		DacAvailStaticWave,
		DacAvailDynamicWave
	};

	DaqDacOutputMode DdomFormat;

	DdomFormat = (DaqDacOutputMode) ((DWORD) outputMode & (DWORD) DdomSigned);
	outputMode = (DaqDacOutputMode) ((DWORD) outputMode & (DWORD) (~DdomSigned));

	err = dacTestChannel(handle, deviceType, chan, DacAvailNone);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	if ((outputMode < DdomVoltage) || (outputMode > DdomDynamicWave)) {
		return apiCtrlProcessError(handle, DerrInvMode);
	}

	dacDevice = session[handle].dacDevice[deviceType];

	if (!(outputModeAvail[outputMode] & dacDevice->dacAvail[chan])) {
		return apiCtrlProcessError(handle, DerrInvMode);
	}

	dacDevice->dacOutputMode[chan] = outputMode;

	if (deviceType == DddtDbk) {

		if (outputMode != DdomVoltage) {
			return apiCtrlProcessError(handle, DerrInvMode);
		} else {

			return DerrNoError;
		}
	}

	dacDevice->dacVoltage[chan].dacSelectedFormat = DdomFormat;
	if (dacDevice->dacVoltage[chan].dacHWFormatCapable == TRUE) {
		outputMode = (DaqDacOutputMode) ((DWORD) outputMode | (DWORD) (DdomFormat));
	}

	dacMode->dacDevType = deviceType;
	dacMode->dacChannel = chan;
	dacMode->dacOutputMode = outputMode;

	err = itfExecuteCommand(handle, &sb, DAC_MODE);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);

	}

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqDacWtMany(DaqHandleT handle, DaqDacDeviceType * deviceTypes, PDWORD chans,
	     PWORD dataVals, DWORD count)
{
	DaqError err;
	daqIOT sb;
	DWORD i;
	DAC_WRITE_MANY_PT dacWriteMany = (DAC_WRITE_MANY_PT) & sb;
	BOOL doItfWriteMany;

	if (count == 0) {
		return apiCtrlProcessError(handle, DerrInvCount);
	}

	for (i = 0; i < count; i++) {

		err = dacTestChannel(handle, deviceTypes[i], chans[i], DacAvailVoltage);
		if (err != DerrNoError) {
			return apiCtrlProcessError(handle, err);
		}
	}

	doItfWriteMany = FALSE;
	for (i = 0; i < count; i++) {

		if (deviceTypes[i] == DddtDbk) {
			err = apiDbkDacWt(handle, chans[i], &dataVals[i], 1);
			if (err != DerrNoError) {
				return apiCtrlProcessError(handle, err);
			}
		} else {
			doItfWriteMany = TRUE;
		}
	}

	if (doItfWriteMany) {

		dacWriteMany->dacDevTypes = (PDWORD) deviceTypes;
		dacWriteMany->dacChannels = chans;
		dacWriteMany->dacValues = dataVals;
		dacWriteMany->dacNChannels = count;

		err = itfExecuteCommand(handle, &sb, DAC_WRITE_MANY);
		if (err != DerrNoError) {
			return apiCtrlProcessError(handle, err);
		}
	}

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqDacWt(DaqHandleT handle, DaqDacDeviceType deviceType, DWORD chan, WORD dataVal)
{

	return daqDacWtMany(handle, &deviceType, &chan, &dataVal, 1);

}

DAQAPI DaqError WINAPI
daqDacWaveSetTrig(DaqHandleT handle, DaqDacDeviceType deviceType, DWORD chan,
		  DaqDacTriggerSource triggerSource, BOOL rising)
{

	DaqError err;
	ApiDacDeviceT *dacDevice;
	ApiDacWaveT *dacWave;

	err = dacTestChannel(handle, deviceType, chan, DacAvailStaticWave | DacAvailDynamicWave);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	if ((IsDaq2000(session[handle].deviceType)) || (IsDaq1000(session[handle].deviceType))) {
		if ((triggerSource < DdtsImmediate)
		    || (triggerSource > DdtsAdcClock)) {
			return apiCtrlProcessError(handle, DerrInvTrigSource);
		}
	} else {
		if ((triggerSource < DdtsImmediate)
		    || (triggerSource > DdtsSoftware)) {
			return apiCtrlProcessError(handle, DerrInvTrigSource);
		}
	}

	dacDevice = session[handle].dacDevice[deviceType];

	if (dacDevice->dacWaveActive) {
		return apiCtrlProcessError(handle, DerrDacWaveformActive);
	}

	dacWave = &dacDevice->dacWave[chan];

	dacWave->dacConfig.dacTrigSource = triggerSource;
	if (rising) {
		dacWave->dacConfig.dacTrigSource |= RisingEdgeFlag;
	}

	dacWave->dacConfigChanged = TRUE;

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqDacWaveSoftTrig(DaqHandleT handle, DaqDacDeviceType deviceType, DWORD chan)
{

	DaqError err;
	daqIOT sb;
	DAC_SOFTTRIG_PT dacSoftTrig = (DAC_SOFTTRIG_PT) & sb;

	err = dacTestChannel(handle, deviceType, chan, DacAvailStaticWave | DacAvailDynamicWave);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	dacSoftTrig->dacDevType = deviceType;
	dacSoftTrig->dacChannel = chan;

	err = itfExecuteCommand(handle, &sb, DAC_SOFTTRIG);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqDacWaveSetClockSource(DaqHandleT handle, DaqDacDeviceType deviceType,
			 DWORD chan, DaqDacClockSource clockSource)
{

	DaqError err;
	ApiDacDeviceT *dacDevice;
	ApiDacWaveT *dacWave;

	err = dacTestChannel(handle, deviceType, chan, DacAvailStaticWave | DacAvailDynamicWave);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	if ((IsDaq2000(session[handle].deviceType)) || (IsDaq1000(session[handle].deviceType))) {
		DWORD flagMask = DdcsOutputEnable | DdcsFallingEdge;
		if (((clockSource & ~flagMask) == DdcsGatedDacClock)
		    || ((clockSource & ~flagMask) == Ddcs9513Ctr1)) {
			return apiCtrlProcessError(handle, DerrInvClockSource);
		}
	} else {
		if ((clockSource < DdcsDacClock)
		    || (clockSource > Ddcs9513Ctr1)) {
			return apiCtrlProcessError(handle, DerrInvClockSource);
		}
	}

	dacDevice = session[handle].dacDevice[deviceType];

	if (dacDevice->dacWaveActive) {
		return apiCtrlProcessError(handle, DerrDacWaveformActive);
	}

	dacWave = &dacDevice->dacWave[chan];

	dacWave->dacConfig.dacClockSource = clockSource;

	dacWave->dacConfigChanged = TRUE;

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqDacWaveSetFreq(DaqHandleT handle, DaqDacDeviceType deviceType, DWORD chan, float freq)
{

	DaqError err;
	ApiDacDeviceT *dacDevice;
	ApiDacWaveT *dacWave;
	double period;
	DWORD nsec, msec;

	err = dacTestChannel(handle, deviceType, chan, DacAvailStaticWave | DacAvailDynamicWave);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	if ((freq <= 0) || (freq > 1000000000.0f)) {
		return apiCtrlProcessError(handle, DerrInvFreq);
	}

	dacDevice = session[handle].dacDevice[deviceType];

	if (dacDevice->dacWaveActive) {
		return apiCtrlProcessError(handle, DerrDacWaveformActive);
	}

	dacWave = &dacDevice->dacWave[chan];

	period = 1.0 / freq;

	period += 0.0000000005;

	period *= 1000.0;

	msec = (DWORD) period;
	period -= (double) msec;

	period *= 1000000.0;

	nsec = (DWORD) period;

	dacWave->dacConfig.dacPeriodnsec = nsec;
	dacWave->dacConfig.dacPeriodmsec = msec;

	dacWave->dacConfigChanged = TRUE;

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqDacWaveGetFreq(DaqHandleT handle, DaqDacDeviceType deviceType, DWORD chan, float * freq)
{

	DaqError err;
	ApiDacDeviceT *dacDevice;
	ApiDacWaveT *dacWave;

	err = dacTestChannel(handle, deviceType, chan, DacAvailStaticWave | DacAvailDynamicWave);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	dacDevice = session[handle].dacDevice[deviceType];

	dacWave = &dacDevice->dacWave[chan];

	*freq = 1.0f / (dacWave->dacConfig.dacPeriodmsec / 1000.0f +
			dacWave->dacConfig.dacPeriodnsec / 1000000000.0f);

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqDacWaveSetMode(DaqHandleT handle, DaqDacDeviceType deviceType, DWORD chan,
		  DaqDacWaveformMode mode, DWORD updateCount)
{

	DaqError err;
	ApiDacDeviceT *dacDevice;
	ApiDacWaveT *dacWave;

	err = dacTestChannel(handle, deviceType, chan, DacAvailStaticWave | DacAvailDynamicWave);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	if ((mode < DdwmNShot) || (mode > DdwmNFileIterations)
	    || (mode == DdwmNShotRearm)) {
		return apiCtrlProcessError(handle, DerrInvMode);
	}

	if ((mode != DdwmInfinite) && (updateCount == 0)) {
		return apiCtrlProcessError(handle, DerrInvCount);
	}

	dacDevice = session[handle].dacDevice[deviceType];

	if (dacDevice->dacWaveActive) {
		return apiCtrlProcessError(handle, DerrDacWaveformActive);
	}

	dacWave = &dacDevice->dacWave[chan];

	dacWave->dacConfig.dacUpdateMode = mode;
	dacWave->dacConfig.dacUpdateCount = updateCount;

	dacWave->dacConfigChanged = TRUE;

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqDacWaveSetDiskFile(DaqHandleT handle,
		      DaqDacDeviceType deviceType, DWORD chan, PCHAR filename,
		      DWORD numUpdateCycles,
		      DWORD offsetBytes,
		      DWORD offsetUpdateCycles, DaqDacWaveFileDataFormat dataFormat)
{

	DaqError err;
	ApiDacDeviceT *dacDevice;
	ApiDacWaveT *dacWave;

    //MAH: 04.05.04 added 1000
    if (!(IsDaq2000(session[handle].deviceType))||(IsDaq1000(session[handle].deviceType) ))
    {
		return apiCtrlProcessError(handle, DerrNotCapable);
	}

	err = dacTestChannel(handle, deviceType, chan, DacAvailStaticWave | DacAvailDynamicWave);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	dacDevice = session[handle].dacDevice[deviceType];

	if (dacDevice->dacWaveActive) {
		return apiCtrlProcessError(handle, DerrDacWaveformActive);
	}

	dacWave = &dacDevice->dacWave[chan];

	strcpy(dacWave->dacFileName, filename);

	dacWave->dacBufferSource = DdbsDiskFile;

	dacWave->dacFileOffsetBytes = offsetBytes;

	dacWave->dacFileOffsetUpdateCycle = offsetUpdateCycles;
	dacWave->dacFileTotalUpdates = numUpdateCycles;
	dacWave->dacFileDataFormat = dataFormat;

	dacWave->dacBufferChanged = TRUE;

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqDacWaveSetPredefWave(DaqHandleT handle, DaqDacDeviceType deviceType,
			DWORD chan, DaqDacWaveType waveType, DWORD amplitude,
			DWORD offset, DWORD dutyCycle, DWORD phaseShift)
{

	DaqError err;
	ApiDacDeviceT *dacDevice;
	ApiDacWaveT *dacWave;

	err = dacTestChannel(handle, deviceType, chan, DacAvailStaticWave | DacAvailDynamicWave);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	if ((dutyCycle >= 100) || (dutyCycle == 0)) {
		return apiCtrlProcessError(handle, DerrInvDacParam);
	}

	if (waveType > DdwtTriangle) {
		return apiCtrlProcessError(handle, DerrInvPredWave);
	}

	dacDevice = session[handle].dacDevice[deviceType];

	if (dacDevice->dacWaveActive) {
		return apiCtrlProcessError(handle, DerrDacWaveformActive);
	}

	dacWave = &dacDevice->dacWave[chan];

	dacWave->dacPreDefWaveType = waveType;
	dacWave->dacPreDefAmplitude = amplitude;
	dacWave->dacPreDefOffset = offset;
	dacWave->dacPreDefDutyCycle = dutyCycle;
	dacWave->dacPreDefPhaseShift = phaseShift;

	dacWave->dacBufferSource = DdbsPreDefWave;

	dacWave->dacBufferChanged = TRUE;

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqDacWaveSetUserWave(DaqHandleT handle, DaqDacDeviceType deviceType, DWORD chan)
{

	DaqError err;
	ApiDacDeviceT *dacDevice;
	ApiDacWaveT *dacWave;

	err = dacTestChannel(handle, deviceType, chan, DacAvailStaticWave | DacAvailDynamicWave);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	dacDevice = session[handle].dacDevice[deviceType];

	if (dacDevice->dacWaveActive) {
		return apiCtrlProcessError(handle, DerrDacWaveformActive);
	}

	dacWave = &dacDevice->dacWave[chan];

	dacWave->dacBufferSource = DdbsUserWave;

	dacWave->dacBufferChanged = TRUE;

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqDacWaveSetBuffer(DaqHandleT handle, DaqDacDeviceType deviceType, DWORD chan,
		    PWORD buf, DWORD bufLen, DWORD transferMask)
{

	DaqError err;
	ApiDacDeviceT *dacDevice;
	ApiDacWaveT *dacWave;

	err = dacTestChannel(handle, deviceType, chan, DacAvailStaticWave | DacAvailDynamicWave);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	if (bufLen == 0) {
		return apiCtrlProcessError(handle, DerrInvCount);
	}

	dacDevice = session[handle].dacDevice[deviceType];

	dacWave = &dacDevice->dacWave[chan];

	if (dacDevice->dacWaveActive) {
		return apiCtrlProcessError(handle, DerrDacWaveformActive);
	}

	dacWave->dacStart.dacXferBufLen = bufLen;
	dacWave->dacStart.dacXferBuf = buf;
	dacWave->dacStart.dacXferEvent = 0;

	if (transferMask & DdtmDriverBuffer) {
		if (!(IsDaq2000(session[handle].deviceType))||(IsDaq1000(session[handle].deviceType) )) {
			return apiCtrlProcessError(handle, DerrNotCapable);
		}
		dacWave->dacStart.dacXferBuf = NULL;
		transferMask = DdtmDriverBuffer;
	} else {
		transferMask = DdtmUserBuffer;
	}

	dacWave->dacStart.dacXferMode = transferMask;

	dacWave->dacBufferChanged = TRUE;

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqDacWaveArm(DaqHandleT handle, DaqDacDeviceType deviceType)
{
	DaqError err;
	ApiDacDeviceT *dacDevice;
	ApiDacWaveT *dacWave;
	ApiDacDeviceT *trueDacDevice;
	ApiDacDeviceT *digDevice;
	DWORD dwMakeDigPass = 0;
//	DWORD dwActiveChanCount = 0;
	DWORD sampleSize, fileUpdateRequest;

	daqIOT sb, sbII;
	DAC_ARM_PT dacArm = (DAC_ARM_PT) & sb;
//	DWORD outputChannelCount = 0;
	DWORD chan;
	BOOL bDone = FALSE;
	BOOL driverBufferAllocated = FALSE;
	BOOL outputFromFile = FALSE;
	DWORD totalActiveChannelCount = 0;
	FILE **outputFilePtr = NULL;
	ApiDacWaveT *dacWaveCopyForFile = 0;

	err = dacTestDeviceType(handle, deviceType);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	if (deviceType == DddtDbk) {
		return apiCtrlProcessError(handle, DerrInvType);
	}

	if (session[handle].dacDevice[DddtLocal] == NULL) {
		if (session[handle].dacDevice[DddtLocalDigital] != NULL) {
			dacDevice = session[handle].dacDevice[DddtLocalDigital];
			trueDacDevice = dacDevice;
		}
	} else {
		dacDevice = session[handle].dacDevice[DddtLocal];
		trueDacDevice = dacDevice;
		if (session[handle].dacDevice[DddtLocalDigital] != NULL) {
			dwMakeDigPass = 1;
			digDevice = session[handle].dacDevice[DddtLocalDigital];
			deviceType = DddtLocal;
			digDevice->dacWaveActive = TRUE;
		}
	}

	if (dacDevice->dacWaveActive) {
		return apiCtrlProcessError(handle, DerrDacWaveformActive);
	}

	dacDevice->dacWaveActive = TRUE;
	dacDevice->dacWaveFromFileActive = FALSE;
	dacDevice->dacNumWaveActive = 0;

	for (chan = dacDevice->dacFirstChannel;
	     chan < dacDevice->dacFirstChannel + dacDevice->dacChannelCount; chan++) {
		if (dacDevice->dacOutputMode[chan] != DdomVoltage) {
			totalActiveChannelCount++;
		}
	}

	if (dwMakeDigPass == 1) {
		if (digDevice->dacOutputMode[0] != DdomVoltage) {
			totalActiveChannelCount++;
		}
		digDevice->dacNumWaveActive = totalActiveChannelCount;
	}
	trueDacDevice->dacNumWaveActive = totalActiveChannelCount;

	for (chan = dacDevice->dacFirstChannel;
	     chan < dacDevice->dacFirstChannel + dacDevice->dacChannelCount + dwMakeDigPass; chan++) {

		if (chan >= dacDevice->dacFirstChannel + dacDevice->dacChannelCount) {
			dacDevice = digDevice;
			deviceType = DddtLocalDigital;
			chan = 0;
			bDone = TRUE;
		}

		if ((dacDevice->dacOutputMode[chan] == DdomStaticWave)
            //MAH: 04.05.04     
		    || (((IsDaq2000(session[handle].deviceType))|| (IsDaq2000(session[handle].deviceType)))
            && (dacDevice->dacOutputMode[chan] == DdomDynamicWave))) {

			dacWave = &dacDevice->dacWave[chan];

			switch (dacDevice->dacOutputMode[chan]) {
			case DdomStaticWave:
				if ((dacWave->dacBufferSource == DdbsDiskFile)
				    || (dacWave->dacStart.dacXferMode & DdtmDriverBuffer)) {
					return apiCtrlProcessError(handle, DerrDacWaveModeConflict);
				}
				break;
			case DdomDynamicWave:

				if ((dacWave->dacBufferSource == DdbsDiskFile)
				    && (dacWave->dacStart.dacXferMode & DdtmDriverBuffer)) {
				} else if ((dacWave->dacBufferSource == DdbsUserWave)
					   && !(dacWave->dacStart.dacXferMode & DdtmDriverBuffer)) {
				} else {
					return apiCtrlProcessError(handle, DerrDacWaveModeConflict);
				}

				if ((dacWave->dacConfig.dacUpdateMode == DdwmNFileIterations)
				    && (dacWave->dacBufferSource != DdbsDiskFile)) {

					return apiCtrlProcessError(handle, DerrDacWaveModeConflict);
				}

				break;
			default:
				break;
			}

			if (dacWave->dacBufferChanged) {
				DAC_START_PT dacStart = (DAC_START_PT) & sb;

				if (dacWave->dacStart.dacXferMode & DdtmDriverBuffer) {

					if (!driverBufferAllocated) {

						if (session[handle].dacXferDrvrBuf) {
							free(session[handle].dacXferDrvrBuf);
							session[handle].dacXferDrvrBuf = NULL;
						}

						session[handle].dacXferDrvrBuf = (PWORD)
						    malloc (totalActiveChannelCount *
						     2 * dacWave->dacStart.dacXferBufLen);
						if (session[handle].dacXferDrvrBuf == NULL) {
							return apiCtrlProcessError
							    (handle, DerrMemAlloc);

						} else {
							driverBufferAllocated = TRUE;
						}
					}

					dacWave->dacStart.dacXferBuf =
					    session[handle].dacXferDrvrBuf;
				}

				dacStart->dacDevType = deviceType;
				dacStart->dacChannel = chan;

				if ((dacWave->dacStart.dacXferMode & DdtmDriverBuffer)
				    && (dacDevice->dacOutputMode[chan] == DdomDynamicWave)) {
					dacStart->dacXferBufLen =
					    dacWave->dacStart.dacXferBufLen *
					    totalActiveChannelCount;
				} else {

					dacStart->dacXferBufLen = dacWave->dacStart.dacXferBufLen;
				}

				dacStart->dacXferMode = dacWave->dacStart.dacXferMode;
				dacStart->dacXferBuf = dacWave->dacStart.dacXferBuf;
				dacStart->dacXferEvent = dacWave->dacStart.dacXferEvent;
				dacWave->dacStart.dacXferBufHead = 0;

				switch (dacWave->dacBufferSource) {
				case DdbsUserWave:
					break;
				case DdbsPreDefWave:
					dacBufferFillPredefWave((WORD *)
								dacStart->dacXferBuf,
								dacStart->dacXferBufLen,
								dacWave->dacPreDefWaveType,
								dacWave->dacPreDefAmplitude,
								dacWave->dacPreDefOffset,
								dacWave->dacPreDefDutyCycle,
								dacWave->dacPreDefPhaseShift);
					break;

				case DdbsDiskFile:
//#ifndef DLINUX
//#if 0

					if (dacStart->dacXferBuf == NULL) {
						return apiCtrlProcessError(handle, DerrMemAlloc);
					}

					if (!outputFromFile) {
						sampleSize = sizeof (WORD);

						dacWave->dacFileCurrentScan = 0;

						if (dacWave->dacFileHandle != NULL) {
							fclose(dacWave->dacFileHandle);
							dacWave->dacFileHandle = NULL;
						}

						err =
						    openAndConvertFile(handle,
								       dacWave, outputFilePtr);
						if (err) {
							return apiCtrlProcessError(handle, err);
						}

						if (dacWave->dacFileDataFormat == DdwdfBinaryCounts) {

							if (dacWave->
							    dacFileOffsetBytes
							    >=
							    (totalActiveChannelCount
							     * sampleSize *
							     dacWave->dacFileUpdatesInFile)) {
								return
								    apiCtrlProcessError
								    (handle,
								     DerrDacWaveFileTooSmall);
							}

							if (((totalActiveChannelCount * sampleSize *
							      dacWave->dacFileUpdatesInFile) -
							     dacWave->dacFileOffsetBytes) <
							    (totalActiveChannelCount * sampleSize *
							     dacWave->dacFileOffsetUpdateCycle)) {
								return apiCtrlProcessError(handle,
											   DerrDacWaveFileTooSmall);
							}

							dacWave->dacFileStartPosition = 
							    (dacWave->dacFileOffsetBytes +
							     (totalActiveChannelCount
							      * sampleSize *
							      dacWave->dacFileOffsetUpdateCycle));
						} else {
							dacWave->dacFileStartPosition = 0;
						}

						dacWave->dacFileUpdatesInFile -=
						    (DWORD) ceil((double)
								 dacWave->
								 dacFileStartPosition
								 /(totalActiveChannelCount
								  * sampleSize));

						if (dacWave->dacFileTotalUpdates >
						    dacWave->dacFileUpdatesInFile) {
							return
							    apiCtrlProcessError
							    (handle, DerrDacWaveFileTooSmall);
						}

						if (dacWave->dacFileTotalUpdates == 0) {
							fileUpdateRequest = 0xffffffff;
						} else {
							fileUpdateRequest =
							    dacWave->dacFileTotalUpdates;
						}

						dacWave->dacFileUpdatesInFile =
						    min(dacWave->
							dacFileUpdatesInFile, fileUpdateRequest);

						err =
						    dacXferFileToBuffer(handle,
									deviceType,
									chan,
									dacWave->
									dacStart.dacXferBufLen);
						if (err != DerrNoError) {
							return apiCtrlProcessError(handle, err);
						}

						outputFromFile = TRUE;
						dacDevice->dacWaveFromFileActive = TRUE;

						dacWaveCopyForFile = dacWave;

					} else {

						dacWave->dacFileStartPosition =
						    dacWaveCopyForFile->dacFileStartPosition;
						dacWave->dacFileCurrentScan =
						    dacWaveCopyForFile->dacFileCurrentScan;
						dacWave->dacFileUpdatesInFile =
						    dacWaveCopyForFile->dacFileUpdatesInFile;
						dacWave->dacFileHandle =
						    dacWaveCopyForFile->dacFileHandle;
					}
//#endif // #if 0
//#else
					return apiCtrlProcessError(handle, DerrNotImplemented);
//#endif
					break;
				default:
					break;
				}

				if (dacWave->dacConfigChanged) {

					DAC_CONFIG_PT dacConfig = (DAC_CONFIG_PT) & sbII;

					dacConfig->dacDevType = DddtLocal;
					dacConfig->dacChannel = chan;
					dacConfig->dacPeriodmsec = dacWave->dacConfig.dacPeriodmsec;
					dacConfig->dacPeriodnsec = dacWave->dacConfig.dacPeriodnsec;

					dacConfig->dacUpdateMode = dacWave->dacConfig.dacUpdateMode;
					dacConfig->dacUpdateCount =
					    dacWave->dacConfig.dacUpdateCount;

					if (dacWave->dacConfig.dacUpdateMode == DdwmNFileIterations) {
						dacConfig->dacUpdateMode = DdwmNShot;
						dacConfig->dacUpdateCount *=
						    dacWave->dacFileUpdatesInFile;
					}

					dacConfig->dacTrigSource = dacWave->dacConfig.dacTrigSource;
					dacConfig->dacClockSource =
					    dacWave->dacConfig.dacClockSource;

					err = itfExecuteCommand(handle, &sbII, DAC_CONFIG);
					if (err != DerrNoError) {
						return apiCtrlProcessError(handle, err);
					}

					dacWave->dacConfig.dacPeriodmsec = dacConfig->dacPeriodmsec;
					dacWave->dacConfig.dacPeriodnsec = dacConfig->dacPeriodnsec;

					dacWave->dacConfigChanged = FALSE;
				}

				err = itfExecuteCommand(handle, &sb, DAC_START);
				if (err != DerrNoError) {
					return apiCtrlProcessError(handle, err);
				}

				dacWave->dacBufferChanged = FALSE;

			}

		}

		if ((chan == 0) && (bDone == TRUE)) {
			break;
		}

	}

	dacDevice->dacOldReadCount = 0;
	trueDacDevice->dacOldReadCount = 0;

	dacArm->dacDevType = DddtLocal;

	err = itfExecuteCommand(handle, &sb, DAC_ARM);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqDacWaveDisarm(DaqHandleT handle, DaqDacDeviceType deviceType)
{

	DaqError err;
	ApiDacDeviceT *dacDevice;

	err = dacTestDeviceType(handle, deviceType);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	dacDevice = session[handle].dacDevice[deviceType];

//	dbg("wave active = %d", dacDevice->dacWaveActive);

	if (dacDevice->dacWaveActive) {
		daqIOT sb;
		DAC_DISARM_PT dacDisarm = (DAC_DISARM_PT) & sb;

		dacDisarm->dacDevType = deviceType;

		err = itfExecuteCommand(handle, &sb, DAC_DISARM);

		if ((IsDaq2000(session[handle].deviceType)) || (IsDaq1000(session[handle].deviceType))) {

			daqDacTransferStop(handle, deviceType, 0);

			if (session[handle].dacDevice[DddtLocalDigital] != NULL) {
				dacDevice = session[handle].dacDevice[DddtLocalDigital];
				dacDevice->dacWaveActive = FALSE;
			}

			if (session[handle].dacDevice[DddtLocal] != NULL) {
				dacDevice = session[handle].dacDevice[DddtLocal];
				dacDevice->dacWaveActive = FALSE;
			}
		}

		dacDevice->dacWaveActive = FALSE;

		if (err != DerrNoError) {
			return apiCtrlProcessError(handle, err);
		}
	}

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqDacTransferStart(DaqHandleT handle, DaqDacDeviceType deviceType, DWORD chan)
{

	DaqError err;

	err = dacTestChannel(handle, deviceType, chan, DacAvailDynamicWave);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqDacTransferGetStat(DaqHandleT handle, DaqDacDeviceType deviceType,
		      DWORD chan, PDWORD pActive, PDWORD pRetCount)
{

	DaqError err;
	daqIOT sb;
	DAC_STATUS_PT dacStat = (DAC_STATUS_PT) & sb;
	ApiDacDeviceT *dacDevice;
	ApiDacWaveT *dacWave;
	DWORD transferCount;
	DWORD updateCount = 0;

	DaqDacDeviceType fileDeviceType;
	DWORD fileChan;

	err = dacTestChannel(handle, deviceType, chan, DacAvailNone);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	dacDevice = session[handle].dacDevice[deviceType];

	if (dacDevice->dacNumWaveActive == 0) {

		if (NULL != pActive) {
			*pActive = (DWORD) 0;
		}
		if (NULL != pRetCount) {
			*pRetCount = (DWORD) 0;
		}
		return DerrNoError;
	}

	dacWave = &session[handle].dacDevice[deviceType]->dacWave[chan];

	dacStat->dacDevType = deviceType;
	dacStat->dacChannel = chan;

	err = itfExecuteCommand(handle, &sb, DAC_STATUS);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	if (session[handle].dacXferDrvrBuf && ((DWORD) dacStat->dacStatus & DdafWaveformActive)) {

		if (dacFindFirstFileChan(handle, &fileDeviceType, &fileChan)) {

			deviceType = fileDeviceType;
			chan = fileChan;

			dacDevice = session[handle].dacDevice[deviceType];
			dacWave = &session[handle].dacDevice[deviceType]->dacWave[chan];
		} else {
		}

		if (dacStat->dacReadCount != dacDevice->dacOldReadCount) {

			if (dacStat->dacReadCount > dacDevice->dacOldReadCount) {
				updateCount =
				    (DWORD) ((dacStat->dacReadCount -
					      dacDevice->dacOldReadCount) /
					     dacDevice->dacNumWaveActive);
			} else {

				updateCount =
				    (DWORD) ((dacStat->dacReadCount +
					      (0x0FFFFFFFL -
					       (0x0FFFFFFFL & dacDevice->
						dacOldReadCount))) / dacDevice->dacNumWaveActive);

			}

			err = dacXferFileToBuffer(handle, deviceType, chan, updateCount);
			if (err != DerrNoError) {
				return apiCtrlProcessError(handle, err);
			}

			dacDevice->dacOldReadCount = dacStat->dacReadCount;
		}
	}

	if (dacDevice->dacWaveActive && !((DWORD) dacStat->dacStatus & DdafWaveformActive)) {
		daqDacWaveDisarm(handle, deviceType);
	}

	if ((dacWave->dacStart.dacXferMode == DdtmUserBuffer)
	    && (dacDevice->dacOutputMode[chan] == DdomDynamicWave)) {
		transferCount = dacStat->dacReadCount;
	} else {
		transferCount = dacStat->dacCurrentCount;
	}

	if (NULL != pActive) {
		*pActive = (DWORD) dacStat->dacStatus;
	}
	if (NULL != pRetCount) {
		*pRetCount = (DWORD) transferCount / dacDevice->dacNumWaveActive;
	}

	return DerrNoError;

}

DAQAPI DaqError WINAPI
daqDacTransferStop(DaqHandleT handle, DaqDacDeviceType deviceType, DWORD chan)
{

	DaqError err;
	daqIOT sb;
	DAC_STOP_PT dacStop = (DAC_STOP_PT) & sb;

	ApiDacDeviceT *dacDevice;
	ApiDacWaveT *dacWave;

	DaqDacDeviceType fileDeviceType;
	DWORD fileChan;

	err = dacTestDeviceType(handle, deviceType);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	dacStop->dacDevType = deviceType;
	dacStop->dacChannel = chan;

	err = itfExecuteCommand(handle, &sb, DAC_STOP);
	if (err != DerrNoError) {
		return apiCtrlProcessError(handle, err);
	}

	if (session[handle].dacXferDrvrBuf) {

		free(session[handle].dacXferDrvrBuf);
		session[handle].dacXferDrvrBuf = NULL;

		if (dacFindFirstFileChan(handle, &fileDeviceType, &fileChan)) {

			deviceType = fileDeviceType;
			chan = fileChan;

			dacDevice = session[handle].dacDevice[deviceType];
			dacWave = &session[handle].dacDevice[deviceType]->dacWave[chan];

			if (dacWave->dacFileHandle != NULL) {
				fclose(dacWave->dacFileHandle);
				dacWave->dacFileHandle = NULL;
			}
			if (dacDevice->dacWaveFromFileActive) {

				dacDevice->dacWaveFromFileActive = FALSE;
			}
		}
	}

	return DerrNoError;

}
