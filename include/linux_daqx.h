/* 
   linux_daqx.h : part of the DaqBoard2000 kernel driver 

   Copyright (C) 86, 91, 1995-2002, Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
 */

/* Originally Written by 
  ////////////////////////////////////////////////////////////////////////////
  //                                 I    OOO                           h
  //                                 I   O   O     t      eee    cccc   h
  //                                 I  O     O  ttttt   e   e  c       h hhh
  // -----------------------------   I  O     O    t     eeeee  c       hh   h
  // copyright: (C) 2002 by IOtech   I   O   O     t     e      c       h    h
  //    email: linux@iotech.com      I    OOO       tt    eee    cccc   h    h
  ////////////////////////////////////////////////////////////////////////////

  Savagely hacked by Paul Knowles <Paul.Knowles@unifr.ch>,  May, 2003
 */

/*
 * stuff used only internally in the library
 * not to be explicitly included in user code...
 */

#ifndef LINUX_DAQX
#define LINUX_DAQX

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MB_OK                       0x00000000L
#define MB_OKCANCEL                 0x00000001L
#define MB_ABORTRETRYIGNORE         0x00000002L
#define MB_YESNOCANCEL              0x00000003L
#define MB_YESNO                    0x00000004L
#define MB_RETRYCANCEL              0x00000005L
#define MB_ICONHAND                 0x00000010L
#define MB_ICONQUESTION             0x00000020L
#define MB_ICONEXCLAMATION          0x00000030L
#define MB_ICONASTERISK             0x00000040L
#define MB_USERICON                 0x00000080L
#define MB_ICONWARNING              MB_ICONEXCLAMATION
#define MB_ICONERROR                MB_ICONHAND
#define MB_ICONINFORMATION          MB_ICONASTERISK
#define MB_ICONSTOP                 MB_ICONHAND
#define MB_DEFBUTTON1               0x00000000L
#define MB_DEFBUTTON2               0x00000100L
#define MB_DEFBUTTON3               0x00000200L
#define MB_DEFBUTTON4               0x00000300L
#define MB_APPLMODAL                0x00000000L
#define MB_SYSTEMMODAL              0x00001000L
#define MB_TASKMODAL                0x00002000L
#define MB_HELP                     0x00004000L
#define MB_NOFOCUS                  0x00008000L
#define MB_SETFOREGROUND            0x00010000L
#define MB_DEFAULT_DESKTOP_ONLY     0x00020000L

#define IDOK		1
#define IDCANCEL	2
#define IDABORT		3
#define IDRETRY		4
#define IDIGNORE	5
#define IDYES		6
#define IDNO		7
#define IDCLOSE		8
#define IDHELP		9

#define FILE_FLAG_WRITE_THROUGH         0x80000000
#define FILE_FLAG_OVERLAPPED            0x40000000
#define FILE_FLAG_NO_BUFFERING          0x20000000
#define FILE_FLAG_RANDOM_ACCESS         0x10000000
#define FILE_FLAG_SEQUENTIAL_SCAN       0x08000000
#define FILE_FLAG_DELETE_ON_CLOSE       0x04000000
#define FILE_FLAG_BACKUP_SEMANTICS      0x02000000
#define FILE_FLAG_POSIX_SEMANTICS       0x01000000
#define FILE_FLAG_OPEN_REPARSE_POINT    0x00200000
#define FILE_FLAG_OPEN_NO_RECALL        0x00100000

#define GENERIC_READ                (0x80000000)
#define GENERIC_WRITE               (0x40000000)
#define FILE_SHARE_READ             (0x00000001)
#define FILE_SHARE_WRITE            (0x00000002)
#define FILE_FLAG_SEQUENTIAL_SCAN   0x08000000
#define CREATE_NEW          1
#define CREATE_ALWAYS       2
#define OPEN_EXISTING       3
#define OPEN_ALWAYS         4
#define TRUNCATE_EXISTING   5
#define DELETE                      0x00010000L
#define FILE_BEGIN                  0
#define FILE_CURRENT                1
#define FILE_END                    2
#define FILE_ATTRIBUTE_READONLY         0x00000001
#define FILE_ATTRIBUTE_HIDDEN           0x00000002
#define FILE_ATTRIBUTE_SYSTEM           0x00000004
#define FILE_ATTRIBUTE_DIRECTORY        0x00000010
#define FILE_ATTRIBUTE_ARCHIVE          0x00000020
#define FILE_ATTRIBUTE_NORMAL           0x00000080
#define FILE_ATTRIBUTE_TEMPORARY        0x00000100
#define FILE_FLAG_WRITE_THROUGH     0x80000000
#define FILE_FLAG_RANDOM_ACCESS     0x10000000

#define VER_PLATFORM_WIN32s             0
#define VER_PLATFORM_WIN32_WINDOWS      1
#define VER_PLATFORM_WIN32_NT           2

#define GMEM_FIXED          0x0000
#define GMEM_MOVEABLE       0x0002
#define GMEM_NOCOMPACT      0x0010
#define GMEM_NODISCARD      0x0020
#define GMEM_ZEROINIT       0x0040
#define GMEM_MODIFY         0x0080
#define GMEM_DISCARDABLE    0x0100
#define GMEM_NOT_BANKED     0x1000
#define GMEM_SHARE          0x2000
#define GMEM_DDESHARE       0x2000
#define GMEM_NOTIFY         0x4000
#define GMEM_LOWER          GMEM_NOT_BANKED
#define GMEM_VALID_FLAGS    0x7F72
#define GMEM_INVALID_HANDLE 0x8000

#define HKEY_LOCAL_MACHINE 1
#define INFINITE 0xFFFFFFFF

VOID _init(VOID);
VOID _fini(VOID);

BOOL DeviceIoControl(DaqHandleT hDevice, DWORD dwIoControlCode,
		     PVOID lpInBuffer, DWORD nInBufferSize, 
		     PVOID lpOutBuffer, DWORD nOutBufferSize, 
		     PDWORD lpBytesReturned, POVERLAPPED lpOverlapped);
BOOL FlushFileBuffers(DaqHandleT handle);
BOOL GetVersionEx(POSVERSIONINFO lpVersionInfo);
BOOL ReleaseMutex(DaqHandleT hMutex);
BOOL SetEndOfFile(DaqHandleT handle);
BOOL SetEvent(DaqHandleT hEvent);
BOOL SetFilePointer(DaqHandleT handle, LONG, PLONG, DWORD);
BOOL WriteFile(DaqHandleT handle, PVOID buffer, DWORD count, 
		PDWORD written, PDWORD overlap);
BOOL CloseHandle(DaqHandleT handle);

DaqHandleT CreateFile(PCHAR filename, DWORD accessMode, 
		 DWORD shareMode, DWORD Security, 
		 DWORD createMode, DWORD flagsAttributes, DWORD temphandle);
DaqHandleT OpenDevice(PCHAR filename, DWORD accessMode, 
		 DWORD shareMode, DWORD Security, 
		 DWORD createMode, DWORD flagsAttributes, DWORD temphandle);
DWORD GetCurrentProcessId(VOID);
DWORD GetFileSize(DaqHandleT handle, PDWORD);
DWORD GetTickCount();
DWORD WaitForSingleObject(DaqHandleT hHandle, DWORD dwMilliseconds);
DWORD filelength(DaqHandleT handle);

FARPROC GetProcAddress(HMODULE hModule, PCHAR lpProcName);
DaqHandleT CreateEvent(PSECURITY_ATTRIBUTES lpEventAttributes,
		       BOOL bManualReset, BOOL bInitialState, PCHAR lpName);
DaqHandleT CreateMutex(PSECURITY_ATTRIBUTES lpMutexAttributes, 
		       BOOL bInitialOwner, PCHAR lpName);
HGLOBAL GlobalAlloc(DWORD type, DWORD size);
HMODULE LoadLibrary(PCHAR lpLibFileName);

LONG RegCloseKey(HKEY hKey);
LONG RegOpenKey(HKEY hKey, PCHAR lpSubKey, PHKEY phkResult);
LONG RegQueryValueEx(HKEY hKey, PCHAR lpValueName, PDWORD lpReserved, 
		     PDWORD lpType, PBYTE lpData, PDWORD lpcbData);
PWORD linMap(int size, DaqHandleT handle, DaqHandleT linhandle);
DWORD GetSystemDirectory(PCHAR lpBuffer, DWORD uSize);
VOID ExitProcess(DWORD uExitCode);
VOID linUnMap(DaqHandleT handle, DaqHandleT linhandle);
VOID GlobalFree(HGLOBAL hglobal);
VOID GlobalUnlock(HGLOBAL hglobal);
VOID OutputDebugString(PCHAR);
VOID strlwr(PCHAR str);
PVOID GlobalLock(HGLOBAL hglobal);
int MessageBox(HWND hWnd, PCHAR lpText, PCHAR lpCaption, DWORD uType);
int stricmp(PCHAR aliasName, PCHAR valueName);

#endif
