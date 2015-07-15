////////////////////////////////////////////////////////////////////////////
//
// dac.c                           I    OOO                           h
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

#define DAC_HZ (HZ/100)

#include "iottypes.h"
#include "daqx.h"
#include "ddapi.h"
#include "device.h"
#include "daqio.h"
#include "adc.h"

#include "w32ioctl.h"


static HW *nulldaqMemoryMap = (HW *) NULL;

static VOID dacEventL(unsigned long data);
VOID prepareToStopDmaOut(PDEVICE_EXTENSION pde);
VOID copyUserSamplesToInternalBuf(PDEVICE_EXTENSION pde, DWORD samples);
DWORD dacCheckEventsAndMoveData(PDEVICE_EXTENSION pde);

VOID __stdcall dacEventX(PDEVICE_EXTENSION pde, PVOID ref);

char cCaption[128];
char cText[128];


VOID
dacScheduleDacEvent(PDEVICE_EXTENSION pde)
{
	/*PEK weird function: rework*/
	dbg("Entered dacScheduleDacEvent \n");
	if (!pde->dacThreadDie) {
		if (!pde->dacEventHandle) {
			pde->dacXferThreadRunning = FALSE;
		}
	} else {
		pde->dacXferThreadRunning = FALSE;
	}
	return ;
}

VOID __stdcall
dacTimeoutCallback(PVOID refData, DWORD extra)
{
	PDEVICE_EXTENSION pde = (PDEVICE_EXTENSION) refData;
	dbg("Entered dacTimeoutCallbac \n");
	pde->dacTimeoutHandle = 0;
	dacScheduleDacEvent(pde);
	return ;
}

VOID
dacEventThread(PDEVICE_EXTENSION pde, PVOID Context)
{
	dbg("Entered dacEventThread \n");
	return ;
}


VOID
kickstartDacEvent(PDEVICE_EXTENSION pde)
{
	dbg("Entered kickstartDacEvent \n");
    if(pde->dacXferThreadRunning == TRUE)
    {
        DWORD timeoutValue = 10000;
        printk("KICKSTART pde->adcXferThreadRunning == TRUE\n");

        // shut down the adc thread
        pde->dacThreadDie = TRUE;

        while (pde->dacXferThreadRunning)
        {
            if(timeoutValue-- == 0)
            {
                break;
            }
            waitXus(pde, 10L);
        }
    }
    
    if(pde->dacXferThreadRunning == FALSE)
    {

        // Set the Adc thread running flag ????
        pde->dacXferThreadRunning = TRUE;
        pde->dacThreadDie = FALSE;
        pde->inDacEvent = FALSE;

        pde->dacTimeoutHandle = 1;
        init_timer(&pde->dac_timer);
        pde->dac_timer.expires = jiffies + DAC_HZ;

        //printk("KICKSTART jiffies + ADC_HZ %li\n",jiffies + ADC_HZ);
        pde->dac_timer.data = (unsigned long)pde;

        pde->dac_timer.function = dacEventL;
        add_timer(&pde->dac_timer);
    }
    else
    {
        info("kickstartDacEvent -> ERROR fell right through KICKSTART \n");
        pde->dacThreadDie = TRUE;
    }



    
	return ;
}


static VOID
enterDacStatusMutex(PDEVICE_EXTENSION pde)
{
//	if (!pde->inDacEvent)
		spin_lock_irq(&pde->dacStatusMutex);
	return ;
}

static VOID
leaveDacStatusMutex(PDEVICE_EXTENSION pde)
{
//	if (!pde->inDacEvent)
		spin_unlock_irq(&pde->dacStatusMutex);
	return ;
}


BOOL
initializeDac(PDEVICE_EXTENSION pde)
{
	DWORD i = 0;
	dbg("Entered initializeDac \n");

	spin_lock_init(&pde->dacStatusMutex);

	pde->dacStatusFlags = 0;
	dacDisarm(pde, NULL);

	for (i = 0; i < 5; i++) {
		pde->dacOut[i].mode = DdomVoltage;
		pde->dacOut[i].bufMdl = NULL;
	}

	pde->dacOut[0].enableBits = Dac0Enable;
	pde->dacOut[1].enableBits = Dac1Enable;
	pde->dacOut[2].enableBits = Dac2Enable;
	pde->dacOut[3].enableBits = Dac3Enable;
	pde->dacOut[4].enableBits = DacPatternEnable;

	return TRUE;
}


BOOL
uninitializeDac(PDEVICE_EXTENSION pde)
{
    DWORD timeoutValue = 10000;
    int x;
    
	dbg("Entered uninitializeDac \n");
	dacDisarm(pde, NULL);
	dacStop(pde, NULL);
	pde->dacThreadDie = TRUE;
    
    while (pde->dacXferThreadRunning)
    {
		if (timeoutValue-- == 0)
        {
			break;
		}
		waitXus(pde, 100L);
    }
    
    for(x = 0; x < 5; x++)
    {
        if(pde->dacOutbuffer[x] != NULL)
        {
            //info("Freeing DaqBuffer %i",x);
            pde->dacOutbuffer[x] = NULL;
            kfree(pde->dacOutbuffer[x]);
        }
    }
	return TRUE;
}


static BOOL
dacCheckStatus(PDEVICE_EXTENSION pde, DWORD mask)
{
	DWORD dacStatus;
	dbg("Entered dacCheckStatus \n");
	enterDacStatusMutex(pde);
	dacStatus = pde->dacStatusFlags;
	leaveDacStatusMutex(pde);

	return ((dacStatus & mask) != 0);
}


DWORD
dacCheckDescriptorBlocks(PDEVICE_EXTENSION pde)
{
	WORD i, lastValidBlock = 0xffff;
	BYTE *ptrA;
	BYTE *ptrB;
	DWORD firstBlockAddr = 0L;

	dbg("Entered dacCheckDescriptorBlocks \n");
	for (i = 0; i < OUTCHAINSIZE; i++) {
		ptrA = (PBYTE) virt_to_phys(&pde->dmaOutChain[i]);
		ptrA += 0x0000000fL;
		ptrB = (PBYTE) virt_to_phys(((BYTE *) & pde->dmaOutChain[i] + 0x0000000fL));

		if (ptrA == ptrB) {
			if (firstBlockAddr == 0L) {
				firstBlockAddr = (DWORD) ptrA - 0x0000000fL;
			} else {
				pde->dmaOutChain[lastValidBlock].
				    descriptorPointer = (DWORD) ptrA - 0x0000000fL;
			}
			lastValidBlock = i;
		} else {
			pde->dmaOutChain[i].descriptorPointer = 0xffffffffL;
		}
	}
	return firstBlockAddr;
}


DWORD
dacSetupChainDescriptors(PDEVICE_EXTENSION pde)
{
	PWORD bufPhyAddr;
	PWORD bufSysAddr = (PWORD) pde->dmaOutBuf;
	DWORD blockSize, bufSize, firstBlockAddr;
	DWORD bufCount = 0;
	WORD descIndex = 0;

	dbg("Entered dacSetupChainDescriptors \n");
	bufSize = (pde->dmaOutBufSize) * 2L;

    
	firstBlockAddr = dacCheckDescriptorBlocks(pde);
    
	bufPhyAddr = (PWORD) virt_to_phys((PVOID) bufSysAddr);
	blockSize = 0x00001000L - ((DWORD) bufPhyAddr & 0x00000fff);

	while (pde->dmaOutChain[descIndex].descriptorPointer == 0xffffffffL)
		descIndex++;

	while (TRUE) {

        pde->dmaOutChain[descIndex].pciAddr = virt_to_phys(bufSysAddr);
        
		pde->dmaOutChain[descIndex].localAddr = (ULONG) (&nulldaqMemoryMap->dacFIFO);
		pde->dmaOutChain[descIndex].transferByteCount = blockSize;
		pde->dmaOutChain[descIndex].descriptorPointer =
			(pde->dmaOutChain[descIndex].descriptorPointer & 0xfffffff0L) | 0x00000001L;
		bufCount += blockSize;
		bufSysAddr += (blockSize / 2);
		if (bufCount >= bufSize) {

			pde->dmaOutChain[descIndex].descriptorPointer =
				(firstBlockAddr & 0xfffffff0L) | 0x00000001L;

			if (bufCount > bufSize) {
				pde->dmaOutChain[descIndex].transferByteCount =
					0x1000L - (bufCount - bufSize);
			}
			break;
		}
		blockSize = 0x1000;
		descIndex++;
		while (pde->dmaOutChain[descIndex].descriptorPointer == 0xffffffffL)
			descIndex++;
	}

	descIndex++;
	while (pde->dmaOutChain[descIndex].descriptorPointer == 0xffffffffL) descIndex++;
	//pde->dmaOutChain[descIndex].descriptorPointer =
	//	(pde->dmaOutChain[descIndex].descriptorPointer & 0xfffffff0L) | 0x00000001L;

    pde->dmaOutChain[descIndex].descriptorPointer = virt_to_phys(&pde->dmaOutChain[descIndex]);
	pde->dmaOutChain[descIndex].descriptorPointer = (pde->dmaOutChain[descIndex].descriptorPointer & 0xfffffff0L) | 0x00000001L;

    pde->dmaOutChain[descIndex].localAddr = (ULONG) (&nulldaqMemoryMap->dacFIFO);
    pde->dmaOutChain[descIndex].pciAddr = virt_to_phys(&pde->dmaOutBuf[pde->wf.padBlockStartIndex]);
    
	pde->dmaOutChain[descIndex].transferByteCount = (DWORD) (1920 * 2);

	pde->wf.padBlockDescPointer = pde->dmaOutChain[descIndex].descriptorPointer;

	return firstBlockAddr;
}

VOID
dacSetupDmaTransfer(PDEVICE_EXTENSION pde)
{
	BYTE *base9080;
	DWORD firstBlockAddr;
	dbg("Entered dacSetupDmaTransfer \n");
	firstBlockAddr = dacSetupChainDescriptors(pde);
	base9080 = (BYTE *) (pde->plxVirtualAddress);
	*(DWORD *) (base9080 + PCI9080_DMA0_MODE) = 0x00021ac1L;
	*(DWORD *) (base9080 + PCI9080_DMA0_PCI_ADDR) = 0x00000000L;
	*(DWORD *) (base9080 + PCI9080_DMA0_LOCAL_ADDR) = 0x00000000L;
	*(DWORD *) (base9080 + PCI9080_DMA0_COUNT) = 0x00000000L;
	*(DWORD *) (base9080 + PCI9080_DMA0_DESC_PTR) =
	    (firstBlockAddr & 0xfffffff0L) | 0x00000001L;
	*(WORD *) (base9080 + PCI9080_DMA0_THRESHOLD) = 0x0000;
	return ;
}


VOID
dacStartDmaTransfer(PDEVICE_EXTENSION pde)
{
	dbg("Entered dacStartDmaTransfer \n");
	*(BYTE *) ((BYTE *) pde->plxVirtualAddress + PCI9080_DMA0_CMD_STATUS) = 0x0b;
	writeDmaControl(pde, DmaCh0Enable);
	pde->dmaOutActive = TRUE;
	return ;
}


VOID
dacStopDmaTransfer(PDEVICE_EXTENSION pde)
{
	BYTE dmaStatus;
	BYTE *base9080;
	DWORD timeoutCount, timeoutCountVal = 2000;
	dbg("Entered dacStopDmaTransfer \n");
	writeDmaControl(pde, DmaCh0Disable);
	base9080 = (BYTE *) (pde->plxVirtualAddress);
	dmaStatus = *(base9080 + PCI9080_DMA0_CMD_STATUS);

	dbg("PLX R %2x %2x %8x PCI9080_DMA0_CMD_STATUS", PCI9080_DMA0_CMD_STATUS, dmaStatus, 0);

	if ((dmaStatus & 0x01) == 0x01) {
		timeoutCount = timeoutCountVal;
		while (dmaStatus & 0x10) {
			dmaStatus = *(base9080 + PCI9080_DMA0_CMD_STATUS);
			dbg("PLX R %2x %2x %8x PCI9080_DMA0_CMD_STATUS",
				 PCI9080_DMA0_CMD_STATUS, dmaStatus, 0);
			if (!timeoutCount--) {
				break;
			}
		}
		*(base9080 + PCI9080_DMA0_CMD_STATUS) = (dmaStatus & 0xfe);

		dbg("PLX W %2x %2x %8x PCI9080_DMA0_CMD_STATUS",
			 PCI9080_DMA0_CMD_STATUS, dmaStatus & 0xfe, 0);

		dmaStatus = *(base9080 + PCI9080_DMA0_CMD_STATUS);
		dbg("PLX R %2x %2x %8x PCI9080_DMA0_CMD_STATUS",
			 PCI9080_DMA0_CMD_STATUS, dmaStatus, 0);

		*(base9080 + PCI9080_DMA0_CMD_STATUS) = ((dmaStatus & 0xfe) | 0x04);
		dbg("PLX W %2x %2x %8x PCI9080_DMA0_CMD_STATUS",
			 PCI9080_DMA0_CMD_STATUS, (dmaStatus & 0xfe) | 0x04, 0);

		timeoutCount = timeoutCountVal;
		while ((dmaStatus & 0x10) == 0x00) {
			dmaStatus = *(base9080 + PCI9080_DMA0_CMD_STATUS);
			dbg("PLX R %2x %2x %8x PCI9080_DMA0_CMD_STATUS",
				 PCI9080_DMA0_CMD_STATUS, dmaStatus, 0);

			if (!timeoutCount--) {
				break;
			}
		}
	}

#if 0
	{
		DWORD currentAddr, currentLocal, currentCount, currentDesc;
		currentAddr = *(DWORD *) (base9080 + PCI9080_DMA0_PCI_ADDR);
		currentLocal = *(DWORD *) (base9080 + PCI9080_DMA0_LOCAL_ADDR);
		currentCount = *(DWORD *) (base9080 + PCI9080_DMA0_COUNT);
		currentDesc = *(DWORD *) (base9080 + PCI9080_DMA0_DESC_PTR);
	}
#endif
	pde->dmaOutActive = FALSE;
	return ;
}



VOID
dacMode(PDEVICE_EXTENSION pde, daqIOTP p)
{
	DAC_MODE_PT mode = (DAC_MODE_PT) p;
	DWORD dacOutMode, dacCh, dacSign;

	if (mode->dacDevType == DddtLocalDigital) {

		if (!pde->usingDigitalIO) {
			mode->errorCode = DerrNotCapable;
			return;
		}
		dacCh = 4L;
		dacSign = DdomUnsigned;
	} else if (mode->dacDevType == DddtLocal) {

		if (((dacCh = mode->dacChannel) + 1L) > pde->numDacs) {
			mode->errorCode = DerrNotCapable;
			return;
		}

		dacSign = mode->dacOutputMode & DdomSigned;
	} else {
		mode->errorCode = DerrInvType;
		return;
	}

	dacOutMode = mode->dacOutputMode & ~DdomSigned;
	if (dacOutMode < 3) {
		if (pde->dmaOutActive) {
			if ((pde->dacOut[dacCh].mode != DdomVoltage)
			    || (dacOutMode != DdomVoltage)) {
				
				mode->errorCode = DerrDacWaveformActive;
				return;
			}
		}

		pde->dacOut[dacCh].mode = dacOutMode;
		pde->dacOut[dacCh].enableBits &= ~DacSelectSignedData;
		if (dacSign) {
			pde->dacOut[dacCh].enableBits |= DacSelectSignedData;
		}
		writeDacControl(pde, (WORD) (pde->dacOut[dacCh].enableBits & ~DacEnableBit));
	} else {

		mode->errorCode = DerrInvMode;
	}
	return ;
}


VOID
dacWriteMany(PDEVICE_EXTENSION pde, daqIOTP p)
{
	DAC_WRITE_MANY_PT writeMany = (DAC_WRITE_MANY_PT) p;
	DWORD i;
	DWORD *channels;
	DWORD *types;
	SHORT *values;

	types = writeMany->dacDevTypes;
	channels = writeMany->dacChannels;
	values = writeMany->dacValues;

	if ((writeMany->dacNChannels == 0)
	    || (writeMany->dacNChannels > pde->numDacs)) {
		writeMany->errorCode = DerrInvChan;
		return;
	}

	for (i = 0; i < writeMany->dacNChannels; i++) {
		if (types[i] != DddtLocal) {
			writeMany->errorCode = DerrInvType;
			return;
		}
		if (pde->dacOut[channels[i]].mode != DdomVoltage) {
			writeMany->errorCode = DerrInvMode;
			return;
		}
	}
	for (i = 0; i < writeMany->dacNChannels; i++) {
		writeDac(pde, (WORD) channels[i], (SHORT) values[i]);
	}
	return ;
}


VOID
dacSoftTrig(PDEVICE_EXTENSION pde, daqIOTP p)
{
	dbg("Entered dacSoftTrig \n");
	if (!dacCheckStatus(pde, DdafWaveformActive)) {
		p->errorCode = DerrDacWaveformNotActive;
		return;
	}
	if (pde->dacTrigSource != DdtsSoftware) {
		p->errorCode = DerrInvTrigSource;
		return;
	}
	writeDacControl(pde, (WORD) (pde->dacPacerClockSelection | DacPacerEnable));
	return ;
}

VOID
dacDisarm(PDEVICE_EXTENSION pde, daqIOTP p)
{
	DWORD i;
    int x;
	//info("Entered dacDisarm \n");
	pde->dacTriggerIsAdc = FALSE;
	writeDacControl(pde, (WORD) (pde->dacPacerClockSelection | DacPacerDisable));
	dacStopDmaTransfer(pde);

	for (i = 0; i < 5; i++) {
		writeDacControl(pde, (WORD) (pde->dacOut[i].enableBits & ~DacEnableBit));
	}

	enterDacStatusMutex(pde);
	pde->dacStatusFlags &= ~(DdafWaveformActive | DdafWaveformTriggered);
    
    
    for(x = 0; x < 5; x++)
    {
        if(pde->dacOutbuffer[x] != NULL)
        {
            //info("Freeing DaqBuffer %i",x);
            pde->dacOutbuffer[x] = NULL;
            kfree(pde->dacOutbuffer[x]);
        }
    }
	leaveDacStatusMutex(pde);

    
	return ;
}


VOID
primeDmaOutputBuffer(PDEVICE_EXTENSION pde)
{
	DWORD i, outBufIndex, k;
	WORD *source;
	WORD *dest;

	dbg("Entered primeDmaOutputBuffer \n");
	if (pde->wf.dynamic) {
		pde->wf.dynamicWaveformBufHead = pde->wf.dynamicWaveformBufAddr;
		if ((pde->wf.dynamicWaveformBufSize / 2L) > pde->dmaOutBufSize) {
			copyUserSamplesToInternalBuf(pde, pde->dmaOutBufSize);
		} else {
			copyUserSamplesToInternalBuf(pde, (pde->wf.dynamicWaveformBufSize / 2L));
		}
	} else {        
		outBufIndex = 0;
		for (i = 0; i < pde->wf.singleWaveformSize; i++) {
			for (k = 0; k < 5; k++) {
				if (pde->dacOut[k].mode == DdomStaticWave)
                {
                    //pde->dacOut[k].bufHead appears to be screwed
					pde->dmaOutBuf[outBufIndex++] = *pde->dacOut[k].bufHead;                    
					pde->dacOut[k].bufHead++;
				}
			}
		}

        
        dest = pde->dmaOutBuf + outBufIndex;
        for(k = 1; k < pde->wf.maxWaveformsPerBuf; k++)
        {
            source = pde->dmaOutBuf;        

            memcpy(dest,source,outBufIndex*2);
            //MAH: TEST TEST
            //printk(" %i",(*(short*)(source+k)));
            dest += outBufIndex;
        }             
        
		pde->totalSamplesRemoved = pde->wf.stopCountInSamples;
	}
}


VOID
fillPadBlock(PDEVICE_EXTENSION pde)
{
	DWORD i, bufIndex = pde->wf.padBlockStartIndex;
	dbg("Entered fillPadBlock %i # of outputs\n",pde->wf.numWaveforms);

	switch (pde->wf.numWaveforms) {
	case 1:
		for (i = 0; i < 1920L; i++) {
			pde->dmaOutBuf[bufIndex + i] = pde->pad[0];
		}
		break;

	case 2:
		for (i = 0; i < 1920L; i += 2) {
			pde->dmaOutBuf[bufIndex + i] = pde->pad[0];
			pde->dmaOutBuf[bufIndex + i + 1] = pde->pad[1];
		}
		break;

	case 3:
		for (i = 0; i < 1920L; i += 3) {
			pde->dmaOutBuf[bufIndex + i] = pde->pad[0];
			pde->dmaOutBuf[bufIndex + i + 1] = pde->pad[1];
			pde->dmaOutBuf[bufIndex + i + 2] = pde->pad[2];
		}
		break;

	case 4:
		for (i = 0; i < 1920L; i += 4) {
			pde->dmaOutBuf[bufIndex + i] = pde->pad[0];
			pde->dmaOutBuf[bufIndex + i + 1] = pde->pad[1];
			pde->dmaOutBuf[bufIndex + i + 2] = pde->pad[2];
			pde->dmaOutBuf[bufIndex + i + 3] = pde->pad[3];
		}
		break;

	case 5:
		for (i = 0; i < 1920L; i += 5) {
			pde->dmaOutBuf[bufIndex + i] = pde->pad[0];
			pde->dmaOutBuf[bufIndex + i + 1] = pde->pad[1];
			pde->dmaOutBuf[bufIndex + i + 2] = pde->pad[2];
			pde->dmaOutBuf[bufIndex + i + 3] = pde->pad[3];
			pde->dmaOutBuf[bufIndex + i + 4] = pde->pad[4];
		}
		break;
	}
	return ;
}

VOID
computePadBlockInfo(PDEVICE_EXTENSION pde)
{
	DWORD lastCycleSize, descNo;
	dbg("Entered computePadBlockInfo \n");
	if (pde->dacStopEvent != DdwmInfinite) {
		lastCycleSize = pde->wf.stopCountInSamples % pde->dmaOutBufSize;
		if (lastCycleSize == 0)
			lastCycleSize = pde->dmaOutBufSize;

		descNo = 0L;
		while (pde->dmaOutChain[descNo].descriptorPointer == 0xffffffffL)
			descNo++;

		while (lastCycleSize > (pde->dmaOutChain[descNo].transferByteCount / 2L)) {
			lastCycleSize -= (pde->dmaOutChain[descNo++].transferByteCount / 2L);
			while (pde->dmaOutChain[descNo].descriptorPointer == 0xffffffffL)
				descNo++;
		}        
		pde->wf.finalDesc = descNo;
		pde->wf.finalDescCount = (ULONG) (lastCycleSize) * 2L;

		if (pde->wf.dynamic) {
			if ((pde->dmaOutChain[pde->wf.finalDesc].transferByteCount * 2) > 
			    pde->wf.stopCountInSamples) {

				pde->dmaOutChain[pde->wf.finalDesc].transferByteCount = pde->wf.finalDescCount;

				pde->dmaOutChain[pde->wf.finalDesc].descriptorPointer = pde->wf.padBlockDescPointer;
			}
		}
	}
	return ;
}


VOID
fillPadBlockForStaticMode(PDEVICE_EXTENSION pde)
{
	DWORD i, lastPointIndex, padIndex = 0;
	WORD *padPtr;
	dbg("Entered fillPadBlockForStaticMode \n");

	lastPointIndex = pde->wf.stopCount % pde->wf.singleWaveformSize;
	if (lastPointIndex == 0L)
		lastPointIndex = pde->wf.singleWaveformSize;
	lastPointIndex--;
    
    

	for (i = 0; i < 5; i++)
    {
		if (pde->dacOut[i].mode == DdomStaticWave)
        {            
			padPtr = pde->dacOut[i].buf + lastPointIndex;
			pde->pad[padIndex++] = *padPtr;
		}
	}
	fillPadBlock(pde);
    
	return ;
}


VOID
fillPadBlockForDynamicMode(PDEVICE_EXTENSION pde)
{
	PWORD bufAddr = (PWORD) pde->dmaOutBuf;
	DWORD i, lastCycleSize;

	dbg("Entered fillPadBlockForDynamicMode \n");

	lastCycleSize = pde->wf.stopCountInSamples % pde->dmaOutBufSize;

	if (lastCycleSize == 0)
		lastCycleSize = pde->dmaOutBufSize;

	if (lastCycleSize >= pde->wf.numWaveforms) {

		bufAddr = pde->dmaOutBuf + lastCycleSize - pde->wf.numWaveforms;
	} else {

		bufAddr =
		    pde->dmaOutBuf + pde->dmaOutBufSize + lastCycleSize - pde->wf.numWaveforms;
	}

	for (i = 0; i < pde->wf.numWaveforms; i++) {

		if ((bufAddr) > &pde->dmaOutBuf[pde->dmaOutBufSize])
			bufAddr = pde->dmaOutBuf;

		pde->pad[i] = *bufAddr++;
	}

	fillPadBlock(pde);
	return ;
}


DWORD
dacSetupBuffer(PDEVICE_EXTENSION pde)
{
	DWORD numStatic, numDynamic, waveformSize[5], allWaveformsSize;
	PWORD bufPhyAddr;
	PWORD waveformAddr[5];
	WORD i, j = 0;

	dbg("Entered dacSetupBuffer \n");

	numStatic = numDynamic = 0;
	for (i = 0; i < 5; i++) {
        //info("pde->dacOut[%i].bufMdl %i\n",i,pde->dacOut[i].bufMdl);
        //info("pde->testingOutputDma %i\n",pde->testingOutputDma);
        //info("pde->dacOut[%i].mode %i\n",i,pde->dacOut[i].mode);

		if (pde->dacOut[i].bufMdl || (pde->testingOutputDma && (pde->dacOut[i].mode == DdomStaticWave)))
        {            
			if (pde->dacOut[i].mode == DdomStaticWave)
				numStatic++;

			if (pde->dacOut[i].mode == DdomDynamicWave)
				numDynamic++;

			if (j == 0)
				pde->wf.dynamicWaveformBufAddr = pde->dacOut[i].buf;

			waveformAddr[j] = (PWORD) virt_to_phys((PVOID) pde->dacOut[i].buf);
			waveformSize[j++] = pde->dacOut[i].bufSize;

		}
	}

	if (numStatic && numDynamic)
    {        
		return DerrInvMode;
	}

	pde->wf.numWaveforms = numStatic + numDynamic;
	if (pde->wf.numWaveforms == 0)
    {        
		return DerrInvMode;
	}

	pde->wf.dynamicWaveformBufSize = waveformSize[0];
	for (i = 0; i < pde->wf.numWaveforms; i++) {
		if (waveformSize[i] != waveformSize[0])
			return DerrDacBuffersNotEqual;
	}

	if (pde->wf.dynamicWaveformBufSize == 0L)
    {        
		return DerrInvMode;
	}

	pde->wf.dmaStopPending = FALSE;
	bufPhyAddr = (PWORD) virt_to_phys((PVOID) pde->dmaOutBuf);
	pde->dmaOutBufSize = DMAOUTBUFSIZE - 0x800L - (((DWORD) bufPhyAddr & 0x00000fff) / 2L);
	pde->wf.padBlockStartIndex = pde->dmaOutBufSize;
	pde->wf.stopCountInSamples = pde->wf.stopCount * pde->wf.numWaveforms;

	if (numDynamic) {
		pde->wf.dynamic = TRUE;
		for (i = 0; i < pde->wf.numWaveforms; i++) {
			if (waveformAddr[i] != waveformAddr[0])
				return DerrDacBuffersNotEqual;
		}
		if (pde->wf.dynamicWaveformBufSize < 256L)
			return DerrDacBufferTooSmall;
	} else {
		pde->wf.dynamic = FALSE;
		allWaveformsSize = pde->wf.singleWaveformSize * numStatic;
		if (!(pde->wf.maxWaveformsPerBuf = pde->dmaOutBufSize / allWaveformsSize)) {
			return DerrDacNotEnoughMemory;
		}
		pde->dmaOutBufSize = (allWaveformsSize * pde->wf.maxWaveformsPerBuf);
	}
	return DerrNoError;
}


VOID
dacArm(PDEVICE_EXTENSION pde, daqIOTP p)
{
	DAC_ARM_PT dacArm = (DAC_ARM_PT) p;
	WORD i;
	DWORD errCode;
	WORD timeoutValue = 10000;

	dbg("Entered dacArm \n");
	if ((dacArm->dacDevType != DddtLocal)
	    && (dacArm->dacDevType != DddtLocalDigital)) {
		dacArm->errorCode = DerrInvType;
		return;
	}
	if (dacCheckStatus(pde, DdafWaveformActive)) {
		dacArm->errorCode = DerrDacWaveformActive;
		return;
	}
   
	dacDisarm(pde, NULL);

	pde->dacCurrentScanCount = pde->dacLastScanCount = 0x00000000L;
	pde->dacOldCounterValue = 0;
	pde->totalSamplesDmaed = pde->totalSamplesRemoved = 0L;
	pde->dmaOutBufHead = pde->dmaOutBuf;
    
	errCode = dacSetupBuffer(pde);    

	if (errCode)
    {
		dacArm->errorCode = errCode;
		return;
	}
    
	dacSetupDmaTransfer(pde);    
	primeDmaOutputBuffer(pde);    
	computePadBlockInfo(pde);

	if (!pde->wf.dynamic)
    {        
		fillPadBlockForStaticMode(pde);         
    }
    
	prepareToStopDmaOut(pde);    
	writeDacControl(pde, DacResetFifo);     

	for (i = 0; i < 5; i++)
    {
		if (pde->dacOut[i].mode != DdomVoltage)
        {            
			writeDacControl(pde, pde->dacOut[i].enableBits);
		}
	}
    
	writeDacControl(pde, DacResetFifo);    
	switch (pde->dacClockSource & 0x000000ffL)
    {
	    case DdcsExternalTTL:
		    pde->dacPacerClockSelection = DacPacerExternal;
		    if (!(pde->dacClockSource & DdcsFallingEdge))
			    pde->dacPacerClockSelection |= DacPacerExternalRising;
		    break;

	    case DdcsAdcClock:
		    pde->dacPacerClockSelection = DacPacerUseAdc;
		    break;

	    case DdcsDacClock:
		    pde->dacPacerClockSelection = DacPacerInternal;
		    if (pde->dacClockSource & DdcsOutputEnable)
			    pde->dacPacerClockSelection |= DacPacerInternalOutEnable;
		    writeDacPacerClock(pde, pde->dacPacerClockDivisor);
	}
    
	dacStartDmaTransfer(pde);
    
	while ((readDacStatus(pde) & DacFull) == 0x0000)
    {
		if (timeoutValue-- == 0)
			break;
	}

	if (pde->dacTrigSource == DdtsImmediate)
		writeDacControl(pde, (WORD) (pde->dacPacerClockSelection | DacPacerEnable));

	else if (pde->dacTrigSource == DdtsAdcClock) {
		writeDacControl(pde, pde->dacPacerClockSelection);
		pde->dacTriggerIsAdc = TRUE;
	}

	enterDacStatusMutex(pde);
	pde->dacStatusFlags |= DdafWaveformActive;
	pde->currentDacErrorCode = DerrNoError;
	leaveDacStatusMutex(pde);    
	kickstartDacEvent(pde);
    
	return ;
}


VOID
dacConfigure(PDEVICE_EXTENSION pde, daqIOTP p)
{
	DAC_CONFIG_PT dacConfig = (DAC_CONFIG_PT) p;
	DWORD divisor;

	dbg("Entered dacConfigure \n");

	if ((dacConfig->dacDevType != DddtLocal)
	    && (dacConfig->dacDevType != DddtLocalDigital)) {
		dacConfig->errorCode = DerrInvType;
		return;
	}

	if ((pde->dacStopEvent = dacConfig->dacUpdateMode) > DdwmInfinite) {
		dacConfig->errorCode = DerrInvOutputCtrl;
		return;
	}

	if (!(pde->wf.stopCount = dacConfig->dacUpdateCount)
	    && (dacConfig->dacUpdateMode < DdwmInfinite)) {
		dacConfig->errorCode = DerrInvCount;
		return;
	}

	pde->dacTrigSource = dacConfig->dacTrigSource & 0xfffeffffL;
	if (pde->dacTrigSource > DdtsAdcClock) {
		dacConfig->errorCode = DerrInvTrigSource;
		return;
	}

	switch ((pde->dacClockSource = dacConfig->dacClockSource) & 0x000000ffL) {

	case DdcsDacClock:
		divisor = dacConfig->dacPeriodmsec * 100;
		divisor += ((dacConfig->dacPeriodnsec + 5000L) / 10000L);
		divisor--;
		pde->dacPacerClockDivisor = (WORD) divisor;
		if (divisor > 0x0000ffffL)
			dacConfig->errorCode = DerrInvFreq;

	case DdcsAdcClock:
		break;

	case DdcsExternalTTL:
		break;

	default:
		dacConfig->errorCode = DerrInvClock;
	}
	return ;
}


VOID
dacStop(PDEVICE_EXTENSION pde, daqIOTP p)
{
	DWORD i;
	DWORD dwUnlock; // 20140529 Kamal - ERROR- This variable is not set before being used in 'if' test below.
	dbg("Entered dacStop \n");

	enterDacStatusMutex(pde);

	pde->dacStatusFlags &= ~DaafTransferActive;
	dbg("SETTING pde->dacThreadDie = TRUE; \n");
	pde->dacThreadDie = TRUE;    

	for (i = 0; i < 4; i++)
	{
		if (pde->dacOut[i].buf != NULL)
		{
			if (dwUnlock != NO_ERROR)
			{
			}
			else
			{
				pde->dacOut[i].buf = NULL;
			}
			pde->dacOut[i].bufMdl = NULL;
		}
	}  

	leaveDacStatusMutex(pde);
	return ;
}



VOID
dacStart(PDEVICE_EXTENSION pde, daqIOTP p)
{
	DAC_START_PT   start = (DAC_START_PT)p;
	DWORD ch;

	dbg("Welcome to dacStart\n");

	// First, check to make sure we can do this.
	if (!pde->usingAnalogOutputs)
	{
		info("dacStart: No Analog Outputs - Not Capable\n");
		start->errorCode = DerrNotCapable;
		return;
	}

	//Check device type for streaming digital:
	if (start->dacDevType == DddtLocalDigital)
	{
		if(!pde->usingDigitalIO)
		{
			// Check for P3 for streaming output -- DaqBook/2000 not capable
			info("dacStart: Digital Streaming - Not Capable\n");
			start->errorCode = DerrNotCapable;
			return;
		}
		ch = 4L;
	}
	else
	{
		// DAC Waveform output
		if (start->dacDevType == DddtLocal)
		{
			// Make sure this board has the requested DAC
			if(((ch = start->dacChannel) + 1L) > pde->numDacs)
			{
				// Error, DAC requested does not exist
				info("dacStart: Requested Analog Output Channel not Present - Not Capable\n");
				start->errorCode = DerrNotCapable;
				return;
			}
		}
		else
		{
			// Not a dac channel, invalid device type
			info("dacStart: Channel Requested not an analog output channel - Invalid Type\n");
			start->errorCode = DerrInvType;
			return;
		}
	}

	// Valid device type, validate the length
	if(start->dacXferBufLen)
	{
		dbg("start->dacXferBufLen %i\n",start->dacXferBufLen);

		//MAH: 04.23.04 let the hacking commence       

		if( pde->dacOutbuffer[ch] == NULL)
		{   
			pde->dacOutbuffer[ch] = kmalloc(start->dacXferBufLen * 2 , GFP_KERNEL);
			dbg(" mallocd Channel%i\n",ch);
			if (pde->dacOutbuffer[ch] == NULL)
			{
				start->errorCode = DerrMemLock;
				pde->dacOut[ch].bufMdl = NULL;
				return;
			}      
		}

		copy_from_user(pde->dacOutbuffer[ch] ,start->dacXferBuf,start->dacXferBufLen*2);            

		pde->dacOut[ch].buf = pde->dacOut[ch].bufHead = (WORD *)(pde->dacOutbuffer[ch]);

		pde->dacOut[ch].bufMdl = (DWORD *) TRUE; // we only use this as a FLAG : NOT an ADDRESS
		// Save the size of the waveform buffer

		pde->wf.singleWaveformSize = pde->dacOut[ch].bufSize = start->dacXferBufLen;        
		// set the transfer active bit
		pde->dacStatusFlags |= DdafTransferActive;
	}
	else
	{
		// Invalid buffer size
		info("dacStart: Invalid Buffer Size\n");
		start->errorCode = DerrInvBuf;
	}
}



VOID
dacGetStatus(PDEVICE_EXTENSION pde, daqIOTP p)
{
	DAC_STATUS_PT status = (DAC_STATUS_PT) p;
	dbg("Entered dacGetStatus \n");
	enterDacStatusMutex(pde);
	status->errorCode = pde->currentDacErrorCode;
	pde->currentDacErrorCode = DerrNoError;
	status->dacStatus = pde->dacStatusFlags;
	status->dacCurrentCount = pde->totalSamplesDmaed;
	status->dacReadCount = pde->totalSamplesRemoved;
	leaveDacStatusMutex(pde);
	return ;
}


BOOL
dacCheckFatalErrors(PDEVICE_EXTENSION pde, DWORD samplesDmaed)
{
	dbg("Entered dacCheckFatalErrors \n");
	if (readAcqStatus(pde) & DacPacerOverrun) {
		dacDisarm(pde, NULL);
		dacStop(pde, NULL);
		pde->currentDacErrorCode = DerrDacPacerOverrun;
		return TRUE;
	}

	if (!(pde->dacStatusFlags & DdafWaveformTriggered))
		if (samplesDmaed)
			pde->dacStatusFlags |= DdafWaveformTriggered;

	if (pde->wf.dynamic) {
		if (samplesDmaed > pde->wf.dynamicWaveformBufSize) {
			dacDisarm(pde, NULL);
			dacStop(pde, NULL);
			pde->currentDacErrorCode = DerrDacBufferUnderrun;
			return TRUE;
		}
	}
	return FALSE;
}


DWORD
computeSamplesDmaed(PDEVICE_EXTENSION pde)
{
	dbg("Entered computeSamplesDmaed \n");
	pde->dacLastScanCount = pde->dacCurrentScanCount;
	pde->dacCurrentScanCount = readDacScanCounter(pde);
	return ((pde->dacCurrentScanCount - pde->dacLastScanCount) * (DWORD) pde->wf.numWaveforms);
}


void
dacRepmov(PDEVICE_EXTENSION pde, DWORD samples)
{
	WORD *source;
	WORD *dest;
	dbg("Entered dacRepmov \n");
	source = pde->wf.dynamicWaveformBufHead;
	dest = pde->dmaOutBufHead;

	if (samples) {

	}
	return ;
}

void
memCopyFromUserBuf(PDEVICE_EXTENSION pde, DWORD samples)
{
	DWORD partialSamples = 0L;
	dbg("Entered memCopyFromUserBuf \n");
	if (samples) {

		if ((pde->dmaOutBufHead + samples) > &pde->dmaOutBuf[pde->dmaOutBufSize]) {
			partialSamples = &pde->dmaOutBuf[pde->dmaOutBufSize] - pde->dmaOutBufHead;
			dacRepmov(pde, partialSamples);
			pde->dmaOutBufHead = pde->dmaOutBuf;
		}
		dacRepmov(pde, samples - partialSamples);
	}
	return ;
}

VOID
copyUserSamplesToInternalBuf(PDEVICE_EXTENSION pde, DWORD samples){
	DWORD partialSamples = 0;
	dbg("Entered copyUserSamplesToInternalBuf \n");
	if (samples && dacCheckStatus(pde, DdafTransferActive)) {

		if ((pde->wf.dynamicWaveformBufHead + samples) >
		    (pde->wf.dynamicWaveformBufAddr + pde->wf.dynamicWaveformBufSize)) {

			partialSamples = (pde->wf.dynamicWaveformBufAddr + pde->wf.dynamicWaveformBufSize)
				- pde->wf.dynamicWaveformBufHead;

			memCopyFromUserBuf(pde, partialSamples);
			pde->wf.dynamicWaveformBufHead = pde->wf.dynamicWaveformBufAddr;
		}

		memCopyFromUserBuf(pde, samples - partialSamples);
		pde->totalSamplesRemoved += samples;
		if (pde->dacStopEvent != DdwmInfinite) {
			if (pde->totalSamplesRemoved > pde->wf.stopCountInSamples) {
				pde->totalSamplesRemoved = pde->wf.stopCountInSamples;
			}
		}
	}
	return ;
}

VOID
prepareToStopDmaOut(PDEVICE_EXTENSION pde)
{
	dbg("Entered prepareToStopDmaOut \n");
	if (pde->wf.dmaStopPending != FALSE) {
		return ;
	}

	if (pde->dacStopEvent == DdwmInfinite) {
		return ;
	}

	if (pde->wf.dynamic) {
		if (pde->totalSamplesRemoved >= pde->wf.stopCountInSamples) {
			fillPadBlockForDynamicMode(pde);
			pde->dmaOutChain[pde->wf.finalDesc].transferByteCount = pde->wf.finalDescCount;
			pde->dmaOutChain[pde->wf.finalDesc].descriptorPointer = pde->wf.padBlockDescPointer;
			pde->wf.dmaStopPending = TRUE;
		}
	} else {
		if ((pde->wf.stopCountInSamples - pde->totalSamplesDmaed) <= pde->dmaOutBufSize) {
			pde->dmaOutChain[pde->wf.finalDesc].transferByteCount = pde->wf.finalDescCount;
			pde->dmaOutChain[pde->wf.finalDesc].descriptorPointer = pde->wf.padBlockDescPointer;
			pde->wf.dmaStopPending = TRUE;
		}
	}

	return ;
}


static VOID
dacEventL(unsigned long data)
{

	//dbg("Entered dacEventL \n");
    BOOL bCallAgain = FALSE;
    DEVICE_EXTENSION *pde;
	pde = (DEVICE_EXTENSION *) data;
    
    
    if (pde->inDacEvent)
    {
        dbg("allready in adcevent ,,, Leaving");
		return;
	}
    pde->inDacEvent = TRUE;
    
    if (!pde->dacThreadDie)
     {
		if (pde->dmaOutActive)
        {
			bCallAgain = (BOOL) dacCheckEventsAndMoveData(pde);
		}
        else
        {
            dbg("dma not active\n");
        }
	}

    if(!bCallAgain)// && pde->dmaOutActive)
    {
        pde->dacXferThreadRunning = FALSE;
		pde->dacEventHandle = 0;
		pde->inDacEvent = FALSE;
		del_timer(&pde->dac_timer);
	} else {
		pde->dacXferThreadRunning = TRUE;
		pde->dacEventHandle = 0;
		pde->inDacEvent = FALSE;
		pde->dacTimeoutHandle = 1;
		pde->dac_timer.expires = jiffies + DAC_HZ;
		add_timer(&pde->dac_timer);
	}     
	return ;
}


DWORD
dacCheckEventsAndMoveData(PDEVICE_EXTENSION pde)
{
	DWORD sampleCount;
	dbg("Entered dacCheckEventsAndMoveData \n");
	sampleCount = computeSamplesDmaed(pde);
	pde->totalSamplesDmaed += sampleCount;

	if (!dacCheckFatalErrors(pde, sampleCount)) {

		if (pde->dacStopEvent != DdwmInfinite) {
			if (pde->totalSamplesDmaed >= pde->wf.stopCountInSamples) {
				pde->totalSamplesDmaed = pde->wf.stopCountInSamples;
				dacDisarm(pde, NULL);
				dacStop(pde, NULL);
				return (DWORD) pde->dmaOutActive;
			}
		}

		if (pde->wf.dynamic && !pde->wf.dmaStopPending) {

			if ((pde->totalSamplesRemoved & 0x80000000L) ==
			    (pde->totalSamplesDmaed & 0x80000000L)) {
				if (pde->totalSamplesRemoved < pde->totalSamplesDmaed) {
					dacDisarm(pde, NULL);
					dacStop(pde, NULL);
					pde->currentDacErrorCode = DerrDacBufferUnderrun;
					return (DWORD) pde->dmaOutActive;
				}
			}
			copyUserSamplesToInternalBuf(pde, sampleCount);
			prepareToStopDmaOut(pde);
		}
	}
	return (DWORD) pde->dmaOutActive;
}


VOID
pauseForDacEvent(PDEVICE_EXTENSION pde)
{
	dbg("Entered pauseForDacEvent \n");
	return ;
}


DWORD
dacTestOutputDma(PDEVICE_EXTENSION pde, DaqAdcFlag chType)
{
	daqIOT sb;
	DAC_CONFIG_PT dacConfig = (DAC_CONFIG_PT) & sb;
	WORD timeout, lastOutput;
	DWORD i, returnCount, waveformSize = 1000L;

	WORD *bufPtr = pde->dmaOutBuf;

	dbg("Entered dacTestOutputDma \n");

	dacDisarm(pde, NULL);
	dacStop(pde, NULL);

	lastOutput = readHSIOPort(pde);

	for (i = 0; i < waveformSize; i++) {
		*bufPtr++ = lastOutput;
	}

	for (i = 0; i < 5; i++) {
		pde->dacOut[i].mode = DdomVoltage;
		pde->dacOut[i].bufMdl = NULL;
	}

	pde->dacOut[4].mode = DdomStaticWave;
	pde->wf.singleWaveformSize = pde->dacOut[4].bufSize = waveformSize;
	pde->dacOut[4].buf = pde->dacOut[4].bufHead = pde->dmaOutBuf;
	dacConfig->dacDevType = DddtLocalDigital;
	dacConfig->dacUpdateMode = DdwmNShot;
	dacConfig->dacUpdateCount = waveformSize;
	dacConfig->dacTrigSource = DdtsImmediate;
	dacConfig->dacClockSource = DdcsDacClock;
	dacConfig->dacPeriodmsec = 0L;
	dacConfig->dacPeriodnsec = 10000L;

	pde->testingOutputDma = TRUE;
	dacConfigure(pde, &sb);
	dacArm(pde, &sb);
	pde->testingOutputDma = FALSE;

	timeout = 15;
	do {
		if (timeout-- == 0) {
			dacDisarm(pde, NULL);
			return 0L;
		}
		pauseForDacEvent(pde);
	} while (dacCheckStatus(pde, DdafWaveformActive));

	returnCount = pde->totalSamplesDmaed;
	dacDisarm(pde, NULL);

	if (returnCount == waveformSize)
		return returnCount * 100L;

	return 0L;
 }

