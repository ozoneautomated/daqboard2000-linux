////////////////////////////////////////////////////////////////////////////
//
// cal.c                           I    OOO                           h
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

#include <linux/version.h>
//#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/bitops.h>
//#include <linux/wrapper.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "iottypes.h"
#include "daqx.h"
#include "ddapi.h"
#include "device.h"
#include "daqio.h"
#include "w32ioctl.h"

#define PREAMBLE   0xFEDCBA98
#define POSTAMBLE  0x01234567

#define FACTORY_CAL_ADDRESS   0x0800
#define USER_CAL_ADDRESS      0x0000
#define BOARD_INFORMATION     0x0fE0

#define SECRETPASSWORD        71546L

BYTE
computeCalConstantsChecksum(PDEVICE_EXTENSION pde)
{
	WORD i;
	BYTE checksum = 0;

	char *srcPtr = (char *) &pde->eeprom;

	for (i = 0; i < sizeof (eepromT) - 1; i++) {
		checksum += *srcPtr++;
	}

	checksum = 0x100 - checksum;
	return checksum;
}


BOOL
verifyCalConstants(PDEVICE_EXTENSION pde)
{
	if ((pde->eeprom.preamble != PREAMBLE)
	    || (pde->eeprom.postamble != POSTAMBLE))
		return FALSE;

	if (computeCalConstantsChecksum(pde) != pde->eeprom.checksum)
		return FALSE;

	return TRUE;
}


void
initCalConstants(PDEVICE_EXTENSION pde)
{
	short i, j, k;

	/*PEK: magic number city!!*/

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 7; j++) {
			for (k = 0; k < 2; k++) {
				pde->eeprom.correctionDACSE[i][j][k].offset = 0x800;
				pde->eeprom.correctionDACSE[i][j][k].gain = 0xc00;
			}
		}
	}

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 7; j++) {
			for (k = 0; k < 2; k++) {
				pde->eeprom.correctionDACDE[i][j][k].offset = 0x800;
				pde->eeprom.correctionDACDE[i][j][k].gain = 0xc00;
			}
		}
	}


	pde->eeprom.posRef = pde->eeprom.negRef = 0x80;

	for (i = 0; i < 2; i++) {
		pde->eeprom.trigThresholdDAC[i].offset = 0x000;
		pde->eeprom.trigThresholdDAC[i].gain = 0xfff;
		pde->eeprom.trigHysteresisDAC[i].offset = 0x000;
		pde->eeprom.trigHysteresisDAC[i].gain = 0xfff;
	}
	return ;
}


BOOL
readCalConstants(PDEVICE_EXTENSION pde, DaqCalTableTypeT tableType)
{
	WORD srcPtr;
	WORD *dstPtr;
	WORD i;

	if (tableType == DcttFactory)
		srcPtr = FACTORY_CAL_ADDRESS;
	else
		srcPtr = USER_CAL_ADDRESS;

	dstPtr = (WORD *) & pde->eeprom;
	writeCalEepromControl(pde, CalEepromSendOneByte | CalEepromHoldLogic);
	writeCalEeprom(pde, EepromReadData);

	writeCalEepromControl(pde, CalEepromSendTwoBytes | CalEepromHoldLogic);
	writeCalEeprom(pde, srcPtr);

	for (i = 0; i < (sizeof (eepromT) - 2) / 2; i++) {
		writeCalEeprom(pde, 0x0000);
		*dstPtr = readCalEeprom(pde);
		dstPtr++;
	}

	writeCalEepromControl(pde, CalEepromSendTwoBytes | CalEepromReleaseLogic);
	writeCalEeprom(pde, 0x0000);
	*dstPtr = readCalEeprom(pde);

	pde->calTableReadOk = verifyCalConstants(pde);

	if (pde->calTableReadOk == FALSE) {
		initCalConstants(pde);
		return FALSE;
	}

	return TRUE;
}


BOOL
writeBlockToEEPROM(PDEVICE_EXTENSION pde, WORD * dataAddr, WORD * eepromAddr)
{
	WORD destAddr = (WORD) ((DWORD) eepromAddr & 0xffff);
	WORD *srcAddr = dataAddr;
	WORD i;

	writeCalEepromControl(pde, CalEepromWriteEnable | CalEepromSendOneByte | CalEepromReleaseLogic);
	writeCalEeprom(pde, EepromWriteEnable);

	writeCalEepromControl(pde, CalEepromWriteEnable | CalEepromSendOneByte | CalEepromHoldLogic);
	writeCalEeprom(pde, EepromWriteData);

	writeCalEepromControl(pde, CalEepromWriteEnable | CalEepromSendTwoBytes | CalEepromHoldLogic);
	writeCalEeprom(pde, destAddr);

	/* PEK 15?!, 15? why 15?*/

	for (i = 0; i < 15; i++) {
		writeCalEeprom(pde, *srcAddr);
		srcAddr++;
	}

	writeCalEepromControl(pde, CalEepromWriteEnable | CalEepromSendTwoBytes | CalEepromReleaseLogic);
	writeCalEeprom(pde, *srcAddr);

	writeCalEepromControl(pde, CalEepromWriteDisable | CalEepromSendOneByte | CalEepromReleaseLogic);
	writeCalEeprom(pde, EepromWriteDisable);

	return TRUE;
}


BOOL
writeCalConstants(PDEVICE_EXTENSION pde, DaqCalTableTypeT tableType)
{
	short i;
	WORD *srcPtr;
	WORD *dstPtr;
	BOOL returnCode = TRUE;

	DWORD dwTime = 1;
	BYTE  wDay = 1, wMonth = 1;
	WORD wYear = 1980;

	pde->eeprom.date[0] = (wMonth / 10) + 48;
	pde->eeprom.date[1] = (wMonth % 10) + 48;
	pde->eeprom.date[2] = '/';
	pde->eeprom.date[3] = (wDay / 10) + 48;
	pde->eeprom.date[4] = (wDay % 10) + 48;
	pde->eeprom.date[5] = '/';
	pde->eeprom.date[6] = (wYear / 1000) + 48;
	pde->eeprom.date[7] = ((wYear / 100) % 10) + 48;
	pde->eeprom.date[8] = ((wYear / 10) % 10) + 48;
	pde->eeprom.date[9] = (wYear % 10) + 48;
	pde->eeprom.date[10] = 0;

	pde->eeprom.time[0] = ((dwTime / 3600) / 10) + 48;
	pde->eeprom.time[1] = ((dwTime / 3600) % 10) + 48;
	pde->eeprom.time[2] = ':';
	pde->eeprom.time[3] = (((dwTime % 3600) / 60) / 10) + 48;
	pde->eeprom.time[4] = (((dwTime % 3600) / 60) % 10) + 48;
	pde->eeprom.time[5] = ':';
	pde->eeprom.time[6] = (((dwTime % 3600) % 60) / 10) + 48;
	pde->eeprom.time[7] = (((dwTime % 3600) % 60) % 10) + 48;
	pde->eeprom.time[8] = 0;

	pde->eeprom.preamble = PREAMBLE;
	pde->eeprom.postamble = POSTAMBLE;

	pde->eeprom.checksum = computeCalConstantsChecksum(pde);

	if (tableType == DcttFactory) {
		dstPtr = (WORD *) FACTORY_CAL_ADDRESS;
		writeCalEepromControl(pde, CalEepromWriteEnable |
				      CalEepromSendTwoBytes | CalEepromReleaseLogic);
		writeCalEeprom(pde, EepromWriteStatusRegister | 0x0000);
	} else {
		dstPtr = (WORD *) USER_CAL_ADDRESS;
		writeCalEepromControl(pde, CalEepromWriteEnable |
				      CalEepromSendTwoBytes | CalEepromReleaseLogic);
		writeCalEeprom(pde, EepromWriteStatusRegister | 0x0008);
	}

	srcPtr = (WORD *) & pde->eeprom;
	for (i = 0; i < (sizeof (eepromT) / 2); i += 16) {
		if (!writeBlockToEEPROM(pde, srcPtr, dstPtr)) {
			returnCode = FALSE;
			break;
		}
		srcPtr += 16;
		dstPtr += 16;
	}

	writeCalEepromControl(pde, CalEepromWriteDisable | CalEepromSendTwoBytes |
			      CalEepromReleaseLogic);
	writeCalEeprom(pde, EepromWriteStatusRegister | 0x008c);

	/*PEK: replaced baroque 4 if/else beast with one if here... */

	if ( ! returnCode ||
	     ! readCalConstants(pde, tableType) ||
	     ! (verifyCalConstants(pde)) ||
	     (pde->eeprom.testTime != dwTime)) {
				returnCode = FALSE;
	}

	return returnCode;
}


BYTE
computeBoardInfoChecksum(PDEVICE_EXTENSION pde)
{
	WORD i;
	BYTE checksum = 0;

	char *srcPtr = (char *) &pde->boardInfo;
	for (i = 0; i < sizeof (brdInfoT) - 1; i++) {
		checksum += *srcPtr++;
	}
	checksum = 0x100 - checksum;

	return checksum;
}


BOOL
readBoardInfo(PDEVICE_EXTENSION pde)
{
	WORD *dstPtr;
	WORD i;

	dstPtr = (WORD *) & pde->boardInfo;

	writeCalEepromControl(pde, CalEepromSendOneByte | CalEepromHoldLogic);
	writeCalEeprom(pde, EepromReadData);

	writeCalEepromControl(pde, CalEepromSendTwoBytes | CalEepromHoldLogic);
	writeCalEeprom(pde, BOARD_INFORMATION);

	for (i = 0; i < (sizeof (brdInfoT) - 2) / 2; i++) {
		writeCalEeprom(pde, 0x0000);
		*dstPtr = readCalEeprom(pde);
		dstPtr++;
	}

	writeCalEepromControl(pde, CalEepromSendTwoBytes | CalEepromReleaseLogic);
	writeCalEeprom(pde, 0x0000);
	*dstPtr = readCalEeprom(pde);

	if ((computeBoardInfoChecksum(pde) != pde->boardInfo.checksum) ||
	    (pde->boardInfo.secretPassword != SECRETPASSWORD)) {
		return FALSE;
	}

	return TRUE;
}


BOOL
writeBoardInfo(PDEVICE_EXTENSION pde)
{
	short i;
	WORD *srcPtr;
	BOOL returnCode = TRUE;

	for (i = 0; i < FILLER_SIZE; i++) {
		pde->boardInfo.filler[i] = 0;
	}

	pde->boardInfo.checksum = computeBoardInfoChecksum(pde);
	srcPtr = (WORD *) & pde->boardInfo;

	writeCalEepromControl(pde, CalEepromWriteEnable | CalEepromSendTwoBytes | CalEepromReleaseLogic);
	writeCalEeprom(pde, EepromWriteStatusRegister | 0x0000);

	if (!writeBlockToEEPROM(pde, srcPtr, (WORD *) BOARD_INFORMATION)) {
		returnCode = FALSE;
	}

	writeCalEepromControl(pde, CalEepromWriteDisable | CalEepromSendTwoBytes |
			      CalEepromReleaseLogic);
	writeCalEeprom(pde, EepromWriteStatusRegister | 0x008c);

	if (returnCode) {
		return readBoardInfo(pde);
	}

	return FALSE;
}


DWORD
readCalTimeStamp(PDEVICE_EXTENSION pde)
{
	return (DWORD) FALSE;
}

VOID
activateReferenceDacs(PDEVICE_EXTENSION pde)
{
	writePosRefDac(pde, pde->eeprom.posRef);
	writeNegRefDac(pde, pde->eeprom.negRef);
}


VOID
calConfigure(PDEVICE_EXTENSION pde, daqIOTP p)
{
	CAL_CONFIG_PT calConfig = (CAL_CONFIG_PT) p;

#define unipolar 0
#define bipolar  1
#define singleEnded  0
#define differential 1

	short ch, gain, pol, end;

	switch (calConfig->operation) {
	case CoGetConstants:
	case CoSetConstants:
		switch (calConfig->adcRange) {
		case DarUniPolarDE:
			pol = unipolar;
			end = differential;
			break;

		case DarUniPolarSE:
			pol = unipolar;
			end = singleEnded;
			break;

		case DarBiPolarDE:
			pol = bipolar;
			end = differential;
			break;

		case DarBiPolarSE:
			pol = bipolar;
			end = singleEnded;
			break;

		default:
			calConfig->errorCode = DerrInvAdcRange;
			return;
		}

		ch = calConfig->channel;

		if (ch > 15 || ((end == differential) && (ch > 7))) {
			calConfig->errorCode = DerrInvChan;
			break;
		}

		gain = calConfig->gain;
		if (gain > 6) {
			calConfig->errorCode = DerrInvGain;
			break;
		}

		if (end == differential) {
			if (calConfig->operation == CoGetConstants) {
				calConfig->offsetConstant =
				    pde->eeprom.correctionDACDE[ch][gain][pol].offset;
				calConfig->gainConstant =
				    pde->eeprom.correctionDACDE[ch][gain][pol].gain;
			} else {
				pde->eeprom.correctionDACDE[ch][gain][pol].
				    offset = calConfig->offsetConstant;
				pde->eeprom.correctionDACDE[ch][gain][pol].
				    gain = calConfig->gainConstant;
			}
		} else {
			if (calConfig->operation == CoGetConstants) {
				calConfig->offsetConstant =
				    pde->eeprom.correctionDACSE[ch][gain][pol].offset;
				calConfig->gainConstant =
				    pde->eeprom.correctionDACSE[ch][gain][pol].gain;
			} else {
				pde->eeprom.correctionDACSE[ch][gain][pol].
				    offset = calConfig->offsetConstant;
				pde->eeprom.correctionDACSE[ch][gain][pol].
				    gain = calConfig->gainConstant;
			}
		}

		break;

	case CoGetRefDacConstants:
		if (calConfig->channel == DcrcPosRefDac)
			calConfig->offsetConstant = pde->eeprom.posRef;
		else if (calConfig->channel == DcrcNegRefDac)
			calConfig->offsetConstant = pde->eeprom.negRef;
		else
			calConfig->errorCode = DerrInvChan;
		break;

	case CoSetRefDacConstants:
		if (calConfig->channel == DcrcPosRefDac) {
			pde->eeprom.posRef = calConfig->offsetConstant;
			writePosRefDac(pde, pde->eeprom.posRef);
		} else if (calConfig->channel == DcrcNegRefDac) {
			pde->eeprom.negRef = calConfig->offsetConstant;
			writeNegRefDac(pde, pde->eeprom.negRef);
		} else
			calConfig->errorCode = DerrInvChan;
		break;

	case CoGetTrigDacConstants:
		switch (calConfig->channel) {
		case (CctdcTrigThresholdDac + CctdcTrigAboveLevel):
			calConfig->offsetConstant = pde->eeprom.trigThresholdDAC[AboveIndex].offset;
			calConfig->gainConstant = pde->eeprom.trigThresholdDAC[AboveIndex].gain;
			break;

		case (CctdcTrigThresholdDac + CctdcTrigBelowLevel):
			calConfig->offsetConstant = pde->eeprom.trigThresholdDAC[BelowIndex].offset;
			calConfig->gainConstant = pde->eeprom.trigThresholdDAC[BelowIndex].gain;
			break;

		case (CctdcTrigHysteresisDac + CctdcTrigAboveLevel):
			calConfig->offsetConstant =
			    pde->eeprom.trigHysteresisDAC[AboveIndex].offset;
			calConfig->gainConstant = pde->eeprom.trigHysteresisDAC[AboveIndex].gain;
			break;

		case (CctdcTrigHysteresisDac + CctdcTrigBelowLevel):
			calConfig->offsetConstant =
			    pde->eeprom.trigHysteresisDAC[BelowIndex].offset;
			calConfig->gainConstant = pde->eeprom.trigHysteresisDAC[BelowIndex].gain;
			break;

		default:
			calConfig->errorCode = DerrInvChan;
			break;
		}
		break;

	case CoSetTrigDacConstants:

		switch (calConfig->channel) {
		case (CctdcTrigThresholdDac + CctdcTrigAboveLevel):
			pde->eeprom.trigThresholdDAC[AboveIndex].offset = calConfig->offsetConstant;
			pde->eeprom.trigThresholdDAC[AboveIndex].gain = calConfig->gainConstant;
			break;

		case (CctdcTrigThresholdDac + CctdcTrigBelowLevel):
			pde->eeprom.trigThresholdDAC[BelowIndex].offset = calConfig->offsetConstant;
			pde->eeprom.trigThresholdDAC[BelowIndex].gain = calConfig->gainConstant;
			break;

		case (CctdcTrigHysteresisDac + CctdcTrigAboveLevel):
			pde->eeprom.trigHysteresisDAC[AboveIndex].offset = calConfig->offsetConstant;
			pde->eeprom.trigHysteresisDAC[AboveIndex].gain = calConfig->gainConstant;
			break;

		case (CctdcTrigHysteresisDac + CctdcTrigBelowLevel):

			pde->eeprom.trigHysteresisDAC[BelowIndex].offset =
			    calConfig->offsetConstant;
			pde->eeprom.trigHysteresisDAC[BelowIndex].gain = calConfig->gainConstant;
			break;

		default:
			calConfig->errorCode = DerrInvChan;
			break;
		}
		break;

	case CoSelectTable:

		switch (calConfig->tableType) {
		case DcttFactory:
		case DcttUser:
			if (readCalConstants(pde, calConfig->tableType)) {
				activateReferenceDacs(pde);
			} else {
				calConfig->errorCode = DerrInvCalFile;
			}
			break;

		case DcttBaseline:
			initCalConstants(pde);
			activateReferenceDacs(pde);
			break;

		default:
			calConfig->errorCode = DerrInvCalTableType;
			break;
		}
		break;

	case CoSaveTable:
		if ((calConfig->tableType != DcttFactory)
		    && (calConfig->tableType != DcttUser)) {
			calConfig->errorCode = DerrInvCalTableType;
			break;
		}

		if (calConfig->tableType == DcttFactory) {
			if (calConfig->option != SECRETPASSWORD) {
				calConfig->errorCode = DerrInvCalTableType;
				break;
			} else {
				pde->boardInfo.secretPassword = calConfig->option;
				pde->boardInfo.serialNumber = calConfig->channel;
				if (!writeBoardInfo(pde)) {
					calConfig->errorCode = DerrEepromCommFailure;
					break;
				}
			}
		}

		if (!writeCalConstants(pde, calConfig->tableType)) {
			calConfig->errorCode = DerrEepromCommFailure;
			break;
		}
		break;

	default:
		calConfig->errorCode = DerrInvCommand;
	}
	return ;
}
