////////////////////////////////////////////////////////////////////////////
//
// daqkeyhit.c                     I    OOO                           h
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
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <ctype.h>

#include "iottypes.h"

#ifdef   __cplusplus
extern "C" 
{
#endif
	INT Daqkeyhit(VOID) {
		struct termios term, oterm;
		int c = 0;

		/* get the terminal settings */
		tcgetattr(STDIN_FILENO, &oterm);

		/* get a copy of the settings, which we modify */
		memcpy(&term, &oterm, sizeof (term));

		/* put the terminal in non-canonical mode, any
		 * reads timeout after 0.4 seconds or when a
		 * single character is read 
		 */
		term.c_lflag = term.c_lflag & (~ICANON);
		term.c_cc[VMIN] = 0;
		term.c_cc[VTIME] = 4;
		tcsetattr(STDIN_FILENO, TCSANOW, &term);

		/* If timed out getchar() returns -1, 
		 * otherwise it returns the character 
		 */
		c = getchar();

		/* reset the terminal to original state */
		tcsetattr(STDIN_FILENO, TCSANOW, &oterm);

		if (c == -1){
			return 0;
		}
		return c;
	}

#ifdef   __cplusplus
};
#endif
