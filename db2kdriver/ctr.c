////////////////////////////////////////////////////////////////////////////
//
// ctr.c                           I    OOO                           h
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
#include "w32ioctl.h"

static DWORD startCh, endCh;


VOID
configureCtr(PDEVICE_EXTENSION pde, WORD ctrNo, WORD clearCtr)
{
	WORD controlWord;
	if ((ctrNo == 0) || (ctrNo == 2) || (ctrNo == 4)) {
		if (pde->counter[0].cascade == DcovCounterCascade) {
			writeTmrCtrControl(pde, Counter0CascadeEnable);
		} else {
			writeTmrCtrControl(pde, Counter0CascadeDisable);
		}
	}

	if ((ctrNo == 1) || (ctrNo == 3) || (ctrNo == 4)) {
		if (pde->counter[1].cascade == DcovCounterCascade) {
			writeTmrCtrControl(pde, Counter1CascadeEnable);
		} else {
			writeTmrCtrControl(pde, Counter1CascadeDisable);
		}
	}

	controlWord = pde->counter[ctrNo].opCode | clearCtr;

	if (pde->counter[ctrNo].clearOnRead == DcovCounterClearOnRead) {
		controlWord |= CounterClearOnRead;
	}

	if (pde->counter[ctrNo].edge == DcovCounterRisingEdge) {
		controlWord |= CountRisingEdges;
	} else {
		controlWord |= CountFallingEdges;
	}

	if (pde->counter[ctrNo].enable == DcovCounterOn) {
		controlWord |= CounterEnable;
	}

	writeTmrCtrControl(pde, controlWord);
	return ;
}


VOID
initializeCtrs(PDEVICE_EXTENSION pde)
{
	/*PEK: many magic numbers here */
	WORD i, opCode = 0x0070;

	for (i = 0; i < 5; i++) {
		pde->counter[i].opCode = opCode;
		pde->counter[i].cascade = DcovCounterSingle;
		pde->counter[i].clearOnRead = DcovCounterClearOnRead;
		pde->counter[i].edge = DcovCounterRisingEdge;
		pde->counter[i].enable = DcovCounterOff;
		if (i == 3)
			opCode = 0x00f0;
		else
			opCode += 0x0010;
	}
	return;
}


VOID
configureTmr(PDEVICE_EXTENSION pde, WORD tmrNo)
{
	if (pde->timer[tmrNo].enable == DcovTimerOn) {
		writeTmrCtrControl(pde, pde->timer[tmrNo].opCode | TimerEnable);
	} else {
		writeTmrCtrControl(pde, pde->timer[tmrNo].opCode | TimerDisable);
	}
	return ;
}

VOID
initializeTmrs(PDEVICE_EXTENSION pde)
{
	WORD i, opCode = 0x0000;
	for (i = 0; i < 3; i++) {
		pde->timer[i].opCode = opCode;
		pde->timer[i].timerDivisor = 0;
		pde->timer[i].enable = DcovTimerOff;

		if (i == 1)
			opCode = 0x0040;
		else
			opCode += 0x0010;
	}
	return ;
}


BOOL
validateOptions(PDEVICE_EXTENSION pde, ADC_CHAN_OPT_PT opt)
{

	if ((startCh < 0) || (startCh > 3) || (endCh < 0) || (endCh > 3)
	    || (startCh > endCh)) {
		opt->errorCode = DerrInvCtrNum;
		return FALSE;
	}

	switch (opt->chanOptType) {
	case DcotCounterCascade:
		switch (opt->optValue) {
		case DcovCounterSingle:
		case DcovCounterCascade:
			return TRUE;

		default:
			opt->errorCode = DerrInvOptionValue;
			return FALSE;
		}

	case DcotCounterMode:
		switch (opt->optValue) {
		case DcovCounterClearOnRead:
		case DcovCounterTotalize:
			return TRUE;

		default:
			opt->errorCode = DerrInvOptionValue;
			return FALSE;
		}

	case DcotCounterControl:
		switch (opt->optValue) {
		case DcovCounterOff:
		case DcovCounterOn:
		case DcovCounterManualClear:
			return TRUE;

		default:
			opt->errorCode = DerrInvOptionValue;
			return FALSE;
		}

	case DmotCounterControl:
		switch (opt->optValue) {
		case DcovCounterOff:
		case DcovCounterOn:
		case DcovCounterManualClear:
			startCh = endCh = 4;
			pde->counter[4].clearOnRead = pde->counter[3].clearOnRead;
			return TRUE;

		default:
			opt->errorCode = DerrInvOptionValue;
			return FALSE;
		}

	case DcotCounterEdge:
		switch (opt->optValue) {
		case DcovCounterFallingEdge:
		case DcovCounterRisingEdge:
			return TRUE;

		default:
			opt->errorCode = DerrInvOptionValue;
			return FALSE;
		}

	case DcotTimerDivisor:
		if (opt->optValue > 0x0000ffffL) {
			opt->errorCode = DerrInvDiv;
			return FALSE;
		}
		return TRUE;

	case DcotTimerControl:
		switch (opt->optValue) {
		case DcovTimerOff:
		case DcovTimerOn:
			if ((startCh == 2) || (startCh == 3) || (endCh == 2)
			    || (endCh == 3)) {
				opt->errorCode = DerrInvCtrNum;
				return FALSE;
			}
			return TRUE;

		default:
			opt->errorCode = DerrInvOptionValue;
			return FALSE;

		}

	case DmotTimerControl:
		switch (opt->optValue) {
		case DcovTimerOff:
		case DcovTimerOn:
			if ((startCh == 2) || (startCh == 3) || (endCh == 2)
			    || (endCh == 3)) {
				opt->errorCode = DerrInvCtrNum;
				return FALSE;
			}
			startCh = endCh = 2;
			return TRUE;

		default:
			opt->errorCode = DerrInvOptionValue;
			return FALSE;
		}

	default:
		opt->errorCode = DerrInvOptionType;
		return FALSE;
	}
	return TRUE;
}


VOID
ctrSetOption(PDEVICE_EXTENSION pde, daqIOTP p)
{
	ADC_CHAN_OPT_PT ctrOpt = (ADC_CHAN_OPT_PT) p;
	WORD i;

	startCh = (WORD) ctrOpt->startChan;
	endCh = (WORD) ctrOpt->endChan;

	if (!pde->usingCounterTimers) {
		ctrOpt->errorCode = DerrNotCapable;
		return ;
	}

	if (! validateOptions(pde, ctrOpt)) {
		return;
	}


	switch (ctrOpt->operation) {
	case 0:
		switch (ctrOpt->chanOptType) {
		case DcotCounterCascade:
			ctrOpt->optValue = pde->counter[startCh].cascade;
			break;

		case DcotCounterMode:
			ctrOpt->optValue = pde->counter[startCh].clearOnRead;
			break;

		case DcotCounterControl:
		case DmotCounterControl:
			ctrOpt->optValue = pde->counter[startCh].enable;
			break;

		case DcotTimerDivisor:
			ctrOpt->optValue = pde->timer[startCh].timerDivisor;
			break;

		case DcotTimerControl:
		case DmotTimerControl:
			ctrOpt->optValue = pde->timer[startCh].enable;
			break;

		default:
			ctrOpt->errorCode = DerrInvOptionType;
		}
		break;

	case 1:
		switch (ctrOpt->chanOptType) {
		case DcotCounterCascade:
			for (i = startCh; i <= endCh; i++) {
				switch (i) {
				case 0:
				case 2:
					pde->counter[0].cascade = (WORD) ctrOpt->optValue;
					pde->counter[2].cascade = (WORD) ctrOpt->optValue;
					configureCtr(pde, 0, 0);
					configureCtr(pde, 2, 0);
					break;

				case 1:
				case 3:
					pde->counter[1].cascade = (WORD) ctrOpt->optValue;
					pde->counter[3].cascade = (WORD) ctrOpt->optValue;
					configureCtr(pde, 1, 0);
					configureCtr(pde, 3, 0);
				}
			}
			break;

		case DcotCounterMode:
			for (i = startCh; i <= endCh; i++) {
				pde->counter[i].clearOnRead = (WORD) ctrOpt->optValue;
				configureCtr(pde, i, 0);
			}
			break;

		case DcotCounterEdge:
			for (i = startCh; i <= endCh; i++) {
				pde->counter[i].edge = (WORD) ctrOpt->optValue;
				configureCtr(pde, i, 0);
			}
			break;

		case DcotCounterControl:
			for (i = startCh; i <= endCh; i++) {
				if (ctrOpt->optValue == DcovCounterManualClear) {
					configureCtr(pde, i, CounterClear);
				} else {
					pde->counter[i].enable = (WORD) ctrOpt->optValue;
					configureCtr(pde, i, 0);
					pde->counter[4].enable = DcovCounterOff;
				}
			}
			break;

		case DmotCounterControl:
			if (ctrOpt->optValue == DcovCounterManualClear) {
				configureCtr(pde, 4, CounterClear);
			} else {
				for (i = 0; i < 5; i++) {
					pde->counter[i].enable = (WORD) ctrOpt->optValue;
				}
				configureCtr(pde, 4, 0);
			}
			break;

		case DcotTimerDivisor:
			for (i = startCh; i <= endCh; i++) {
				pde->timer[i].timerDivisor = (WORD) ctrOpt->optValue;
				writeTmrDivisor(pde, i, pde->timer[i].timerDivisor);
			}
			break;

		case DcotTimerControl:
			for (i = startCh; i <= endCh; i++) {
				pde->timer[i].enable = (WORD) ctrOpt->optValue;
				configureTmr(pde, i);
			}
			pde->timer[2].enable = DcovTimerOff;
			break;

		case DmotTimerControl:
			for (i = 0; i < 3; i++) {
				pde->timer[i].enable = (WORD) ctrOpt->optValue;
			}
			configureTmr(pde, 2);
			break;

		default:
			ctrOpt->errorCode = DerrInvOptionType;
		}
		break;

	default:
		ctrOpt->errorCode = DerrInvCtrCmd;
	}

	return ;
}


VOID
ctrRead(PDEVICE_EXTENSION pde, daqIOTP p)
{
	CTR_READ_PT ctrRead = (CTR_READ_PT) p;
	DWORD ctrNo;

	if (!pde->usingCounterTimers) {
		ctrRead->errorCode = DerrNotCapable;
		return ;
	}

	if (ctrRead->ctrDevType != CtrLocal9513) {
		ctrRead->errorCode = DerrInvType;
		return;
	}

	ctrNo = ctrRead->ctrPortOffset2;

	if ((ctrNo < 0) || (ctrNo > 3)) {
		ctrRead->errorCode = DerrInvCtrNum;
		return;
	}

	ctrRead->ctrValue2 = (DWORD) readCtrInput(pde, ctrNo);
	return ;
}
