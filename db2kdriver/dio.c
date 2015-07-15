////////////////////////////////////////////////////////////////////////////
//
// dio.c                           I    OOO                           h
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

VOID
dioRead(PDEVICE_EXTENSION pde, daqIOTP p)
{
	DIO_READ_PT dioRead = (DIO_READ_PT) p;

	if (!pde->usingDigitalIO) {
		dioRead->errorCode = DerrNotCapable;
		return ;
	}

	switch (dioRead->dioDevType) {
	case DioTypeLocal:
		switch (dioRead->dioWhichExpPort) {
		case DioepP2:
			if (dioRead->dioPortOffset > 3) {
				dioRead->errorCode = DerrInvPort;
				break;
			}
			writeDioControl(pde, p2Is82c55);
			dioRead->dioValue =
				(DWORD) read8255Port(pde, dioRead->dioPortOffset);

			break;

		case DioepP3:
			if (dioRead->dioPortOffset) {
				dioRead->dioValue = pde->p3HSIOConfig;
			} else {
				dioRead->dioValue = (DWORD) readHSIOPort(pde);
			}
			break;

		default:
			dioRead->errorCode = DerrInvPort;
			break;
		}
		break;

	case DioTypeExp:
		if (dioRead->dioPortOffset > 31) {
			dioRead->errorCode = DerrInvPort;
			break;
		}
		writeDioControl(pde, p2IsExpansionPort);
		dioRead->dioValue = (DWORD) readExpansionIOPort(pde, dioRead->dioPortOffset);
		break;

	default:
		dioRead->errorCode = DerrInvType;
		break;
	}

	return ;
}


VOID
dioWrite(PDEVICE_EXTENSION pde, daqIOTP p)
{
	DIO_WRITE_PT dioWrite = (DIO_WRITE_PT) p;
	WORD portNo, portVal, mask;

	if (! pde->usingDigitalIO) {
		dioWrite->errorCode = DerrNotCapable;
		return ;
	}

	portNo = (WORD) dioWrite->dioPortOffset;
	portVal = (WORD) dioWrite->dioValue;
	mask = (WORD) dioWrite->dioBitMask;

	switch (dioWrite->dioDevType) {
	case DioTypeLocal:

		switch (dioWrite->dioWhichExpPort) {
		case DioepP2:
			if (dioWrite->dioPortOffset > 3) {
				dioWrite->errorCode = DerrInvPort;
				break;
			}
			writeDioControl(pde, p2Is82c55);
			pde->p28255OutputShadow[portNo] =
				((portVal & mask) | (pde->p28255OutputShadow[portNo] & ~mask));
			write8255Port(pde, portNo, pde->p28255OutputShadow[portNo]);
			break;

		case DioepP3:
			if (dioWrite->dioPortOffset) {
				if ((pde->p3HSIOConfig = dioWrite->dioValue))
					writeDioControl(pde, p3HSIOIsOutput);
				else
					writeDioControl(pde, p3HSIOIsInput);
			} else {
				pde->p3HSIOOutputShadow =
					((portVal & mask) | (pde->p3HSIOOutputShadow & ~mask));
				writeHSIOPort(pde, pde->p3HSIOOutputShadow);
			}
			break;

		default:
			dioWrite->errorCode = DerrInvPort;
			break;
		}
		break;

	case DioTypeExp:
		if (dioWrite->dioPortOffset > 31) {
			dioWrite->errorCode = DerrInvPort;
			break;
		}
		writeDioControl(pde, p2IsExpansionPort);
		pde->p2ExpansionOutputShadow[portNo] =
			((portVal & mask) | (pde->p2ExpansionOutputShadow[portNo] & ~mask));
		writeExpansionIOPort(pde, portNo, pde->p2ExpansionOutputShadow[portNo]);
		break;

	default:
		dioWrite->errorCode = DerrInvType;
		break;
	}

	return ;
}


DWORD
dioTestInputSpeed(DWORD count)
{
	return 1;
}


DWORD
dioTestOutputSpeed(DWORD count)
{
	return 1;
}
