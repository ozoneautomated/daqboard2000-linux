////////////////////////////////////////////////////////////////////////////
//
// DaqAdcEx02.c                    I    OOO                           h
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
// Multiple Channel, Software Trigger, Fixed Scan Length
//
//--------------------------------------------------------------------------
 

#include <stdio.h>
#include "daqx_linuxuser.h"

//////////////////////////////
//Trigger and Scan Definitions
//
#define STARTSOURCE  	DatsSoftware	// Use Software Start Trigger
#define STOPSOURCE   	DatsScanCount	// Stop when the requested scan count is reached
#define CHANCOUNT		3	            // 3 total channels
#define SCANS			25	            // 25 Scans per Channel
#define RATE			10000            // 1000 Hz Scan Rate

int main(int argc, char *argv[])
{
	WORD i,j;

	// DaqBoard Name and Device Handle
	PCHAR devName = "daqBoard2k0";
	DaqHandleT handle;

	// Base Unit Voltage Parameters
	DWORD daq10V = 1;
	DWORD compMask = daq10V;

	// Buffer for Data Storage
	WORD buffer[SCANS * CHANCOUNT];

	// Channels, Gains, and Flags Array to setup the Acquisition
	DaqAdcGain gains[CHANCOUNT] = { DgainX1, DgainX1, DgainX1 };
	DWORD channels[CHANCOUNT] = { 0, 1, 2 };
	DWORD flags[CHANCOUNT] = { DafBipolar, DafBipolar, DafBipolar };

	// Acquisition Status Variables
	DWORD active, retCount;
	
	// scan count - defaults to SCANS
	int scans = SCANS;

	// Scaling Variables
	float maxVolt;
	float scale[CHANCOUNT];
	float offset[CHANCOUNT];
	float conv_buffer[SCANS * CHANCOUNT];

        int calls = 1;

	if (argc == 2) {
		scans = atoi(argv[1]);
	        printf("collecting %d scans\n", scans);
        }

	// Open the DaqBoard2000 and get the handle
	printf("\n\nAttempting to Connect with %s\n", devName );
	handle = daqOpen(devName);

	if (handle == NO_GRIP)
	{
		printf("Could not connect to %s\n",devName);    
		return 2;
	}
	printf("%s opened\n\n",devName);


	// Set the acquisition type to 
	// NShot with 0 pre-trig scans and "SCANS" post-trig scans
	daqAdcSetAcq(handle, DaamNShot, 0, scans);

	// Pass the Channels, Gains, and Flags Arrays along with the channel count to setup the Acquisition
	daqAdcSetScan(handle, channels, gains, flags, CHANCOUNT);

	// Create a Driver managed buffer of SCANS length
	daqAdcTransferSetBuffer(handle, buffer, scans,
			DatmUpdateSingle | DatmDriverBuf);

	// Set to Trigger on software trigger
	daqSetTriggerEvent(handle, STARTSOURCE,
			(DaqEnhTrigSensT) NULL, channels[0],
			gains[0], flags[0],
			DaqTypeAnalogLocal, 0.0, 0.0, DaqStartEvent);


	// Set to Stop when the requested number of scans is completed
	daqSetTriggerEvent(handle, STOPSOURCE,
			(DaqEnhTrigSensT) NULL, channels[0],
			gains[0], flags[0],
			DaqTypeAnalogLocal, 0.0, 0.0, DaqStopEvent);

	// Set the scan rate                         
	daqAdcSetFreq(handle, RATE);

	// Prepare for Data Transfer
	daqAdcTransferStart(handle);

	// Arm the Acquisition                  
	daqAdcArm(handle);
	printf("\n%s Armed\n\n", devName);

	// Wait for keystroke to issue software trigger
	printf("Hit any key to issue software trigger...\n");


	while (!Daqkeyhit()) ;

	// Issue software trigger                               
	daqAdcSoftTrig(handle);
	printf("\nScan Triggered\n\n");

	// Loop until the Acquisition is complete
	do
	{
                active = 0;
                retCount = 0;
		daqAdcTransferGetStat(handle, &active, (PDWORD) & retCount);                      
                fprintf(stderr, "daqAdcTransferGetStat : active = 0x%x, retCount = %d, calls = %d\n", active, retCount, calls);
                ++calls;

	} while (active & DaafAcqActive);

	printf("Scan Completed\n\n %i scans returned\n",retCount);

	// transfer acquired data into "buffer"
	daqAdcTransferBufData(handle, buffer, scans, DabtmNoWait, &retCount);    

	//Disarm when completed
	daqAdcDisarm(handle);

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
	else
		maxVolt = 5.0;

	for (i = 0; i < CHANCOUNT; i++)
	{
		//flag&DafBiplor equals 2 if bipolar or 0 if unipolar
		//scale should equal maxVolt/65335 for unipolar, maxVolt/32767 for bipolar
		scale[i] = maxVolt / (65535 - (flags[i] & DafBipolar) * 16384);

		//offset should equal maxVolt for bipolar, 0 for unipolar
		offset[i] = maxVolt * (flags[i] & DafBipolar) / 2;
	}

	for (i = 0; i< retCount * CHANCOUNT; i++)
		conv_buffer[i] =
			buffer[i] * scale[i % CHANCOUNT] - offset[i % CHANCOUNT];

	//print scan data
	printf("Analog ch %d\tAnalog ch %d\tAnalog ch %d\n",
			channels[0], channels[1], channels[2]);

	for (i = 0; i < retCount; i++)
	{
		for (j = 0; j < CHANCOUNT; j++)
			printf("%1.3f\t\t", conv_buffer[(CHANCOUNT * i) + j]);
		printf("\n");
	}

	//close device connections
	daqAdcDisarm(handle);

	daqClose(handle);
	printf("\n%s Closed\n\n", devName);
	//while(!Daqkeyhit());
	return 1;
}
