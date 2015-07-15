////////////////////////////////////////////////////////////////////////////
//
// linux_init_utils.c              I    OOO                           h
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
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "iottypes.h"
#include "daqx.h"
#include "ddapi.h"
#include "device.h"
#include "topbit.h"

#define STATUS_SUCCESS 1
#define STATUS_DEVICE_DOES_NOT_EXIST 0
#define STATUS_DEVICE_CONFIGURATION_ERROR 0
#define ACQUIRE_STEAL 1
#define RELEASE_RESOURCE 3

#define CFG_VENDOR_ID               0x000
#define CFG_SUB_VENDOR_ID           0x02C
#define CFG_RTR_BASE                0x010
#define CFG_RTR_IO_BASE             0x014
#define CFG_LOCAL_BASE0             0x018
#define CFG_LOCAL_BASE1             0x01C
#define CFG_LOCAL_BASE2             0x020
#define CFG_LOCAL_BASE3             0x024
#define CFG_LAT_GNT_INTPIN_INTLINE  0x03C

#define MAX_PCI_BUS                 32
#define MAX_PCI_DEV                 32


BOOL
getBoardInfo(PDEVICE_EXTENSION pde)
{
	if (readBoardInfo(pde)) {
		pde->psite.serialNumber = pde->boardInfo.serialNumber;
	} else {
		pde->psite.serialNumber = 0xffffffff;
	}

	pde->boardInfo.deviceType = pde->psite.deviceType;
	switch (pde->boardInfo.deviceType) {
	case DaqBoard2000:
	case DaqBook2000:
		pde->usingAnalogInputs = TRUE;
		pde->usingAnalogOutputs = TRUE;
		pde->usingDigitalIO = TRUE;
		pde->usingCounterTimers = TRUE;
		pde->numDacs = 2;
		break;

	case DaqBoard2001:
	case DaqBook2001:
		pde->usingAnalogInputs = TRUE;
		pde->usingAnalogOutputs = TRUE;
		pde->usingDigitalIO = TRUE;
		pde->usingCounterTimers = TRUE;
		pde->numDacs = 4;
		break;

	case DaqBoard2002:
	case DaqBook2002:
		pde->usingAnalogInputs = FALSE;
		pde->usingAnalogOutputs = FALSE;
		pde->usingDigitalIO = TRUE;
		pde->usingCounterTimers = TRUE;
		pde->numDacs = 0;
		break;

	case DaqBoard2003:
	case DaqBook2003:
		pde->usingAnalogInputs = FALSE;
		pde->usingAnalogOutputs = TRUE;
		pde->usingDigitalIO = FALSE;
		pde->usingCounterTimers = FALSE;
		pde->numDacs = 4;
		break;

	case DaqBoard2004:
	case DaqBook2004:
		pde->usingAnalogInputs = FALSE;
		pde->usingAnalogOutputs = TRUE;
		pde->usingDigitalIO = TRUE;
		pde->usingCounterTimers = TRUE;
		pde->numDacs = 4;
		break;

	case DaqBoard2005:
	case DaqBook2005:
		pde->usingAnalogInputs = TRUE;
		pde->usingAnalogOutputs = FALSE;
		pde->usingDigitalIO = TRUE;
		pde->usingCounterTimers = TRUE;
		pde->numDacs = 0;
		break;

        
    
    //MAH: 04.05.04 added 1000
  	case DaqBoard1000:	
		pde->usingAnalogInputs = TRUE;
		pde->usingAnalogOutputs = TRUE;
		pde->usingDigitalIO = TRUE;
		pde->usingCounterTimers = TRUE;
		pde->numDacs = 2;
		break;
        
  	case DaqBoard1005:	
		pde->usingAnalogInputs = TRUE;
		pde->usingAnalogOutputs = FALSE;
		pde->usingDigitalIO = TRUE;
		pde->usingCounterTimers = TRUE;
		pde->numDacs = 0;
		break;

	default:
		pde->usingAnalogInputs = FALSE;
		pde->usingAnalogOutputs = FALSE;
		pde->usingDigitalIO = FALSE;
		pde->usingCounterTimers = FALSE;
		pde->numDacs = 0;
		break;
	}

        info("Device serial number: %u", pde->psite.serialNumber);
        info("Device type: %d", pde->boardInfo.deviceType);
        info("Has analog inputs: %d", pde->usingAnalogInputs);
        info("Has analog outputs: %d", pde->usingAnalogOutputs);
        info("Has digital IO: %d", pde->usingDigitalIO);
        info("Has counter timers: %d", pde->usingCounterTimers);
        info("No of Dacs: %d", pde->numDacs);

	return TRUE;
}

#define SECRProgPinHi            0x8001767e
#define SECRProgPinLo            0x8000767e
#define SECRLocalBusHi           0xc000767e
#define SECRLocalBusLo           0x8000767e
#define SECRReloadHi             0xa000767e
#define SECRReloadLo             0x8000767e

#define EEPROM_PRESENT           0x10000000
#define FPGA_PROGRAMMED          0x00020000

#define CPLD_INIT       0x0002
#define CPLD_TXDONE     0x0004
#define CPLD_SIGNATURE  0x5000

#define exceeded10ms() ((time1.LowPart-time0.LowPart) > 200000L) || ((time1.HighPart > time0.HighPart)&&(time1.LowPart>200000L))
#define exceeded5ms() ((time1.LowPart-time0.LowPart) > 200000L) || ((time1.HighPart > time0.HighPart)&&(time1.LowPart>200000L))

void
writeSECR(PDEVICE_EXTENSION pde, DWORD command)
{
	*(DWORD *) ((BYTE *) pde->plxVirtualAddress + 0x006cL) = command;
	return;
}


DWORD
readSECR(PDEVICE_EXTENSION pde)
{
	/*PEK: read[b,w,l] should be used here? */
	return *(DWORD *) ((BYTE *) pde->plxVirtualAddress + 0x006cL);
}


WORD
readCPLD(PDEVICE_EXTENSION pde)
{
	return *(WORD *) ((BYTE *) pde->daqBaseAddress + 0x1000L);
}

BOOL
writeCPLD(PDEVICE_EXTENSION pde, WORD codeWord)
{
	*(WORD *) ((BYTE *) pde->daqBaseAddress + 0x1000L) = codeWord;
	if ((readCPLD(pde) & CPLD_INIT) == CPLD_INIT)
		return TRUE;

	return FALSE;
}


VOID
waitForCPLD(PDEVICE_EXTENSION pde)
{
	WORD timeoutValue = 10000;
	while ((readCPLD(pde) & CPLD_TXDONE) != CPLD_TXDONE) {
		udelay(2); // Kamal - If this delay is not added, driver fails to load correctly.
		if (timeoutValue-- == 0) {
			break;
		}
	}
	return ;
}


void
pause(void)
{
	mdelay(20);
	return;
}


DWORD
checkForEEPROM(PDEVICE_EXTENSION pde)
{
	if (readSECR(pde) & EEPROM_PRESENT)
		return STATUS_SUCCESS;
	return STATUS_DEVICE_DOES_NOT_EXIST;
}


BOOL
pollFPGADone(PDEVICE_EXTENSION pde)
{
	DWORD timeoutValue = 20;
	while (!(readSECR(pde) & FPGA_PROGRAMMED)) {
		if (timeoutValue-- == 0) {
			return FALSE;
		}
		udelay(1000);
	}
	return TRUE;
}


BOOL
pollCPLD(PDEVICE_EXTENSION pde, WORD bitMask)
{
	DWORD timeoutValue = 20;
	while ((readCPLD(pde) & bitMask) != bitMask) {
		if (timeoutValue-- == 0) {
			return FALSE;
		}
		udelay(1000);
	}
	pause();
	return TRUE;
}


void
reloadPLX(PDEVICE_EXTENSION pde)
{
	writeSECR(pde, SECRReloadLo);
	pause();
	writeSECR(pde, SECRReloadHi);
	pause();
	writeSECR(pde, SECRReloadLo);
	pause();
	return ;
}


void
pulseProgPin(PDEVICE_EXTENSION pde)
{
	writeSECR(pde, SECRProgPinHi);
	pause();
	writeSECR(pde, SECRProgPinLo);
	pause();
	return ;
}


void
resetLocalBus(PDEVICE_EXTENSION pde)
{
	writeSECR(pde, SECRLocalBusHi);
	pause();
	writeSECR(pde, SECRLocalBusLo);
	pause();
	return ;
}


NTSTATUS
loadFpga(PDEVICE_EXTENSION pde)
{
    WORD fpgaSwapWord, numberOfTrys = 3;
    BOOL fpgaStartFound;
    BYTE fpgaByte;
    DWORD fileSize;
    BOOL newCpld = FALSE;
    DWORD secrReg;
    DWORD tbLocation = 0;
    secrReg = readSECR(pde);

    while (numberOfTrys--) {
	fpgaStartFound = FALSE;
	fpgaByte = 0x00;
	resetLocalBus(pde);
	reloadPLX(pde);
	pulseProgPin(pde);

	if (pollCPLD(pde, CPLD_INIT)) {

	    if ((readCPLD(pde) & 0xf000) == CPLD_SIGNATURE)
		newCpld = TRUE;

	    fileSize = TopBitSize;
	    tbLocation = 0;

	    while (!fpgaStartFound) {
		while (fpgaByte != 0xff) {
		    fpgaByte = bTopBitArray[tbLocation++];
		    if (--fileSize == 0)
			break;
		}

		if (fpgaByte != 0xff)
		    break;

		fpgaByte = bTopBitArray[tbLocation++];
		if (--fileSize == 0)
		    break;
		if (fpgaByte == 0x20)
		    fpgaStartFound = TRUE;
	    }

	    if (fpgaStartFound) {
		if (newCpld)
		    waitForCPLD(pde);

		else
		    udelay(2);

		if (writeCPLD(pde, 0xff20)) {
		    fileSize /= 2;

		    while (fileSize) {
			if (newCpld) {
			    waitForCPLD(pde);
 			}

			else {
			    udelay(2);
                        }
			fpgaSwapWord =
			    (WORD) (bTopBitArray
				    [tbLocation] << 8) +
			    (WORD) (bTopBitArray[tbLocation + 1]);

			if (!writeCPLD(pde, fpgaSwapWord)) {
			    break;
			}
			tbLocation += 2;
			fileSize--;
		    }
		    if (fileSize == 0) {
			if (pollFPGADone(pde)) {
			    resetLocalBus(pde);
			    reloadPLX(pde);
			    return STATUS_SUCCESS;
			}
		    }
		}
	    }
	}
    }
    resetLocalBus(pde);
    reloadPLX(pde);
    warn(" loadFpga ****** FAILED *******");
    return STATUS_DEVICE_CONFIGURATION_ERROR;
}
