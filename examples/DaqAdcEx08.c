////////////////////////////////////////////////////////////////////////////
//
// DaqAdcEx08.c                    I    OOO                           h
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
// 2 Channel, Software Start Trigger, Infinite Acquisition
//
//--------------------------------------------------------------------------

#include <stdio.h>
#include "daqx_linuxuser.h"

///////////////////////////////////////
//Trigger and Scan Definitions
//

#define STARTSOURCE		DatsSoftware	// Use Software Start Trigger
#define STOPSOURCE		DatsScanCount	// Stop when requested Scan Count is reached (overridden by InfinitePost)
#define CHANCOUNT		2	            // 2 Total Channels
#define SCANS			25	            // Acquire 25 Scans at a time
#define RATE			50	            // 50 Hz Scan Rate
#define TRIGSENSE		DetsRisingEdge	// Rising Edge (overridden by Software Trigger)

int main(void)
{

	// DaqBoard Name and Device Handle
	PCHAR devName;
	DaqHandleT handle;
	unsigned int i,j;
	int rep = 1;
	// Base Unit Voltage Parameters
	DWORD daq10V = 1;
	DWORD compMask = daq10V;

	// Buffer for Data Storage
	WORD buffer[SCANS * CHANCOUNT];
    int buffsize = 0;

	// Channels, Gains, and Flags Array to setup the Acquisition
	DaqAdcGain gains[CHANCOUNT] = { DgainX1, DgainX1 };
	DWORD channels[CHANCOUNT] = { 0, 2 };
	DWORD flags[CHANCOUNT] = { DafBipolar, DafBipolar };

	// Acquisition Status Variables
	DWORD retCount;
	DWORD totalScans = 0;

	// Scaling Variables
	float maxVolt;
	float scale[CHANCOUNT];
	float offset[CHANCOUNT];
	float conv_buffer[SCANS * CHANCOUNT];

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

	// Set the acquisition type to InfinitePost with 0 pre-trig scans and "SCANS" post-trig scans
	daqAdcSetAcq(handle, DaamInfinitePost, 0, SCANS);

	// Pass the Channels, Gains, and Flags Arrays along with the channel count to setup the Acquisition

	daqAdcSetScan(handle, channels, gains, flags, CHANCOUNT);

	// Create a Driver managed buffer of BUFFSIZE * CHANCOUNT * 2 length
    // or min size 4096
	
    if(SCANS * CHANCOUNT * 2 < 4096)
    {
        buffsize = 4096;
    }
    else
    {
        buffsize = SCANS * CHANCOUNT * 2;
    }

    daqAdcTransferSetBuffer(handle, buffer, buffsize
				, DatmCycleOn | DatmDriverBuf);


	// Set Start Trigger Event for STARTSOURCE (software trigger)
	daqSetTriggerEvent(handle, STARTSOURCE, TRIGSENSE,
			   channels[0], gains[0], flags[0],
			   DaqTypeAnalogLocal, 0.0, 0.0, DaqStartEvent);

	// Set Start Trigger Event for STOPSOURCE
	daqSetTriggerEvent(handle, STOPSOURCE, TRIGSENSE,
			   channels[0], gains[0], flags[0],
			   DaqTypeAnalogLocal, 0.0, 0.0, DaqStopEvent);

	// Set the scan rate
	daqAdcSetFreq(handle, RATE);

	// Prepare for Data Transfer
	daqAdcTransferStart(handle);

	// Arm the Acquisition                  
	daqAdcArm(handle);

	printf("\nAcquisition Armed...\nHit <space> to begin and halt scan\n");

	while (!Daqkeyhit());

	//issue software trigger
	daqAdcSoftTrig(handle);

	//convert the data to volts:

	//DaqBoards convert all data to an unsigned, 16-bit number (range 0 to 65535.  Zero corresponds 
	//to the minimum voltage, which is -maxVolt if in bipolar mode, or zero if unipolar.  
	//65535 corresponds to maxVolt if bipolar, or 2xmaxVolt if unipolar.  Note that a voltage 
	//higher than the device's absolute range (+/-10V for DaqBoard2000, +/-5V for other Daq* devices)
	//can damage the device.  Setting flags and gain settings which indicate a voltage higher than 
	//the device's max, such as an unipolar, unscaled scan will result in an error before the scan 
	//is performed.
	//
	//The following routine automatically determines the max voltage for the device used
	//and the proper scaling and offset factors for the polarity flags selected.
    
	if (compMask & daq10V)	//Max voltage for Daq2K is +/-10V
		maxVolt = 10.0;
	else	//Max voltage for Legacy is +/-5V
		maxVolt = 5.0;

	for (i = 0; i < CHANCOUNT; i++)
    {
		//(flag&DafBiplor) equals 2 if bipolar or 0 if unipolar
		//scale should equal maxVolt/65335 for unipolar, maxVolt/32767 for bipolar
		scale[i] = maxVolt / (65535 - (flags[i] & DafBipolar) * 16384);
		//offset should equal maxVolt for bipolar, 0 for unipolar
		offset[i] = maxVolt * (flags[i] & DafBipolar) / 2;
	}

	rep = 1;
    
	do
    {
		// Collect data from the driver's circular buffer as it becomes available
		// Transfer data from driver buffer to ours

		daqAdcTransferBufData(handle, buffer, SCANS,
				      DabtmRetAvail, &retCount);                      

		if (retCount)
        {
			totalScans += retCount;
			for (i = 0; i < retCount * CHANCOUNT; i++)
				conv_buffer[i] = buffer[i] * scale[i % CHANCOUNT]
					- offset[i % CHANCOUNT];

			printf("\nScan %d:\n", rep++);	//print scan data
			printf("Scans Collected %i\n", retCount);
			printf("Total Collected %i\n", totalScans);
			printf("Chan 0\t\tChan 2\n");

			for (i = 0; i < retCount; i++)
            {
				for (j = 0; j < CHANCOUNT; j++)
					printf("%2.3fV\t\t",
					       conv_buffer[(CHANCOUNT * i) + j]);
				printf("\n");
			}
		}
	}
    
	while (!Daqkeyhit());
    
	//Disarm Daqboard
	daqAdcDisarm(handle);
	printf("\nScan Halted\n");

	//close device connections
	daqClose(handle);
	printf("\n%s Closed\n\n", devName);

	return 0;
}
