////////////////////////////////////////////////////////////////////////////
//
// DaqAdcEx10.c                    I    OOO                           h
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
//  This example performs a simple immediate trigger of ten scans, but
//  illustrates differences between bipolar and unipolar gains and 
//  the use of the API provided routines to convert to SI units vs.
//  writing code to convert data.
// 
//--------------------------------------------------------------------------

#include <stdio.h>
#include "daqx_linuxuser.h"

///////////////////////////////////////
//Trigger and Scan Definitions
//

#define STARTSOURCE	DatsImmediate   // Use Immediate Start Trigger
#define STOPSOURCE	DatsScanCount   // Stop when the requested scan count is reached
#define CHANCOUNT	2               // 2 total channels
#define SCANS		15              // 15 Scans per Channel
#define RATE		1000            // 1000 Hz Scan Rate

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
	WORD buffer[SCANS * CHANCOUNT];

	// Channels, Gains, and Flags Array to setup the Acquisition
	DaqAdcGain gains[CHANCOUNT] = { DgainX1, DgainX2 };
	DWORD channels[CHANCOUNT] = { 0, 0 };
	DWORD flags[CHANCOUNT] = { DafBipolar, DafUnipolar };

	// Acquisition Status Variables
	DWORD active, retCount;

	// Scaling Variables
	float maxVolt;
	float scale[CHANCOUNT];
	float offset[CHANCOUNT];
	float conv_buffer[SCANS * CHANCOUNT];
	float temp_buffer[SCANS * CHANCOUNT];

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

	//scan set up
	printf("Setting up scan...\n");
	// set # of scans to perform and scan mode
	daqAdcSetAcq(handle, DaamNShot, 0, SCANS);

	//Scan settings
	daqAdcSetScan(handle, channels, gains, flags, CHANCOUNT);

	//set scan rate
	daqAdcSetFreq(handle, RATE);

	//Set buffer location, size and flag settings
	daqAdcTransferSetBuffer(handle, buffer, SCANS, DatmCycleOn | DatmDriverBuf);

	//Set to Trigger on software trigger
	daqSetTriggerEvent(handle, STARTSOURCE,
			   (DaqEnhTrigSensT) NULL, channels[0],
			   gains[0], flags[0],
			   DaqTypeAnalogLocal, 0.0, 0.0, DaqStartEvent);
	//Set to Stop when the requested number of scans is completed
	daqSetTriggerEvent(handle, STOPSOURCE,
			   (DaqEnhTrigSensT) NULL, channels[0],
			   gains[0], flags[0],
			   DaqTypeAnalogLocal, 0.0, 0.0, DaqStopEvent);

	//begin data acquisition
	printf("Scanning...\n");
	daqAdcTransferStart(handle);
	daqAdcArm(handle);

	do
    {
		//transfer data (voltage readings) into computer memory and halt acquisition when done
		daqAdcTransferGetStat(handle, &active, (PDWORD) & retCount);
	}
	while (active & DaafAcqActive);

	daqAdcTransferBufData(handle, buffer, SCANS, DabtmRetAvail, &retCount);

	//Disarm when completed
	daqAdcDisarm(handle);
    
	printf("Scan Completed\n\n %i scans returned\n",retCount);

	//convert the data to volts manually:
	//DaqBoards convert all data to an unsigned, 16-bit number (range 0 to 65535.  Zero corresponds 
	//to the minimum voltage, which is -maxVolt if in bipolar mode, or zero if unipolar.  
	//65535 corresponds to maxVolt if bipolar, or 2xmaxVolt if unipolar.  Note that a voltage 
	//higher than the device's absolute range (+/-10V for DaqBoard2000, +/-5V for other Daq* devices)
	//can damage the device.  Setting flags and gain settings which indicate a voltage higher than 

	//the device's max, such as an unipolar, unscaled scan will result in an error before the scan 
	//is performed.                 

	// NOTE: this only works because the unipolar channel is given a gain of X2

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

	for (i = 0; i < retCount * CHANCOUNT; i++)
		conv_buffer[i] =
			buffer[i] * scale[i % CHANCOUNT] - offset[i % CHANCOUNT];

	//print scan data
	printf("Manual Data Conversion:\n\n");
	printf("Bipolar\tUnipolar\n");
	for (i = 0; i < retCount; i++)
    {
		for (j = 0; j < CHANCOUNT; j++)
			printf("%1.3f\t", conv_buffer[(CHANCOUNT * i) + j]);
		printf("\n");
	}

	//convert data into volts using daqCvtLinearSetupConvert
	//this function accepts two pairs of paramaters, which report what a voltage
	//as measured by the device should corespond to as an output, i.e. Five volts is 
	//five volts for a daqboard, but five volts can be 50 volts for a DBK8 or 400 
	//degrees for a DBK42
	//The function will be called twice, once for the bipoalr channel and once for 
	//the polar channel.  
	//you must set the range before you can call the CvtLinear functions

	// NOTE: this only works because the unipolar channel is given a gain of X2

	//Bipolar
	daqCvtSetAdcRange(-maxVolt, maxVolt);

	daqCvtLinearSetupConvert(CHANCOUNT,	//Number of channels
				 0,	//position in scan array of first channel to covnert
				 1,	//number of consecutive scans to convert 
				 maxVolt,	//Input signal
				 maxVolt,	//Corresponding output voltage
				 0.0,	//Input signal
				 0.0,	//Corresponding output voltage
				 1,	//No averaging
				 buffer,	//location of scans
				 SCANS,	//number of scans in buffer
				 temp_buffer,	//array to hold float values    
				 SCANS);	//size of float array

	//Unipolar 
	daqCvtSetAdcRange(0, maxVolt);
	daqCvtLinearSetupConvert(CHANCOUNT,	//Number of channels
				 1,	//position in scan array of first channel to covnert
				 1,	//number of consecutive scans to convert 
				 maxVolt,	//Input signal
				 maxVolt,	//Corresponding output voltage
				 0.0,	//Input signal
				 0.0,	//Corresponding output voltage
				 1,	//No averaging
				 buffer,	//location of scans
				 SCANS,	//number of scans in buffer
				 conv_buffer,	//array to hold float values    
				 SCANS);	//size of float array

	//print scan data
	printf("\n\nData converted by daqCvtLinearSetupConvert:\n\n");
	printf("Bipolar\tUnipolar\n");

	for (i = 0; i < SCANS; i++)
    {
		printf("%1.3f\t%1.3f\n", temp_buffer[i], conv_buffer[i]);
	}

	//close device connections
	daqClose(handle);
	printf("\n%s Closed\n\n", devName);

	return 0;
}
