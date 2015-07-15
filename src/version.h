////////////////////////////////////////////////////////////////////////////
//
// version.h                       I    OOO                           h
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

#ifndef VERSION_H
#define VERSION_H

#ifdef _DEBUG
#define VDEBUG "  DEBUG"
#else
#define VDEBUG
#endif

#define VER_MAJOR    3
#define VER_MINOR    02
#define VER_REVISION 02
#define VER_RELEASE "3.02.02" VDEBUG

#define VER_COMPANY      "IOtech, Inc."
#define VER_COPYRIGHT    "Copyright \251 " VER_COMPANY " 1997-2002"
#define VER_FILEDESC     "DaqX Enhanced API Library"
#define VER_FILETYPE     VFT_DLL
#define VER_FILEVER      VER_RELEASE
#define VER_INTERNALNAME "DAQX"
#define VER_ORIGFILENAME "DAQX.DLL"
#define VER_PRODNAME     "Data Acquisition Products"
#define VER_PRODVER      VER_RELEASE

#endif
