/******************************************************************************
 * $Id$
 *
 * Project:  CEOS Translator
 * Purpose:  Test mainline.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.4  2006/04/04 00:34:40  fwarmerdam
 * updated contact info
 *
 * Revision 1.3  2001/11/07 14:11:25  warmerda
 * changed to just dump records
 *
 * Revision 1.2  2001/10/19 15:36:55  warmerda
 * added support for filename on commandline
 *
 * Revision 1.1  1999/05/05 17:32:38  warmerda
 * New
 *
 */

#include "ceosopen.h"

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    const char  *pszFilename;
    FILE	*fp;
    CEOSRecord *psRecord;
    int nPosition = 0;

    if( nArgc > 1 )
        pszFilename = papszArgv[1];
    else
        pszFilename = "imag_01.dat";

    fp = VSIFOpen( pszFilename, "rb" );
    if( fp == NULL )
    {
        fprintf( stderr, "Can't open %s at all.\n", pszFilename );
        exit( 1 );
    }

    while( !VSIFEof(fp) 
           && (psRecord = CEOSReadRecord( fp )) != NULL )
    {
        printf( "%9d:%4d:%8x:%d\n", 
                nPosition, psRecord->nRecordNum, 
                psRecord->nRecordType, psRecord->nLength );
        CEOSDestroyRecord( psRecord );

        nPosition = (int) VSIFTell( fp );
    }
    VSIFClose( fp );

    exit( 0 );
}
