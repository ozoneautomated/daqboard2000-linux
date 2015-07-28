/* 
   iotypes.h  : typedef the things we use, plus some structures common 
                to all things daqboard2000ish

   Copyright (C) 2002, 2002 by IOtech

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

#ifndef IOTTYPES_H
#define IOTTYPES_H
#include <linux/types.h>

#ifdef __KERNEL__
#ifdef DEBUG_DB2K
// #warning Doing Debugging driver build
#define  dbg(format, arg...)   dev_dbg(Daq_dev, "DB2K:%s:%d: " format "\n",__FUNCTION__, __LINE__ , ## arg)
#define showwhere() printk(KERN_DEBUG "DB2K: location: %s :%d:\n", __FUNCTION__, __LINE__)
#else
#define  dbg(format, arg...) do {} while (0)
#define  showwhere() do {} while (0)
#endif /*DEBUG_DB2K*/

#define  err(format, arg...) printk(KERN_ERR "DB2K:%s:%d: " format "\n", __FUNCTION__, __LINE__, ## arg)
#define info(format, arg...) printk(KERN_INFO "DB2K:%s:%d: " format "\n", __FUNCTION__, __LINE__, ## arg)
#define warn(format, arg...) printk(KERN_WARNING "DB2K:%s:%d: " format "\n", __FUNCTION__, __LINE__, ## arg)
#else /*__KERNEL__*/

#ifdef DEBUG_DB2K
//#warning Doing Debugging build
#define showwhere() fprintf(stderr,"location:%s,%s:%d\n", __FILE__, __FUNCTION__, __LINE__)
#define  dbg(format, arg...) fprintf(stderr, "Debug:%s:%d: " format "\n",  __FUNCTION__, __LINE__, ## arg)
#else
#define showwhere() do {} while (0)
#define  dbg(format, arg...) do {} while (0)
#endif /*DEBUG_DB2K*/

/*
 * min()/max() macros that also do strict type-checking...
 * bonus: arguments are only evaluated once.
 * already in kernel.h, only define if kernel.h is not included
 * probably only works with gcc, but then again this is for linux.
 */
#define min(x,y) ({ \
        const typeof(x) _x = (x);       \
        const typeof(y) _y = (y);       \
        (void) (&_x == &_y);            \
        _x < _y ? _x : _y; })
#define max(x,y) ({ \
        const typeof(x) _x = (x);       \
        const typeof(y) _y = (y);       \
        (void) (&_x == &_y);            \
        _x > _y ? _x : _y; })
#endif /*__KERNEL__*/

/* Don't change this value unless you are prepared to suffer the 
 * consequences of _hard_ _coded_ length twiddling done by the
 * scranky IOtech code.  I'm still working on fixing it...
 */
#define NAMESIZE 64

/* defined as nothing to make them go away */
#define FAR
#define WINAPI
#define CALLBACK
#ifndef _DAQAPI32_
#define _DAQAPI32_
#endif
#define __stdcall
#define HighNTMemory 0x00000000ffffff

#define ERROR_SUCCESS          0
#define NO_ERROR               0
#define MAX_SUPPORTED_DEVICE  40

/*PEK: the following are intrinsic types: NEVER redefine them
 * repeat after me: 
 *     "we are not smarter than the glibc and kernel hackers"
 *     " and will never again redefine intrinsic system types"
 */
// #define size_t DWORD  
// #define MAX_PATH 255  /* use PATH_MAX instead */
// #define __int64 long long int

#define DLL_PROCESS_DETACH      0
#define DLL_PROCESS_ATTACH      1
#define DLL_THREAD_ATTACH       2
#define DLL_THREAD_DETACH       3

/* 
 * PEK: someone had these as #defines, not typedefs :-(
 * and why do we need five (5) different names for `char *' ???
 * and who are the idiots who succeeded in _using_ all 5 names
 * in the code: left hand not knowing what the right hand is doing? 
 * (plus feet and forehead?).
 * in addition to char * and PCHAR we had:
 typedef char * LPSTR;
 typedef char * LPCSTR;
 typedef char * LPCTSTR;
 * only PCHAR is now used
 */
typedef char CHAR;
typedef char * PCHAR;
typedef unsigned char BYTE ;
typedef BYTE * PBYTE;

typedef short int SHORT;
typedef SHORT * PSHORT;
typedef unsigned short int WORD;
typedef WORD * PWORD;

/* the INT, UINT, DWORD was a mess
 * now (P)DWORD is the unsigned version
 *     (P)INT   is the signed version
 */
typedef int INT;
typedef int * PINT;
typedef unsigned int DWORD;
typedef DWORD * PDWORD;


typedef unsigned long ULONG;
typedef DWORD INTERFACE_TYPE;
typedef ULONG HGLOBAL;
typedef PDWORD HWND;
typedef DWORD HKEY;
typedef HKEY *PHKEY;
typedef DWORD HMODULE;
typedef ULONG FARPROC;
typedef DWORD HINSTANCE;

typedef DWORD BOOL;
typedef BOOL *PBOOL;

#define TRUE                  ((BOOL) 1)
#define FALSE                 ((BOOL) 0)

typedef signed long int LONG;
typedef signed long int * PLONG;

typedef void VOID;
typedef VOID * PVOID;

/* Handle Type Definition */
typedef unsigned long  DaqHandleT;
#define NO_GRIP ((DaqHandleT) -1)

typedef struct _OSVERSIONINFO {
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwMinorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
	CHAR szCSDVersion[128];
} OSVERSIONINFO, *POSVERSIONINFO;

typedef struct _OVERLAPPED {
	DWORD Internal;
	DWORD InternalHigh;
	DWORD Offset;
	DWORD OffsetHigh;
	DaqHandleT hEvent;
} OVERLAPPED, *POVERLAPPED;

typedef struct _SECURITY_ATTRIBUTES {
	DWORD nLength;
	PVOID lpSecurityDescriptor;
	BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES;

#ifndef __KERNEL__
#include "linux_daqx.h"
#endif

#endif
