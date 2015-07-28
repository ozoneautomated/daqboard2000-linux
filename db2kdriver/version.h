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

#define  PRODNAME "DaqBoard/2000"

#define VER_MAJOR    3
#define VER_MINOR    01
#define VER_REVISION 00

#define VER_RELEASE "2.00.00"

#define VER_COMPANY       "IOtech, Inc."
#define VER_FILEDESC      PRODNAME " Device Driver "
#define VER_FILEVER       VER_RELEASE
#define VER_INTERNALNAME  DDNAME
#define VER_ORIGFILENAME  DDNAME "." DDEXT
#define VER_PRODNAME      PRODNAME
#define VER_PRODVER       VER_RELEASE
#define VER_COPYRIGHT    "Copyright 2000-2001, " VER_COMPANY
#define VER_RELEASE_TYPE "Release of"

#define DB2KVERSION    VER_PRODNAME " " VER_RELEASE_TYPE " " __DATE__

#endif

