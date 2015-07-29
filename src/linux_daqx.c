////////////////////////////////////////////////////////////////////////////
//
// linux_daqx.c                    I    OOO                           h
//                                 I   O   O     t      eee    cccc   h
//                                 I  O     O  ttttt   e   e  c       h hhh
// -----------------------------   I  O     O    t     eeeee  c       hh   h
// copyright: (C) 2002 by IOtech   I   O   O     t     e      c       h    h
//    email: linux@iotech.com      I    OOO       tt    eee    cccc   h    h
//
////////////////////////////////////////////////////////////////////////////
//
//   Copyright (C) 86, 91, 1995-2002, Free Software Foundation, Inc.
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2, or (at your option)
//   any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software Foundation,
//   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
//  
////////////////////////////////////////////////////////////////////////////
//
// Thanks to Amish S. Dave for getting us started up!
// Savagely hacked by Paul Knowles <Paul.Knowles@unifr.ch>,  May, 2003
//
// Many Thanks Paul, MAH
//
////////////////////////////////////////////////////////////////////////////
//
// Please email liunx@iotech with any bug reports questions or comments
//
////////////////////////////////////////////////////////////////////////////


#include <stdlib.h>
#include <sys/mman.h>
#include <sys/timeb.h>
#include <string.h>
#include <errno.h>

//#include "iottypes.h"
#include "daqx_linuxuser.h"

//MAH: 04.15.04 Test
#include "daqx.h"
#include "cmnapi.h"
#include "api.h"
#include "ddapi.h"
#include "itfapi.h"
#include "w32ioctl.h"
//END

typedef struct _LINMMAP {
	int gfpsize[4];
} linMmapT, *linMapPT;

linMmapT linMmap;

int GBL_devtype;

DWORD
GetTickCount(void)
{
	struct timeb the_time;
//	struct timeval tv;
//	struct timezone tz;
	DWORD i;
	/* ftime is obsolete. Don't use it.
	 * needs to be replaced by gettimeofday
	 * when I understand the implications I will do so...
	 */
//      gettimeofday(&tv, &tz);
//      i = (DWORD) (tv.tv_usec/1000)

	ftime(&the_time);
	i = the_time.millitm;

	return i;
}

void
strlwr(PCHAR str)
{
	DWORD i;
	for (i = 0; i < strlen(str); i++)
		str[i] = tolower(str[i]);
	return;
}

int
stricmp(PCHAR aliasName, PCHAR valueName)
{
	int z;
	size_t x,y;

	x = strlen(aliasName);
	y = strlen(valueName);
	z = strncasecmp(aliasName, valueName, min(x,y));

	return z;
}

void
OutputDebugString(PCHAR str)
{
	return;
}



DaqHandleT  CreateFile(char* filename,DWORD accessMode,DWORD shareMode,DWORD Security,DWORD createMode,DWORD flagsAttributes,DWORD temphandle)
{

        DaqHandleT	daq_handle;

	//printf("linux_daqx CreateFile -filename = %s\n",filename);


        if ((createMode & OPEN_ALWAYS) != OPEN_ALWAYS)
        {
          daq_handle = (DaqHandleT)open(filename,O_RDWR| O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP  | S_IROTH); 
        }
        else
        {
                daq_handle = (DaqHandleT)open(filename,O_RDWR | O_APPEND);
        }
        if (NO_GRIP == daq_handle) {
               perror("open");
        }
        return  daq_handle;
}
DaqHandleT  OpenDevice(char* filename,DWORD accessMode,DWORD shareMode,DWORD Security,DWORD createMode,DWORD flagsAttributes,DWORD temphandle)
{

        DaqHandleT	daq_handle;
	char path_name[80];

	//printf("linux_daqx CreateFile -filename = %s\n",filename);

        strcpy(path_name, "/dev/db2k/");

	if (!strcmp(filename,"\\\\.\\daqbrd2k0"))
	{
		strcat(path_name,"DaqBoard2K0");
	}
	if (!strcmp(filename,"\\\\.\\daqbrd2k1"))
	{
		strcat(path_name,"DaqBoard2K1");
	}
	if (!strcmp(filename,"\\\\.\\daqbrd2k2"))
	{
		strcat(path_name,"DaqBoard2K2");
	}
	if (!strcmp(filename,"\\\\.\\daqbrd2k3"))
	{
		strcat(path_name,"DaqBoard2K3");
	}

        if ((createMode & OPEN_ALWAYS) != OPEN_ALWAYS)
        {
                daq_handle = (DaqHandleT)open(path_name,O_RDWR); 
        }
        else
        {
                daq_handle = (DaqHandleT)open(path_name,O_RDWR | O_APPEND);
        }
        if (NO_GRIP == daq_handle) {
               perror("open");
        }
        return  daq_handle;
}


BOOL
FlushFileBuffers(DaqHandleT handle)
{
        int res;
	if ( NO_GRIP == handle )
       {
		return FALSE;
	}
	res = fsync(handle);
	if (res != -1)
		return TRUE;
	dbg("errno =%d",errno);
	return FALSE;
}

BOOL
SetFilePointer(DaqHandleT handle, LONG fileSize, PLONG fileSizeHigh, DWORD filebegin)
{
	off_t  res;
	res = lseek(handle, fileSize, SEEK_SET);
	if (res != -1)
		return TRUE;
	dbg("errno =%d",errno);
	return FALSE;
}

BOOL
WriteFile(DaqHandleT handle, PVOID buffer, DWORD count, PDWORD written, PDWORD overlap)
{
	size_t res;
	res = write(handle, buffer, count);
	if (res == count)
		return TRUE;
	dbg("short write %ld of %ld",(unsigned long) res,(unsigned long)count);
	return FALSE;
}

BOOL
SetEndOfFile(DaqHandleT handle)
{
	off_t  res;
	res = lseek(handle, 0, SEEK_END);
	if (res != -1)
		return TRUE;
	dbg("errno =%d",errno);
	return FALSE;
}


BOOL
CloseHandle(DaqHandleT handle)
{
	if(close(handle)){
		dbg("errno =%d",errno);
		return FALSE;
	}
	return TRUE;
}


LONG
RegOpenKey(HKEY hKey, PCHAR lpSubKey, PHKEY phkResult)
{
	char string2[]="\\daqbrd2k";
	int match;
	char* substring;
	// size_t size;


	substring= strrchr( (char *) lpSubKey,'\\');
	// size = strlen(string2);
	//fprintf(stderr,"substring %s  string2 %s  \n",substring,string2);
	match = strncmp(substring,string2,9);

	if(match != 0)
	{
		//fprintf(stderr,"no match \n");
		return ~ERROR_SUCCESS;
	}
	else
	{
		// Its a DaqBoard2K(X) return x

		//fprintf(stderr,"substring %s\n",substring);
		if (!strcmp(substring,"\\daqbrd2k0"))
		{
			*phkResult = 1;
			return ERROR_SUCCESS;
		}
		if (!strcmp(substring,"\\daqbrd2k1"))
		{
			*phkResult = 2;
			return ERROR_SUCCESS;
		}
		if (!strcmp(substring,"\\daqbrd2k2"))
		{
			*phkResult = 3;
			return ERROR_SUCCESS;
		}
		if (!strcmp(substring,"\\daqbrd2k3"))
		{
			*phkResult = 4;
			return ERROR_SUCCESS;
		}
	}
	//Should never reach here
	return ~ERROR_SUCCESS;
}


LONG
RegCloseKey(HKEY hKey)
{
	return ERROR_SUCCESS;
}



LONG
RegQueryValueEx(HKEY hKey, PCHAR lpValueName, PDWORD lpReserved,
		PDWORD lpType, PBYTE lpData, PDWORD lpcbData)
{

	char deviceName[64];
	//int dev_type = 0;
	char proc_file_name[64];
	size_t count;
	char proc_dev_type[3];
	int proc_fd;

        memset((void *)proc_dev_type, 0, 2);
	PDWORD pdwData = (PDWORD) lpData;
	//fprintf(stderr,"lpValueName %s\n",lpValueName);
	if(!strcasecmp((char*) lpValueName,"AliasName"))
	{
		switch(hKey)
		{
			case 1:
				sprintf(deviceName,"DaqBoard2K0");
				break;
			case 2:
				sprintf(deviceName,"DaqBoard2K1");
				break;
			case 3:
				sprintf(deviceName,"DaqBoard2K2");
				break;
			case 4:
				sprintf(deviceName,"DaqBoard2K3");
				break;
			default:
				fprintf(stderr,"Somthing REAL broken");
		}

		strcpy((char*)lpData,deviceName);
		return ERROR_SUCCESS;
	}


	if((	!strcasecmp((char*) lpValueName,"BasePortAddress"))
			||(	!strcasecmp((char*) lpValueName,"Location")))
	{
		*pdwData =hKey;
		//*pdwData = 0x00020009;
		//regQueryIndex++;
		return ERROR_SUCCESS;
	}

	if(!strcasecmp((char*) lpValueName,"DeviceType"))
	{
		//MAH: 04.05.04 Ok the fun is over - must call down to /proc
		// to find out who we really are - Paul was very very right!

		//*pdwData = 0x00000012;  // 2001     formerly 0x16 2005 NO DACS
		switch(hKey)
		{
			case 1:
				sprintf(proc_file_name,"/proc/iotech/drivers/db2k/DaqBoard2K0/type");
				break;
			case 2:
				sprintf(proc_file_name,"/proc/iotech/drivers/db2k/DaqBoard2K1/type");
				break;
			case 3:
				sprintf(proc_file_name,"/proc/iotech/drivers/db2k/DaqBoard2K2/type");
				break;
			case 4:
				sprintf(proc_file_name,"/proc/iotech/drivers/db2k/DaqBoard2K3/type");
				break;
			default:
				fprintf(stderr,"Somthing REAL broken");
		}

		proc_fd =  open(proc_file_name, O_RDONLY);
		if(proc_fd == -1)
		{
			printf("proc open failed\n");
		}
		else
		{
			count = read(proc_fd,proc_dev_type,2);
                        if (2 != count) {
                          dbg("read proc_dev_type failed");
                        }
			//printf("proc_dev_type %s\n",proc_dev_type);
			close(proc_fd);
		}        

		*pdwData  = atoi(proc_dev_type);
		//printf("PDW %i\n",*(int *)pdwData);

		//regQueryIndex++;
		return ERROR_SUCCESS;
	}

	if(!strcasecmp((char*) lpValueName,"DMAChannel"))
	{
		*pdwData = 0x0002c909;
		//regQueryIndex++;
		return ERROR_SUCCESS;
	}

	if(!strcasecmp((char*) lpValueName,"InterruptLevel"))
	{
		*pdwData = 0x0;
		//regQueryIndex++;
		return ERROR_SUCCESS;
	}

	if(!strcasecmp((char*) lpValueName,"Protocol"))
	{
		*pdwData = 0x00000190;
		//regQueryIndex++;
		return ERROR_SUCCESS;
	}

	fprintf(stderr,"Failed! Passed through linux_daqx -> RegQueryValueEx");
	return ~ERROR_SUCCESS;
}


BOOL
GetVersionEx(POSVERSIONINFO lpVersionInfo)
{
	lpVersionInfo->dwPlatformId = VER_PLATFORM_WIN32_NT;
	return TRUE;
}

VOID
GlobalFree(HGLOBAL hglobal)
{
	free((void *) hglobal);
}

HGLOBAL
GlobalAlloc(DWORD type, DWORD size)
{
	PVOID got;
	got = malloc(size);
	if(got != NULL)
		return (HGLOBAL) got;
	dbg("malloc failed. Fetch the shift worker a beer!");
	abort();
}

void
GlobalUnlock(HGLOBAL hg)
{
	return;
}

void *
GlobalLock(HGLOBAL hg)
{
	return NULL;
}


DWORD
GetFileSize(DaqHandleT handle, PDWORD lpdword)
{
	struct stat stat_p;
	if (stat((PCHAR) handle, &stat_p)){
		dbg("stat failed errno =%d",errno);
		return 0;
	}
	return (DWORD) stat_p.st_size;
}

DWORD
filelength(DaqHandleT handle)
{
	PDWORD lpdword = 0;
	return GetFileSize(handle, lpdword);
}


int
MessageBox(HWND hWnd, PCHAR lpText, PCHAR lpCaption, DWORD uType)
{
	int data;
	PCHAR stub;
	stub = strtok(lpText, "?");
	fprintf(stderr, "\n*** %s \n\n", lpCaption);
	fprintf(stderr, "MSG: -> %s?\n", stub);    
	fprintf(stderr, "Press 'y' or 'n' ");    
    
	fflush(NULL);

    

	while((data = Daqkeyhit()) == 0);

	if (data == 'y') {
		fprintf(stderr, "\nProgram will attempt to continue\n");
		return 0;
	}

	fprintf(stderr, "\nProgram will terminate\n");
	return IDNO;
}

BOOL
DeviceIoControl(DaqHandleT hDevice, DWORD dwIoControlCode, PVOID lpInBuffer,
		DWORD nInBufferSize, PVOID lpOutBuffer, DWORD nOutBufferSize,
		PDWORD lpBytesReturned, POVERLAPPED lpOverlapped)
{
	int err;
	err = ioctl(hDevice, dwIoControlCode, lpInBuffer);
    
    //MAH: 04.15.04 TEST
    if(err)
    {
        printf("IOCTL ERR %i\n",err);
    }
	return (BOOL) (err < 0 ? FALSE : TRUE);
}

DWORD
GetSystemDirectory(PCHAR lpBuffer, DWORD uSize)
{
	return 1;
}

HMODULE
LoadLibrary(PCHAR lpLibFileName)
{
	return 1;
}

FARPROC
GetProcAddress(HMODULE hModule, PCHAR lpProcName)
{
	return 1;
}

DaqHandleT
CreateEvent(PSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset,
	    BOOL bInitialState, PCHAR lpName)
{
	return 1;
}

DaqHandleT
CreateMutex(PSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, PCHAR lpName)
{
	return 1;
}

BOOL
ReleaseMutex(DaqHandleT hMutex)
{
	return 1;
}

DWORD
WaitForSingleObject(DaqHandleT hHandle, DWORD dwMilliseconds)
{
	return 1;
}

DWORD
GetCurrentProcessId(VOID)
{

	return (DWORD) getpid();
}

BOOL
SetEvent(DaqHandleT hEvent)
{
	return 1;
}

VOID
ExitProcess(DWORD uExitCode)
{

	exit (uExitCode);
}

PWORD
linMap(int size, DaqHandleT handle, DaqHandleT linhandle)
{
	int gfp_size = 4096;
	PWORD pmap;      


	/* (1<<21) is 2Mb : this while loop finishes when
	 * gfp_size can hold `size' or is maxed out at 2Mb
	 */
	while( (size > gfp_size) && (gfp_size < ( 1 << 21 )) )
		gfp_size <<= 1;

	if(size > gfp_size){
		dbg("Greedy memory user! asked for %i, max is %i", size, gfp_size);
		return NULL;
	}

	linMmap.gfpsize[handle] = gfp_size;

	/* PEK: I've changed the request from size to gfp_size
	 * since this is what we free with later on.
	 * NB: the driver rechecks all this anyway.
	 */
	pmap = (PWORD) mmap(NULL, /*size*/ gfp_size, PROT_READ, MAP_PRIVATE, linhandle, 0);
	if (pmap != MAP_FAILED){
          dbg("mmap'ed %i bytes on fd=%ld, gfp_size=%i", size, (unsigned long) linhandle, gfp_size);
		return pmap;
	}

	dbg("mmap failed errno=%d",errno);
	return NULL;
}


VOID
linUnMap(DaqHandleT handle, DaqHandleT linhandle)
{
	munmap((PVOID) linhandle, linMmap.gfpsize[handle]);
	return;
}
