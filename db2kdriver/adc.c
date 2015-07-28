////////////////////////////////////////////////////////////////////////////
//
// adc.c                           I    OOO                           h
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

#define ADC_HZ (HZ/100)

#include "iottypes.h"
#include "daqx.h"
#include "ddapi.h"

#define ADCFILE /* get the cfgChNum stuff */
#include "device.h"
#include "daqio.h"
#include "w32ioctl.h"
#include "adc.h"


static HW *nulldaqmap = (HW *) NULL;


VOID copyNewScans(PDEVICE_EXTENSION pde, DWORD scans, BOOL calledFromAdcEvent);
VOID setupPacerClock(PDEVICE_EXTENSION pde);
DWORD checkEventsAndMoveData(PDEVICE_EXTENSION pde);
//VOID waitXus(PDEVICE_EXTENSION pde, DWORD usCount);

NTSTATUS threadTermStatus;
VOID __stdcall adcEvent(PDEVICE_EXTENSION pde, PVOID ref);
VOID __stdcall adcEventX(PDEVICE_EXTENSION pde, PVOID ref);

static VOID adcEventL(unsigned long data);

VOID
adcScheduleAdcEvent(PDEVICE_EXTENSION pde)
{
	if (!pde->adcThreadDie) {
		pde->adcEventHandle = TRUE;

		if (!pde->adcEventHandle) {
			pde->adcXferThreadRunning = FALSE;
		}
	} else {
		pde->adcXferThreadRunning = FALSE;
	}
	return ;
}


VOID __stdcall
adcTimeoutCallback(PVOID refData, DWORD extra)
{
	PDEVICE_EXTENSION pde = (PDEVICE_EXTENSION) refData;
	pde->adcTimeoutHandle = 0;
	adcScheduleAdcEvent(pde);
	return ;
}

VOID
adcScheduleAdcEventAfterTO(PDEVICE_EXTENSION pde, DWORD msec)
{
	if (!pde->adcTimeoutHandle) {
		pde->adcTimeoutHandle = TRUE;
		if (!pde->adcTimeoutHandle) {

			pde->adcXferThreadRunning = FALSE;
		} else {

			pde->adcXferThreadRunning = TRUE;
		}
	}
	return ;
}


VOID
adcEventThread(PDEVICE_EXTENSION pde, PVOID Context)
{
	return;
}



VOID
kickstartAdcEvent(PDEVICE_EXTENSION pde)
{
    if(pde->adcXferThreadRunning == TRUE)
    {
        DWORD timeoutValue = 10000; 
        printk("KICKSTART pde->adcXferThreadRunning == TRUE\n");
        
        // shut down the adc thread
        pde->adcThreadDie = TRUE;
        
        while (pde->adcXferThreadRunning)
        {
            if(timeoutValue-- == 0)
            {                
                break;
            }            
            waitXus(pde, 10L);
        }
    }

    if(pde->adcXferThreadRunning == FALSE)
    {       

        // Set the Adc thread running flag ????
        pde->adcXferThreadRunning = TRUE;
        pde->adcThreadDie = FALSE;
        pde->inAdcEvent = FALSE;
        
        pde->adcTimeoutHandle = 1;        
        init_timer(&pde->adc_timer);
        pde->adc_timer.expires = jiffies + ADC_HZ;
        
        //printk("KICKSTART jiffies + ADC_HZ %li\n",jiffies + ADC_HZ);
        pde->adc_timer.data = (unsigned long)pde;

        pde->adc_timer.function = adcEventL;
        add_timer(&pde->adc_timer);        
    }
    else
    {
        printk("DB2K: adc: kickstartAdcEvent -> ERROR fell right through KICKSTART \n");
        pde->adcThreadDie = TRUE;
    }
}


static VOID
enterAdcStatusMutex(PDEVICE_EXTENSION pde)
{
//	if (!pde->inAdcEvent)
		spin_lock_irq(&pde->adcStatusMutex);
	return ;
}


static VOID
leaveAdcStatusMutex(PDEVICE_EXTENSION pde)
{
//	if (!pde->inAdcEvent)
		spin_unlock_irq(&pde->adcStatusMutex);
	return ;
}


VOID
adcSetWin32Event(PDEVICE_EXTENSION pde)
{
	return;
}

BOOL
initializeAdc(PDEVICE_EXTENSION pde)
{
	spin_lock_init(&pde->adcStatusMutex);

	pde->adcStatusFlags = 0;
	adcDisarm(pde, NULL);
	adcStop(pde, NULL);
	pde->userInpBufMdl = NULL;
	if (!readCalConstants(pde, DcttFactory)) {
		/* PEK: what error should go here? */
	}
	activateReferenceDacs(pde);
	initializeCtrs(pde);
	initializeTmrs(pde);

	return TRUE;
}

BOOL
uninitializeAdc(PDEVICE_EXTENSION pde)
{
	DWORD timeoutValue = 10000;

	adcDisarm(pde, NULL);
	pde->adcThreadDie = TRUE;

	while (pde->adcXferThreadRunning) {
		if (timeoutValue-- == 0) {
			break;
		}
		waitXus(pde, 100L);
	}
	return TRUE;
}


static BOOL
adcCheckStatus(PDEVICE_EXTENSION pde, DWORD mask)
{
	DWORD adcAcqStatus;

	enterAdcStatusMutex(pde);
	adcAcqStatus = pde->adcStatusFlags;
	leaveAdcStatusMutex(pde);

	return ((adcAcqStatus & mask) != 0);
}


VOID
initializeOutputs(PDEVICE_EXTENSION pde)
{
	WORD i;
	/* PEK why 4? */

	for (i = 0; i < 4; i++) {
		if (pde->counterControlImage[i]) {
			writeTmrCtrControl(pde, pde->counterControlImage[i]);
			pde->counter[i].enable = DcovCounterOn;
			if (pde->counterControlImage[i] & CounterClearOnRead) {
				pde->counter[i].clearOnRead = DcovCounterClearOnRead;
			} else {
				pde->counter[i].clearOnRead = DcovCounterTotalize;
			}
		}
	}
	return ;
}

PDWORD
adcCheckDescriptorBlocks(PDEVICE_EXTENSION pde)
{
	WORD i, lastValidBlock = 0xffff;
	BYTE *ptrA;
	BYTE *ptrB;
	PDWORD firstBlockAddr = 0L;

	for (i = 0; i < INPCHAINSIZE; i++) {

		ptrA = (PBYTE) virt_to_phys(&pde->dmaInpChain[i]);
		ptrA += 0x0000000fL;
		ptrB = (PBYTE) virt_to_phys(((BYTE *) & pde->dmaInpChain[i] + 0x0000000fL));

		if (ptrA == ptrB) {
			if (firstBlockAddr == 0L) {
				firstBlockAddr = (PDWORD) ptrA - 0x0000000fL;
			} else {
				pde->dmaInpChain[lastValidBlock].descriptorPointer = (PDWORD) ptrA - 0x0000000fL;
			}
			lastValidBlock = i;
		} else {
                       pde->dmaInpChain[i].descriptorPointer = (PDWORD) 0xffffffffL;
		}
	}

	return firstBlockAddr;
}


PDWORD
adcSetupChainDescriptors(PDEVICE_EXTENSION pde)
{
	PWORD bufPhyAddr;
	PWORD bufSysAddr = (PWORD) pde->dmaInpBuf;
	ULONG blockSize, firstBlockSize, lastBlockSize;
	DWORD bufSize = DMAINPBUFSIZE * 2L;
	DWORD bufCount = 0;
	WORD descIndex = 0;
	PDWORD firstBlockAddr;
        ULONG tmp;

	firstBlockAddr = adcCheckDescriptorBlocks(pde);
	bufPhyAddr = (PWORD) virt_to_phys((PVOID) bufSysAddr);

	firstBlockSize = blockSize = (ULONG) (0x00001000L - ((ULONG) (bufPhyAddr) & 0x00000fff ) );
	lastBlockSize = 0x00001000L - firstBlockSize;

	while (pde->dmaInpChain[descIndex].descriptorPointer ==(PDWORD) 0xffffffffL)
		descIndex++;

	while (TRUE) {
          pde->dmaInpChain[descIndex].pciAddr = (PDWORD)  virt_to_phys(bufSysAddr);
		pde->dmaInpChain[descIndex].localAddr = (PDWORD) (&nulldaqmap->acqResultsFIFO);
		pde->dmaInpChain[descIndex].transferByteCount = blockSize;
                tmp = (ULONG) pde->dmaInpChain[descIndex].descriptorPointer ;
                tmp = (tmp & 0xfffffff0L) | 0x00000009L;
		pde->dmaInpChain[descIndex].descriptorPointer = (PDWORD) tmp;

		bufCount += blockSize;
		bufSysAddr += (blockSize / 2);
		if (bufCount >= bufSize) {
                  tmp = (ULONG) firstBlockAddr;
                  tmp = ( tmp & 0xfffffff0L) | 0x00000009L;
                  pde->dmaInpChain[descIndex].descriptorPointer = (PDWORD) tmp;

			if (bufCount > bufSize)
				pde->dmaInpChain[descIndex].transferByteCount = lastBlockSize;
			break;
		}
		blockSize = 0x1000;
		descIndex++;
		while (pde->dmaInpChain[descIndex].descriptorPointer == (PDWORD)0xffffffffL)
			descIndex++;
	}
	return firstBlockAddr;
}


VOID
adcSetupDmaTransfer(PDEVICE_EXTENSION pde)
{
	BYTE *base9080;
	PDWORD firstBlockAddr;

	base9080 = (BYTE *) (pde->plxVirtualAddress);
	/* This is a configuration word, 
	 * see the 9080 manual DMAMODE1 register PCI:0x94
	 */
	*(DWORD *) (base9080 + PCI9080_DMA1_MODE) = 0x00021ac1L;

	firstBlockAddr = adcSetupChainDescriptors(pde);

	dbg("PLX W %2lX %2lX %8X PCI9080_DMA1_MODE", PCI9080_DMA1_MODE, 0x00021ac1L, 0);

	/* address in PCI (bus) space the transfers start, DMAPADR1, PCI:0x98*/
	*(DWORD *) (base9080 + PCI9080_DMA1_PCI_ADDR) = 0x00000000L;
	dbg("PLX W %2lx %2lx %8x PCI9080_DMA1_PCI_ADDR",
		 PCI9080_DMA1_PCI_ADDR, 0x0L, 0);

	/* Where in local memory the transfer starts, DMALADR1, PCI:0x9c */
	*(DWORD *) (base9080 + PCI9080_DMA1_LOCAL_ADDR) = 0x00000000L;
	dbg("PLX W %2lx %2lx %8x PCI9080_DMA1_LOCAL_ADDR",
		 PCI9080_DMA1_LOCAL_ADDR, 0x0L, 0);

	/* number of _bytes_ to transfer, DMASIZ1, PCI:0xa0 */
	*(DWORD *) (base9080 + PCI9080_DMA1_COUNT) = 0x00000000L;
	dbg("PLX W %2lx %2lx %8x PCI9080_DMA1_COUNT", PCI9080_DMA1_COUNT, 0x0L, 0);

	/* description point register, DMADPR1, PCI:0xa4 
	 * DWORD aligned byte address plus 4 config bits
	 */
	*(DWORD *) (base9080 + PCI9080_DMA1_DESC_PTR) =
          ((ULONG) firstBlockAddr & 0xfffffff0L) | 0x00000009L;
	dbg("PLX W %2lx %2lx %8x PCI9080_DMA1_DESC_PTR", PCI9080_DMA1_DESC_PTR,
            ((ULONG) firstBlockAddr & 0xfffffff0L) | 0x00000009L, 0);

	/*threshold DMATHR, PCI:0xb2*/
	*(WORD *) (base9080 + PCI9080_DMA1_THRESHOLD) = 0x0000;
	dbg("PLX W %2lx %2lx %8x PCI9080_DMA1_THRESHOLD",
		 PCI9080_DMA1_THRESHOLD, 0x0L, 0);
	return ;
}


VOID
adcStartDmaTransfer(PDEVICE_EXTENSION pde)
{     

	/* command status register, DMACSR1, PCI:0xa9 */
	/* enable, start, and clear interrupt */
	*(BYTE *) ((BYTE *) pde->plxVirtualAddress + PCI9080_DMA1_CMD_STATUS) = 0x0b;
	dbg("PLX W %2lx %2lx %8x PCI9080_DMA1_CMD_STATUS", PCI9080_DMA1_CMD_STATUS, 0x0bL, 0);
	writeDmaControl(pde, DmaCh1Enable);
	pde->dmaInpActive = TRUE;
	return ;
}


VOID
adcStopDmaTransfer(PDEVICE_EXTENSION pde)
{    
	BYTE dmaStatus;
	BYTE *base9080;

	DWORD timeoutCount, timeoutCountVal = 2000;
	writeDmaControl(pde, DmaCh1Disable);
	base9080 = (BYTE *) (pde->plxVirtualAddress);

	dmaStatus = *(base9080 + PCI9080_DMA1_CMD_STATUS);
	dbg("PLX R %2lx %2x %8x PCI9080_DMA1_CMD_STATUS",
		 PCI9080_DMA1_CMD_STATUS, dmaStatus, 0);

	/* if DMA enabled */
	if ((dmaStatus & 0x01) == 0x01) {

		timeoutCount = timeoutCountVal;
		/*?? channel 1 finished with the transfer ??*/
		while (dmaStatus & 0x10) {
			
			/* Is this not better handled by timers? 
			 * Why the spinning on thins?
			 */
			dmaStatus = *(base9080 + PCI9080_DMA1_CMD_STATUS);
			dbg("PLX R %2lx %2x %8x PCI9080_DMA1_CMD_STATUS",
				 PCI9080_DMA1_CMD_STATUS, dmaStatus, 0);

			if (!timeoutCount--) {
				break;
			}
		}


		/* disable DMA, keep all other bits intact */
		*(base9080 + PCI9080_DMA1_CMD_STATUS) = (dmaStatus & 0xfe);
		dbg("PLX W %2lx %2x %8x PCI9080_DMA1_CMD_STATUS",
			 PCI9080_DMA1_CMD_STATUS, dmaStatus & 0xfe, 0);

		/* get the status back */
		dmaStatus = *(base9080 + PCI9080_DMA1_CMD_STATUS);
		dbg("PLX R %2lx %2x %8x PCI9080_DMA1_CMD_STATUS",
			 PCI9080_DMA1_CMD_STATUS, dmaStatus, 0);

		/* set the abort flag */
		*(base9080 + PCI9080_DMA1_CMD_STATUS) = ((dmaStatus & 0xfe) | 0x04);
		dbg("PLX W %2lx %2x %8x PCI9080_DMA1_CMD_STATUS",
			 PCI9080_DMA1_CMD_STATUS, (dmaStatus & 0xfe) | 0x04, 0);

		timeoutCount = timeoutCountVal;
		while ((dmaStatus & 0x10) == 0x00) {
			/* wait until the done flag shows up */
			dmaStatus = *(base9080 + PCI9080_DMA1_CMD_STATUS);
			dbg("PLX R %2lx %2x %8x PCI9080_DMA1_CMD_STATUS",
				 PCI9080_DMA1_CMD_STATUS, dmaStatus, 0);

			if (!timeoutCount--) {
				break;
			}
		}
	}
	pde->dmaInpActive = FALSE;
	return ;
}

DaqError
adcTestConfiguration(PDEVICE_EXTENSION pde, ADC_CONFIG_PT adcConfig)
{
	WORD thisChannel;
	DWORD chNum, locGain, flags, settlingTime = 0L;
	BOOL p2LocDefined = FALSE, p2ExpDefined = FALSE;
	BOOL ctr0and2Single = FALSE, ctr0and2Cascaded = FALSE;
	BOOL ctr1and3Single = FALSE, ctr1and3Cascaded = FALSE;
	DWORD minPeriodmsec, minPeriodnsec;
	BOOL scanPeriodTooSmall = FALSE;

	if ((adcConfig->adcScanLength == 0L)
	    || (adcConfig->adcScanLength > 512)){
		dbg("adcScanLength out of bounds: 0 < %d <= 512 required",
		    adcConfig->adcScanLength);
		return DerrInvCount;
	}
    
    
	for (thisChannel = 0; thisChannel < adcConfig->adcScanLength; thisChannel++) {

		chNum = adcConfig->adcScanSeq[thisChannel].channel;
		flags = adcConfig->adcScanSeq[thisChannel].flags;

		settlingTime++;
		if (flags & DafSettle10us)
			settlingTime++;

		if (!(flags & DafScanDigital)) {

			if (!pde->usingAnalogInputs)
				return DerrNotCapable;

			if (chNum > 271L){
				dbg("chNum = %i > 271",chNum);
				return DerrInvChan;
			}

			locGain = (WORD) adcConfig->adcScanSeq[thisChannel].gain;
			if (chNum > 15L)
				locGain = (WORD) adcConfig->adcScanSeq[thisChannel].gain >> 2;

			if (locGain > DgainX64){
				dbg("gain too big");
				return DerrInvGain;
			}            
            
            dbg("gain X1 when not Bipolar mode");
			if ((locGain == DgainX1) && !(flags & DafBipolar)){
				dbg("gain X1 when not Bipolar mode");
				return DerrInvGain;
			}

			if ((chNum > 7L) && (flags & DafDifferential)){
				dbg("channel > 7 when in Differential mode");
				return DerrInvAdcRange;
			}

		} else {

			switch (flags & 0x0000ff01L) {
			case DafP3Local16:
				if (!pde->usingDigitalIO)
					return DerrNotCapable;
				break;

			case DafCtr16:
				if (!pde->usingCounterTimers)
					return DerrNotCapable;

				switch (chNum) {
				case 0:
				case 2:
					if (ctr0and2Cascaded)
						return DerrParamConflict;
					ctr0and2Single = TRUE;
					break;

				case 1:
				case 3:
					if (ctr1and3Cascaded)
						return DerrParamConflict;
					ctr1and3Single = TRUE;
					break;

				default:
					return DerrInvChan;
				}
				break;

			case DafCtr32Low:
			case DafCtr32High:
				if (!pde->usingCounterTimers)
					return DerrNotCapable;

				switch (chNum) {
				case 0:
				case 2:
					if (ctr0and2Single)
						return DerrParamConflict;
					ctr0and2Cascaded = TRUE;
					break;

				case 1:
				case 3:
					if (ctr1and3Single)
						return DerrParamConflict;
					ctr1and3Cascaded = TRUE;
					break;
				default:
					return DerrInvChan;
				}
				break;

			case DafP2Local8:
				if (!pde->usingDigitalIO)
					return DerrNotCapable;
				if (p2ExpDefined)
					return DerrParamConflict;
				if (chNum > 3L)
					return DerrInvChan;
				p2LocDefined = TRUE;
				break;

			case DafP2Exp8:
				if (!pde->usingDigitalIO)
					return DerrNotCapable;
				if (p2LocDefined)
					return DerrParamConflict;
				if (chNum > 31L)
					return DerrInvChan;
				p2ExpDefined = TRUE;
				break;
			default:
				return DerrInvDioDeviceType;
			}
		}
	}

	if (adcConfig->adcAcqPreCount > 100000L){
		dbg("adcAcqPreCount out of bounds: %d < 100000 required",
		    adcConfig->adcAcqPreCount);
		return DerrInvCount;
	}

	if (adcConfig->adcAcqMode != DaamInfinitePost) {
		if (adcConfig->adcAcqPostCount < adcConfig->adcScanLength){
			dbg("adcAcqPostCount out of bounds: %d < adcScanLength(=%d)",
			    adcConfig->adcAcqPostCount, adcConfig->adcScanLength);
			return DerrInvCount;
		}
	}

	switch (adcConfig->triggerEventList[0].TrigCount) {
	case 0:
		pde->trig[TRIGGER_EVENT].event = adcConfig->triggerEventList[0];
		pde->trig[STOP_EVENT].event.TrigSource = DatsScanCount;
		break;

	case 1:
		pde->trig[TRIGGER_EVENT].event = adcConfig->triggerEventList[1];
		pde->trig[STOP_EVENT].event.TrigSource = DatsScanCount;
		break;

	case 2:
		pde->trig[TRIGGER_EVENT].event = adcConfig->triggerEventList[1];
		pde->trig[STOP_EVENT].event = adcConfig->triggerEventList[2];
		break;

	default:
		return DerrInvTrigEvent;
	}

	switch (pde->trig[TRIGGER_EVENT].event.TrigSource) {
	case DatsImmediate:
		if (adcConfig->adcAcqPreCount){
			dbg("adcAcqPreCount nonzero: %d",
			    adcConfig->adcAcqPreCount);
			return DerrInvCount;
		}
		break;

	case DatsSoftware:
		break;

	case DatsExternalTTL:
		if (adcConfig->adcAcqPreCount){
			dbg("adcAcqPreCount nonzero: %d",
			    adcConfig->adcAcqPreCount);
			return DerrInvCount;
		}
		switch (pde->trig[TRIGGER_EVENT].event.TrigSens) {
		case DetsAboveLevel:
		case DetsBelowLevel:
		case DetsRisingEdge:
		case DetsFallingEdge:
			break;

		default:
			return DerrInvTrigSense;
		}
		break;

	case DatsHardwareAnalog:

		if (adcConfig->adcAcqPreCount){
			dbg("adcAcqPreCount nonzero: %d",
			    adcConfig->adcAcqPreCount);
			return DerrInvCount;
		}
		switch (pde->trig[TRIGGER_EVENT].event.TrigSens) {
		case DetsAboveLevel:
		case DetsBelowLevel:
			break;

		case DetsRisingEdge:
			if (pde->trig[TRIGGER_EVENT].event.TrigLevel <
			    pde->trig[TRIGGER_EVENT].event.TrigVariance)
				return DerrInvLevel;
			break;

		case DetsFallingEdge:
			if (pde->trig[TRIGGER_EVENT].event.TrigLevel >
			    pde->trig[TRIGGER_EVENT].event.TrigVariance)
				return DerrInvLevel;
			break;

		default:
			return DerrInvTrigSense;
		}
		break;

	case DatsDigPattern:
		if (pde->trig[TRIGGER_EVENT].event.TrigLocation > adcConfig->adcScanLength)
			return DerrInvTrigChannel;
		switch (pde->trig[TRIGGER_EVENT].event.TrigSens) {
		case DetsAboveLevel:
		case DetsBelowLevel:
		case DetsEQLevel:
		case DetsNELevel:
			break;

		default:
			return DerrInvTrigSense;
		}
		break;

	case DatsSoftwareAnalog:
		if (pde->trig[TRIGGER_EVENT].event.TrigLocation > adcConfig->adcScanLength)
			return DerrInvTrigChannel;

		switch (pde->trig[TRIGGER_EVENT].event.TrigSens) {
		case DetsAboveLevel:
		case DetsBelowLevel:
		case DetsEQLevel:
		case DetsNELevel:
			break;

		case DetsRisingEdge:
			if (pde->trig[TRIGGER_EVENT].event.Trigflags & DafSigned) {
				if ((short) pde->trig[TRIGGER_EVENT].event.
				    TrigLevel < (short) pde->trig[TRIGGER_EVENT].event.TrigVariance)
					return DerrInvLevel;
			} else {
				if (pde->trig[TRIGGER_EVENT].event.TrigLevel <
				    pde->trig[TRIGGER_EVENT].event.TrigVariance)
					return DerrInvLevel;
			}
			break;

		case DetsFallingEdge:
			if (pde->trig[TRIGGER_EVENT].event.Trigflags & DafSigned) {
				if ((short) pde->trig[TRIGGER_EVENT].event.
				    TrigLevel > (short) pde->trig[TRIGGER_EVENT].event.TrigVariance)
					return DerrInvLevel;
			} else {
				if (pde->trig[TRIGGER_EVENT].event.TrigLevel >
				    pde->trig[TRIGGER_EVENT].event.TrigVariance)
					return DerrInvLevel;
			}
			break;

		default:
			return DerrInvTrigSense;
		}
		break;

	default:
		return DerrInvTrigSource;
	}

	switch (pde->trig[STOP_EVENT].event.TrigSource) {

	case DatsSoftware:
	case DatsScanCount:
		break;

	case DatsDigPattern:

		if (pde->trig[STOP_EVENT].event.TrigLocation > adcConfig->adcScanLength)
			return DerrInvTrigChannel;

		switch (pde->trig[STOP_EVENT].event.TrigSens) {
		case DetsAboveLevel:
		case DetsBelowLevel:
		case DetsEQLevel:
		case DetsNELevel:
			break;

		default:
			return DerrInvTrigSense;
		}
		break;

	case DatsSoftwareAnalog:

		if (pde->trig[STOP_EVENT].event.TrigLocation > adcConfig->adcScanLength)
			return DerrInvTrigChannel;
		switch (pde->trig[STOP_EVENT].event.TrigSens) {
		case DetsAboveLevel:
		case DetsBelowLevel:
		case DetsEQLevel:
		case DetsNELevel:
			break;

		case DetsRisingEdge:
			if (pde->trig[STOP_EVENT].event.Trigflags & DafSigned) {
				if ((short) pde->trig[STOP_EVENT].event.
				    TrigLevel < (short) pde->trig[STOP_EVENT].event.TrigVariance)
					return DerrInvLevel;
			} else {
				if (pde->trig[STOP_EVENT].event.TrigLevel <
				    pde->trig[STOP_EVENT].event.TrigVariance)
					return DerrInvLevel;
			}
			break;

		case DetsFallingEdge:

			if (pde->trig[STOP_EVENT].event.Trigflags & DafSigned) {
				if ((short) pde->trig[STOP_EVENT].event.
				    TrigLevel > (short) pde->trig[STOP_EVENT].event.TrigVariance)
					return DerrInvLevel;
			} else {
				if (pde->trig[STOP_EVENT].event.TrigLevel >
				    pde->trig[STOP_EVENT].event.TrigVariance)
					return DerrInvLevel;
			}
			break;

		default:
			return DerrInvTrigSense;
		}
		break;

	default:
		return DerrInvTrigSource;
	}

	switch (adcConfig->adcClockSource & 0x000000ffL) {
	case DacsExternalTTL:
		break;

	case DacsAdcClock:
		settlingTime *= 5L;
		break;

	case DacsAdcClockDiv2:
		settlingTime = adcConfig->adcScanLength * 10L;
		break;

	default:
		return DerrInvClockSource;
	}

	if (adcConfig->adcPeriodmsec > 68719476L)
		return DerrInvFreq;
	if ((adcConfig->adcPeriodmsec == 68719476L) && (adcConfig->adcPeriodnsec > 740000L))
		return DerrInvFreq;

	if ((adcConfig->adcPeriodmsec == 0L) && (adcConfig->adcPeriodnsec < 5000L))
		return DerrInvFreq;

	adcConfig->adcPeriodnsec = (adcConfig->adcPeriodnsec / 1000L) * 1000L;
	minPeriodmsec = settlingTime / 1000L;
	minPeriodnsec = (settlingTime % 1000L) * 1000L;
	if (adcConfig->adcPeriodmsec < minPeriodmsec)
		scanPeriodTooSmall = TRUE;
	else if ((adcConfig->adcPeriodmsec == minPeriodmsec) &&
		 (adcConfig->adcPeriodnsec < minPeriodnsec))
		scanPeriodTooSmall = TRUE;

	if (scanPeriodTooSmall) {
		adcConfig->adcPeriodmsec = minPeriodmsec;
		adcConfig->adcPeriodnsec = minPeriodnsec;
	}
	return DerrNoError;
}



static VOID
adcSetScanSeq(PDEVICE_EXTENSION pde, ADC_CONFIG_PT adcConfig)
{
	WORD locGain, expGain, thisChannel;
	WORD word0=0, word1=0, word2=0, word3=0;
	WORD lastAnalogWord0, lastAnalogWord1, lastAnalogWord2, lastAnalogWord3;
	WORD digChanNum, locCh, expCh, biIndex;
	WORD correctionGain, correctionOffset;
	DWORD flags;
	WORD clearOnRead;

	writeAcqControl(pde, AcqResetScanListFifo);

	lastAnalogWord0 = 0x0000;
	lastAnalogWord1 = 0x0000;
	lastAnalogWord2 = 0x0800;
	lastAnalogWord3 = 0xc001;

	/* PEK: again , why 4 */

	for (thisChannel = 0; thisChannel < 4; thisChannel++) {
		pde->counterControlImage[thisChannel] = 0;
	}

	for (thisChannel = 0; thisChannel < adcConfig->adcScanLength; thisChannel++) {

		flags = adcConfig->adcScanSeq[thisChannel].flags;

		if (!(flags & DafScanDigital)) {

			locCh = (WORD) (adcConfig->adcScanSeq[thisChannel].channel & 0x000001ff);
			expCh = 0;
			locGain = (WORD) adcConfig->adcScanSeq[thisChannel].gain & 0x07;
			expGain = 0;

			if (locCh > 15) {
				expCh = locCh & 0x0f;
				locCh = (locCh >> 4) - 1;
				locGain =
				    ((WORD) adcConfig->adcScanSeq[thisChannel].gain & 0x1f) >> 2;
				expGain = ((WORD) adcConfig->adcScanSeq[thisChannel].gain & 0x03);
			}

			if (flags & DafDifferential) {
				word0 = cfgChNum[locCh][0];
				word1 = cfgChNum[locCh][1];
				word2 = cfgChNum[locCh][2];
				word3 = cfgChNum[locCh][3];
			} else {
				word0 = cfgChNum[locCh + 8][0];
				word1 = cfgChNum[locCh + 8][1];
				word2 = cfgChNum[locCh + 8][2];
				word3 = cfgChNum[locCh + 8][3];
			}

			word2 |= expCh;

			biIndex = 1;

			if (!(flags & DafBipolar)) {
				word3 |= UNI;
				biIndex--;
			}

			word3 |= cfgChGain[locGain];
			word2 |= (expGain << 4);

			if (flags & DafDifferential) {
				correctionGain =
				    pde->eeprom.correctionDACDE[locCh][locGain][biIndex].gain;
				correctionOffset =
				    pde->eeprom.correctionDACDE[locCh][locGain][biIndex].offset;
			} else {
				correctionGain =
				    pde->eeprom.correctionDACSE[locCh][locGain][biIndex].gain;
				correctionOffset =
				    pde->eeprom.correctionDACSE[locCh][locGain][biIndex].offset;
			}

			word2 |= (correctionGain << 12);
			word3 |= ((correctionGain << 4) & 0xff00);
			word1 |= (correctionOffset << 8);
			word2 |= (correctionOffset & 0x0f00);

			lastAnalogWord0 = word0;
			lastAnalogWord1 = word1;
			lastAnalogWord2 = word2;
			lastAnalogWord3 = word3;
		} else {

			digChanNum = (WORD) (adcConfig->adcScanSeq[thisChannel].channel);

			if (flags & DafCtrTotalize)
				clearOnRead = 0;
			else
				clearOnRead = CounterClearOnRead;

			switch (flags & 0x0000ff01L) {
			case DafP3Local16:
                                 word2 = (WORD)  ((ULONG) (&nulldaqmap->dioP3hsioData)) & 0xffff; 
				break;

			case DafCtr16:

				switch (digChanNum) {
				case 0:
					pde->counterControlImage[0] =
					    Counter0Enable + clearOnRead + CounterClearOnAcqStart;

					if (!(flags & DafCtrFallingEdge))
						pde->counterControlImage[0] |= CountRisingEdges;

					writeTmrCtrControl(pde, Counter0CascadeDisable);

					pde->counter[0].cascade =
					    pde->counter[2].cascade = DcovCounterSingle;
					break;

				case 1:
					pde->counterControlImage[1] =
					    Counter1Enable + clearOnRead + CounterClearOnAcqStart;

					if (!(flags & DafCtrFallingEdge))
						pde->counterControlImage[1] |= CountRisingEdges;

					writeTmrCtrControl(pde, Counter1CascadeDisable);

					pde->counter[1].cascade =
					    pde->counter[3].cascade = DcovCounterSingle;
					break;

				case 2:
					pde->counterControlImage[2] =
					    Counter2Enable + clearOnRead + CounterClearOnAcqStart;

					if (!(flags & DafCtrFallingEdge))
						pde->counterControlImage[2] |= CountRisingEdges;

					writeTmrCtrControl(pde, Counter0CascadeDisable);

					pde->counter[0].cascade =
					    pde->counter[2].cascade = DcovCounterSingle;
					break;

				case 3:
					pde->counterControlImage[3] =
					    Counter3Enable + clearOnRead + CounterClearOnAcqStart;

					if (!(flags & DafCtrFallingEdge))
						pde->counterControlImage[3] |= CountRisingEdges;

					writeTmrCtrControl(pde, Counter1CascadeDisable);

					pde->counter[1].cascade =
					    pde->counter[3].cascade = DcovCounterSingle;
					break;
				}
				word2 = (WORD) (ULONG) (&nulldaqmap->ctrInput[digChanNum]);
				break;

			case DafCtr32Low:
			case DafCtr32High:

				switch (digChanNum) {
				case 0:
				case 2:
					pde->counterControlImage[0] =
					    Counter0Enable + clearOnRead + CounterClearOnAcqStart;

					if (!(flags & DafCtrFallingEdge))
						pde->counterControlImage[0] |= CountRisingEdges;

					writeTmrCtrControl(pde, Counter0CascadeEnable);

					pde->counter[0].cascade =
					    pde->counter[2].cascade = DcovCounterCascade;
					break;

				case 1:
				case 3:
					pde->counterControlImage[1] =
					    Counter1Enable + clearOnRead + CounterClearOnAcqStart;

					if (!(flags & DafCtrFallingEdge))
						pde->counterControlImage[1] |= CountRisingEdges;

					writeTmrCtrControl(pde, Counter1CascadeEnable);

					pde->counter[1].cascade =
					    pde->counter[3].cascade = DcovCounterCascade;
					break;
				}
				digChanNum &= 0x0001;
				if ((flags & 0x0000ff01L) == DafCtr32High)
					digChanNum |= 0x0002;

				word2 = (WORD) (ULONG) (&nulldaqmap->ctrInput[digChanNum]);
				break;

			case DafP2Local8:
				writeDioControl(pde, p2Is82c55);
				word2 = (WORD) (ULONG) (&nulldaqmap->dioP28255[digChanNum]);
				break;

			case DafP2Exp8:
				writeDioControl(pde, p2IsExpansionPort);
				word2 =
				    (WORD) (ULONG) (&nulldaqmap->
						    dioP2ExpansionIO8Bit[digChanNum]);
				break;
			}

			word0 = lastAnalogWord0;
			word1 = lastAnalogWord1 | DIGIN;
			word2 = (word2 & 0x00ff) | (lastAnalogWord2 & 0xff00);
			word3 = lastAnalogWord3;
		}

		if (flags & DafSSHHold)
			word1 |= SSH;

		if ((thisChannel + 1) == adcConfig->adcScanLength)
			word1 |= LASTCONFIG;

		if (flags & DafSigned)
			word1 |= SIGNED;

		if (flags & DafSettle10us)
			word1 |= COMP;

		if (flags & DafIgnoreType)
			word3 |= 0x0044;

		dbg("Seq Loc=%d: Word1=%04x: Word2=%04x: Word3=%04x", 
		    thisChannel, word1, word2, word3);

		writeAcqScanListEntry(pde, word0);
		writeAcqScanListEntry(pde, word1);
		writeAcqScanListEntry(pde, word2);
		writeAcqScanListEntry(pde, word3);
	}
	return ;
}

static VOID
adcSetTrig(PDEVICE_EXTENSION pde, ADC_CONFIG_PT adcConfig)
{
	DWORD level, variance;
	DWORD tOffset, tGain, hOffset, hGain;
	WORD levelIndex=0;

	pde->trig[TRIGGER_EVENT].count = adcConfig->adcAcqPreCount / pde->scanSize;
	pde->trig[STOP_EVENT].count = adcConfig->adcAcqPostCount / pde->scanSize;

	pde->trig[TRIGGER_EVENT].countValue = pde->trig[TRIGGER_EVENT].count;
	pde->trig[STOP_EVENT].countValue = pde->trig[STOP_EVENT].count;

	switch (adcConfig->triggerEventList[0].TrigCount) {
	case 0:
		pde->trig[TRIGGER_EVENT].event = adcConfig->triggerEventList[0];
		pde->trig[STOP_EVENT].event.TrigSource = DatsScanCount;
		break;

	case 1:
		pde->trig[TRIGGER_EVENT].event = adcConfig->triggerEventList[1];
		pde->trig[STOP_EVENT].event.TrigSource = DatsScanCount;
		break;

	case 2:
		pde->trig[TRIGGER_EVENT].event = adcConfig->triggerEventList[1];
		pde->trig[STOP_EVENT].event = adcConfig->triggerEventList[2];
		break;

	default:
		adcConfig->errorCode = DerrInvTrigEvent;
		return;
	}

	if (adcConfig->adcAcqMode == DaamInfinitePost) {
		pde->trig[STOP_EVENT].event.TrigSource = 0xffffffff;
	}

	pde->trig[TRIGGER_EVENT].trigControl = 0;

	if ((pde->trig[TRIGGER_EVENT].event.TrigSource == DatsExternalTTL) ||
	    (pde->trig[TRIGGER_EVENT].event.TrigSource == DatsHardwareAnalog)) {
		switch (pde->trig[TRIGGER_EVENT].event.TrigSens) {
		case DetsAboveLevel:
			pde->trig[TRIGGER_EVENT].trigControl = TrigAbove | TrigLevelSense | TrigEnable;
			levelIndex = AboveIndex;
			break;

		case DetsBelowLevel:
			pde->trig[TRIGGER_EVENT].trigControl = TrigBelow | TrigLevelSense | TrigEnable;
			levelIndex = BelowIndex;
			break;

		case DetsRisingEdge:
			pde->trig[TRIGGER_EVENT].trigControl = TrigTransLoHi | TrigEdgeSense | TrigEnable;
			levelIndex = AboveIndex;
			break;

		case DetsFallingEdge:
			pde->trig[TRIGGER_EVENT].trigControl =TrigTransHiLo | TrigEdgeSense | TrigEnable;
			levelIndex = BelowIndex;
			break;

		default:
			adcConfig->errorCode = DerrInvTrigSense;
			return;
		}
	}

	switch (pde->trig[TRIGGER_EVENT].event.TrigSource) {
	case DatsImmediate:
		pde->trig[TRIGGER_EVENT].count = 0L;
		break;

	case DatsExternalTTL:
		pde->trig[TRIGGER_EVENT].count = 0L;
		pde->trig[TRIGGER_EVENT].trigControl |= TrigTTL;
		break;

	case DatsHardwareAnalog:
		pde->trig[TRIGGER_EVENT].count = 0L;
		pde->trig[TRIGGER_EVENT].trigControl |= TrigAnalog;
		level = (DWORD) pde->trig[TRIGGER_EVENT].event.TrigLevel;
		variance = (DWORD) pde->trig[TRIGGER_EVENT].event.TrigVariance;
		tOffset = (DWORD) pde->eeprom.trigThresholdDAC[levelIndex].offset;
		/* PEK: extra casts? */
		tGain = (DWORD) ((DWORD) pde->eeprom.trigThresholdDAC[levelIndex].gain - tOffset + 1L);
		hOffset = (DWORD) pde->eeprom.trigHysteresisDAC[levelIndex].offset;
		hGain = (DWORD) ((DWORD) pde->eeprom.trigHysteresisDAC[levelIndex].gain - hOffset + 1L);

		level = (((level * tGain) + 32768L) / 65536L) + tOffset;
		variance = (((variance * hGain) + 32768L) / 65536L) + hOffset;

		pde->trig[TRIGGER_EVENT].event.TrigLevel = level;
		pde->trig[TRIGGER_EVENT].event.TrigVariance = variance;
		break;
	}
	return;
}

void
setupStartEvent(PDEVICE_EXTENSION pde)
{

	pde->eventExpected = TRIGGER_EVENT;
	pde->trig[TRIGGER_EVENT].hystSatisfied = FALSE;
	pde->trig[STOP_EVENT].hystSatisfied = FALSE;
	pde->trig[TRIGGER_EVENT].swEventOccurred = FALSE;
	pde->trig[STOP_EVENT].swEventOccurred = FALSE;
	pde->trig[TRIGGER_EVENT].hwEventOccurred = FALSE;
	pde->trig[STOP_EVENT].hwEventOccurred = FALSE;

	pde->eventSearchPtr = pde->dmaInpBuf + (pde->trig[TRIGGER_EVENT].countValue * pde->scanSize)
	    + pde->trig[TRIGGER_EVENT].event.TrigLocation;

	if (pde->trig[TRIGGER_EVENT].event.TrigSource == DatsHardwareAnalog) {

		writeTrigHysteresis(pde, (WORD) pde->trig[TRIGGER_EVENT].event.TrigVariance);
		writeTrigThreshold(pde, (WORD) pde->trig[TRIGGER_EVENT].event.TrigLevel);
	}
	return;
}

void
setupStopEvent(PDEVICE_EXTENSION pde)
{

	pde->eventExpected = STOP_EVENT;
	pde->eventSearchPtr = pde->dmaInpBuf +
	    ((((pde->trig[TRIGGER_EVENT].eventScan + pde->trig[STOP_EVENT].count - 1L)
	       * pde->scanSize) + pde->trig[STOP_EVENT].event.TrigLocation) % DMAINPBUFSIZE);

	enterAdcStatusMutex(pde);
	pde->adcStatusFlags |= DaafAcqTriggered;
	leaveAdcStatusMutex(pde);

	writeTrigControl(pde, TrigAnalog | TrigDisable);
	writeTrigControl(pde, TrigTTL | TrigDisable);
	return;
}


void
setupPostStopEvent(PDEVICE_EXTENSION pde)
{
	pde->eventExpected = POSTSTOP_EVENT;
	adcStopDmaTransfer(pde);
	return;
}

VOID
adcGetStatus(PDEVICE_EXTENSION pde, daqIOTP p)
{
	ADC_STATUS_PT adcStatus = (ADC_STATUS_PT) p;
	enterAdcStatusMutex(pde);
	adcStatus->errorCode = pde->currentAdcErrorCode;
	pde->currentAdcErrorCode = DerrNoError;

	adcStatus->adcAcqStatus = pde->adcStatusFlags;
	adcStatus->adcXferCount = pde->scansInUserBuffer * pde->scanSize;    
	leaveAdcStatusMutex(pde);

	return;
}


VOID
waitForStatusChg(PDEVICE_EXTENSION pde, daqIOTP p)
{
	return;
}

VOID
adcDevDspCmds(PDEVICE_EXTENSION pde, daqIOTP p)
{
	DEV_DSP_CMDS_PT options = (DEV_DSP_CMDS_PT) p;
	WORD locCh, locGain, biIndex, diffIndex;

	locCh = (WORD) (options->memType >> 16);
	locGain = (WORD) (options->memType & 0x0000001f);

	if (locCh > 15) {
		locCh = (locCh >> 4) - 1;
		locGain = locGain >> 2;
	}

	biIndex = (WORD) (options->addr >> 16);
	diffIndex = (WORD) (options->addr & 0x0000ffff);

	switch (options->action) {
	case 0:
		if (diffIndex)
			pde->eeprom.correctionDACDE[locCh][locGain][biIndex].
			    offset = (WORD) (options->data);
		else
			pde->eeprom.correctionDACSE[locCh][locGain][biIndex].
			    offset = (WORD) (options->data);
		break;

	case 1:
		if (diffIndex)
			pde->eeprom.correctionDACDE[locCh][locGain][biIndex].
			    gain = (WORD) options->data;
		else
			pde->eeprom.correctionDACSE[locCh][locGain][biIndex].
			    gain = (WORD) options->data;
		break;

	case 2:
		pde->eeprom.negRef = (BYTE) (options->data);
		writeNegRefDac(pde, pde->eeprom.negRef);
		break;

	case 3:
		pde->eeprom.posRef = (BYTE) (options->data);
		writePosRefDac(pde, pde->eeprom.negRef);
		break;
	}

	return;
}


VOID
adcSoftTrig(PDEVICE_EXTENSION pde, daqIOTP p)
{
	if (pde->trig[pde->eventExpected].count == 0L) {
		pde->trig[pde->eventExpected].eventScan = readAcqScanCounter(pde);
		pde->trig[pde->eventExpected].swEventOccurred = TRUE;
	}
	return;
}


VOID
adcDisarmAcquisition(PDEVICE_EXTENSION pde)
{
	writeTrigControl(pde, TrigAnalog | TrigDisable);
	writeTrigControl(pde, TrigTTL | TrigDisable);

	writeAcqControl(pde, SeqStopScanList);
	writeAcqControl(pde, AdcPacerDisable);
	adcStopDmaTransfer(pde);

	enterAdcStatusMutex(pde);
	pde->adcStatusFlags &= ~(DaafAcqActive | DaafAcqTriggered);
	leaveAdcStatusMutex(pde);

	return;
}


VOID
adcDisarm(PDEVICE_EXTENSION pde, daqIOTP p)
{
	adcDisarmAcquisition(pde);
	pde->scansToMoveToUserBuf = 0L;
	dbg("Succesfully disarmed");
	return ;
}


VOID
waitXus(PDEVICE_EXTENSION pde, DWORD usCount)
{
	udelay(usCount);
	return;
}


VOID
adcArm(PDEVICE_EXTENSION pde, daqIOTP p)
{
	DWORD timeoutCount;
	WORD stat;

	if (adcCheckStatus(pde, DaafAcqActive)) {
		p->errorCode = DerrAcqArmed;
		dbg("called when AcqArmed");
		return;
	}

	if (!pde->scanConfiguredOK) {
		p->errorCode = DerrScanConfig;
		return;
	}
	adcDisarm(pde, NULL);

	if (pde->adcXferThreadRunning == TRUE) {
		pde->adcThreadDie = TRUE;
		timeoutCount = 10000;
		while (pde->adcXferThreadRunning) {
			if (timeoutCount-- == 0) {
				p->errorCode = DerrAcqArmed;
				dbg("XferThread timeout called when AcqArmed");
				return;
			}
			waitXus(pde, 10L);
		}
	}

	pde->firstTime = TRUE;

#if 0 /* PEK: uncommented checking code? */
	{
		DWORD i;
		pde->dmaInpBufHead = pde->dmaInpBuf;
		for (i = 0; i < DMAINPBUFSIZE; i++) {
			*pde->dmaInpBufHead = (WORD) ((DWORD) (pde->dmaInpBufHead) & 0x0000ffffL);
			pde->dmaInpBufHead++;
		}
	}
#endif

	pde->trig[TRIGGER_EVENT].count = pde->trig[TRIGGER_EVENT].countValue;
	pde->trig[STOP_EVENT].count = pde->trig[STOP_EVENT].countValue;

	pde->dmaInpBufHead = pde->dmaInpBuf;
	pde->adcCurrentScanCount = pde->adcLastScanCount = 0x00000000L;
	pde->adcOldCounterValue = 0x0000;

	pde->adcTRUECurrentScanCount = 0x00000000L;
	pde->adcLastFifoCheck = (DWORD) TRUE;
	pde->adcTRUEOldCounterValue = 0x0000;

	pde->adcScansInFifo = (DWORD) (66L / pde->scanSize);
	pde->maxDmaInpBufScans = DMAINPBUFSIZE / pde->scanSize;
	pde->lastScanCopied = 0;

	pde->scansAcquiredLastPass = 0;
	pde->scansToMoveToUserBuf = 0L;
	pde->scansInUserBuffer = 0L;

	setupStartEvent(pde);
	setupPacerClock(pde);
	adcSetupDmaTransfer(pde);
	writeAcqControl(pde, AcqResetResultsFifo | AcqResetConfigPipe);
	adcStartDmaTransfer(pde);
	writeAcqControl(pde, SeqStartScanList);

	timeoutCount = 2000;
	while (timeoutCount--) {
		stat = readAcqStatus(pde);
		if (stat & AcqHardwareError) {
			adcDisarm(pde, NULL);
			p->errorCode = DerrInitFailure;
			dbg("DerrInitFailure");
			return;
		}

		if (stat &
		    (AcqResultsFIFOMore1Sample | AcqResultsFIFOHasValidData |
		     AcqResultsFIFOOverrun)) {

			adcDisarm(pde, NULL);
			p->errorCode = DerrInitFailure;
			dbg("DerrInitFailure");
			return;
		}
		if ((stat & (AcqConfigPipeFull | AcqLogicScanning)) == AcqConfigPipeFull)
			break;
	}

	if (timeoutCount == 0L) {
		adcDisarm(pde, NULL);
		p->errorCode = DerrInitFailure;
		dbg("DerrInitFailure");
		return;
	}

	waitXus(pde, 10L);
	initializeOutputs(pde);

	if (pde->trig[TRIGGER_EVENT].trigControl) {
		writeAcqControl(pde, pde->adcPacerClockSelection);
		writeTrigControl(pde, pde->trig[TRIGGER_EVENT].trigControl);
	} else {
		writeAcqControl(pde, pde->adcPacerClockSelection | AdcPacerEnable);
	}

	enterAdcStatusMutex(pde);
	pde->adcStatusFlags |= DaafAcqActive;
	pde->currentAdcErrorCode = DerrNoError;
	leaveAdcStatusMutex(pde);
	kickstartAdcEvent(pde);
    	
	return ;
}


VOID
setupPacerClock(PDEVICE_EXTENSION pde)
{
	switch (pde->adcClockSource & 0x000000ffL) {
	case DacsExternalTTL:
		pde->adcPacerClockSelection = AdcPacerExternal;
		if (!(pde->adcClockSource & DacsFallingEdge))
			pde->adcPacerClockSelection |= AdcPacerExternalRising;
		break;

	case DacsAdcClockDiv2:
		writeAcqControl(pde, AdcPacerCompatibilityMode);
		writeAcqPacerClock(pde, pde->adcPacerClockDivisorHigh,
				   pde->adcPacerClockDivisorLow);
		pde->adcPacerClockSelection = AdcPacerInternal;
		if (pde->adcClockSource & DacsOutputEnable)
			pde->adcPacerClockSelection |= AdcPacerInternalOutEnable;
		break;

	default:
		writeAcqControl(pde, AdcPacerNormalMode);
		writeAcqPacerClock(pde, pde->adcPacerClockDivisorHigh,
				   pde->adcPacerClockDivisorLow);
		pde->adcPacerClockSelection = AdcPacerInternal;
		if (pde->adcClockSource & DacsOutputEnable)
			pde->adcPacerClockSelection |= AdcPacerInternalOutEnable;
	}

	if (pde->dacTriggerIsAdc) {
		pde->adcPacerClockSelection |= AdcPacerEnableDacPacer;
		pde->dacTriggerIsAdc = FALSE;
	}
	return ;
}


VOID
computePacerClockDivisor(PDEVICE_EXTENSION pde, ADC_CONFIG_PT adcConfig)
{
	DWORD low, high;
	low = (adcConfig->adcPeriodmsec & 0x0000ffffL) * 1000L;
	low += adcConfig->adcPeriodnsec / 1000L;
	high = (adcConfig->adcPeriodmsec >> 16) * 1000L;
	high = high + (low >> 16);
	low = (high << 16) + (low & 0x0000ffffL);
	high = high >> 16;
	if (low == 0L)
		high--;
	low--;

	pde->adcPacerClockDivisorHigh = high;
	pde->adcPacerClockDivisorLow = low;
	pde->adcClockSource = adcConfig->adcClockSource;

#if 0 /* PEK: more who knows what... */
	if ((adcConfig->adcClockSource & 0x000000ffL) == DacsAdcClockDiv2) {
		pde->adcPacerClockDivisor = adcConfig->adcPeriodmsec * 100;
		pde->adcPacerClockDivisor += ((adcConfig->adcPeriodnsec + 5000L) / 10000L);
	} else {
		pde->adcPacerClockDivisor = adcConfig->adcPeriodmsec * 200;
		pde->adcPacerClockDivisor += ((adcConfig->adcPeriodnsec + 2500L) / 5000L);
	}
	pde->adcPacerClockDivisor--;

#endif
	return ;
}

VOID
adcConfigure(PDEVICE_EXTENSION pde, daqIOTP p)
{
	ADC_CONFIG_PT adcConfig = (ADC_CONFIG_PT) p;
	pde->scanConfiguredOK = FALSE;

	if (adcCheckStatus(pde, DaafAcqActive)){
		adcConfig->errorCode = DerrAcqArmed;
		dbg("Called when AcqArmed");
		return;
	}

	adcConfig->errorCode = adcTestConfiguration(pde, adcConfig);

	if (adcConfig->errorCode != DerrNoError) {
		dbg("adcTestConfiguration failed: code %i.",
		     adcConfig->errorCode);
		return;
	}

	pde->scanSize = adcConfig->adcScanLength;
	/* PEK: scanSize is unsigned int on i386, 
	 * lets be quick about this! % is so slow!
	 */
	pde->scanSizeOdd = (BOOL) (pde->scanSize & 1);
//		pde->scanSizeOdd = (BOOL) (pde->scanSize % 2L);
	computePacerClockDivisor(pde, adcConfig);
	
	if (pde->scanSize) {
		adcSetScanSeq(pde, adcConfig);
		adcSetTrig(pde, adcConfig);
		pde->scanConfiguredOK = TRUE;
	}

	return ;
}

VOID
adcStop(PDEVICE_EXTENSION pde, daqIOTP p)
{

	enterAdcStatusMutex(pde);
	
	pde->adcStatusFlags &= ~DaafTransferActive;
	pde->adcThreadDie = TRUE;
	pde->userInpBuf = NULL;
	
	leaveAdcStatusMutex(pde);
	dbg("Succesfully stopped");

	return ;
}


VOID
adcStart(PDEVICE_EXTENSION pde, daqIOTP p)
{
	ADC_START_PT adcStartXfer = (ADC_START_PT) p;
	DWORD scansInDmaInpBuf;

	if (adcCheckStatus(pde, DaafTransferActive)) {
		adcStartXfer->errorCode = DerrMultBackXfer;
		return;
	}

	if (!pde->scanConfiguredOK) {
		adcStartXfer->errorCode = DerrScanConfig;
		return;
	}

	pde->userInpBufSize = adcStartXfer->adcXferBufLen;
	pde->userInpBuf = pde->userInpBufHead = pde->in_buf;

	pde->userInpBufCyclic = adcStartXfer->adcXferMode & DatmCycleOn;
	pde->maxUserBufScans = pde->userInpBufSize / pde->scanSize;
	pde->scansInUserBuffer = 0L;

	enterAdcStatusMutex(pde);
	pde->adcStatusFlags |= DaafTransferActive;
	leaveAdcStatusMutex(pde);

	if (pde->scansToMoveToUserBuf) {
		scansInDmaInpBuf = pde->scansToMoveToUserBuf;
		pde->scansToMoveToUserBuf = 0;
		copyNewScans(pde, scansInDmaInpBuf, FALSE);
		if (!adcCheckStatus(pde, DaafAcqActive))
			adcStop(pde, NULL);
	}

	dbg("Succesfully started");
	return ;
}


BOOL
adcCheckFatalErrors(PDEVICE_EXTENSION pde, DWORD scansAcquired)
{
	WORD acqStatus;
	acqStatus = readAcqStatus(pde);

	/* we can only hold one error message: 
	 * check them all, keep the most severe 'til last
	 */

	if (acqStatus & AcqResultsFIFOOverrun)
		pde->currentAdcErrorCode = DerrFIFOFull;

	if (acqStatus & AcqAdcNotReady)
		pde->currentAdcErrorCode = DerrAdcNotReady;

	if (acqStatus & AcqPacerOverrun)
		pde->currentAdcErrorCode = DerrAdcPacerOverrun;

	if (acqStatus & ArbitrationFailure)
		pde->currentAdcErrorCode = DerrArbitrationFailure;

	if (acqStatus &
	    (ArbitrationFailure | AcqPacerOverrun |
	     AcqAdcNotReady | AcqResultsFIFOOverrun)) {
		adcDisarm(pde, NULL);
		adcStop(pde, NULL);
		return TRUE;
	}

	if ((scansAcquired + pde->scansAcquiredLastPass +
	     pde->scansToMoveToUserBuf) > pde->maxDmaInpBufScans) {
		adcDisarm(pde, NULL);
		adcStop(pde, NULL);
		pde->currentAdcErrorCode = DerrOverrun;
		return TRUE;
	}

	pde->scansAcquiredLastPass = scansAcquired;
	return FALSE;
}


BOOL
checkForLevelEvent(PDEVICE_EXTENSION pde, DWORD dwCounts)
{

	LONG lCounts, lLevel, lVar;
	DWORD dwCountsMask, dwTriggerMask;

	switch (pde->trig[pde->eventExpected].event.TrigSource) {

	case DatsDigPattern:
		dwCountsMask = dwCounts & pde->trig[pde->eventExpected].event.TrigVariance;
		dwTriggerMask = pde->trig[pde->eventExpected].event.TrigLevel &
			pde->trig[pde->eventExpected].event.TrigVariance;

		switch (pde->trig[pde->eventExpected].event.TrigSens) {
		case DetsAboveLevel:
			if (dwCountsMask > dwTriggerMask)
				return TRUE;
			return FALSE;

		case DetsBelowLevel:
			if (dwCountsMask < dwTriggerMask)
				return TRUE;
			return FALSE;

		case DetsEQLevel:
			if (dwCountsMask == dwTriggerMask)
				return TRUE;
			return FALSE;

		case DetsNELevel:
			if (dwCountsMask != dwTriggerMask)
				return TRUE;
			return FALSE;

		default:
			return FALSE;
		}

	case DatsSoftwareAnalog:
		if (pde->trig[pde->eventExpected].event.Trigflags & DafSigned) {
			lCounts = (SHORT) dwCounts;
			lLevel = (SHORT) pde->trig[pde->eventExpected].event.TrigLevel;
			lVar = (SHORT) pde->trig[pde->eventExpected].event.TrigVariance;
		} else {
			lCounts = (WORD) dwCounts;
			lLevel = (WORD) pde->trig[pde->eventExpected].event.TrigLevel;
			lVar = (WORD) pde->trig[pde->eventExpected].event.TrigVariance;
		}

		switch (pde->trig[pde->eventExpected].event.TrigSens) {
		case DetsAboveLevel:
			if (lCounts > lLevel)
				return TRUE;
			return FALSE;

		case DetsBelowLevel:
			if (lCounts < lLevel)
				return TRUE;
			return FALSE;

		case DetsEQLevel:
			if ((lCounts >= (lLevel - lVar))
			    && (lCounts <= (lLevel + lVar)))
				return TRUE;
			return FALSE;

		case DetsNELevel:
			if ((lCounts < (lLevel - lVar))
			    || (lCounts > (lLevel + lVar)))
				return TRUE;
			return FALSE;

		case DetsRisingEdge:
			if (pde->trig[pde->eventExpected].hystSatisfied) {
				if (lCounts > lLevel)
					return TRUE;
			} else {
				if (lCounts < lVar)
					pde->trig[pde->eventExpected].hystSatisfied = TRUE;
			}
			return FALSE;

		case DetsFallingEdge:
			if (pde->trig[pde->eventExpected].hystSatisfied) {
				if (lCounts < lLevel)
					return TRUE;
			} else {
				if (lCounts > lVar)
					pde->trig[pde->eventExpected].hystSatisfied = TRUE;
			}
			return FALSE;

		default:
			return FALSE;
		}

	default:
		return FALSE;
	}
	/* NOTREACHED */
	err("Ack!! TBhbtbth! Ack! all pear shaped...");
	/*PEK: put impossible case catcher here... */
}


EventT
checkForEvent(PDEVICE_EXTENSION pde, DWORD scansAcquired)
{
	DWORD i;

    
	switch (pde->eventExpected) {

	case TRIGGER_EVENT:
		if (scansAcquired <= pde->trig[TRIGGER_EVENT].count) {
			pde->trig[TRIGGER_EVENT].count -= scansAcquired;
			return WAITING_FOR_TRIGGER;
		} else {
			scansAcquired -= pde->trig[TRIGGER_EVENT].count;
			pde->trig[TRIGGER_EVENT].count = 0L;
		}

		if (pde->trig[TRIGGER_EVENT].swEventOccurred) {
			if (pde->trig[TRIGGER_EVENT].eventScan == 0L)
				pde->trig[TRIGGER_EVENT].eventScan = 1L;
			return TRIGGER_EVENT;
		}

		switch (pde->trig[TRIGGER_EVENT].event.TrigSource) {
		case DatsImmediate:
			pde->trig[TRIGGER_EVENT].eventScan = 1L;
			return TRIGGER_EVENT;

		case DatsExternalTTL:
		case DatsHardwareAnalog:
			if (pde->trig[TRIGGER_EVENT].hwEventOccurred) {
				pde->trig[TRIGGER_EVENT].eventScan = 1L;
				return TRIGGER_EVENT;
			}
			break;

		case DatsDigPattern:
		case DatsSoftwareAnalog:
			if (scansAcquired) {
				for (i = 0L; i < scansAcquired; i++) {
					if (checkForLevelEvent(pde, (DWORD) (*pde->eventSearchPtr))) {
						pde->trig[TRIGGER_EVENT].eventScan =
						    pde->adcCurrentScanCount - scansAcquired + i + 1L;
						return TRIGGER_EVENT;
					}
					pde->eventSearchPtr += pde->scanSize;
					if (pde->eventSearchPtr >= &pde->dmaInpBuf[DMAINPBUFSIZE])
						pde->eventSearchPtr -= DMAINPBUFSIZE;
				}
			}
			break;
		}
		return WAITING_FOR_TRIGGER;

	case STOP_EVENT:
		if (scansAcquired < pde->trig[STOP_EVENT].count) {
			pde->trig[STOP_EVENT].count -= scansAcquired;
			return WAITING_FOR_STOP;

		} else {
			scansAcquired -= pde->trig[STOP_EVENT].count;
			pde->trig[STOP_EVENT].count = 0L;
		}

		switch (pde->trig[STOP_EVENT].event.TrigSource) {
		case DatsSoftware:
			if (pde->trig[STOP_EVENT].swEventOccurred)
				return STOP_EVENT;
			break;

		case DatsScanCount:
			pde->trig[STOP_EVENT].eventScan = pde->adcCurrentScanCount - scansAcquired;
			return STOP_EVENT;

		case DatsDigPattern:
		case DatsSoftwareAnalog:
			if ((pde->trig[STOP_EVENT].count == 0L) && scansAcquired) {
				for (i = 0; i < scansAcquired; i++) {
					if (checkForLevelEvent
					    (pde, (DWORD) * (pde->eventSearchPtr))) {

						pde->trig[STOP_EVENT].
						    eventScan = pde->adcCurrentScanCount - scansAcquired + i + 1L;
						return STOP_EVENT;
					}
					pde->eventSearchPtr += pde->scanSize;
					if (pde->eventSearchPtr >= &pde->dmaInpBuf[DMAINPBUFSIZE])
						pde->eventSearchPtr -= DMAINPBUFSIZE;
				}
			}
			break;
		}
        default:
          printk("unhandled eventExcpected  code %d",  pde->eventExpected); 
	}
        
	return WAITING_FOR_STOP;
}


DWORD
computeScansAcquired(PDEVICE_EXTENSION pde)
{

	pde->adcLastScanCount = pde->adcCurrentScanCount;
	if (pde->eventExpected == POSTSTOP_EVENT) {
		pde->adcCurrentScanCount = pde->trig[STOP_EVENT].eventScan;
	} else {
		pde->adcCurrentScanCount = readAcqScanCounter(pde);
	}
	if (pde->adcCurrentScanCount < pde->adcLastScanCount)
		return (pde->adcCurrentScanCount + (0xFFFFFFFFL - pde->adcLastScanCount));
	else
		return (pde->adcCurrentScanCount - pde->adcLastScanCount);
}


VOID
repmov(PDEVICE_EXTENSION pde, DWORD samples)
{
	WORD *source;
	WORD *dest;
	source = pde->dmaInpBufHead;
	dest = pde->userInpBufHead;
    //int x;    

	if (samples)
    {
		memcpy(dest, source, samples * 2);
        
/*
  		printk("dest %p,  source %p  samples %i \n",dest,source,samples);
  		for (x = 0; x < samples; x++)
  		{
  			//printk(" 0x%X",source+x);
            printk(" %i",(*(short*)(source+x)));
  		}

  		for (x = 0; x < samples; x++)
  		{
  			//printk(" %i",*(source+x));
  			printk(" %i",(*(short*)(source+x)));

  		}
  		printk("\n");
		for (x = 0; x < samples; x++)
		{
			printk(" 0x%X",dest+x);
		}
*/


#if 0 /* PEK: more debugging of something? */
		{
			DWORD i;
			source = pde->dmaInpBufHead;
			for (i = 0; i < samples; i++) {
				*source = (WORD) ((DWORD) (source) & 0x0000ffffL);
				source++;
			}
		}
#endif

		pde->dmaInpBufHead += samples;
		pde->userInpBufHead += samples;
	}
	return ;
}


VOID
memCopyToUserBuf(PDEVICE_EXTENSION pde, DWORD samples)
{
	DWORD partialSamples = 0L;
	if (samples) {

		if ((pde->userInpBufHead + samples) > (pde->userInpBuf + pde->userInpBufSize)) {
			partialSamples = pde->userInpBuf + pde->userInpBufSize - pde->userInpBufHead;
			repmov(pde, partialSamples);
			pde->userInpBufHead = pde->userInpBuf;
		}
		repmov(pde, samples - partialSamples);
	}
	return ;
}


VOID
copyNewScans(PDEVICE_EXTENSION pde, DWORD scans, BOOL calledFromAdcEvent)
{
	DWORD samples, partialScans, partialSamples = 0;
	BOOL needToStopTransfer = FALSE;

	if (!scans) {
		return ;
	}

	if (adcCheckStatus(pde, DaafTransferActive)) {
		if (!pde->userInpBufCyclic) {
			if ((pde->scansInUserBuffer + scans) > pde->maxUserBufScans) {
				needToStopTransfer = TRUE;
				partialScans = pde->maxUserBufScans - pde->scansInUserBuffer;
				pde->scansToMoveToUserBuf = scans - partialScans;
				scans = partialScans;
			}
		}
		samples = scans * pde->scanSize;
		samples = scans * pde->scanSize;

		do {
			partialSamples = &pde->dmaInpBuf[DMAINPBUFSIZE] - pde->dmaInpBufHead;
			if (pde->maxUserBufScans * pde->scanSize < partialSamples)
				partialSamples = pde->maxUserBufScans * pde->scanSize;
			if (samples < partialSamples)
				partialSamples = samples;

			memCopyToUserBuf(pde, partialSamples);
			samples -= partialSamples;

			if (pde->dmaInpBufHead == &pde->dmaInpBuf[DMAINPBUFSIZE]) {
				pde->dmaInpBufHead = pde->dmaInpBuf;
			}
		} while (samples > 0);

		pde->scansInUserBuffer += scans;

		if (needToStopTransfer)
			adcStop(pde, NULL);

	} else {
		pde->scansToMoveToUserBuf += scans;
	}
	return ;
}


VOID __stdcall
adcEvent(PDEVICE_EXTENSION pde, PVOID ref)
{
	return ;
}

VOID __stdcall
adcEventX(PDEVICE_EXTENSION pde, PVOID ref)
{
	return ;
}

static VOID
adcEventL(unsigned long data)
{
	BOOL bCallAgain = FALSE;
	DEVICE_EXTENSION *pde;
	pde = (DEVICE_EXTENSION *) data; 

	if (pde->inAdcEvent) {
        dbg("allready in adcevent ,,, Leaving");
		return;
	}

	pde->inAdcEvent = TRUE;    
    
	if (!pde->adcThreadDie)
     {        
		if (pde->dmaInpActive)
        {           
			bCallAgain = (BOOL) checkEventsAndMoveData(pde);            
		}
        else
        {
            dbg("dma not active\n");
        }
	}

	if (!bCallAgain) {
		pde->adcXferThreadRunning = FALSE;
		pde->adcEventHandle = 0;
		pde->inAdcEvent = FALSE;                                  
		del_timer(&pde->adc_timer);
	} else {
		pde->adcXferThreadRunning = TRUE;
		pde->adcEventHandle = 0;
		pde->inAdcEvent = FALSE;
		pde->adcTimeoutHandle = 1;
		pde->adc_timer.expires = jiffies + ADC_HZ;
		add_timer(&pde->adc_timer);
	}
	return ;
}


DWORD
checkEventsAndMoveData(PDEVICE_EXTENSION pde)
{
	DWORD scansAcquired, preTrigSize;
	scansAcquired = computeScansAcquired(pde);
    

	if (adcCheckFatalErrors(pde, scansAcquired)) {
		/* PEK:CHECKME best error return here? */
		return (DWORD) pde->dmaInpActive;
	}

	while (scansAcquired) {
		switch (checkForEvent(pde, scansAcquired))
        {
          
		case WAITING_FOR_TRIGGER:              
			scansAcquired = 0L;
			break;

		case TRIGGER_EVENT:             
			preTrigSize = (pde->trig[TRIGGER_EVENT].eventScan -
				       pde->trig[TRIGGER_EVENT].countValue - 1L)
				* pde->scanSize;

			preTrigSize %= DMAINPBUFSIZE;
			pde->dmaInpBufHead = pde->dmaInpBuf + preTrigSize;
			copyNewScans(pde, pde->trig[TRIGGER_EVENT].countValue, TRUE);
			setupStopEvent(pde);

			scansAcquired = pde->adcCurrentScanCount -
				pde->trig[TRIGGER_EVENT].eventScan + 1L;
			pde->adcLastScanCount = pde->trig[TRIGGER_EVENT].eventScan - 1L;
			break;

		case WAITING_FOR_STOP:              
			copyNewScans(pde, scansAcquired, TRUE);
			scansAcquired = 0L;
			break;

		case STOP_EVENT:            
			setupPostStopEvent(pde);
			pde->adcCurrentScanCount = pde->adcLastScanCount;
			scansAcquired = computeScansAcquired(pde);
			copyNewScans(pde, scansAcquired, TRUE);
			adcDisarmAcquisition(pde);
			adcStop(pde, NULL);
			scansAcquired = 0L;
			break;

		case POSTSTOP_EVENT:
		default:            
			scansAcquired = 0L;
			break;
		}
	}

	return (DWORD) pde->dmaInpActive;
}


VOID
adcGetHWInfo(PDEVICE_EXTENSION pde, daqIOTP p)
{

    //info("Entered adcGetHWInfo");
	ADC_HWINFO_PT adcHWInfo = (ADC_HWINFO_PT) p;

	switch (adcHWInfo->dtEnum) {

	case AhiSerialNumber:
		sprintf((PCHAR) adcHWInfo->usrPtr, "%u", pde->boardInfo.serialNumber);
		break;

	case AhiTime:
		strcpy((PCHAR) adcHWInfo->usrPtr, pde->eeprom.time);
		break;

	case AhiDate:
		strcpy((PCHAR) adcHWInfo->usrPtr, pde->eeprom.date);
		break;

	case AhiChanType:        
		adcHWInfo->chanType = pde->boardInfo.deviceType;
        //info("leaving AhiChanType case %i",adcHWInfo->chanType);
		break;

	case AhiCycles:
	case AhiExtFeatures:
	case AhiFifoSize:
	case AhiFifoCount:
	case AhiPostTrigScans:
	case AhiHardwareVer:
	case AhiFirmwareVer:
		adcHWInfo->errorCode = DerrNotImplemented;

	default:
		adcHWInfo->errorCode = DerrInvCommand;
		break;
	}
	return ;
}


VOID
pauseForAdcEvent(PDEVICE_EXTENSION pde)
{
	return ;
}


DWORD
adcTestInputDma(PDEVICE_EXTENSION pde, DaqAdcFlag chType)
{
	daqIOT sb;
	ADC_CONFIG_PT adcConfig = (ADC_CONFIG_PT) & sb;
	SCAN_SEQ_T adcScanSeq;
	TRIG_EVENT_T triggerEventList;
	WORD timeout;
	DWORD returnCount, acquisitionSize = 2000L;

	adcDisarm(pde, NULL);
	adcStop(pde, NULL);

	adcScanSeq.channel = adcScanSeq.reserved = 0;

	if (chType == DafAnalog) {
		adcScanSeq.gain = DgainX1;
		adcScanSeq.flags = DafSigned | DafBipolar | DafSingleEnded | DafAnalog | DafSettle5us;
	} else {
		adcScanSeq.flags = DafSigned | DafP3Local16 | DafScanDigital | DafSettle5us;
	}

	adcConfig->adcScanSeq = &adcScanSeq;
	adcConfig->adcScanLength = 1L;

	triggerEventList.TrigCount = 0;
	triggerEventList.TrigLocation = 0;
	triggerEventList.TrigSource = DatsImmediate;

	adcConfig->triggerEventList = &triggerEventList;

	adcConfig->adcAcqPreCount = 0;
	adcConfig->adcAcqPostCount = acquisitionSize;
	adcConfig->adcAcqMode = DaamNShot;

	adcConfig->adcClockSource = DacsAdcClock;
	adcConfig->adcPeriodmsec = 0L;
	adcConfig->adcPeriodnsec = 5000L;

	adcConfigure(pde, &sb);
	adcArm(pde, &sb);

	timeout = 15;

	do {
		if (timeout-- == 0) {
			adcDisarm(pde, NULL);
			return 0L;
		}

		pauseForAdcEvent(pde);
	} while (adcCheckStatus(pde, DaafAcqActive));

	returnCount = pde->scansToMoveToUserBuf;
	adcDisarm(pde, NULL);

	if (returnCount == acquisitionSize)
		return returnCount * 100L;
	else
		return 0L;
}
