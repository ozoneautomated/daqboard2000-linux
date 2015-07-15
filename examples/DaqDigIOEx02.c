////////////////////////////////////////////////////////////////////////////
//
// DaqDigIOEx02.c                  I    OOO                           h
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
// This example demonstrates the Digital I/O on P3.
// Daqboard 2000 Series devices only.                                                   
//
//--------------------------------------------------------------------------                                                                                                                                                    

#include <stdio.h>
#include "daqx_linuxuser.h"

///////////////////////////////////////
//Trigger and Scan Definitions
//
#define	 WORD_P3		0xf0f0

int
main()
{
	// DaqBoard Name and Device Handle
	PCHAR devName;
	DaqHandleT handle;

	//used during I/O
	DWORD retvalue;

	devName = "daqBoard2k0";

        // Open the DaqBoard2000 and get the handle
        printf("\n\nAttempting to Connect with %s\n", devName);
        handle = daqOpen("daqboard2k0");
        if (handle == NO_GRIP){
                printf("Could not connect to %s\n", devName);    
                return 2;
        }
        printf("%s opened\n\n",devName);                        

	//configure port as output
	printf("\nConfiguing P3 Port for Output\n");
	daqIOWrite(handle, DiodtP3LocalDig16, DiodpP3LocalDigIR, 0, DioepP3, 1);

	//send word from daq*
	printf("\nWriting 0x%x to Digital Port P3\n", WORD_P3);
	daqIOWrite(handle, DiodtP3LocalDig16, DiodpP3LocalDig16,
		   0, DioepP3, WORD_P3);

	//configure port as input
	printf("\nConfiguing P3 Port for Input\n");
	daqIOWrite(handle, DiodtP3LocalDig16, DiodpP3LocalDigIR, 0, DioepP3, 0);

	//read word on daq*
	daqIORead(handle, DiodtP3LocalDig16, DiodpP3LocalDig16,
		  0, DioepP3, &retvalue);
	printf("\nPort P3 Read Returned 0x%x\n", retvalue);

	printf("\nDigital input/output complete\n\n");

	//close device connections
	daqClose(handle);
	printf("\n%s Closed\n\n", devName);

	return 0;
}
