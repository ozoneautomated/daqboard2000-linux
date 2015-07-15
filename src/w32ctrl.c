////////////////////////////////////////////////////////////////////////////
//
// w32ctrl.c                       I    OOO                           h
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

#include <stdio.h>
#include "iottypes.h"

#include "daqx.h"
#include "cmnapi.h"
#include "ddapi.h"
#include "itfapi.h"
#include "w32ioctl.h"

const CHAR DAQ112_DEVICE_NAME[] = "\\\\?\\pcmcia#iotech_inc-pcmcia_12-bit_analog_input-";
const CHAR DAQ216_DEVICE_NAME[] = "\\\\?\\pcmcia#iotech_inc-pcmcia_16-bit_analog_input-";
const CHAR GUID_FOR_DAQPCC[] = "{d325f120-790a-11d4-b63c-0050da7b1eff}";

#define MaxddLen 128

typedef struct {
	DaqHandleT ddHandle;
	BOOL bDaqPCC;
} ItfCtrlT;

static ItfCtrlT session[MaxSessions];

VOID
itfCtrlBroadcastMessage(MsgPT msg)
{
	itfCtrlMessageHandler(msg);
}

VOID
itfCtrlMessageHandler(MsgPT msg)
{
	int whichSession;

	if (msg->errCode == DerrNoError) {
		switch (msg->msg) {
		case MsgAttach:
			for (whichSession = 0; whichSession < MaxSessions; whichSession++)
				session[whichSession].ddHandle = NO_GRIP;
			break;

		case MsgDetach:
			for (whichSession = 0; whichSession < MaxSessions; whichSession++)
				itfClose(whichSession);
			break;

		case MsgInit:            
			msg->errCode = itfInit(msg->handle, msg->daqName, msg);
			break;

		case MsgClose:
			msg->errCode = itfClose(msg->handle);
			break;
		}
	}

}

DaqError
itfExecuteCommand(DaqHandleT handle, daqIOTP sb, ItfCommandCodeT cmdCode)
{
	DWORD nBytes, err;
	nBytes = sizeof (daqIOT);
	sb->errorCode = 0;
	cmnWaitForMutex(IoctlMutex);

	err = !DeviceIoControl(session[handle].ddHandle,
			       CTL_CODE(IOTECH_TYPE, FUNCTION_BASE + cmdCode,
					METHOD_BUFFERED, FILE_ANY_ACCESS), sb,
			       sizeof (daqIOT), sb, sizeof (daqIOT), &nBytes, NULL);

	cmnReleaseMutex(IoctlMutex);

	if (err) {
		return DerrNotOnLine;
	}

	return (DaqError) sb->errorCode;
}


DWORD
GetLPTPorts(PBYTE UserLPTPortsArray, DWORD numPorts)
{

#define MAX_LPTPORTS 4
#define GET_LPTPORTS 6
#define DAQRES_FUNCTION_BASE  0x1200
#define CTL_GET_LPTPORTS   CTL_CODE(IOTECH_TYPE, DAQRES_FUNCTION_BASE+GET_LPTPORTS, METHOD_BUFFERED, FILE_ANY_ACCESS)

	WORD i;
	BYTE LPTPortsArray[MAX_LPTPORTS] = { 0, 0, 0, 0 };

	DWORD dwNumLPTPorts = MAX_LPTPORTS;
	DaqHandleT handle;

	if (numPorts > MAX_LPTPORTS)
		numPorts = MAX_LPTPORTS;

#if 0
	if (OS.IsWNT()) {
		if (NO_GRIP ==
		    (handle =
		     CreateFile("\\\\.\\daqres", GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
				OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0))) {

			return (FALSE);
		}

	}
	else {
#endif
		if (NO_GRIP ==
		    (handle =
		     CreateFile("\\\\.\\daqres.vxd", 0, 0, 0, 0, FILE_FLAG_DELETE_ON_CLOSE, 0))) {
			return (FALSE);
		}
//	}

	if (!DeviceIoControl
	    (handle, CTL_GET_LPTPORTS, NULL, 0, LPTPortsArray, MAX_LPTPORTS,
	     &dwNumLPTPorts, NULL)) {

		return (FALSE);
	}

	CloseHandle(handle);

	if (UserLPTPortsArray != NULL) {
		for (i = 0; i < numPorts; i++) {
			UserLPTPortsArray[i] = LPTPortsArray[i];
		}
	}

	return (TRUE);

}

DaqError
searchForBkDevName(PCHAR aliasName, PCHAR vxdName, MsgPT msg)
{
	int i;
	HKEY key;
	DWORD strSize, value, keySize, devices;
	CHAR searchPath[MaxddLen];
	CHAR deviceName[MaxddLen];
	CHAR valueName[MaxddLen];
	CHAR devNumStr[3];
	LONG retVal;

	keySize = sizeof (DWORD);
	devices = 0;
	for (i = 0; (i < MaxSessions) && (devices < 3) ; i++) {

		strSize = sizeof (valueName);
		if (i <= 3 && devices > 0) {
			sprintf(deviceName, "WaveBk");
		}

		if (i <= 3 && devices > 1) {
			sprintf(deviceName, "TempBk");
		}

		if (devices < 1)
			sprintf(deviceName, "DaqBk");

		sprintf(devNumStr, "%d", i);
		strcat(deviceName, devNumStr);

		strcpy(searchPath, "SYSTEM\\CurrentControlSet\\Services\\Daqx\\");
		strcat(searchPath, deviceName);

		if (RegOpenKey(HKEY_LOCAL_MACHINE, searchPath, &key) == ERROR_SUCCESS) {
			fprintf(stderr, "Key OPENED\n");
			retVal =
			    RegQueryValueEx(key, "AliasName", NULL, NULL,
					    (PBYTE) valueName, &strSize);
			if (retVal == ERROR_SUCCESS) {

				if (!stricmp(aliasName, valueName)) {
					retVal =
					    RegQueryValueEx(key,
							    "BasePortAddress",
							    NULL, NULL, (PBYTE) & value, &keySize);
					if (retVal == ERROR_SUCCESS) {
						BYTE LPTPortsArray[4];
						sprintf(vxdName, "\\\\.\\%s", deviceName);

#if 0
						if (OS.IsW9X()) {
							strcat(vxdName, ".vxd");
						}
#endif

						if (RegQueryValueEx
						    (key, "DeviceType", NULL,
						     NULL, (PBYTE) & value,
						     &keySize) == ERROR_SUCCESS)
							msg->deviceType = (DaqHardwareVersion)
							    value;
						if (RegQueryValueEx
						    (key, "Protocol", NULL,
						     NULL, (PBYTE) & value,
						     &keySize) == ERROR_SUCCESS)
							msg->protocol = (DaqProtocol) value;
						RegCloseKey(key);

						if (GetLPTPorts(LPTPortsArray, 4)) {

						}

						return DerrNoError;
					} else
						return DerrNotOnLine;
				}
			}
		}

		if (i == 3) {
			if (devices == 0 || devices == 1)
				i = -1;
			devices++;
		}
	}
	RegCloseKey(key);
	return DerrBadAlias;
}

DaqError
searchForDbrdVxdName(PCHAR aliasName, PCHAR vxdName, MsgPT msg)
{
	int i;
	HKEY key;
	DWORD strSize, keySize, value;
	CHAR searchPath[MaxddLen];
	CHAR deviceName[MaxddLen];
	CHAR valueName[MaxddLen];

	keySize = sizeof (DWORD);
	for (i = 0; i < MaxSessions; i++) {
		strSize = sizeof (valueName);
		sprintf(deviceName, "daqbrd2k%d", i);
		strcpy(searchPath, "SYSTEM\\CurrentControlSet\\Services\\Daqx\\");
		strcat(searchPath, deviceName);
		if (RegOpenKey(HKEY_LOCAL_MACHINE, searchPath, &key) == ERROR_SUCCESS) {
			if (RegQueryValueEx
			    (key, "AliasName", NULL, NULL, (PBYTE) valueName,
			     &strSize) == ERROR_SUCCESS) {
				if (RegQueryValueEx
				    (key, "DeviceType", NULL, NULL,
				     (PBYTE) & value, &keySize) == ERROR_SUCCESS)
					msg->deviceType = (DaqHardwareVersion) value;
				if (RegQueryValueEx
				    (key, "Protocol", NULL, NULL,
				     (PBYTE) & value, &keySize) == ERROR_SUCCESS)
					msg->protocol = (DaqProtocol) value;
				RegCloseKey(key);

				if (!stricmp(aliasName, valueName)) {
					strcpy(vxdName, "\\\\.\\");
					strcat(vxdName, deviceName);

#if 0
					if (OS.IsW9X()) {
						strcat(vxdName, ".vxd");
					}
#endif

					return DerrNoError;
				}
			}
		}
	}
	RegCloseKey(key);
	return DerrBadAlias;
}

DaqError
GetDbrd2KDriverName(DWORD PCIAddress, PCHAR vxdName)
{
	int i;
	HKEY key;

	DWORD keySize, value;
	CHAR searchPath[MaxddLen + 256];
	CHAR deviceName[MaxddLen];

	keySize = sizeof (DWORD);
	for (i = 0; i < MaxSessions; i++) {
		sprintf(deviceName, "daqbrd2k%d", i);
		strcpy(searchPath, "SYSTEM\\CurrentControlSet\\Services\\Daqx_Present_Devices\\");
		strcat(searchPath, deviceName);
		if (RegOpenKey(HKEY_LOCAL_MACHINE, searchPath, &key) == ERROR_SUCCESS) {
			if (RegQueryValueEx
			    (key, "Location", NULL, NULL, (PBYTE) & value,
			     &keySize) == ERROR_SUCCESS) {
				if (value == PCIAddress) {

#if 0
					if (OS.IsW9X()) {
						strcpy(vxdName, "\\\\.\\");
						strcat(vxdName, "daq2k");
						strcat(vxdName,
						       deviceName + (strlen(deviceName) - 1));
						strcat(vxdName, ".vxd");
					} else 
#endif

					{
						strcpy(vxdName, "\\\\.\\");
						strcat(vxdName, deviceName);
					}

					RegCloseKey(key);
					return DerrNoError;
				}
			}
		}
	}

	RegCloseKey(key);
	return DerrBadAlias;
}

DaqError
searchForDbrd2KVxdName(PCHAR aliasName, PCHAR vxdName, MsgPT msg)
{
	int i;
	HKEY key;
	DWORD strSize, keySize, value;
	CHAR searchPath[MaxddLen + 256];
	CHAR deviceName[MaxddLen];
	CHAR valueName[MaxddLen];

	DWORD basePortAddress;

	keySize = sizeof (DWORD);
	for (i = 0; i < MaxSessions; i++) {
		strSize = sizeof (valueName);
		sprintf(deviceName, "daqbrd2k%d", i);
		strcpy(searchPath, "SYSTEM\\CurrentControlSet\\Services\\Daqx\\");
		strcat(searchPath, deviceName);
		if (RegOpenKey(HKEY_LOCAL_MACHINE, searchPath, &key) == ERROR_SUCCESS) {
			if (RegQueryValueEx
			    (key, "AliasName", NULL, NULL, (PBYTE) valueName,
			     &strSize) == ERROR_SUCCESS) {
				if (RegQueryValueEx
				    (key, "DeviceType", NULL, NULL,
				     (PBYTE) & value, &keySize) == ERROR_SUCCESS)
					msg->deviceType = (DaqHardwareVersion) value;

				if (RegQueryValueEx
				    (key, "Protocol", NULL, NULL,
				     (PBYTE) & value, &keySize) == ERROR_SUCCESS)
					msg->protocol = (DaqProtocol) value;

				if (RegQueryValueEx
				    (key, "BasePortAddress", NULL, NULL,
				     (PBYTE) & value, &keySize) == ERROR_SUCCESS)
					basePortAddress = value;

				RegCloseKey(key);

				if (!stricmp(aliasName, valueName)) {
					return GetDbrd2KDriverName(basePortAddress, vxdName);
				}

			}
		}
	}
	RegCloseKey(key);

	return DerrBadAlias;
}

DaqError
searchForDaqVxdName(PCHAR aliasName, PCHAR vxdName, MsgPT msg)
{
	int i;
	HKEY key;
	DWORD strSize, keySize, value;
	CHAR searchPath[MaxddLen];
	CHAR deviceName[MaxddLen];
	CHAR valueName[MaxddLen];

	DWORD daqModel = 0xA50C;
	DWORD driverInstance = 1;

	CHAR daqName[64] = { 0 };
	DaqHardwareVersion daqType = Daq112;

	keySize = sizeof (DWORD);
	for (i = 0; i < MaxSessions; i++) {
		strSize = sizeof (valueName);
		sprintf(deviceName, "daqpcc%d", i);
		strcpy(searchPath, "SYSTEM\\CurrentControlSet\\Services\\Daqx\\");
		strcat(searchPath, deviceName);
		if (RegOpenKey(HKEY_LOCAL_MACHINE, searchPath, &key) == ERROR_SUCCESS) {
			if (RegQueryValueEx
			    (key, "AliasName", NULL, NULL, (PBYTE) valueName,
			     &strSize) == ERROR_SUCCESS) {
				if (!stricmp(aliasName, valueName)) {

					value = (DWORD) Daq112;

					if (RegQueryValueEx
					    (key, "DeviceType", NULL, NULL,
					     (PBYTE) & value, &keySize) == ERROR_SUCCESS)
						msg->deviceType = daqType =
						    (DaqHardwareVersion) value;

#if 0
					if (OS.IsW9X()) {
						strcpy(vxdName, "\\\\.\\daqpcc.vxd");
					} else 
#endif

					{

						value = 0;

						if (RegQueryValueEx
						    (key, "DaqModel", NULL,
						     NULL, (PBYTE) & value,
						     &keySize) == ERROR_SUCCESS)
							daqModel = value;

						if (RegQueryValueEx
						    (key, "DriverInstance",
						     NULL, NULL,
						     (PBYTE) & value, &keySize) == ERROR_SUCCESS)
							driverInstance = value;

						if (daqType == Daq216)
							strcpy(daqName, DAQ216_DEVICE_NAME);
						else
							strcpy(daqName, DAQ112_DEVICE_NAME);

						sprintf(vxdName, "%s%4X#%1d#%s",
							daqName, daqModel,
							driverInstance, GUID_FOR_DAQPCC);

					}

					RegCloseKey(key);
					return DerrNoError;
				}
			}
		}
	}
	RegCloseKey(key);
	return DerrBadAlias;
}

DaqError
itfInit(DaqHandleT handle, PCHAR aliasName, MsgPT msg)
{
	DaqError err;
	CHAR ddName[MaxddLen];

	daqIOT sb;
	DAQPCC_OPEN_PT pdaqPCCOpen = (DAQPCC_OPEN_PT) & sb;
	
	err = itfClose(handle);

	if (err != DerrNoError)
		return err;

    
    //MAH: 04.05.04 Here is the name issue
	err = searchForDbrdVxdName(aliasName, ddName, msg);
   
	if (err != DerrNoError) {
       
		err = searchForBkDevName(aliasName, ddName, msg);
		if (err != DerrNoError) {
           
			err = searchForDaqVxdName(aliasName, ddName, msg);
           
			if (err != DerrNoError) {
               
				err = searchForDbrd2KVxdName(aliasName, ddName, msg);
				if (err != DerrNoError) {
					fprintf(stderr, "Error: %s:%s:%d: Nodevices found. err=%i\n", 
						__FILE__, __FUNCTION__, __LINE__, msg->errCode);
					return err;
				}
			}
		}
	}

#if 0
	if (OS.IsW9X()) {
		session[handle].ddHandle =
			CreateFile(ddName, 0, 0, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
	}
	if (OS.IsWNT()) {
		session[handle].ddHandle = CreateFile(ddName,
						      GENERIC_READ |
						      GENERIC_WRITE,
						      FILE_SHARE_READ |
						      FILE_SHARE_WRITE, 0,
						      OPEN_EXISTING, 
						      FILE_FLAG_OVERLAPPED, 0);
	} else 
#endif

	session[handle].ddHandle = CreateFile(ddName, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);

	if (session[handle].ddHandle == NO_GRIP) {
		return DerrNotOnLine;
	}

	if ((msg->deviceType == Daq112) || (msg->deviceType == Daq216)) {

		strcpy(pdaqPCCOpen->alias, aliasName);
		err = itfExecuteCommand(handle, &sb, DAQPCC_OPEN);
		if (err != DerrNoError) {
			session[handle].bDaqPCC = FALSE;
			return DerrNotOnLine;
		}
		session[handle].bDaqPCC = TRUE;
	} else
		session[handle].bDaqPCC = FALSE;

	return DerrNoError;
}

DaqError
itfClose(DaqHandleT handle)
{
	DaqError err;
	CHAR dbugmsg[100];
	daqIOT sb;

	if (session[handle].ddHandle != NO_GRIP) {
		if (session[handle].bDaqPCC) {
			err = itfExecuteCommand(handle, &sb, DAQPCC_CLOSE);
			if (err != DerrNoError) {
				sprintf(dbugmsg,
					"DAQX: error calling DAQPCC_CLOSE control service handler: %d",
					err);
				OutputDebugString(dbugmsg);
				return DerrNotOnLine;
			}
		}

		if (CloseHandle(session[handle].ddHandle)) {
			session[handle].ddHandle = NO_GRIP;
		}
		else {
			return DerrNotOnLine;
		}
	}
	return DerrNoError;
}

DaqError
itfOnline(DaqHandleT handle, PBOOL online)
{
	*online = (session[handle].ddHandle != NO_GRIP);
	return DerrNoError;
}

DAQAPI DaqError WINAPI
dspCmd(DaqHandleT handle, DWORD action, DWORD memType, DWORD addr, PDWORD data)
{
	DWORD count;
	daqIOT sb;
	DEV_DSP_CMDS_PT pdspCmd = (DEV_DSP_CMDS_PT) & sb;

	pdspCmd->errorCode = DerrNoError;
	pdspCmd->action = action;
	pdspCmd->memType = memType;
	pdspCmd->addr = addr;

	if (action == 28)
		pdspCmd->data = (DWORD) data;
	else
		pdspCmd->data = *data;

	DeviceIoControl(session[handle].ddHandle, (DWORD) DB_DEV_DSP_CMDS, &sb,
			sizeof (sb), &sb, sizeof (sb), &count, NULL);

	if (action != 28)
		*data = pdspCmd->data;

	return (DaqError) pdspCmd->errorCode;
}

DAQAPI DaqError WINAPI
daqBlockMemIO(DaqHandleT handle, DWORD action, DWORD blockType,
	      DWORD whichBlock, DWORD addr, DWORD count, PVOID buf)
{

	DWORD iocount;

	daqIOT sb;
	BLOCK_MEM_IO_PT pBMIO = (BLOCK_MEM_IO_PT) & sb;

	pBMIO->errorCode = DerrNoError;
	pBMIO->action = action;
	pBMIO->blockType = blockType;
	pBMIO->whichBlock = whichBlock;
	pBMIO->addr = addr;
	pBMIO->count = count;
	pBMIO->buf = (DWORD) buf;

	DeviceIoControl(session[handle].ddHandle, (DWORD) DB_BLOCK_MEM_IO, &sb,
			sizeof (sb), &sb, sizeof (sb), &iocount, NULL);

	return (DaqError) pBMIO->errorCode;

}

#if 0
DaqErrors
itfProtocol(DaqHandleT handle, DaqProtocols desired, DaqProtocols * actual)
{

	if (desired)
		session[handle].protocol = desired;

	*actual = session[handle].protocol;

	return DerrNoError;

}

int

daqGetDevDesc(unsigned port, DevDescT * session)
{

	if (session) ;
	if (port) ;

	return DerrInvPort;

}

void
itfEnableInts(void)
{

}

void
itfDisableInts(void)
{

}

void
daqOnEvent(EventFPT process)
{

	if (process) ;

}

BYTE
daqRead(unsigned int cmd)
{

	if (cmd) ;

	return 0;

}

void
daqWrite(unsigned int cmd, BYTE val)
{

	if (cmd) ;
	if (val) ;

}

unsigned
daqReadBlock(unsigned int cmd, unsigned int samples, unsigned int *storage)
{

	if (cmd) ;
	if (samples) ;
	if (*storage) ;

	return 0;

}
#endif

DaqHandleT
itfGetDriverHandle(DaqHandleT handle)
{
	return session[handle].ddHandle;
}
