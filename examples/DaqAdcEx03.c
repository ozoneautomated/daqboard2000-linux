////////////////////////////////////////////////////////////////////////////
//
// DaqAdcEx03.c                    I    OOO                           h
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
// Single Channel, Hardware Start Trigger and Software Stop Triggers
// Change Scan Timeout to 5 seconds
//
//--------------------------------------------------------------------------

#include <stdio.h>
#include "daqx_linuxuser.h"

///////////////////////////////////////
//Trigger and Scan Definitions
//
#define STARTSOURCE		DatsHardwareAnalog	// Use Hareware Start Trigger
#define STOPSOURCE		DatsSoftwareAnalog	// Use Software Stop Trigger
#define VOLTAGE			2.0f	            // 2.0 Volt Trigger Level
#define CHANCOUNT		1	                // Single Channel
#define SCANS			100	                // Perform 100 Scans before looking for the Stop Trigger Event
#define RATE			100	                // 100 Hz Scan Rate
#define BUFFSIZE		4096	            // Use Adaquate Buffer Size **per channel in this example

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
	WORD buffer[BUFFSIZE * CHANCOUNT];

	// Channels, Gains, and Flags Array to setup the Acquisition
	DaqAdcGain gains[CHANCOUNT] = { DgainX1 };
	DWORD channels[CHANCOUNT] = { 0 };
	DWORD flags[CHANCOUNT] = { DafBipolar };

	// Acquisition Status Variables
	DWORD retCount;

	// Scaling Variables
	float maxVolt;
	float scale[CHANCOUNT];
	float offset[CHANCOUNT];
	float conv_buffer[BUFFSIZE * CHANCOUNT];

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

	// Create a Driver managed buffer of BUFFSIZE * CHANCOUNT length
	daqAdcTransferSetBuffer(handle, buffer,
				BUFFSIZE * CHANCOUNT,
				DatmUpdateSingle | DatmDriverBuf);

	// Set Start Trigger Event for VOLTAGE on a Rising Edge >> Using The First Channel
	daqSetTriggerEvent(handle, STARTSOURCE, DetsRisingEdge,
			   channels[0], gains[0], flags[0],
			   DaqTypeAnalogLocal, VOLTAGE, 0.0, DaqStartEvent);

	// Set Stop  Trigger Event for -VOLTAGE on a Falling Edge >> Using The First Channel
	daqSetTriggerEvent(handle, STOPSOURCE, DetsFallingEdge,
			   channels[0], gains[0], flags[0],
			   DaqTypeAnalogLocal, -VOLTAGE, 0.0, DaqStopEvent);

	// Set the scan rate
	daqAdcSetFreq(handle, RATE);

	// Prepare for Data Transfer
	daqAdcTransferStart(handle);

	// Arm the Acquisition                  
	daqAdcArm(handle);

	printf("Waiting for trigger voltage...\n");

	// Change the defaut timeout to 5 seconds
	// --This applies to the daqAdcTransferBufData Event below                                                              
	daqSetTimeout(handle, 5000);

	// Wait For the requested number of SCANS to become available and transfer the data
	// or timeout
	daqAdcTransferBufData(handle, buffer, SCANS, DabtmWait, &retCount);

	//Disarm when completed
	daqAdcDisarm(handle);
	printf("Completed - Performed %d scans\n\n", retCount);

	if (retCount)
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

		for (i = 0; i < CHANCOUNT; i++)
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
		printf("Analog ch %d\n", channels[0]);
        
		for (i = 0; i < retCount; i++)
        {
			for (j = 0; j < CHANCOUNT; j++)
            {
				printf("Point %d  %1.3f\t\t", i,
				       conv_buffer[(CHANCOUNT * i) + j]);
			}
			printf("\n");
		}        
	}
    else //(retCount)
	{
		printf("TIMED OUT\n");
	}

	//close device connections
	daqClose(handle);
	printf("\n%s Closed\n\n", devName);

	return 0;
}
