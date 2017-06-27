/******************************************************************************
 *
 * Project:  NTF Translator
 * Purpose:  NTFRecord class implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ntf.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

static int nFieldBufSize = 0;
static char *pszFieldBuf = NULL;

static const int MAX_RECORD_LEN = 160;

/************************************************************************/
/*                             NTFRecord()                              */
/*                                                                      */
/*      The constructor is where the record is read.  This includes     */
/*      transparent merging of continuation lines.                      */
/************************************************************************/

NTFRecord::NTFRecord( VSILFILE * fp ) :
    nType(99),
    nLength(0),
    pszData(NULL)
{
    if( fp == NULL )
        return;

/* ==================================================================== */
/*      Read lines until we get to one without a continuation mark.     */
/* ==================================================================== */
    char szLine[MAX_RECORD_LEN+3] = {};
    int nNewLength = 0;

    do {
        nNewLength = ReadPhysicalLine( fp, szLine );
        if( nNewLength == -1 || nNewLength == -2 )
            break;

        while( nNewLength > 0 && szLine[nNewLength-1] == ' ' )
               szLine[--nNewLength] = '\0';

        if( nNewLength < 2 || szLine[nNewLength-1] != '%' )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Corrupt NTF record, missing end '%%'." );
            CPLFree( pszData );
            pszData = NULL;
            break;
        }

        if( pszData == NULL )
        {
            nLength = nNewLength - 2;
            pszData = static_cast<char *>(VSI_MALLOC_VERBOSE(nLength+1));
            if (pszData == NULL)
            {
                return;
            }
            memcpy( pszData, szLine, nLength );
            pszData[nLength] = '\0';
        }
        else
        {
            if( !STARTS_WITH_CI(szLine, "00") || nNewLength < 4 )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Invalid line");
                VSIFree(pszData);
                pszData = NULL;
                return;
            }

            char* pszNewData = static_cast<char *>(
                VSI_REALLOC_VERBOSE(pszData, nLength + (nNewLength - 4) + 1));
            if (pszNewData == NULL)
            {
                VSIFree(pszData);
                pszData = NULL;
                return;
            }

            pszData = pszNewData;
            memcpy( pszData+nLength, szLine+2, nNewLength-4 );
            nLength += nNewLength-4;
            pszData[nLength] = '\0';
        }
    } while( szLine[nNewLength-2] == '1' );

/* -------------------------------------------------------------------- */
/*      Figure out the record type.                                     */
/* -------------------------------------------------------------------- */
    if( pszData != NULL )
    {
        char  szType[3];

        strncpy( szType, pszData, 2 );
        szType[2] = '\0';

        nType = atoi(szType);
    }
}

/************************************************************************/
/*                             ~NTFRecord()                             */
/************************************************************************/

NTFRecord::~NTFRecord()

{
    CPLFree( pszData );

    if( pszFieldBuf != NULL )
    {
        CPLFree( pszFieldBuf );
        pszFieldBuf = NULL;
        nFieldBufSize = 0;
    }
}

/************************************************************************/
/*                          ReadPhysicalLine()                          */
/************************************************************************/

int NTFRecord::ReadPhysicalLine( VSILFILE *fp, char *pszLine )

{
/* -------------------------------------------------------------------- */
/*      Read enough data that we are sure we have a whole record.       */
/* -------------------------------------------------------------------- */
    int nRecordStart = static_cast<int>(VSIFTellL( fp ));
    const int nBytesRead =
        static_cast<int>(VSIFReadL( pszLine, 1, MAX_RECORD_LEN+2, fp ));

    if( nBytesRead == 0 )
    {
        if( VSIFEofL( fp ) )
            return -1;
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Low level read error occurred while reading NTF file." );
            return -2;
        }
    }

/* -------------------------------------------------------------------- */
/*      Search for CR or LF.                                            */
/* -------------------------------------------------------------------- */
    int i = 0;  // Used after for.
    for( ; i < nBytesRead; i++ )
    {
        if( pszLine[i] == 10 || pszLine[i] == 13 )
            break;
    }

/* -------------------------------------------------------------------- */
/*      If we don't find EOL within 80 characters something has gone    */
/*      badly wrong!                                                    */
/* -------------------------------------------------------------------- */
    if( i == MAX_RECORD_LEN+2 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%d byte record too long for NTF format.  "
                  "No line may be longer than 80 characters though up "
                  "to %d tolerated.",
                  nBytesRead, MAX_RECORD_LEN );
        return -2;
    }

/* -------------------------------------------------------------------- */
/*      Trim CR/LF.                                                     */
/* -------------------------------------------------------------------- */
    const int l_nLength = i;
    const int nRecordEnd =
        nRecordStart + i + ( pszLine[i+1] == 10 || pszLine[i+1] == 13 ? 2 : 1 );

    pszLine[l_nLength] = '\0';

/* -------------------------------------------------------------------- */
/*      Restore read pointer to beginning of next record.               */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( fp, nRecordEnd, SEEK_SET ) != 0 )
        return -1;

    return l_nLength;
}

/************************************************************************/
/*                              GetField()                              */
/*                                                                      */
/*      Note that the start position is 1 based, to match the           */
/*      notation in the NTF document.  The returned pointer is to an    */
/*      internal buffer, but is zero terminated.                        */
/************************************************************************/

const char * NTFRecord::GetField( int nStart, int nEnd )

{
    const int nSize = nEnd - nStart + 1;

    if( pszData == NULL )
        return "";

/* -------------------------------------------------------------------- */
/*      Reallocate working buffer larger if needed.                     */
/* -------------------------------------------------------------------- */
    if( nFieldBufSize < nSize + 1 )
    {
        CPLFree( pszFieldBuf );
        nFieldBufSize = nSize + 1;
        pszFieldBuf = static_cast<char *>(CPLMalloc(nFieldBufSize));
    }

/* -------------------------------------------------------------------- */
/*      Copy out desired data.                                          */
/* -------------------------------------------------------------------- */
    if( nStart + nSize > nLength+1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to read %d to %d, beyond the end of %d byte long\n"
                  "type `%2.2s' record.\n",
                  nStart, nEnd, nLength, pszData );
        memset( pszFieldBuf, ' ', nSize );
        pszFieldBuf[nSize] = '\0';
    }
    else
    {
        strncpy( pszFieldBuf, pszData + nStart - 1, nSize );
        pszFieldBuf[nSize] = '\0';
    }

    return pszFieldBuf;
}
