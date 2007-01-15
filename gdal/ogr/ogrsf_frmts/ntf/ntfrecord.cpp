/******************************************************************************
 * $Id$
 *
 * Project:  NTF Translator
 * Purpose:  NTFRecord class implementation.
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
 * Revision 1.9  2003/01/07 16:46:47  warmerda
 * improve long line check
 *
 * Revision 1.8  2002/02/18 21:40:03  warmerda
 * Fixed error message.
 *
 * Revision 1.7  2002/02/11 17:04:45  warmerda
 * cleanup tabs
 *
 * Revision 1.6  2002/02/11 17:04:03  warmerda
 * tolerate somewhat longer lines, irish data has long lines
 *
 * Revision 1.5  2002/02/11 16:54:47  warmerda
 * Use internal code to read records, ensuring that "zero" bytes or other
 * binary garbage are survivable.   Added implicit assumption that records
 * are no more than 80 data bytes.
 *
 * Revision 1.4  2002/02/08 20:43:06  warmerda
 * improved error checking and propagation
 *
 * Revision 1.3  2001/12/12 02:48:43  warmerda
 * ensure cplreadline buffer freed
 *
 * Revision 1.2  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.1  1999/08/28 03:13:35  warmerda
 * New
 *
 */

#include "ntf.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

static int nFieldBufSize = 0;
static char *pszFieldBuf = NULL;

#define MAX_RECORD_LEN  160

/************************************************************************/
/*                             NTFRecord()                              */
/*                                                                      */
/*      The constructor is where the record is read.  This includes     */
/*      transparent merging of continuation lines.                      */
/************************************************************************/

NTFRecord::NTFRecord( FILE * fp )

{
    nType = 99;
    nLength = 0;
    pszData = NULL;

    if( fp == NULL )
        return;

/* ==================================================================== */
/*      Read lines untill we get to one without a continuation mark.    */
/* ==================================================================== */
    char      szLine[MAX_RECORD_LEN+3];
    int       nNewLength;

    do { 
        nNewLength = ReadPhysicalLine( fp, szLine );
        if( nNewLength == -1 || nNewLength == -2 )
            break;

        while( nNewLength > 0 && szLine[nNewLength-1] == ' ' )
               szLine[--nNewLength] = '\0';

        if( szLine[nNewLength-1] != '%' )
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
            pszData = (char *) CPLMalloc(nLength+1);
            memcpy( pszData, szLine, nLength );
            pszData[nLength] = '\0';
        }
        else
        {
            pszData = (char *) CPLRealloc(pszData,nLength+(nNewLength-4)+1);
            
            CPLAssert( EQUALN(szLine,"00",2) );
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
        char      szType[3];

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

int NTFRecord::ReadPhysicalLine( FILE *fp, char *pszLine )

{
    int         nBytesRead = 0;
    int         nRecordStart, nRecordEnd, i, nLength = 0;

/* -------------------------------------------------------------------- */
/*      Read enough data that we are sure we have a whole record.       */
/* -------------------------------------------------------------------- */
    nRecordStart = VSIFTell( fp );
    nBytesRead = VSIFRead( pszLine, 1, MAX_RECORD_LEN+2, fp );

    if( nBytesRead == 0 )
    {
        if( VSIFEof( fp ) )
            return -1;
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Low level read error occured while reading NTF file." );
            return -2;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Search for CR or LF.                                            */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nBytesRead; i++ )
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
                  "%d byte record too long for NTF format.\n"
                  "No line may be longer than 80 characters though up to %d tolerated.\n",
                  nBytesRead, MAX_RECORD_LEN );
        return -2;
    }

/* -------------------------------------------------------------------- */
/*      Trim CR/LF.                                                     */
/* -------------------------------------------------------------------- */
    nLength = i;
    if( pszLine[i+1] == 10 || pszLine[i+1] == 13 )
        nRecordEnd = nRecordStart + i + 2;
    else
        nRecordEnd = nRecordStart + i + 1;

    pszLine[nLength] = '\0';

/* -------------------------------------------------------------------- */
/*      Restore read pointer to beginning of next record.               */
/* -------------------------------------------------------------------- */
    VSIFSeek( fp, nRecordEnd, SEEK_SET );
    
    return nLength;
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
    int      nSize = nEnd - nStart + 1;

/* -------------------------------------------------------------------- */
/*      Reallocate working buffer larger if needed.                     */
/* -------------------------------------------------------------------- */
    if( nFieldBufSize < nSize + 1 )
    {
        CPLFree( pszFieldBuf );
        nFieldBufSize = nSize + 1;
        pszFieldBuf = (char *) CPLMalloc(nFieldBufSize);
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



