/******************************************************************************
 * $Id$
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.7  2006/04/04 04:24:07  fwarmerdam
 * update contact info
 *
 * Revision 1.6  2001/07/18 04:51:57  warmerda
 * added CPL_CVSID
 *
 * Revision 1.5  1999/11/18 19:03:04  warmerda
 * expanded tabs
 *
 * Revision 1.4  1999/09/20 19:29:16  warmerda
 * make forgiving of UNIT/FIELD terminator mixup in Tiger SDTS files
 *
 * Revision 1.3  1999/05/06 14:41:03  warmerda
 * Removed unused variable.
 *
 * Revision 1.2  1999/05/06 14:23:49  warmerda
 * optimised DDFScanInt()
 *
 * Revision 1.1  1999/04/27 18:45:05  warmerda
 * New
 *
 */

#include "iso8211.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                             DDFScanInt()                             */
/*                                                                      */
/*      Read up to nMaxChars from the passed string, and interpret      */
/*      as an integer.                                                  */
/************************************************************************/

long DDFScanInt( const char * pszString, int nMaxChars )

{
    char        szWorking[33];

    if( nMaxChars > 32 || nMaxChars == 0 )
        nMaxChars = 32;

    memcpy( szWorking, pszString, nMaxChars );
    szWorking[nMaxChars] = '\0';

    return( atoi(szWorking) );
}

/************************************************************************/
/*                          DDFScanVariable()                           */
/*                                                                      */
/*      Establish the length of a variable length string in a           */
/*      record.                                                         */
/************************************************************************/

int DDFScanVariable( const char *pszRecord, int nMaxChars, int nDelimChar )

{
    int         i;
    
    for( i = 0; i < nMaxChars-1 && pszRecord[i] != nDelimChar; i++ ) {}

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
    int         i;
    char        *pszReturn;

    for( i = 0; i < nMaxChars-1 && pszRecord[i] != nDelimChar1
                                && pszRecord[i] != nDelimChar2; i++ ) {}

    *pnConsumedChars = i;
    if( i < nMaxChars
        && (pszRecord[i] == nDelimChar1 || pszRecord[i] == nDelimChar2) )
        (*pnConsumedChars)++;

    pszReturn = (char *) CPLMalloc(i+1);
    pszReturn[i] = '\0';
    strncpy( pszReturn, pszRecord, i );

    return pszReturn;
}
