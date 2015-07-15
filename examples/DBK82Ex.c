// DBK81EX.CPP
//
// uses 32bit enh API  
//
// This example demonstrates the setup and use of the DBK81 card
// and the use of the API data conversion functions
// 
//
// Functions used:

//	daqOpen(handle);
//  daqReadCalFile(handle, filename);	
//	daqAdcSetAcq(handle, mode, preTrigCount, postTrigCount);
//	daqAdcSetScan(handle, &channels, &gains, &flags, chanCount);
//  daqAdcSetFreq(handle, freq );
//	daqAdcSetTriggerEvent(handle, trigSource, trigSensitivity,
//						  channel, gainCode, flags, channelType,
//						  level, variance, trigevent);	
//	daqAdcTransferSetBuffer(handle, buf, scanCount,transferMask);
//	daqAdcTransferStart(DaqHandleT handle);
//	daqAdcTransferGetStat(DaqHandleT handle, active, retCount);
//	daqAdcArm(handle);
//	daqAdcDisarm(handle);
//  daqCvtTCSetupConvert(nScans, cjcPosition, ntc, tcType, bipolar, avg, counts, scans, temp, ntemp);
//	daqClose(handle);

#include <stdio.h>
#include "daqx_linuxuser.h"


#define STARTSOURCE		DatsImmediate
#define STOPSOURCE		DatsScanCount
#define CHANCOUNT		5
#define SCANS			20
#define RATE			10
#define TRIGSENSE		(DaqEnhTrigSensT)NULL
	
int main()
{
	//used to connect to device
	char* devName;        
    int x = 0;

	DaqHandleT handle; 	

	DWORD channels[CHANCOUNT] = {16, 17, 18, 19, 20}; // CJC + 4 Channels

	DaqAdcGain gains[CHANCOUNT] = { DbkPCC81CJC,      // Use the "PCC" gain types to put the
                                    DbkPCC81TypeT,    // DaqBoard2000 into +/- 5V mode for DBK81/82
                                    DbkPCC81TypeT,
                                    DbkPCC81TypeT,
                                    DbkPCC81TypeT,};	
	                                                                                   
	DWORD flags[CHANCOUNT] = {  DafTcCJC | DafBipolar | DafUnsigned,                              
                                DafTcTypeT |DafBipolar| DafUnsigned,
                                DafTcTypeT |DafBipolar| DafUnsigned,
                                DafTcTypeT |DafBipolar| DafUnsigned,
                                DafTcTypeT | DafBipolar | DafUnsigned};

	//buffer to hold raw data
	unsigned short rawbuffer[SCANS*CHANCOUNT];	
	//buffer to hold converted data, scans will be averaged to one temp value
	short convbuffer[SCANS*CHANCOUNT];
	//buffer to hold scaled float value of convbuffer
	float conv_buffer[SCANS*CHANCOUNT];    
	
	//used to monitor scan
	DWORD active, retCount;
	
	devName = "daqBoard2k0";
                                                         
    printf( "Attempting to Connect with %s\n", devName );

    //attempt to open device
	handle = daqOpen(devName);
    
	if (handle + 1)	//a return of -1 indicates failure
	{			
		//scan set up
		// set # of scans to perform and scan mode
		daqAdcSetAcq(handle, DaamNShot, 0, SCANS); 

		//Scan settings
		daqAdcSetScan(handle, channels, gains, flags, CHANCOUNT);   
		
		//DBK's require 100 kHz clock, Daq2K has 200kHz 
		//if (compMask & daq2000)
		daqAdcSetClockSource(handle,DacsAdcClockDiv2);

		//set scan rate
		daqAdcSetFreq(handle, RATE);

		//Set buffer location, size and flag settings
		daqAdcTransferSetBuffer(handle, rawbuffer, SCANS, DatmUpdateSingle|DatmCycleOn|DatmDriverBuf);	
			
		//Set to Trigger Immediatly
		daqSetTriggerEvent(handle, STARTSOURCE, TRIGSENSE, channels[0], gains[0], flags[0],
    						DaqTypeDBK81, 0, 0, DaqStartEvent);
		//Set to Stop when scan is complete
		daqSetTriggerEvent(handle, STOPSOURCE, TRIGSENSE, channels[0], gains[0], flags[0],
							DaqTypeDBK81, 0,0, DaqStopEvent);

		//begin data acquisition
		daqAdcTransferStart(handle);
		daqAdcArm(handle);
		printf("\nAcquiring Data...\n");
			
		do 
		{
			//transfer data (voltage readings) into computer memory and halt acquisition when done
			daqAdcTransferGetStat(handle, &active,  &retCount);
		}
		while ( active & DaafAcqActive);
            
		// transfer acquired data into "buffer"
		daqAdcTransferBufData(handle, rawbuffer, SCANS, DabtmNoWait, &retCount);
         
             
		//Disarm Daqboard
		daqAdcDisarm(handle);

		printf("Scan Completed\n\n"); 
		    
		daqCvtTCSetupConvert(CHANCOUNT,     //Number of scans
							 0,             //CJC position in scan array
							 4,             //number of scans following CJC scan
							 Dbk81TCTypeT,  //TC type
							 TRUE,          //Bipolar
		    				 0,             //avg, 0 means average all scans to one reading
							 rawbuffer,     //location of scans
							 SCANS,         //number of scans in buffer
							 convbuffer,    //array to hold temp values	
							 4);            //returns number of elements, should be 1 for each TC                                			
		
        
		//The conversion routine converts the data into tenths of a degree Celcius
		//scaling is very simple, just recall that you have a new buffer and fewer channels

		for (x = 0; x < 20 ; x++)        //CHANCOUNT-1 if not using shorted channels
			conv_buffer[x] = convbuffer[x] / 10.0f ;
						
		//print scan data
		printf("Test Data: \n");
		printf("TC01\t\tTC02\t\tTC03\t\tTC04\n");

        for(x = 0; x < 4; x++)
        {   
		    printf("%3.1f C\t\t", conv_buffer[x]);
        }
        
        printf("\n");
        			
		//close device connections
        daqClose(handle);
        

	}
	else
    {
	    printf("Could not connect to device\n");
    }
	
    return 0;
}
