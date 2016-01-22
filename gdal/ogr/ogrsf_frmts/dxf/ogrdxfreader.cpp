/******************************************************************************
 * $Id: ogrdxf_diskio.cpp 20278 2010-08-14 15:11:01Z warmerdam $
 *
 * Project:  DXF Translator
 * Purpose:  Implements low level DXF reading with caching and parsing of
 *           of the code/value pairs. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_dxf.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"

CPL_CVSID("$Id: ogrdxf_diskio.cpp 20278 2010-08-14 15:11:01Z warmerdam $");

/************************************************************************/
/*                            OGRDXFReader()                            */
/************************************************************************/

OGRDXFReader::OGRDXFReader()

{
    fp = NULL;

    iSrcBufferOffset = 0;
    nSrcBufferBytes = 0;
    iSrcBufferFileOffset = 0;

    nLastValueSize = 0;
    nLineNumber = 0;
}

/************************************************************************/
/*                           ~OGRDXFReader()                            */
/************************************************************************/

OGRDXFReader::~OGRDXFReader()

{
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void OGRDXFReader::Initialize( VSILFILE *fp )

{
    this->fp = fp;
}

/************************************************************************/
/*                          ResetReadPointer()                          */
/************************************************************************/

void OGRDXFReader::ResetReadPointer( int iNewOffset )

{
    nSrcBufferBytes = 0;
    iSrcBufferOffset = 0;
    iSrcBufferFileOffset = iNewOffset;
    nLastValueSize = 0;
    nLineNumber = 0;

    VSIFSeekL( fp, iNewOffset, SEEK_SET );
}

/************************************************************************/
/*                           LoadDiskChunk()                            */
/*                                                                      */
/*      Load another block (512 bytes) of input from the source         */
/*      file.                                                           */
/************************************************************************/

void OGRDXFReader::LoadDiskChunk()

{
    CPLAssert( iSrcBufferOffset >= 0 );

    if( nSrcBufferBytes - iSrcBufferOffset > 511 )
        return;

    if( iSrcBufferOffset > 0 )
    {
        CPLAssert( nSrcBufferBytes <= 1024 );
        CPLAssert( iSrcBufferOffset <= nSrcBufferBytes );

        memmove( achSrcBuffer, achSrcBuffer + iSrcBufferOffset,
                 nSrcBufferBytes - iSrcBufferOffset );
        iSrcBufferFileOffset += iSrcBufferOffset; 
        nSrcBufferBytes -= iSrcBufferOffset;
        iSrcBufferOffset = 0;
    }

    nSrcBufferBytes += VSIFReadL( achSrcBuffer + nSrcBufferBytes, 
                                  1, 512, fp );
    achSrcBuffer[nSrcBufferBytes] = '\0';

    CPLAssert( nSrcBufferBytes <= 1024 );
    CPLAssert( iSrcBufferOffset <= nSrcBufferBytes );
}

/************************************************************************/
/*                             ReadValue()                              */
/*                                                                      */
/*      Read one type code and value line pair from the DXF file.       */
/************************************************************************/

int OGRDXFReader::ReadValue( char *pszValueBuf, int nValueBufSize )

{
/* -------------------------------------------------------------------- */
/*      Make sure we have lots of data in our buffer for one value.     */
/* -------------------------------------------------------------------- */
    if( nSrcBufferBytes - iSrcBufferOffset < 512 )
        LoadDiskChunk();

    if( nValueBufSize > 512 )
        nValueBufSize = 512;

/* -------------------------------------------------------------------- */
/*      Capture the value code, and skip past it.                       */
/* -------------------------------------------------------------------- */
    int iStartSrcBufferOffset = iSrcBufferOffset;
    int nValueCode = atoi(achSrcBuffer + iSrcBufferOffset);

    nLineNumber ++;

    // proceed to newline.
    while( achSrcBuffer[iSrcBufferOffset] != '\n' 
           && achSrcBuffer[iSrcBufferOffset] != '\r' 
           && achSrcBuffer[iSrcBufferOffset] != '\0' ) 
        iSrcBufferOffset++;

    if( achSrcBuffer[iSrcBufferOffset] == '\0' )
        return -1;

    // skip past newline.  CR, CRLF, or LFCR
    if( (achSrcBuffer[iSrcBufferOffset] == '\r'
         && achSrcBuffer[iSrcBufferOffset+1] == '\n' )
        || (achSrcBuffer[iSrcBufferOffset] == '\n'
            && achSrcBuffer[iSrcBufferOffset+1] == '\r' ) )
        iSrcBufferOffset += 2;
    else
        iSrcBufferOffset += 1;

    if( achSrcBuffer[iSrcBufferOffset] == '\0' )
        return -1;

/* -------------------------------------------------------------------- */
/*      Capture the value string.                                       */
/* -------------------------------------------------------------------- */
    int iEOL = iSrcBufferOffset;

    nLineNumber ++;

    // proceed to newline.
    while( achSrcBuffer[iEOL] != '\n' 
           && achSrcBuffer[iEOL] != '\r' 
           && achSrcBuffer[iEOL] != '\0' ) 
        iEOL++;

    if( achSrcBuffer[iEOL] == '\0' )
        return -1;

    if( (iEOL - iSrcBufferOffset) > nValueBufSize-1 )
    {
        strncpy( pszValueBuf, achSrcBuffer + iSrcBufferOffset, 
                 nValueBufSize-1 );
        pszValueBuf[nValueBufSize-1] = '\0';

        CPLDebug( "DXF", "Long line truncated to %d characters.\n%s...",
                  nValueBufSize-1,
                  pszValueBuf );
    }
    else
    {
        strncpy( pszValueBuf, achSrcBuffer + iSrcBufferOffset, 
                 iEOL - iSrcBufferOffset );
        pszValueBuf[iEOL - iSrcBufferOffset] = '\0';
    }

    iSrcBufferOffset = iEOL;

    // skip past newline.  CR, CRLF, or LFCR
    if( (achSrcBuffer[iSrcBufferOffset] == '\r'
         && achSrcBuffer[iSrcBufferOffset+1] == '\n' )
        || (achSrcBuffer[iSrcBufferOffset] == '\n'
            && achSrcBuffer[iSrcBufferOffset+1] == '\r' ) )
        iSrcBufferOffset += 2;
    else
        iSrcBufferOffset += 1;

/* -------------------------------------------------------------------- */
/*      Record how big this value was, so it can be unread safely.      */
/* -------------------------------------------------------------------- */
    nLastValueSize = iSrcBufferOffset - iStartSrcBufferOffset;

/* -------------------------------------------------------------------- */
/*      Is this a comment?  If so, tail recurse to get another line.    */
/* -------------------------------------------------------------------- */
    if( nValueCode == 999 )
        return ReadValue(pszValueBuf,nValueBufSize);
    else
        return nValueCode;
}

/************************************************************************/
/*                            UnreadValue()                             */
/*                                                                      */
/*      Unread the last value read, accomplished by resetting the       */
/*      read pointer.                                                   */
/************************************************************************/

void OGRDXFReader::UnreadValue()

{
    CPLAssert( iSrcBufferOffset >= nLastValueSize );
    CPLAssert( nLastValueSize > 0 );

    iSrcBufferOffset -= nLastValueSize;

    nLastValueSize = 0;
}

