////////////////////////////////////////////////////////////////////////////
//
// DaqAdcEx11.c                    I    OOO                           h
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
// Single Step Acquisition Functions
//
//--------------------------------------------------------------------------

#include <stdio.h>
#include "daqx_linuxuser.h"

///////////////////////////////////////
//Trigger and Scan Definitions
//
#define STARTSOURCE	DatsImmediate   // Use Immediate Start Trigger
#define SCANS		30              // 30 Scans 
#define RATE		1000	        // 1000 Hz Scan Rate 


void printConvertedData(int channels, int points, PWORD buffer, DWORD flag);

int main(void)
{
	// DaqBoard Name and Device Handle
	PCHAR devName;
	DaqHandleT handle;

	// Gains and Flags Array to setup the Acquisition
	// Channels are specified in the individual commands
	DaqAdcGain gain = DgainX1;
	DWORD flag = DafBipolar;

	// Buffer for Data Storage
	WORD buffer[2 * SCANS];

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

	// 1 Channel 1 Point
	daqAdcRd(handle, 0, buffer, gain, flag);
	printf("\n1 Channel 1 Point from daqAdcRd\n");
	printConvertedData(1, 1, buffer, flag);

	// 1 Channel 10 Points
	daqAdcRdN(handle, 0, buffer, 10, STARTSOURCE, 0, 0, RATE, gain, flag);
	printf("\n1 Channel 10 Points from daqAdcRdN\n");
	printConvertedData(1, 10, buffer, flag);

	// 10 Channels (0 - 9) 1 Point
	daqAdcRdScan(handle, 0, 9, buffer, gain, flag);
	printf("\n10 Channels (0 - 9) 1 Point from daqAdcRdScan\n");
	printConvertedData(10, 1, buffer, flag);

	// 5 Channels (0 - 4) 2 Points each
	daqAdcRdScanN(handle, 0, 4, buffer, 2, STARTSOURCE, 0,
		      0, RATE, gain, flag);
	printf("\n5 Channels (0 - 4) 2 Points each from daqAdcRdScanN\n");
	printConvertedData(5, 2, buffer, flag);

	// close device connections
	daqClose(handle);
	printf("\n%s Closed\n\n", devName);

	return 0;
}


VOID 
printConvertedData(int channels, int points, PWORD buffer, DWORD flag)
{
	// Base Unit Voltage Parameters
	DWORD daq10V = 1;
	DWORD compMask = daq10V;
	// Scaling Variables
	float maxVolt;
	float scale;
	float offset;

	int i, j;

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
	else			//Max voltage for Legacy is +/-5V
		maxVolt = 5.0;

	//(flag&DafBiplor) equals 2 if bipolar or 0 if unipolar
	//scale should equal maxVolt/65335 for unipolar, maxVolt/32767 for bipolar
	scale = maxVolt / (65535 - (flag & DafBipolar) * 16384);
	//offset should equal maxVolt for bipolar, 0 for unipolar
	offset = maxVolt * (flag & DafBipolar) / 2;

	//print scan data
	for (i = 0; i < points; i++) {
		for (j = 0; j < channels; j++) {
			printf("\tChannel %d:Scan %d %1.3fV\n", j, i + 1,
			       (float) buffer[i + j] * scale - offset);
		}
	}
	return;
}
