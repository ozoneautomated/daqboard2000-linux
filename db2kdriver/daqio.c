////////////////////////////////////////////////////////////////////////////
//
// daqio.c                         I    OOO                           h
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

#ifdef DEBUG
static HW *nulldaqmap = (HW *) NULL;
#endif

VOID
fpgaPause(PDEVICE_EXTENSION pde)
{
	if (pde->dmaInpActive || pde->dmaOutActive) {
	}
	return ;
}


VOID
writeAcqScanListEntry(PDEVICE_EXTENSION pde, WORD scanListEntry)
{
	HW *fpga = (HW *) pde->daqBaseAddress;

	dbg("W reg:0x%02x val:0x%08x Acquisition Scan List FIFO",
		 &nulldaqmap->acqScanListFIFO, scanListEntry);

	fpgaPause(pde);
	fpga->acqScanListFIFO = scanListEntry & 0x00ff;
	fpgaPause(pde);
	fpga->acqScanListFIFO = scanListEntry >> 8;
	return ;
}

WORD
readAcqStatus(PDEVICE_EXTENSION pde)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	WORD status;
	fpgaPause(pde);
	status = fpga->acqStatus;

	dbg("R reg:0x%02x val:0x%08x Acquisition Status",
		 &nulldaqmap->acqStatus, status);

	if (status & AcqResultsFIFOOverrun)
		info("AcqStatus: Acq Results FIFO Overrun!\n");
	if (status & AcqAdcNotReady)
		info("AcqStatus: ADC not ready when read!\n");
	if (status & ArbitrationFailure)
		info("AcqStatus: Internal Bus Arbitration Failure!\n");
	if (status & AcqPacerOverrun)
		info("AcqStatus: Acq Pacer Overrun!\n");
	if (status & DacPacerOverrun)
		info("AcqStatus: Dac Pacer Overrun!\n");

	return status;
}


VOID
writeAcqControl(PDEVICE_EXTENSION pde, WORD control)
{
	HW *fpga = (HW *) pde->daqBaseAddress;

	dbg("W reg:0x%02x val:0x%08x Acquisition Control",
		 &nulldaqmap->acqControl, control);

	fpgaPause(pde);
	fpga->acqControl = control;
	return ;
}


VOID
writeAcqPacerClock(PDEVICE_EXTENSION pde, DWORD divisorHigh, DWORD divisorLow)
{
	HW *fpga = (HW *) pde->daqBaseAddress;

	dbg("W reg:0x%02x val:0x%08x Acquisition Pacer Clock Divisor Low",
		 &nulldaqmap->acqPacerClockDivLow, divisorLow);

	fpgaPause(pde);
	fpga->acqPacerClockDivLow = divisorLow;

	dbg("W reg:0x%02x val:0x%08x Acquisition Pacer Clock Divisor High",
		 &nulldaqmap->acqPacerClockDivHigh, divisorHigh);

	fpgaPause(pde);
	fpga->acqPacerClockDivHigh = (WORD) divisorHigh;
	return ;
}


SHORT
readAcqResultsFifo(PDEVICE_EXTENSION pde)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	SHORT result;
	fpgaPause(pde);
	result = fpga->acqResultsFIFO;

	dbg("R reg:0x%02x val:0x%08x Acquisition Results FIFO",
		 &nulldaqmap->acqResultsFIFO, result);

	return result;
}


SHORT
peekAcqResultsFifo(PDEVICE_EXTENSION pde)
{
	SHORT result;
	HW *fpga = (HW *) pde->daqBaseAddress;
	fpgaPause(pde);
	result = fpga->acqResultsShadow;


	dbg("R reg:0x%02x val:0x%08x Acquisition Results Shadow",
		 &nulldaqmap->acqResultsShadow, result);

	return result;
}


DWORD
readAcqScanCounter(PDEVICE_EXTENSION pde)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	WORD newCounterValue;
	WORD status, verifyStatus;
	int dwDiff, dwAdjustedValue;
	DWORD currentScan;
	SHORT lastChannel;
	WORD *bufPtr;

	fpgaPause(pde);
	newCounterValue = fpga->acqScanCounter;
	fpgaPause(pde);
	status = fpga->acqStatus;


	dbg("R reg:0x%02x val:0x%08x Acquisition Scan Counter",
		 &nulldaqmap->acqScanCounter, newCounterValue);
	dbg("R reg:0x%02x val:0x%08x Acquisition Status",
		 &nulldaqmap->acqStatus, status);


	if ( (pde->eventExpected == TRIGGER_EVENT) &&
	     (pde->trig[TRIGGER_EVENT].trigControl)  &&
	     (!pde->trig[TRIGGER_EVENT].hwEventOccurred) )  {

		if (readTrigStatus(pde) & TriggerEvent) {
			pde->trig[TRIGGER_EVENT].hwEventOccurred = TRUE;
		} else {
			pde->adcLastFifoCheck = (DWORD) TRUE;
			return pde->adcTRUECurrentScanCount;
		}
	}

	if ((status & AcqLogicScanning) ||
	    (status & AcqResultsFIFOMore1Sample) ||
	    (newCounterValue != pde->adcTRUEOldCounterValue) || pde->adcLastFifoCheck) {

		pde->adcTRUEOldCounterValue = newCounterValue;

		if (newCounterValue < pde->adcOldCounterValue)
			dwDiff = newCounterValue + 0x10000L - pde->adcOldCounterValue;
		else
			dwDiff = newCounterValue - pde->adcOldCounterValue;

		if (dwDiff > pde->adcScansInFifo)
			dwAdjustedValue = pde->adcOldCounterValue + dwDiff - pde->adcScansInFifo;
		else
			dwAdjustedValue = pde->adcOldCounterValue;

		if (dwAdjustedValue > 0xFFFFL)
			newCounterValue = (WORD) (dwAdjustedValue - 0x10000L);
		else
			newCounterValue = (WORD) dwAdjustedValue;


		pde->adcTRUECurrentScanCount =
			(pde->adcTRUECurrentScanCount & 0xffff0000) + (DWORD) newCounterValue;

		if (newCounterValue < pde->adcOldCounterValue)
			pde->adcTRUECurrentScanCount += 0x10000L;

		pde->adcOldCounterValue = newCounterValue;

	} else {

		if (newCounterValue == pde->adcOldCounterValue)
			return pde->adcTRUECurrentScanCount;

		pde->adcTRUECurrentScanCount =
			(pde->adcTRUECurrentScanCount & 0xffff0000) + (DWORD) newCounterValue;

		if (newCounterValue < pde->adcOldCounterValue)
			pde->adcTRUECurrentScanCount += 0x10000L;

		pde->adcOldCounterValue = pde->adcTRUEOldCounterValue = newCounterValue;

		if (pde->scanSizeOdd &&
		    (!(status & AcqResultsFIFOMore1Sample) &&
		     (status & AcqResultsFIFOHasValidData))) {

			lastChannel = peekAcqResultsFifo(pde);
			currentScan = pde->adcTRUECurrentScanCount;

			bufPtr = pde->dmaInpBuf + ((currentScan * pde->scanSize) % DMAINPBUFSIZE) - 1L;

			fpgaPause(pde);
			newCounterValue = fpga->acqScanCounter;
			fpgaPause(pde);
			verifyStatus = fpga->acqStatus;

			if (!(verifyStatus & AcqLogicScanning) &&
			    (!(verifyStatus & AcqResultsFIFOMore1Sample) &&
			     (verifyStatus & AcqResultsFIFOHasValidData)) &&
			    (newCounterValue == pde->adcTRUEOldCounterValue)) {

				*bufPtr = lastChannel;
			} else {
				pde->adcTRUECurrentScanCount--;
			}
		}
	}

	pde->adcLastFifoCheck = (DWORD) (status & (AcqLogicScanning | AcqResultsFIFOMore1Sample));
	return pde->adcTRUECurrentScanCount;
}


DWORD
readAcqTriggerCount(PDEVICE_EXTENSION pde)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	WORD triggerCount;
	DWORD triggerScan;
	fpgaPause(pde);
	triggerCount = fpga->acqTriggerCount;


	dbg("R reg:0x%02x val:0x%08x Acquisition Trigger Count",
		 &nulldaqmap->acqTriggerCount, triggerCount);

	triggerScan = (pde->adcCurrentScanCount & 0xffff0000) + (DWORD) triggerCount;
	if (triggerCount < pde->adcOldCounterValue)
		triggerScan += 0x10000L;
	return triggerScan;
}


VOID
writeAcqDigitalMark(PDEVICE_EXTENSION pde, WORD markValue)
{
	HW *fpga = (HW *) pde->daqBaseAddress;

	dbg("W reg:0x%02x val:0x%08x Digital Mark",
		 &nulldaqmap->acqDigitalMark, markValue);

	fpgaPause(pde);
	fpga->acqDigitalMark = markValue;
	return ;
}

WORD
readAcqDigitalMark(PDEVICE_EXTENSION pde)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	WORD result;
	fpgaPause(pde);
	result = fpga->acqDigitalMark;

	dbg("R reg:0x%02x val:0x%08x Digital Mark",
		 &nulldaqmap->acqDigitalMark, result);

	return result;
}


VOID
writeDmaControl(PDEVICE_EXTENSION pde, WORD control)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	fpgaPause(pde);
	fpga->dmaControl = control;

	dbg("W 0x%02x 0x%02x 0x%02x DMA Control", 
	    &nulldaqmap->dmaControl, control);

	return ;
}

WORD
readDacStatus(PDEVICE_EXTENSION pde)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	WORD result;
	fpgaPause(pde);
	result = fpga->dacStatusReg;

	dbg("R reg:0x%02x val:0x%08x Analog Output Dac Status",
		 &nulldaqmap->dacStatusReg, result);

	return result;
}


DWORD
readDacScanCounter(PDEVICE_EXTENSION pde)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	WORD newCounterValue;
	fpgaPause(pde);
	newCounterValue = fpga->dacScanCounter;

	dbg("R reg:0x%02x val:0x%08x Dma output Scan Counter",
		 &nulldaqmap->dacScanCounter, newCounterValue);


	pde->dacCurrentScanCount =
	    (pde->dacCurrentScanCount & 0xffff0000) + (DWORD) newCounterValue;
	if (newCounterValue < pde->dacOldCounterValue)
		pde->dacCurrentScanCount += 0x10000L;
	pde->dacOldCounterValue = newCounterValue;
	return pde->dacCurrentScanCount;
}


VOID
writeDacControl(PDEVICE_EXTENSION pde, WORD control)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	fpgaPause(pde);
	fpga->dacControl = control;

	dbg("W reg:0x%02x val:0x%08x Analog Output Control",
		 &nulldaqmap->dacControl, control);

	return ;
}


VOID
writeDac(PDEVICE_EXTENSION pde, WORD dacNo, SHORT outValue)
{
	WORD timeoutValue = 10000;
	WORD dacBusy = Dac0Busy;
	HW *fpga = (HW *) pde->daqBaseAddress;
	dbg("Setting busy for dacNo=%i",dacNo);
	switch (dacNo) {
	case 0:
		dacBusy = Dac0Busy;
		break;

	case 1:
		dacBusy = Dac1Busy;
		break;

	case 2:
		dacBusy = Dac2Busy;
		break;

	case 3:
		dacBusy = Dac3Busy;
		break;
	}

	fpgaPause(pde);
	fpga->dacSetting[dacNo] = outValue;


	dbg("W reg:0x%02x val:0x%08x Analog Output DAC FIFO",
		 &nulldaqmap->dacSetting[dacNo], outValue);


	while (readDacStatus(pde) & dacBusy) {
		if (timeoutValue-- == 0) {
			dbg("! Timeout Writing to DAC %d!\n", dacNo);
			break;
		}
	}
	return ;
}


VOID
writeDacPacerClock(PDEVICE_EXTENSION pde, WORD divisor)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	fpgaPause(pde);
	fpga->dacPacerClockDiv = divisor;

	dbg("W reg:0x%02x val:0x%08x Analog Output Pacer Clock Divisor",
		 &nulldaqmap->dacPacerClockDiv, divisor);

	return ;
}


VOID
writePosRefDac(PDEVICE_EXTENSION pde, BYTE posRef)
{
	WORD timeoutValue = 10000;
	HW *fpga = (HW *) pde->daqBaseAddress;


	dbg("W reg:0x%02x val:0x%08x Posref DAC", 
	    &nulldaqmap->refDacs,
	    ((WORD) posRef) | PosRefDacSelect);

	fpgaPause(pde);
	fpga->refDacs = ((WORD) posRef) | PosRefDacSelect;

	while (readDacStatus(pde) & RefBusy) {
		if (timeoutValue-- == 0) {
			dbg("Timeout Writing to Positive Ref. Dac!\n");
			break;
		}
	}
	return ;
}


VOID
writeNegRefDac(PDEVICE_EXTENSION pde, BYTE negRef)
{
	WORD timeoutValue = 10000;
	HW *fpga = (HW *) pde->daqBaseAddress;

	dbg("W reg:0x%02x val:0x%08x Negref DAC", 
	    &nulldaqmap->refDacs,
	    ((WORD) negRef) | NegRefDacSelect);

	fpgaPause(pde);
	fpga->refDacs = ((WORD) negRef) | NegRefDacSelect;
	while (readDacStatus(pde) & RefBusy) {
		if (timeoutValue-- == 0) {
			dbg("Timeout Writing to Negative Ref. Dac!\n");
			break;
		}
	}
	return ;
}


VOID
writeTrigControl(PDEVICE_EXTENSION pde, WORD trigCommand)
{
	HW *fpga = (HW *) pde->daqBaseAddress;

	dbg("W reg:0x%02x val:0x%08x Trigger Control",
		 &nulldaqmap->trigControl, trigCommand);

	fpgaPause(pde);
	fpga->trigControl = trigCommand;
	return ;
}


WORD
readTrigStatus(PDEVICE_EXTENSION pde)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	WORD result;
	fpgaPause(pde);
	result = fpga->trigStatus;

	dbg("R reg:0x%02x val:0x%08x Trigger Status", 
	    &nulldaqmap->trigStatus, result);

	return result;
}


VOID
writeTrigHysteresis(PDEVICE_EXTENSION pde, WORD trigHysteresis)
{
	WORD timeoutValue = 10000;
	HW *fpga = (HW *) pde->daqBaseAddress;

	dbg("W reg:0x%02x val:0x%08x Analog TRIGA(Hysteresis) DAC",
	    &nulldaqmap->trigDacs,
	    (trigHysteresis & 0x0fff) | HysteresisDacSelect);

	fpgaPause(pde);
	fpga->trigDacs = (trigHysteresis & 0x0fff) | HysteresisDacSelect;
	while (readDacStatus(pde) & TrgBusy) {
		if (timeoutValue-- == 0)
			break;
	}
	return ;
}

VOID
writeTrigThreshold(PDEVICE_EXTENSION pde, WORD trigThreshold)
{
	WORD timeoutValue = 10000;
	HW *fpga = (HW *) pde->daqBaseAddress;

	dbg("W reg:0x%02x val:0x%08x Analog TRIGB(Threshold) DAC",
	    &nulldaqmap->trigDacs,
	    (trigThreshold & 0x0fff) | ThresholdDacSelect);

	fpgaPause(pde);
	fpga->trigDacs = (trigThreshold & 0x0fff) | ThresholdDacSelect;
	while (readDacStatus(pde) & TrgBusy) {
		if (timeoutValue-- == 0)
			break;
	}
	return ;
}


VOID
writeCalEepromControl(PDEVICE_EXTENSION pde, WORD control)
{
	HW *fpga = (HW *) pde->daqBaseAddress;

	dbg("W reg:0x%02x val:0x%08x Acquisition Control",
	    &nulldaqmap->calEepromControl, control);

	fpgaPause(pde);
	fpga->calEepromControl = control;
	return ;
}


VOID
writeCalEeprom(PDEVICE_EXTENSION pde, WORD value)
{
	WORD timeoutValue = 10000;
	HW *fpga = (HW *) pde->daqBaseAddress;

	dbg("W reg:0x%02x val:0x%08x Calibration Table EEPROM",
	    &nulldaqmap->calEeprom, value);

	fpgaPause(pde);
	fpga->calEeprom = value;
	while (readDacStatus(pde) & CalBusy) {
		if (timeoutValue-- == 0)
			break;
	}
	return ;
}


WORD
readCalEeprom(PDEVICE_EXTENSION pde)
{
	WORD timeoutValue = 10000;
	WORD eepromValue;
	HW *fpga = (HW *) pde->daqBaseAddress;
	fpgaPause(pde);
	eepromValue = fpga->calEeprom;


	dbg("R reg:0x%02x val:0x%08x Calibration Table EEPROM",
	     &nulldaqmap->calEeprom, eepromValue);

	while (readDacStatus(pde) & CalBusy) {
		if (timeoutValue-- == 0)
			break;
	}
	return eepromValue;
}


WORD
readCtrTmrStatus(PDEVICE_EXTENSION pde)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	WORD result;
	fpgaPause(pde);
	result = fpga->ctrTmrStatus;


	dbg("R reg:0x%02x val:0x%08x Counter/Timer Status",
	    &nulldaqmap->ctrTmrStatus, result);

	return result;
}


VOID
writeTmrCtrControl(PDEVICE_EXTENSION pde, WORD control)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	
	dbg("W reg:0x%02x val:0x%08x Counter/Timer Control",
	    &nulldaqmap->ctrTmrControl, control);

	fpgaPause(pde);
	fpga->ctrTmrControl = control;
	return ;
}


WORD
readCtrInput(PDEVICE_EXTENSION pde, WORD ctrNo)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	WORD result;
	fpgaPause(pde);
	result = fpga->ctrInput[ctrNo];

	dbg("R reg:0x%02x val:0x%08x Counter Input",
	    &nulldaqmap->ctrInput[ctrNo], result);

	return result;
}


VOID
writeTmrDivisor(PDEVICE_EXTENSION pde, WORD tmrNo, WORD divisor)
{
	HW *fpga = (HW *) pde->daqBaseAddress;


	dbg("W reg:0x%02x val:0x%08x Timer Divisor",
	    &nulldaqmap->timerDivisor[tmrNo], divisor);

	fpgaPause(pde);
	fpga->timerDivisor[tmrNo] = divisor;
	return ;
}


WORD
readDioStatus(PDEVICE_EXTENSION pde)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	WORD result;
	fpgaPause(pde);
	result = fpga->dioStatus;

	dbg("R reg:0x%02x val:0x%08x Digital I/O Status",
	    &nulldaqmap->dioStatus, result);

	return result;
}


VOID
writeDioControl(PDEVICE_EXTENSION pde, WORD control)
{
	HW *fpga = (HW *) pde->daqBaseAddress;


	dbg("W reg:0x%02x val:0x%08x Digital I/O Control",
	    &nulldaqmap->dioControl, control);

	fpgaPause(pde);
	fpga->dioControl = control;
	return ;
}


WORD
readHSIOPort(PDEVICE_EXTENSION pde)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	WORD result;
	fpgaPause(pde);
	result = fpga->dioP3hsioData;

	dbg("R reg:0x%02x val:0x%08x HSIO Data",
	    &nulldaqmap->dioP3hsioData, result);

	return result;
}


VOID
writeHSIOPort(PDEVICE_EXTENSION pde, WORD portValue)
{
	HW *fpga = (HW *) pde->daqBaseAddress;

	dbg("W reg:0x%02x val:0x%08x HSIO Data",
	    &nulldaqmap->dioP3hsioData, portValue);

	fpgaPause(pde);
	fpga->dioP3hsioData = portValue;
	return ;
}


WORD
read8255Port(PDEVICE_EXTENSION pde, WORD portNumber)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	WORD result;
	fpgaPause(pde);
	result = fpga->dioP28255[portNumber];

	dbg("R reg:0x%02x val:0x%08x 8255 Port",
	    &nulldaqmap->dioP28255[portNumber], result);
	
	return result;
}


VOID
write8255Port(PDEVICE_EXTENSION pde, WORD portNumber, WORD portValue)
{
	HW *fpga = (HW *) pde->daqBaseAddress;


	dbg("W reg:0x%02x val:0x%08x 8255",
	    &nulldaqmap->dioP28255[portNumber],portValue);

	fpgaPause(pde);
	fpga->dioP28255[portNumber] = portValue;
	return ;
}


WORD
readExpansionIOPort(PDEVICE_EXTENSION pde, WORD portNumber)
{
	HW *fpga = (HW *) pde->daqBaseAddress;
	WORD result;
	fpgaPause(pde);
	result = fpga->dioP2ExpansionIO8Bit[portNumber];

	dbg("R reg:0x%02x val:0x%08x P2 Expansion I/O",
	    &nulldaqmap->dioP2ExpansionIO8Bit[portNumber], result);

	return result;
}


VOID
writeExpansionIOPort(PDEVICE_EXTENSION pde, WORD portNumber, WORD portValue)
{
	HW *fpga = (HW *) pde->daqBaseAddress;

	dbg("W reg:0x%02x val:0x%08x P2 Expansion I/O",
	    &nulldaqmap->dioP2ExpansionIO8Bit[portNumber], portValue);

	fpgaPause(pde);
	fpga->dioP2ExpansionIO8Bit[portNumber] = portValue;
	return ;
}


DAQIOSTATUS
daqIoRead(PDEVICE_EXTENSION pde, DAQPIRP * irp)
{
	BYTE *readAddress;
	HW *fpga = (HW *) pde->daqBaseAddress;
	readAddress = (BYTE *) pde->daqBaseAddress + (DWORD) irp->Address;

	if (readAddress == (BYTE *) & fpga->acqPacerClockDivLow) {
		*((DWORD *) irp->SystemBuffer) = *((DWORD *) readAddress);
	} else {
		*((DWORD *) irp->SystemBuffer) = (WORD) (*((WORD *) readAddress));
	}

	dbg("R reg:0x%02x val:0x%08x REG_READ",
	    irp->Address, *((DWORD *) irp->SystemBuffer));

	return DaqIoNoErrors;
}


DAQIOSTATUS
daqIoWrite(PDEVICE_EXTENSION pde, DAQPIRP * irp)
{
	BYTE *writeAddress;
	HW *fpga = (HW *) pde->daqBaseAddress;
	writeAddress = (BYTE *) pde->daqBaseAddress + (DWORD) irp->Address;
	if (writeAddress == (BYTE *) & fpga->acqPacerClockDivLow) {
		*((DWORD *) writeAddress) = *((DWORD *) irp->SystemBuffer);
		dbg("W reg:0x%02x val:0x%08x REG_WRITE",
		    irp->Address,*((DWORD *) irp->SystemBuffer));

	} else {
		*((WORD *) writeAddress) = *((WORD *) irp->SystemBuffer);
		dbg("W reg:0x%02x val:0x%08x REG_WRITE",
		    irp->Address, *((WORD *) irp->SystemBuffer));
	}
	return DaqIoNoErrors;
}


DAQIOSTATUS
IoDaqCallDriver(PDEVICE_EXTENSION pde, DAQPIRP * Irp)
{
	DAQIOSTATUS IoStatus = DaqIoBadCommand;
	switch (Irp->MajorFunction) {
	case DAQ_IO_READ:
		IoStatus = daqIoRead(pde, Irp);
		break;

	case DAQ_IO_WRITE:
		IoStatus = daqIoWrite(pde, Irp);
		break;
	}

	return IoStatus;
}
