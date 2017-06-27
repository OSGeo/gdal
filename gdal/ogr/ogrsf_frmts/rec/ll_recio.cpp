/******************************************************************************
 *
 * Project:  EPIInfo .REC Reader
 * Purpose:  Implements low level REC reading API.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_rec.h"

CPL_CVSID("$Id$")

static int nNextRecLine = 0;

/************************************************************************/
/*                          RECGetFieldCount()                          */
/************************************************************************/

int RECGetFieldCount( FILE * fp )

{
    const char *pszLine = CPLReadLine( fp );
    if( pszLine == NULL )
        return -1;
    if( atoi(pszLine) < 1 )
        return -1;

    nNextRecLine = 1;

    return atoi(pszLine);
}

/************************************************************************/
/*                       RECGetFieldDefinition()                        */
/************************************************************************/

int RECGetFieldDefinition( FILE *fp, char *pszFieldname,
                           int *pnType, int *pnWidth, int *pnPrecision )

{
    const char *pszLine = CPLReadLine( fp );

    if( pszLine == NULL )
        return FALSE;

    if( strlen(pszLine) < 44 )
        return FALSE;

    // Extract field width.
    *pnWidth = atoi( RECGetField( pszLine, 37, 4 ) );

    OGRFieldType eFType = OFTString;

    // Is this an real, integer or string field?  Default to string.
    int nTypeCode = atoi(RECGetField(pszLine,33,4));
    if( nTypeCode == 0 )
    {
        eFType = OFTInteger;
    }
    else if( nTypeCode > 100 && nTypeCode < 120 )
    {
        eFType = OFTReal;
    }
    else if( nTypeCode == 6 )
    {
        if( *pnWidth < 3 )
            eFType = OFTInteger;
        else
            eFType = OFTReal;
    }
    else
    {
      eFType = OFTString;
    }

    *pnType = static_cast<int>(eFType);

    strcpy( pszFieldname, RECGetField( pszLine, 2, 10 ) );
    *pnPrecision = 0;

    if( nTypeCode > 100 && nTypeCode < 120 )
    {
      *pnPrecision = nTypeCode - 100;
    }
    else if( eFType == OFTReal )
    {
        *pnPrecision = *pnWidth - 1;
    }

    nNextRecLine++;

    return TRUE;
}

/************************************************************************/
/*                            RECGetField()                             */
/************************************************************************/

const char *RECGetField( const char *pszSrc, int nStart, int nWidth )

{
    // FIXME non thread safe
    static char szWorkField[128] = {};

    if( nWidth >= static_cast<int>(sizeof(szWorkField)) )
        nWidth = sizeof(szWorkField)-1;
    strncpy( szWorkField, pszSrc + nStart - 1, nWidth );
    szWorkField[nWidth] = '\0';

    int i = static_cast<int>(strlen(szWorkField)) - 1;

    while( i >= 0 && szWorkField[i] == ' ' )
        szWorkField[i--] = '\0';

    return szWorkField;
}

/************************************************************************/
/*                           RECReadRecord()                            */
/************************************************************************/

int RECReadRecord( FILE *fp, char *pszRecord, int nRecordLength )

{
    int nDataLen = 0;

    while( nDataLen < nRecordLength )
    {
        const char *pszLine = CPLReadLine( fp );

        nNextRecLine++;

        if( pszLine == NULL )
            return FALSE;

        if( *pszLine == 0 || *pszLine == 26 /* Cntl-Z - DOS EOF */ )
            return FALSE;

        // If the end-of-line markers is '?' the record is deleted.
        int iSegLen = (int)strlen(pszLine);
        if( pszLine[iSegLen-1] == '?' )
        {
            pszRecord[0] = '\0';
            nDataLen = 0;
            continue;
        }

        // Strip off end-of-line '!' marker.
        if( pszLine[iSegLen-1] != '!'
            && pszLine[iSegLen-1] != '^' )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Apparent corrupt data line at line=%d",
                      nNextRecLine );
            return FALSE;
        }

        iSegLen--;
        if( nDataLen + iSegLen > nRecordLength )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Too much data for line at line %d.",
                      nNextRecLine-1 );
            return FALSE;
        }

        strncpy( pszRecord+nDataLen, pszLine, iSegLen );
        pszRecord[nDataLen+iSegLen] = '\0';
        nDataLen += iSegLen;
    }

    return nDataLen;
}
