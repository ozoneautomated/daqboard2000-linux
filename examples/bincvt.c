////////////////////////////////////////////////////////////////////////////
//
// bincvt.cpp                      I    OOO                           h
//                                 I   O   O     t      eee    cccc   h
//                                 I  O     O  ttttt   e   e  c       h hhh
// -----------------------------   I  O     O    t     eeeee  c       hh   h
// copyright: (C) 2002 by IOtech   I   O   O     t     e      c       h    h
//    email: linux@iotech.com      I    OOO       tt    eee    cccc   h    h
//
////////////////////////////////////////////////////////////////////////////
//
// *************************************************************************
// *                                                                       *
// * This program is free software; you can redistribute it and/or modify  *
// * it under the terms of the GNU General Public License as published by  *
// * the Free Software Foundation; either version 2 of the License, or     *
// * (at your option) any later version.                                   *
// *                                                                       *
// *************************************************************************
//
////////////////////////////////////////////////////////////////////////////
//
// Please email liunx@iotech with any bug reports questions or comments
//
////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------                                                                                                                                    
//                                                                                                                                                      
// Binary file converter:
//      configured to work with DaqAdcEx04.cpp
//      Reads binary file named bindata.bin
//              Scales data and outputs to text file named data.out
//      and stderr                                              
//
//--------------------------------------------------------------------------                                                                                                                                                    

#include <stdio.h>

int main()
{
	FILE *infile;
	FILE *outfile;
	int x, high, low, count = 0;
	int numchannels = 2;

	//Get Pointers to Files
	infile = fopen("bindata.bin", "rb");
	outfile = fopen("data.out", "w");

	fprintf(stderr, "\n");
	fprintf(outfile, "\n");

	do {
		count++;
		for (x = 1; x < numchannels + 1; x++) {
			low = fgetc(infile);
			high = fgetc(infile) * 256;
			if (!feof(infile)) {
				if (x == 1)
					fprintf(stderr, "Point %i \t", count);
				fprintf(stderr, "Value %2.3f\t",
					(high + low - 32767) * .0003051850948);
				fprintf(outfile, "Value %2.3f\t",
					(high + low - 32767) * .0003051850948);

				if (x == numchannels) {
					fprintf(stderr, "\n");
					fprintf(outfile, "\n");
				}
			}

		}

	} while (!feof(infile));

	fprintf(stderr, "\n");
	fprintf(outfile, "\n");

	fclose(infile);
	fclose(outfile);

	return 0;
}
