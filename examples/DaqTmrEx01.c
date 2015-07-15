////////////////////////////////////////////////////////////////////////////
//
// DaqTmrEx01.c                    I    OOO                           h
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
// Timer Squarewave Output, 2 channel
//
//--------------------------------------------------------------------------                                                                                                                                                                                    

#include <stdio.h>
#include "daqx_linuxuser.h"

///////////////////////////////////////
// Output Definitions
//

// Frequency_Divisor = ( 1Mhz / desired_frequency ) -1
#define  FREQ_DIV0	999	// for 1KHz
#define  FREQ_DIV1	19	// for 50 KHz

int main(void)
{
	//used to connect to device

	PCHAR devName;
	DaqHandleT handle;

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

	//settings for timer 0
	daqSetOption(handle, 0, DcofChannel, DcotTimerDivisor, FREQ_DIV0);
	//settings for timer 1
	daqSetOption(handle, 1, DcofChannel, DcotTimerDivisor, FREQ_DIV1);

	//turn on both timers simultaneously
	daqSetOption(handle, 0, DcofModule, DmotTimerControl, DcovTimerOn);

	printf("\n\nSignals currently being output...\nPress <Enter> to quit");
	getchar();

	//turn off both timers simultaneously
	daqSetOption(handle, 0, DcofModule, DmotTimerControl, DcovTimerOff);

	//close device connections
	daqClose(handle);
	printf("\n%s Closed\n\n", devName);

	return 0;

}
