/******************************************************************************
 *
 * Project:  ISO 8211 Access
 * Purpose:  Various utility functions.
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
 ****************************************************************************/

#include "cpl_port.h"
#include "iso8211.h"

#include <cstdlib>
#include <cstring>

#include "cpl_conv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                             DDFScanInt()                             */
/*                                                                      */
/*      Read up to nMaxChars from the passed string, and interpret      */
/*      as an integer.                                                  */
/************************************************************************/

int DDFScanInt( const char * pszString, int nMaxChars )

{
    char szWorking[33] = {};

    if( nMaxChars > 32 || nMaxChars == 0 )
        nMaxChars = 32;

    memcpy( szWorking, pszString, nMaxChars );
    szWorking[nMaxChars] = '\0';

    return atoi(szWorking);
}

/************************************************************************/
/*                          DDFScanVariable()                           */
/*                                                                      */
/*      Establish the length of a variable length string in a           */
/*      record.                                                         */
/************************************************************************/

int DDFScanVariable( const char *pszRecord, int nMaxChars, int nDelimChar )

{
    int i = 0;  // Used after for.

    for( ; i < nMaxChars - 1 && pszRecord[i] != nDelimChar; i++ ) {}

    return i;
}

/************************************************************************/
/*                          DDFFetchVariable()                          */
/*                                                                      */
/*      Fetch a variable length string from a record, and allocate      */
/*      it as a new string (with CPLStrdup()).                          */
/************************************************************************/

char * DDFFetchVariable( const char *pszRecord, int nMaxChars,
                         int nDelimChar1, int nDelimChar2,
                         int *pnConsumedChars )

{
    int i = 0;  // Used after for.
    for( ;
         i < nMaxChars-1 && pszRecord[i] != nDelimChar1
         && pszRecord[i] != nDelimChar2;
         i++ )
    {}

    *pnConsumedChars = i;
    if( i < nMaxChars
        && (pszRecord[i] == nDelimChar1 || pszRecord[i] == nDelimChar2) )
        (*pnConsumedChars)++;

    char *pszReturn = static_cast<char *>(CPLMalloc(i + 1));
    pszReturn[i] = '\0';
    strncpy( pszReturn, pszRecord, i );

    return pszReturn;
}
