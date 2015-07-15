////////////////////////////////////////////////////////////////////////////
//
// DaqAdcEx07.c                    I    OOO                           h
//                                 I   O   O     t      eee    cccc   h
//                                 I  O     O  ttttt   e   e  c       h hhh
// -----------------------------   I  O     O    t     eeeee  c       hh   h
// copyright: (C) 2002 by IOtech   I   O   O     t     e      c       h    h
//    email: linux@iotech.com      I    OOO       tt    eee    cccc   h    h
//
////////////////////////////////////////////////////////////////////////////
//
// *************************************************************************
// *                                                                       *
// * This program is free software; you can redistribute it and/or modify  *
// * it under the terms of the GNU General Public License as published by  *
// * the Free Software Foundation; either version 2 of the License, or     *
// * (at your option) any later version.                                   *
// *                                                                       *
// *************************************************************************
//
////////////////////////////////////////////////////////////////////////////
//
// Please email liunx@iotech with any bug reports questions or comments
//
////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------
//
//  This Example uses:
//
//  Three (P2-A,B,C) Digital Channels
//  One (P3-CNT1) 16 bit Counter
//      One (P3-CNT0) 32 bit Counter Cascaded CNT0 CNT2
//  One (P3-D0..D15) 16 bit Digital Channel
//
//  With an immediate trigger and a finite scan count
//
//--------------------------------------------------------------------------    

#include <stdio.h>
#include "daqx_linuxuser.h"

///////////////////////////////////////
//Trigger and Scan Definitions
//
#define STARTSOURCE	DatsImmediate	// Use Immediate Trigger
#define STOPSOURCE	DatsScanCount	// Stop when the requested scan count is reached
#define CHANCOUNT	7	            // 7 total channels
#define SCANS		25	            // 25 Scans per Channel
#define RATE		10	            // 10 Hz Scan Rate (use a low rate to allow for collection of pulses)

int main(void)
{
	int i,j;
    
	// DaqBoard Name and Device Handle
	PCHAR devName;
	DaqHandleT handle;

	// Buffer for Data Storage
	WORD buffer[SCANS * CHANCOUNT];

	// Channels, Gains, and Flags Array to setup the Acquisition
	DWORD channels[CHANCOUNT] = { 0,	/* P2(A) local digital */
				      1,	/* P2(B) local digital */
				      2,	/* P2(C) local digital */
				      1,	/* P3(CTR1) 16 bit counter */
				      0,	/* P3(CTR0) 32 bit counter LOW (cascaded) */
				      2,	/* P3(CTR2) 32 bit counter HIGH (cascaded) */
				      0
	};			/* P3 16bit Digital */

	DaqAdcGain gains[CHANCOUNT] = { DgainX1,
					DgainX1,
					DgainX1,
					DgainX1,
					DgainX1,
					DgainX1,
					DgainX1
	};

	DWORD flags[CHANCOUNT] = { DafP2Local8,
				   DafP2Local8,
				   DafP2Local8,
				   DafCtr16 | DafCtrTotalize,
				   DafCtr32Low | DafCtrPulse,
				   DafCtr32High | DafCtrPulse,
				   DafP3Local16
	};

	// Acquisition Status Variables
	DWORD active, retCount;

	devName = "daqBoard2k0";

    // Open the DaqBoard2000 and get the handle
    printf("\n\nAttempting to Connect with %s\n", devName );
    handle = daqOpen("daqboard2k0");
    
    if (handle == NO_GRIP)
    {
        printf("Could not connect to %s\n",devName);
        return 2;
    }
    printf("%s opened\n\n",devName);                        

	// set # of scans to perform and scan mode
	daqAdcSetAcq(handle, DaamNShot, 0, SCANS);

	//Scan settings
	daqAdcSetScan(handle, channels, gains, flags, CHANCOUNT);

	//set scan rate
	daqAdcSetFreq(handle, RATE);

	//Set buffer location, size and flag settings
	daqAdcTransferSetBuffer(handle, buffer, SCANS,
				DatmUpdateSingle | DatmDriverBuf);

	//Set to Trigger on software trigger
	daqSetTriggerEvent(handle, STARTSOURCE,
			   (DaqEnhTrigSensT) TRUE, channels[0],
			   gains[0], flags[0],
			   DaqTypeAnalogLocal, 0.0, 0.0, DaqStartEvent);
	//Set to Stop when the requested number of scans is completed
	daqSetTriggerEvent(handle, STOPSOURCE,
			   (DaqEnhTrigSensT) NULL, channels[0],
			   gains[0], flags[0],
			   DaqTypeAnalogLocal, 0.0, 0.0, DaqStopEvent);

	//begin data acquisition
	printf("\nScanning...\n");
	daqAdcTransferStart(handle);
	daqAdcArm(handle);

	do
    {
		//transfer data (voltage readings) into computer memory and halt acquisition when done
		daqAdcTransferGetStat(handle, &active, (PDWORD) & retCount);
		printf("Scans Acquired %i\r", retCount);
	}
	while (active & DaafAcqActive);

	// Wait For the requested number of SCANS to become available and transfer the data
	// or timeout
	daqAdcTransferBufData(handle, buffer, SCANS, DabtmWait, &retCount);

	//Disarm when completed
	daqAdcDisarm(handle);
	printf("Scan Completed\n\n");

	//No scaling is necessary for Digital ports or counters
	//print scan data

	printf("P2 ch0\tP2 ch1\tP2 ch2\tCounter 16Bit\tCounter 32Bit\tP3 Dig\n");

	for (j = 0; j < SCANS; j++)
    {
		//you must left shift the 32High data 16 bits then add the 32BitLow to get 
		//correct 32 bit measurement
		DWORD bit32 = ((DWORD) buffer[j * CHANCOUNT + 5] << 16)
			+ (DWORD) buffer[j * CHANCOUNT + 4];
		for (i = 0; i < 3; i++)
			printf("%4d\t", buffer[i + j * CHANCOUNT]);
		printf("%6d Cnts\t", (DWORD) buffer[j * CHANCOUNT + 3]);
		printf("%6.1f Hz\t", (float) bit32 * RATE);
		printf("%d", (DWORD) buffer[j * CHANCOUNT + 6]);
		printf("\n");
	}
	//close device connections
	daqClose(handle);
	printf("\n%s Closed\n\n", devName);

	return 0;
}
