////////////////////////////////////////////////////////////////////////////
//
// DaqDacEx01.c                    I    OOO                           h
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
// This example demonstrates the use of DC output               
//
//--------------------------------------------------------------------------

#include <stdio.h>
#include "daqx_linuxuser.h"

#define  CHANCOUNT	2       // 2 total channels
#define	 OUT_ONE	2.5F	// 2.5 volts out
#define  OUT_TWO	3.0F	// 3.0 volts out

int main(void)
{
	// DaqBoard Name and Device Handle
	PCHAR devName;
	DaqHandleT handle;

	// Base Unit Voltage Parameters
	DWORD daq10V = 1;
	DWORD compMask = daq10V;

	DaqDacDeviceType deviceTypes[CHANCOUNT] = { DddtLocal, DddtLocal };
	DWORD chans[CHANCOUNT] = { 0, 1 };
	float maxVolt, minVolt, flt_vlt[CHANCOUNT] = { OUT_ONE, OUT_TWO };
	WORD cnt_vlt[CHANCOUNT];
	int i;

	devName = "daqBoard2k0";

	// Open the DaqBoard2000 and get the handle             
	printf("\n\nAttempting to Connect with %s\n", devName);

	handle = daqOpen("daqboard2k0");
    
	if (handle == NO_GRIP)
    {
		printf("Could not connect to %s\n", devName);
		return 2;
	}
	printf("%s opened\n", devName);

	//DAC takes values from 0 for the minumum output to
	//65535 for the maximum output

	//The following routine automatically determines 
	//the max and min voltage for the device used
	//Voltage range for Daq2K is +/-10V

	if (compMask & daq10V)
    {
		maxVolt = 10.0;
		minVolt = -10.0;
	}
    else
    {
		maxVolt = 5.0;
		minVolt = 0.0;
	}

	//set output settings
	for (i = 0; i < CHANCOUNT; i++)
    {
		cnt_vlt[i] = (WORD) ((flt_vlt[i] - minVolt) 
				     * 65535 / (maxVolt - minVolt));
		daqDacSetOutputMode(handle, DddtLocal, i, DdomVoltage);
	}

	//start output
	daqDacWtMany(handle, deviceTypes, chans, cnt_vlt, CHANCOUNT);

	printf ("Outputting %2.2f on chan 0 and %2.2f on chan 1\n",	OUT_ONE, OUT_TWO);

	printf("Press enter to turn off DC output..");
	while (!Daqkeyhit());

	printf("...Exiting\n");

	// Shutdown the DACS - 0 volt output
	for (i = 0; i < CHANCOUNT; i++)
		cnt_vlt[i] = (WORD) ((0 - minVolt) * 65535 / (maxVolt - minVolt));

	daqDacWtMany(handle, deviceTypes, chans, cnt_vlt, CHANCOUNT);

	//close device connections
	daqClose(handle);
	printf("\n%s Closed\n\n", devName);

	return 0;
}
