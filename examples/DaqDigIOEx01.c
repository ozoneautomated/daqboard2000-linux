////////////////////////////////////////////////////////////////////////////
//
// DaqDigIOEx01.c                  I    OOO                           h
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
//  This example demonstrates the Digital I/O on P2
//
//--------------------------------------------------------------------------
 


#include <stdio.h>
#include "daqx_linuxuser.h"

// Port Data Definitions

#define      BYTE_A         0xff
#define      BYTE_B         0x7f
#define      BYTE_C         0x00


int main(void)
{
    // DaqBoard Name and Device Handle     
    PCHAR devName;
    DaqHandleT              handle;

    // used during I/O     
    DWORD retvalueA, retvalueB, retvalueC, config;

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


    // set ports A, B, and C as outputs  
    printf("Setting ports A, B, and C as outputs\n");                       

    // Create 8255 config number for these (0,0,0,0) settings       
    daqIOGet8255Conf(handle, 0 ,0, 0, 0, &config);

    // write settings and config number to internal register 
    daqIOWrite(handle, DiodtLocal8255, Diodp8255IR, 0, DioepP2, config);                    

    // update ports A, B, and C                     
    printf("Writing... \n0x%x to Port A,\n0x%x to Port B\n0x%x to Port C\n",
	    BYTE_A,BYTE_B,BYTE_C);
    daqIOWrite(handle, DiodtLocal8255, Diodp8255A, 0, DioepP2, BYTE_A);                   
    daqIOWrite(handle, DiodtLocal8255, Diodp8255B, 0, DioepP2, BYTE_B);              
    daqIOWrite(handle, DiodtLocal8255, Diodp8255C, 0, DioepP2, BYTE_C);                                             

    //	set port A, B, and C as inputs    
    printf("\nSetting ports A, B, and C as inputs\n");                      

    // Create 8255 config number for these (1,1,1,1) settings     
    daqIOGet8255Conf(handle, 1 ,1, 1, 1, &config);            

    // write settings and config number to internal register           
    daqIOWrite(handle, DiodtLocal8255, Diodp8255IR, 0, DioepP2, config);                    

    // read word on daq*, must read as two bytes                    
    daqIORead(handle, DiodtLocal8255, Diodp8255A, 0, DioepP2, &retvalueA);        
    daqIORead(handle, DiodtLocal8255, Diodp8255B, 0, DioepP2, &retvalueB);      
    daqIORead(handle, DiodtLocal8255, Diodp8255C, 0, DioepP2, &retvalueC);    
    printf("Reading... \nPort A = 0x%x\nPort B = 0x%x\nPort C = 0x%x\n",
	    retvalueA,retvalueB,retvalueC);                  

    printf("Digital input/output complete\n\n");                    

    // close device connections   
    daqClose(handle); 

    printf("\n%s Closed\n\n",devName);

    return 0;
}
