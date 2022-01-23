/******************************************************************************
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

CPL_CVSID("$Id$")

/************************************************************************/
/*                            OGRDXFReader()                            */
/************************************************************************/

OGRDXFReader::OGRDXFReader() :
    fp(nullptr),
    iSrcBufferOffset(0),
    nSrcBufferBytes(0),
    iSrcBufferFileOffset(0),
    achSrcBuffer{},
    nLastValueSize(0),
    nLineNumber(0)
{}

/************************************************************************/
/*                           ~OGRDXFReader()                            */
/************************************************************************/

OGRDXFReader::~OGRDXFReader()

{
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void OGRDXFReader::Initialize( VSILFILE *fpIn )

{
    fp = fpIn;
}

/************************************************************************/
/*                          ResetReadPointer()                          */
/************************************************************************/

void OGRDXFReader::ResetReadPointer( int iNewOffset,
    int nNewLineNumber /* = 0 */ )

{
    nSrcBufferBytes = 0;
    iSrcBufferOffset = 0;
    iSrcBufferFileOffset = iNewOffset;
    nLastValueSize = 0;
    nLineNumber = nNewLineNumber;

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

    nSrcBufferBytes += static_cast<int>(VSIFReadL( achSrcBuffer + nSrcBufferBytes,
                                  1, 512, fp ));
    achSrcBuffer[nSrcBufferBytes] = '\0';

    CPLAssert( nSrcBufferBytes <= 1024 );
    CPLAssert( iSrcBufferOffset <= nSrcBufferBytes );
}

/************************************************************************/
/*                             ReadValue()                              */
/*                                                                      */
/*      Read one type code and value line pair from the DXF file.       */
/************************************************************************/

int OGRDXFReader::ReadValueRaw( char *pszValueBuf, int nValueBufSize )

{
/* -------------------------------------------------------------------- */
/*      Make sure we have lots of data in our buffer for one value.     */
/* -------------------------------------------------------------------- */
    if( nSrcBufferBytes - iSrcBufferOffset < 512 )
        LoadDiskChunk();

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
    CPLString osValue;

    nLineNumber ++;

    // proceed to newline.
    while( achSrcBuffer[iEOL] != '\n'
           && achSrcBuffer[iEOL] != '\r'
           && achSrcBuffer[iEOL] != '\0' )
        iEOL++;

    bool bLongLine = false;
    while( achSrcBuffer[iEOL] == '\0' )
    {
        // The line is longer than the buffer. Let's copy what we have so
        // far into our string, and read more
        const auto nValueLength = osValue.length();

        if( nValueLength + iEOL - iSrcBufferOffset > 1048576 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Line %d is too long", nLineNumber );
            return -1;
        }

        osValue.resize( nValueLength + iEOL - iSrcBufferOffset, '\0' );
        std::copy( achSrcBuffer + iSrcBufferOffset,
            achSrcBuffer + iEOL,
            osValue.begin() + nValueLength );

        iSrcBufferOffset = iEOL;
        LoadDiskChunk();
        iEOL = iSrcBufferOffset;
        bLongLine = true;

        // Have we prematurely reached the end of the file?
        if( achSrcBuffer[iEOL] == '\0' )
            return -1;

        // Proceed to newline again
        while( achSrcBuffer[iEOL] != '\n'
               && achSrcBuffer[iEOL] != '\r'
               && achSrcBuffer[iEOL] != '\0' )
            iEOL++;
    }

    size_t nValueBufLen = 0;

    // If this was an extremely long line, copy from osValue into the buffer
    if( !osValue.empty() )
    {
        strncpy( pszValueBuf, osValue.c_str(), nValueBufSize - 1 );
        pszValueBuf[nValueBufSize - 1] = '\0';

        nValueBufLen = strlen(pszValueBuf);

        if( static_cast<int>( osValue.length() ) > nValueBufSize - 1 )
        {
            CPLDebug( "DXF", "Long line truncated to %d characters.\n%s...",
                      nValueBufSize - 1,
                      pszValueBuf );
        }
    }

    // Copy the last (normally, the only) section of this line into the buffer
    if( (iEOL - iSrcBufferOffset) >
        nValueBufSize - static_cast<int>(nValueBufLen) - 1 )
    {
        strncpy( pszValueBuf + nValueBufLen,
                 achSrcBuffer + iSrcBufferOffset,
                 nValueBufSize - static_cast<int>(nValueBufLen) - 1 );
        pszValueBuf[nValueBufSize-1] = '\0';

        CPLDebug( "DXF", "Long line truncated to %d characters.\n%s...",
                  nValueBufSize-1,
                  pszValueBuf );
    }
    else
    {
        strncpy( pszValueBuf + nValueBufLen,
                 achSrcBuffer + iSrcBufferOffset,
                 iEOL - iSrcBufferOffset );
        pszValueBuf[nValueBufLen + iEOL - iSrcBufferOffset] = '\0';
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
    if( bLongLine )
        nLastValueSize = 0;
    else
    {
        nLastValueSize = iSrcBufferOffset - iStartSrcBufferOffset;
        CPLAssert( nLastValueSize > 0 );
    }

    return nValueCode;
}

int OGRDXFReader::ReadValue( char *pszValueBuf, int nValueBufSize )
{
    int nValueCode;
    while( true )
    {
        nValueCode = ReadValueRaw(pszValueBuf,nValueBufSize);
        if( nValueCode == 999 )
        {
            // Skip comments
            continue;
        }
        break;
    }
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
    if( nLastValueSize == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot UnreadValue(), likely due to a previous long line");
        return;
    }
    CPLAssert( iSrcBufferOffset >= nLastValueSize );
    CPLAssert( nLastValueSize > 0 );

    iSrcBufferOffset -= nLastValueSize;
    nLineNumber -= 2;
    nLastValueSize = 0;
}
