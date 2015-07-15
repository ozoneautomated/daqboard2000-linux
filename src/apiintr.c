////////////////////////////////////////////////////////////////////////////
//
// apiintr.c                       I    OOO                           h
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
/////////////////////////////////////////////////////////////////////////////
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

#ifdef THIS_FILE_IS_UNSUPPORTED

#include "iottypes.h"

#include "daqx.h"
#include "cmnapi.h"
#include "api.h"
#include "ddapi.h"
#include "itfapi.h"

typedef struct {
	BYTE curIntrMask;

} ApiIntrT;

void apiIntrSetMask(unsigned sesNumber, BYTE intrBits);

static ApiIntrT session[MaxSessions];
static ApiIntrT *curSession = &session[0];
static unsigned curSesNumber;

VOID
apiIntrMessageHandler(MsgPT msg)
{

	if (msg->errCode == DerrNoError) {
		switch (msg->msg) {
		case MsgAttach:
			break;

		case MsgDetach:
			break;

		case MsgInit:
			apiIntrSetMask(curSesNumber, 0x00);
			break;

		case MsgClose:
			break;
		}
	}

	return;
}

void
apiIntrSetMask(unsigned sesNumber, BYTE intrBits)
{
	if (intrBits) {
		daqRead(iStatus);
		session[sesNumber].curIntrMask |= intrBits;
	} else {
		session[sesNumber].curIntrMask = intrBits;
	}

#ifdef DebugDevFunc
	DebugString2("\ndevSetIntrMask: session:%d  curMask:%02x",
		     sesNumber, session[sesNumber].curIntrMask);
#endif
	return;
}

#endif
