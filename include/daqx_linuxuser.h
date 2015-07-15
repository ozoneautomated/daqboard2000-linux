
/*
 *   This should be the only header you need in any file
 *   using the LINUX API Data Acquisition Library.
 */

/* 
   daqx_linuxuser.h : part of the DaqBoard2000 kernel driver 

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

#ifndef LINUXDAQ_H
#define LINUXDAQ_H

#include "iottypes.h"
#include "daqx.h"

#ifdef   __cplusplus
extern "C" 
{
#endif

/* in the library, used only externally */
	INT Daqkeyhit(VOID);


#ifdef   __cplusplus
};
#endif

#endif				/* #ifndef LINUXDAQ_H */
