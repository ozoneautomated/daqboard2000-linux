////////////////////////////////////////////////////////////////////////////
//
// daqio.h                         I    OOO                           h 
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


#ifndef DAQIO_H
#define DAQIO_H

typedef struct {
	DWORD MajorFunction;
	DWORD MinorFunction;
	DWORD Status;
	PVOID SystemBuffer;
	DWORD Information;
	DWORD Address;
} DAQPIRP;

typedef enum {
	DaqIoNoErrors = 0x00,
	DaqIoNotOpen = 0x01,
	DaqIoNoInterface = 0x02,
	DaqIoNoDevice = 0x03,
	DaqIoBadCommand = 0x04,
	DaqIoBadAddress = 0x05,
} DAQIOSTATUS;

#define DAQ_IO_READ		0
#define DAQ_IO_READDWORD	1
#define DAQ_IO_READBLOCK	2
#define DAQ_IO_WRITE	        3

#define DAQ_IO_WRITEDWORD	4
#define DAQ_IO_WRITEBLOCK	5
#define DAQ_IO_STATUS		6
#define DAQ_IO_OPEN		7
#define DAQ_IO_CLOSE	        8

extern DAQIOSTATUS IoDaqInitialize(PDEVICE_EXTENSION DeviceExtension, DAQPIRP * Irp);
extern DAQIOSTATUS IoDaqOpen(PDEVICE_EXTENSION DeviceExtension, DAQPIRP * Irp);
extern DAQIOSTATUS IoDaqClose(PDEVICE_EXTENSION DeviceExtension, DAQPIRP * Irp);
extern VOID IoDaqUnload(PDEVICE_EXTENSION DeviceExtension);
extern DAQIOSTATUS IoDaqCallDriver(PDEVICE_EXTENSION DeviceExtension, DAQPIRP * Irp);

DWORD
readDacScanCounter(PDEVICE_EXTENSION pde);

#endif
