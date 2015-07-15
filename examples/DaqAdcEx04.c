////////////////////////////////////////////////////////////////////////////
//
// DaqAdcEx04.c                    I    OOO                           h
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
//      Single Channel, External TTL Start Trigger, Fixed Scan Length
//  Binary Data Saved to Disk
//
//--------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "daqx_linuxuser.h"

#define STARTSOURCE	DatsExternalTTL	// Use TTL Start Trigger
#define STOPSOURCE	DatsScanCount	// Stop when requested Scan Count is reached
#define FILENAME	"bindata.bin"	// name of file to save to
#define PREWRITE	0	            // Do Not "PreWrite" the file
#define CHANCOUNT	1	            // Single Channel
#define SCANS		500	            // 10000 Scans per Channel
#define RATE		1000	        // 1000 Hz Scan Rate

int main(void)
{
	FILE *exists;
    
	// DaqBoard Name and Device Handle
	PCHAR devName;
	DaqHandleT handle;

	// Buffer for Data Storage
	WORD buffer[SCANS * CHANCOUNT];

	// Channels, Gains, and Flags Array to setup the Acquisition
	DaqAdcGain gains[CHANCOUNT] = { DgainX1 };
	DWORD channels[CHANCOUNT] = { 0 };
	DWORD flags[CHANCOUNT] = { DafBipolar };

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

	// Set the acquisition type to NShot with 0 pre-trig scans and "SCANS" post-trig scans
	daqAdcSetAcq(handle, DaamNShot, 0, SCANS);

	// Pass the Channels, Gains, and Flags Arrays along with the channel count to setup the Acquisition
	daqAdcSetScan(handle, channels, gains, flags, CHANCOUNT);

	// Create a Driver managed buffer of SCANS length
	daqAdcTransferSetBuffer(handle, buffer, (SCANS), DatmDriverBuf);

	// Set to Trigger on TTL (STARTSOURCE) signal
	daqSetTriggerEvent(handle, STARTSOURCE, DetsRisingEdge,
			   channels[0], gains[0], flags[0],
			   DaqTypeAnalogLocal, 0.0, 0.0, DaqStartEvent);

	// Set to Stop when the requested number of scans (STOPSOURCE) is completed
	daqSetTriggerEvent(handle, STOPSOURCE, DetsFallingEdge,
			   channels[0], gains[0], flags[0],
			   DaqTypeAnalogLocal, 0.0, 0.0, DaqStopEvent);

	// Set the scan rate
	daqAdcSetFreq(handle, RATE);

	//Settings for destination file
	//test if file exists to determine write mode
	if ((exists = fopen(FILENAME, "r")))
    {
		daqAdcSetDiskFile(handle, FILENAME, DaomWriteFile, PREWRITE);
		fclose(exists);
	}
    else
    {
		daqAdcSetDiskFile(handle, FILENAME, DaomCreateFile, PREWRITE);
	}

	//set timeout to exit after 10 seconds if no TTL trigger occurs
	daqSetTimeout(handle, 10000);

	// Prepare for Data Transfer
	daqAdcTransferStart(handle);

	// Arm the Acquisition                  
	daqAdcArm(handle);
	printf("\n%s Armed\n\n", devName);

	printf("Waiting for TTL trigger...\n\n");

	// Test for Data or Timeout
	daqWaitForEvent(handle, DteAdcData);

	//Test to See if Timeout Ocured
	daqAdcTransferGetStat(handle, &active, &retCount);

	if (retCount)
    {
		printf("Scan triggered, Press <space> to halt scan\n\n");

		do
        {
			daqAdcTransferGetStat(handle, &active, &retCount);
			printf("Scans Acquired %i\r", retCount);
		}
		while (!Daqkeyhit() && retCount < 10000);	//keep max file size small

		printf("Scan Completed, %d scans performed\n", retCount);
	}
    else	//(retCount)
	{
		printf("TIMED OUT\n");
	}

	//Disarm when completed
	daqAdcDisarm(handle);

	//close device connections
	daqClose(handle);
	printf("\n%s Closed\n\n", devName);

	return 0;
}
