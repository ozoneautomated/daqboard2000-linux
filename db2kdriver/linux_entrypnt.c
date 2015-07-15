////////////////////////////////////////////////////////////////////////////
//
// linux_entrypnt.c                I    OOO                           h
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

VOID entryTest(PDEVICE_EXTENSION pde, daqIOTP p);

VOID
entryIoctl(PDEVICE_EXTENSION pde, DWORD ioctlCode, daqIOTP pIn)
{
	//info("code=0x%08x p1:%d p2:%d, p3:%d, p4:%d, p5:%d", ioctlCode, pIn->p1, pIn->p2, pIn->p3, pIn->p4, pIn->p5);

	switch (ioctlCode) {
	case -1:
		break;

	case DB_DAQ_ONLINE:
		info("DB_DAQ_ONLINE");
		pIn->p1 = daqOnline();
		break;

	case DB_DAQ_TEST:
		info("DB_DAQ_TEST");
		entryTest(pde, pIn);
		break;

	case DB_REG_READ:
		{
			DAQPIRP daqIoIrp;
			info("DB_REG_READ");
			daqIoIrp.MajorFunction = DAQ_IO_READ;
			daqIoIrp.Address = (WORD) pIn->p1;
			daqIoIrp.SystemBuffer = (PVOID) & pIn->p2;
			IoDaqCallDriver(pde, &daqIoIrp);
		}
		break;

	case DB_REG_WRITE:
		{
			DAQPIRP daqIoIrp;
			info("DB_REG_WRITE");
			daqIoIrp.MajorFunction = DAQ_IO_WRITE;
			daqIoIrp.Address = (WORD) pIn->p1;
			daqIoIrp.SystemBuffer = (PVOID) & pIn->p2;
			IoDaqCallDriver(pde, &daqIoIrp);
		}
		break;

	case DB_DIO_READ:
		info("DB_DIO_READ");
		dioRead(pde, pIn);
		break;

	case DB_DIO_WRITE:
		info("DB_IO_WRITE");
		dioWrite(pde, pIn);
		break;

	case DB_CTR_READ:
		info("DB_CTR_READ");
		ctrRead(pde, pIn);
		break;

	case DB_ADC_CHAN_OPT:
		info("DB_ADC_CHAN_OPT");
		ctrSetOption(pde, pIn);
		break;

	case DB_ADC_CONFIG:
		info("DB_ADC_CONFIG");
		adcConfigure(pde, pIn);
		break;

	case DB_ADC_SOFTTRIG:
		info("DB_ADC_SOFTTRIG");
		adcSoftTrig(pde, pIn);
		break;

	case DB_ADC_ARM:
		info("DB_ADC_ARM");
		adcArm(pde, pIn);
		break;

	case DB_ADC_DISARM:
		info("DB_ADC_DISARM");
		adcDisarm(pde, pIn);
		break;

	case DB_ADC_START:
		info("DB_ADC_START");
		adcStart(pde, pIn);
		break;

	case DB_ADC_STOP:
		info("DB_ADC_STOP");
		adcStop(pde, pIn);
		break;

	case DB_ADC_STATUS:
		//info("DB_ADC_STATUS");
		adcGetStatus(pde, pIn);
		break;

	case DB_ADC_STATUS_CHG:
		info("DB_ADC_STATUS_CHG");
		waitForStatusChg(pde, pIn);
		break;

	case DB_CAL_CONFIG:
		info("DB_CAL_CONFIG");
		calConfigure(pde, pIn);
		break;

#if 0
	case DB_SET_OPTION:
		info("DB_SET_OPTION");
		setOption(pde, pIn);
		break;
#endif

	case DB_DEV_DSP_CMDS:
		info("DB_DEV_DSP_CMDS");
		adcDevDspCmds(pde, pIn);
		break;

	case DB_DAC_MODE:
		info("DB_DAC_MODE");
		dacMode(pde, pIn);
		break;

	case DB_DAC_WRITE_MANY:
		info("DB_WRITE_MANY");
		dacWriteMany(pde, pIn);
		break;

	case DB_DAC_CONFIG:
		info("DB_DAC_CONFIG");
		dacConfigure(pde, pIn);
		break;

	case DB_DAC_SOFTTRIG:
		info("DB_DAC_SOFTTRIG");
		dacSoftTrig(pde, pIn);
		break;

	case DB_DAC_ARM:
		info("DB_DAC_ARM");
		dacArm(pde, pIn);
		break;

	case DB_DAC_DISARM:
		info("DB_DAC_DISARM");
		dacDisarm(pde, pIn);
		break;

	case DB_DAC_START:
		info("DB_DAC_START");
		dacStart(pde, pIn);
		break;

	case DB_DAC_STOP:
		info("DB_DAC_STOP");
		dacStop(pde, pIn);
		break;

	case DB_DAC_STATUS:
		//info("DB_DAC_STATUS");
		dacGetStatus(pde, pIn);
		break;

	case DB_DDVER:
		info("===========DAQIOCTL_DDVER============\n");
		break;

	case DB_ADC_HWINFO:
		info("DB_ADC_HWINFO");
		adcGetHWInfo(pde, pIn);
		break;

	default:
		info("Unknown IOCTL cmd=%d", ioctlCode);
		pIn->errorCode = DerrNotCapable;
		break;

	}
	return ;
}


VOID
entryTest(PDEVICE_EXTENSION pde, daqIOTP p)
{
	DAQ_TEST_PT daqTestP = (DAQ_TEST_PT) p;

	daqTestP->available = FALSE;
	daqTestP->result = 0;

	switch (daqTestP->command) {

	case DtstBaseAddressValid:
		daqTestP->available = TRUE;
		daqTestP->result = pde->busAndSlot;
		break;

	case DtstDmaChannelValid:
		daqTestP->result = 1;

		if (pde->usingAnalogInputs) {
			daqTestP->available = TRUE;
			if (adcTestInputDma(pde, DafAnalog))
				daqTestP->result = 0L;
			else
				break;
		} else if (pde->usingDigitalIO) {
			daqTestP->available = TRUE;
			if (adcTestInputDma(pde, DafScanDigital))
				daqTestP->result = 0L;
			else
				break;
		}

		if (pde->usingAnalogOutputs) {
		} else if (pde->usingDigitalIO) {
			/* ???PEK: why blank ???*/
		}
		break;

	case DtstAdcFifoInputSpeed:
		if (pde->usingAnalogInputs) {
			daqTestP->available = TRUE;
			daqTestP->result = adcTestInputDma(pde, DafAnalog);
		}
		break;

	case DtstDacFifoOutputSpeed:
		if (pde->usingAnalogOutputs) {
		}
		break;

	case DtstIOInputSpeed:
		if (pde->usingDigitalIO) {
			daqTestP->available = TRUE;
			daqTestP->result = adcTestInputDma(pde, DafScanDigital);
		}
		break;

	case DtstIOOutputSpeed:
		if (pde->usingDigitalIO) {
		}
		break;

	case DtstFifoAddrDataBusValid:
	case DtstFifoMemCellValid:
	case DtstHardwareCompatibility:
	case DtstFirmwareCompatibility:
	case DtstInterruptLevelValid:
		break;

	default:
		daqTestP->errorCode = DerrInvCommand;
		break;
	}
	return ;
}
