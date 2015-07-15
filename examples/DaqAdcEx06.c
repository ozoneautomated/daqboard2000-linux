////////////////////////////////////////////////////////////////////////////
//
// DaqAdcEx06.c                    I    OOO                           h
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
// This example utilizes the Digital Pattern Trigger as a start
// and stop trigger, and an analog channel is then scanned.  
//
//--------------------------------------------------------------------------

#include <stdio.h>
#include "daqx_linuxuser.h"

//////////////////////////////
//Trigger and Scan Definitions
//
#define STARTSOURCE  DatsDigPattern	// Use a Digital Pattern for the Start Trigger
#define STOPSOURCE   DatsDigPattern	// Use a Digital Pattern for the Stop Trigger

#define CHANCOUNT    2		        // 2 total channels - 1 for DATA, 1 for Trigger
#define SCANS        20		        // Perform 20 Scans before looking for the Stop Trigger Event
#define RATE         100	        // 100 Hz Scan Rate
#define STARTPATTERN 0x01	        // Start Trigger Pattern = P3 input D0
#define STOPPATTERN  0x00	        // Stop Trigger Pattern = P3 input D0
#define MASK         0x01	        // pattern mask, '0' is a don't care
#define TRIGSENSE    DetsEQLevel	// trigger on pattern match (==)
#define BUFFSIZE     SCANS*1000	    // Use Adaquate Buffer Size

int main(void)
{
	unsigned int i,j;
    
	// DaqBoard Name and Device Handle
	PCHAR devName;
	DaqHandleT handle;

	// Base Unit Voltage Parameters
	DWORD daq10V = 1;
	DWORD compMask = daq10V;

	// Buffer for Data Storage
	WORD buffer[BUFFSIZE];

	// Channels, Gains, and Flags Array to setup the Acquisition
	DaqAdcGain gains[CHANCOUNT] = { DgainX1, DgainX1 };
	DWORD channels[CHANCOUNT] = { 0, 0 };
	DWORD flags[CHANCOUNT] = { DafP3Local16, DafBipolar | DafAnalog };

	// Acquisition Status Variables
	DWORD active, retCount;

	// Scaling Variables
	float maxVolt;
	float scale[CHANCOUNT];
	float offset[CHANCOUNT];
	float conv_buffer[BUFFSIZE];

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
	daqAdcTransferSetBuffer(handle, buffer, BUFFSIZE,
				DatmUpdateSingle | DatmDriverBuf);

	// Set Start Trigger to Trigger on Digial Pattern
	daqSetTriggerEvent(handle, STARTSOURCE, TRIGSENSE,
			   channels[0], gains[0], flags[0],
			   DaqTypeDigitalLocal, STARTPATTERN, MASK, DaqStartEvent);

	// Set Stop Trigger to Trigger on Digial Pattern
	daqSetTriggerEvent(handle, STOPSOURCE, TRIGSENSE,
			   channels[0], gains[0], flags[0],
			   DaqTypeDigitalLocal, STOPPATTERN, MASK, DaqStopEvent);

	// Set the scan rate
	daqAdcSetFreq(handle, RATE);

	// Timeout if not triggered within a minute
	daqSetTimeout(handle, 60000);

	// Prepare for Data Transfer
	daqAdcTransferStart(handle);

	// Arm the Acquisition                  
	daqAdcArm(handle);
	printf("\n%s Armed\n\n", devName);

	//wait for trigger
	printf("\nWaiting for Digital Pattern...\n");
	daqWaitForEvent(handle, DteAdcDone);
	daqAdcTransferGetStat(handle, &active, (PDWORD) & retCount);

	// transfer acquired data into "buffer"
	daqAdcTransferBufData(handle, buffer, SCANS, DabtmNoWait, &retCount);

	//Disarm Daqboard
	daqAdcDisarm(handle);

    printf("\nCompleted - Performed %d scans\n", retCount);     

	//test if triggered or timed out
	if (retCount > 0)
    {		
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
		//don't need to scale digital channel
		for (i = 1; i < CHANCOUNT; i++)
        {
			//(flag&DafBiplor) equals 2 if bipolar or 0 if unipolar
			//scale should equal maxVolt/65335 for unipolar, maxVolt/32767 for bipolar
			scale[i] =
				maxVolt / (65535 - (flags[i] & DafBipolar) * 16384);
			//offset should equal maxVolt for bipolar, 0 for unipolar
			offset[i] = maxVolt * (flags[i] & DafBipolar) / 2;
		}

		for (i = 0; i < retCount * CHANCOUNT; i++)
			conv_buffer[i] =
				buffer[i] * scale[i % CHANCOUNT] -
				offset[i % CHANCOUNT];

		//print scan data
		printf("Analog ch %d\n", channels[1]);
		for (i = 0; i < retCount; i++)
        {
			for (j = 1; j < CHANCOUNT; j++)	//don't print digital channel
				printf("Point %d \t  %1.3f\t\t",
				       i, conv_buffer[(CHANCOUNT * i) + j]);
			printf("\n");
		}
	}
    else
    {
		printf("TIMED OUT\n");
		printf("\nDigital Trigger did not occur\n");
	}

	//close device connections
	daqClose(handle);
	printf("\n%s Closed\n\n", devName);

	return 0;
}
