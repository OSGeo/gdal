/******************************************************************************
 *
 * Project:  ISO 8211 Access
 * Purpose:  Various utility functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "iso8211.h"

#include <cstdlib>
#include <cstring>

#include "cpl_conv.h"

/************************************************************************/
/*                             DDFScanInt()                             */
/*                                                                      */
/*      Read up to nMaxChars from the passed string, and interpret      */
/*      as an integer.                                                  */
/************************************************************************/

int DDFScanInt(const char *pszString, int nMaxChars)

{
    char szWorking[33] = {};

    if (nMaxChars > 32 || nMaxChars == 0)
        nMaxChars = 32;

    memcpy(szWorking, pszString, nMaxChars);
    szWorking[nMaxChars] = '\0';

    return atoi(szWorking);
}

/************************************************************************/
/*                          DDFScanVariable()                           */
/*                                                                      */
/*      Establish the length of a variable length string in a           */
/*      record.                                                         */
/************************************************************************/

int DDFScanVariable(const char *pszRecord, int nMaxChars, int nDelimChar)

{
    int i = 0;  // Used after for.

    for (; i < nMaxChars - 1 && pszRecord[i] != nDelimChar; i++)
    {
    }

    return i;
}

/************************************************************************/
/*                          DDFFetchVariable()                          */
/*                                                                      */
/*      Fetch a variable length string from a record, and allocate      */
/*      it as a new string (with CPLStrdup()).                          */
/************************************************************************/

char *DDFFetchVariable(const char *pszRecord, int nMaxChars, int nDelimChar1,
                       int nDelimChar2, int *pnConsumedChars)

{
    int i = 0;  // Used after for.
    for (; i < nMaxChars - 1 && pszRecord[i] != nDelimChar1 &&
           pszRecord[i] != nDelimChar2;
         i++)
    {
    }

    *pnConsumedChars = i;
    if (i < nMaxChars &&
        (pszRecord[i] == nDelimChar1 || pszRecord[i] == nDelimChar2))
        (*pnConsumedChars)++;

    char *pszReturn = static_cast<char *>(CPLMalloc(i + 1));
    pszReturn[i] = '\0';
    strncpy(pszReturn, pszRecord, i);

    return pszReturn;
}
