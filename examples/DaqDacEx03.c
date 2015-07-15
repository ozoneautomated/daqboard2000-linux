////////////////////////////////////////////////////////////////////////////
//
// DaqDacEx03.c                    I    OOO                           h
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
// This example demonstrates 2 Channel Static waveform output
//
//--------------------------------------------------------------------------

#include <stdio.h>
#include "daqx_linuxuser.h"

#define  CHANCOUNT	2           // 2 total channels
#define  FREQ		3800        // 3800 / 127 * 2 = ~60 Hz.
#define  BUFFSIZE		127	    //a buffer of 4096 is the largest allowed.
									
int main()
{
	//used to connect to device
	char* devName;
	DaqHandleT handle;
	DWORD	active, retCount;
	WORD  buf[BUFFSIZE];   
    
    int i;
    float x = 0.0;
    
	for (i = 0; i < BUFFSIZE/CHANCOUNT ; i++)
	{        
        //CH 1 Ramp
        buf[i] = i*1000;
        
        //CH 2 Sine
        x= (sin((float)i/10.0))+1.0;
        buf[i+BUFFSIZE/2] =  x*10000;         
    }
    
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
	//a return of -1 indicates failure
		
	//settings for channel 0
	//Set for a static waveform
	daqDacSetOutputMode(handle, DddtLocal, 0, DdomStaticWave); 
	//Immediate Trigger, (the final paramater is not yet used)
	daqDacWaveSetTrig(handle, DddtLocal, 0, DdtsImmediate, 0);
	//Use the Local DAC clock
	daqDacWaveSetClockSource(handle, DddtLocal, 0,(DaqDacClockSource)DdcsDacClock);
	//Set the frequency
	daqDacWaveSetFreq(handle, DddtLocal, 0, FREQ);  
	//Repeat infinatly, the final parameter (updateCount) is ignore with infinite loop
	daqDacWaveSetMode(handle, DddtLocal, 0, DdwmInfinite, 0);	
	//for an infintite loop, the buffer must cycle
	daqDacWaveSetBuffer(handle, DddtLocal, 0, (WORD*)buf, BUFFSIZE/CHANCOUNT,
                        DdtmCycleOn|DdtmUpdateBlock);
    
	//set channel 0 to be a user wave
	daqDacWaveSetUserWave(handle, DddtLocal, 0);

    //settings for channel 1
    daqDacSetOutputMode(handle, DddtLocal, 1, DdomStaticWave);	
	daqDacWaveSetTrig(handle, DddtLocal, 1, DdtsImmediate, 0);	
	daqDacWaveSetClockSource(handle, DddtLocal, 1,(DaqDacClockSource)DdcsDacClock);	
	daqDacWaveSetFreq(handle, DddtLocal, 1, FREQ);	
	daqDacWaveSetMode(handle, DddtLocal, 1, DdwmInfinite, 0);	
	daqDacWaveSetBuffer(handle, DddtLocal, 1, (WORD*)(buf+BUFFSIZE/CHANCOUNT),
                        BUFFSIZE/CHANCOUNT, DdtmCycleOn|DdtmUpdateBlock);
                        	
	daqDacWaveSetUserWave(handle, DddtLocal, 1);
    
	//begin output
	daqDacWaveArm(handle, DddtLocal);

	printf("\nOutputting signals on channels 0 and 1");
	printf("\nPress <Enter> to halt signal and quit\n");
	while (!Daqkeyhit())
	{
	    daqDacTransferGetStat(handle, DddtLocal,0, &active,  &retCount);
	    printf("ACTIVE: 0x%x \t RETCOUNT %i\n", active, retCount);
	    sleep(1);
	}

	daqDacWaveDisarm(handle, DddtLocal);

	//Close device connection
	daqClose(handle);

    return(1);
	
}

