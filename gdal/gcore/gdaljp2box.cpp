/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  GDALJP2Box Implementation - Low level JP2 box reader.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
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
#include "gdaljp2metadata.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$")

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                             GDALJP2Box()                             */
/************************************************************************/

// GDALJP2Box does *not* take ownership of fpIn
GDALJP2Box::GDALJP2Box( VSILFILE *fpIn ) :
    fpVSIL(fpIn),
    szBoxType{'\0', '\0', '\0', '\0', '\0'},
    nBoxOffset(-1),
    nBoxLength(0),
    nDataOffset(-1),
    pabyData(nullptr)
{
}

/************************************************************************/
/*                            ~GDALJP2Box()                             */
/************************************************************************/

GDALJP2Box::~GDALJP2Box()

{
    // Do not close fpVSIL. Ownership remains to the caller of GDALJP2Box
    // constructor
    CPLFree( pabyData );
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

int GDALJP2Box::SetOffset( GIntBig nNewOffset )

{
    szBoxType[0] = '\0';
    return VSIFSeekL( fpVSIL, nNewOffset, SEEK_SET ) == 0;
}

/************************************************************************/
/*                             ReadFirst()                              */
/************************************************************************/

int GDALJP2Box::ReadFirst()

{
    return SetOffset(0) && ReadBox();
}

/************************************************************************/
/*                              ReadNext()                              */
/************************************************************************/

int GDALJP2Box::ReadNext()

{
    return SetOffset( nBoxOffset + nBoxLength ) && ReadBox();
}

/************************************************************************/
/*                           ReadFirstChild()                           */
/************************************************************************/

int GDALJP2Box::ReadFirstChild( GDALJP2Box *poSuperBox )

{
    if( poSuperBox == nullptr )
        return ReadFirst();

    szBoxType[0] = '\0';
    if( !poSuperBox->IsSuperBox() )
        return FALSE;

    return SetOffset( poSuperBox->nDataOffset ) && ReadBox();
}

/************************************************************************/
/*                           ReadNextChild()                            */
/************************************************************************/

int GDALJP2Box::ReadNextChild( GDALJP2Box *poSuperBox )

{
    if( poSuperBox == nullptr )
        return ReadNext();

    if( !ReadNext() )
        return FALSE;

    if( nBoxOffset >= poSuperBox->nBoxOffset + poSuperBox->nBoxLength )
    {
        szBoxType[0] = '\0';
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                              ReadBox()                               */
/************************************************************************/

int GDALJP2Box::ReadBox()

{
    GUInt32 nLBox = 0;
    GUInt32 nTBox = 0;

    nBoxOffset = VSIFTellL( fpVSIL );

    if( VSIFReadL( &nLBox, 4, 1, fpVSIL ) != 1
        || VSIFReadL( &nTBox, 4, 1, fpVSIL ) != 1 )
    {
        return FALSE;
    }

    memcpy( szBoxType, &nTBox, 4 );
    szBoxType[4] = '\0';

    nLBox = CPL_MSBWORD32( nLBox );

    if( nLBox != 1 )
    {
        nBoxLength = nLBox;
        nDataOffset = nBoxOffset + 8;
    }
    else
    {
        GByte abyXLBox[8] = { 0 };
        if( VSIFReadL( abyXLBox, 8, 1, fpVSIL ) != 1 )
            return FALSE;

#ifdef CPL_HAS_GINT64
        CPL_MSBPTR64( abyXLBox );
        memcpy( &nBoxLength, abyXLBox, 8 );
#else
        // In case we lack a 64 bit integer type
        if( abyXLBox[0] != 0 || abyXLBox[1] != 0 || abyXLBox[2] != 0 ||
            abyXLBox[3] != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Box size requires a 64 bit integer type");
            return FALSE;
        }
        CPL_MSBPTR32( abyXLBox+4 );
        memcpy( &nBoxLength, abyXLBox+4, 4 );
#endif
        if( nBoxLength < 0 )
        {
            CPLDebug("GDALJP2", "Invalid length for box %s", szBoxType);
            return FALSE;
        }
        nDataOffset = nBoxOffset + 16;
    }

    if( nBoxLength == 0 )
    {
        if( VSIFSeekL( fpVSIL, 0, SEEK_END ) != 0 )
            return FALSE;
        nBoxLength = VSIFTellL( fpVSIL ) - nBoxOffset;
        if( VSIFSeekL( fpVSIL, nDataOffset, SEEK_SET ) != 0 )
            return FALSE;
    }

    if( EQUAL(szBoxType,"uuid") )
    {
        if( VSIFReadL( abyUUID, 16, 1, fpVSIL ) != 1 )
            return FALSE;
        nDataOffset += 16;
    }

    if( GetDataLength() < 0 )
    {
        CPLDebug("GDALJP2", "Invalid length for box %s", szBoxType);
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                             IsSuperBox()                             */
/************************************************************************/

int GDALJP2Box::IsSuperBox()

{
    if( EQUAL(GetType(),"asoc") || EQUAL(GetType(),"jp2h")
        || EQUAL(GetType(),"res ") )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                            ReadBoxData()                             */
/************************************************************************/

GByte *GDALJP2Box::ReadBoxData()

{
    GIntBig nDataLength = GetDataLength();
    if( nDataLength > 100 * 1024 * 1024 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Too big box : " CPL_FRMT_GIB " bytes",
                  nDataLength );
        return nullptr;
    }

    if( VSIFSeekL( fpVSIL, nDataOffset, SEEK_SET ) != 0 )
        return nullptr;

    char *pszData = static_cast<char *>(
        VSI_MALLOC_VERBOSE( static_cast<int>(nDataLength) + 1) );
    if( pszData == nullptr )
        return nullptr;

    if( static_cast<GIntBig>( VSIFReadL(
           pszData, 1, static_cast<int>(nDataLength), fpVSIL ) )
        != nDataLength )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot read box content");
        CPLFree( pszData );
        return nullptr;
    }

    pszData[nDataLength] = '\0';

    return reinterpret_cast<GByte *>( pszData );
}

/************************************************************************/
/*                           GetDataLength()                            */
/************************************************************************/

GIntBig GDALJP2Box::GetDataLength()
{
    return nBoxLength - (nDataOffset - nBoxOffset);
}

/************************************************************************/
/*                            DumpReadable()                            */
/************************************************************************/

int GDALJP2Box::DumpReadable( FILE *fpOut, int nIndentLevel )

{
    if( fpOut == nullptr )
        fpOut = stdout;

    for( int i=0; i < nIndentLevel; ++i)
        fprintf( fpOut, "  " );

    char szBuffer[128];
    CPLsnprintf( szBuffer, sizeof(szBuffer),
             "  Type=%s, Offset=" CPL_FRMT_GIB "/" CPL_FRMT_GIB
             ", Data Size=" CPL_FRMT_GIB,
             szBoxType, nBoxOffset, nDataOffset,
             GetDataLength() );
    fprintf( fpOut, "%s", szBuffer );

    if( IsSuperBox() )
    {
        fprintf( fpOut, " (super)" );
    }

    fprintf( fpOut, "\n" );

    if( IsSuperBox() )
    {
        GDALJP2Box oSubBox( GetFILE() );

        for( oSubBox.ReadFirstChild( this );
             strlen(oSubBox.GetType()) > 0;
             oSubBox.ReadNextChild( this ) )
        {
            oSubBox.DumpReadable( fpOut, nIndentLevel + 1 );
        }
    }

    if( EQUAL(GetType(),"uuid") )
    {
        char *pszHex = CPLBinaryToHex( 16, GetUUID() );
        for( int i = 0; i < nIndentLevel; ++i )
            fprintf( fpOut, "  " );

        fprintf( fpOut, "    UUID=%s", pszHex );

        if( EQUAL(pszHex,"B14BF8BD083D4B43A5AE8CD7D5A6CE03") )
            fprintf( fpOut, " (GeoTIFF)" );
        if( EQUAL(pszHex,"96A9F1F1DC98402DA7AED68E34451809") )
            fprintf( fpOut, " (MSI Worldfile)" );
        if( EQUAL(pszHex,"BE7ACFCB97A942E89C71999491E3AFAC") )
            fprintf( fpOut, " (XMP)" );
        CPLFree( pszHex );

        fprintf( fpOut, "\n" );
    }

    return 0;
}

/************************************************************************/
/*                              SetType()                               */
/************************************************************************/

void GDALJP2Box::SetType( const char *pszType )

{
    CPLAssert( strlen(pszType) == 4 );

    memcpy(szBoxType, pszType, 4);
    szBoxType[4] = '\0';
}

/************************************************************************/
/*                          SetWritableData()                           */
/************************************************************************/

void GDALJP2Box::SetWritableData( int nLength, const GByte *pabyDataIn )

{
    CPLFree( pabyData );

    pabyData = static_cast<GByte *>( CPLMalloc(nLength) );
    memcpy( pabyData, pabyDataIn, nLength );

    nBoxOffset = -9; // Virtual offsets for data length computation.
    nDataOffset = -1;

    nBoxLength = 8 + nLength;
}

/************************************************************************/
/*                          AppendWritableData()                        */
/************************************************************************/

void GDALJP2Box::AppendWritableData( int nLength, const void *pabyDataIn )

{
    if( pabyData == nullptr )
    {
        nBoxOffset = -9; // Virtual offsets for data length computation.
        nDataOffset = -1;
        nBoxLength = 8;
    }

    pabyData = static_cast<GByte *>(
        CPLRealloc(pabyData, static_cast<size_t>(GetDataLength() + nLength)) );
    memcpy( pabyData + GetDataLength(), pabyDataIn, nLength );

    nBoxLength += nLength;
}

/************************************************************************/
/*                              AppendUInt32()                          */
/************************************************************************/

void GDALJP2Box::AppendUInt32( GUInt32 nVal )
{
    CPL_MSBPTR32(&nVal);
    AppendWritableData(4, &nVal);
}

/************************************************************************/
/*                              AppendUInt16()                          */
/************************************************************************/

void GDALJP2Box::AppendUInt16( GUInt16 nVal )
{
    CPL_MSBPTR16(&nVal);
    AppendWritableData(2, &nVal);
}
/************************************************************************/
/*                              AppendUInt8()                           */
/************************************************************************/

void GDALJP2Box::AppendUInt8( GByte nVal )
{
    AppendWritableData(1, &nVal);
}

/************************************************************************/
/*                           CreateUUIDBox()                            */
/************************************************************************/

GDALJP2Box *GDALJP2Box::CreateUUIDBox(
    const GByte *pabyUUID, int nDataSize, const GByte *pabyDataIn )

{
    GDALJP2Box * const poBox = new GDALJP2Box();
    poBox->SetType( "uuid" );

    poBox->AppendWritableData( 16, pabyUUID );
    poBox->AppendWritableData( nDataSize, pabyDataIn );

    return poBox;
}

/************************************************************************/
/*                           CreateAsocBox()                            */
/************************************************************************/

GDALJP2Box *GDALJP2Box::CreateAsocBox( int nCount, GDALJP2Box **papoBoxes )
{
    return CreateSuperBox("asoc", nCount, papoBoxes);
}

/************************************************************************/
/*                           CreateAsocBox()                            */
/************************************************************************/

GDALJP2Box *GDALJP2Box::CreateSuperBox( const char* pszType,
                                        int nCount, GDALJP2Box **papoBoxes )
{
    int nDataSize = 0;

/* -------------------------------------------------------------------- */
/*      Compute size of data area of asoc box.                          */
/* -------------------------------------------------------------------- */
    for( int iBox = 0; iBox < nCount; ++iBox )
        nDataSize += 8 + static_cast<int>( papoBoxes[iBox]->GetDataLength() );

    GByte *pabyNext = static_cast<GByte *>( CPLMalloc(nDataSize) );
    GByte *pabyCompositeData = pabyNext;

/* -------------------------------------------------------------------- */
/*      Copy subboxes headers and data into buffer.                     */
/* -------------------------------------------------------------------- */
    for( int iBox = 0; iBox < nCount; ++iBox )
    {
        GUInt32 nLBox = CPL_MSBWORD32(
            static_cast<GUInt32>(papoBoxes[iBox]->nBoxLength));
        memcpy( pabyNext, &nLBox, 4 );
        pabyNext += 4;

        memcpy( pabyNext, papoBoxes[iBox]->szBoxType, 4 );
        pabyNext += 4;

        memcpy( pabyNext, papoBoxes[iBox]->pabyData,
                static_cast<int>(papoBoxes[iBox]->GetDataLength()) );
        pabyNext += papoBoxes[iBox]->GetDataLength();
    }

/* -------------------------------------------------------------------- */
/*      Create asoc box.                                                */
/* -------------------------------------------------------------------- */
    GDALJP2Box * const poAsoc = new GDALJP2Box();

    poAsoc->SetType( pszType );
    poAsoc->SetWritableData( nDataSize, pabyCompositeData );

    CPLFree( pabyCompositeData );

    return poAsoc;
}

/************************************************************************/
/*                            CreateLblBox()                            */
/************************************************************************/

GDALJP2Box *GDALJP2Box::CreateLblBox( const char *pszLabel )

{
    GDALJP2Box * const poBox = new GDALJP2Box();
    poBox->SetType( "lbl " );
    poBox->SetWritableData( static_cast<int>(strlen(pszLabel)+1),
                            reinterpret_cast<const GByte *>( pszLabel ) );

    return poBox;
}

/************************************************************************/
/*                       CreateLabelledXMLAssoc()                       */
/************************************************************************/

GDALJP2Box *GDALJP2Box::CreateLabelledXMLAssoc( const char *pszLabel,
                                                const char *pszXML )

{
    GDALJP2Box oLabel;
    oLabel.SetType( "lbl " );
    oLabel.SetWritableData( static_cast<int>(strlen(pszLabel)+1),
                            reinterpret_cast<const GByte *>(pszLabel) );

    GDALJP2Box oXML;
    oXML.SetType( "xml " );
    oXML.SetWritableData( static_cast<int>(strlen(pszXML)+1),
                          reinterpret_cast<const GByte *>(pszXML) );

    GDALJP2Box *aoList[2] = { &oLabel, &oXML };

    return CreateAsocBox( 2, aoList );
}

/*! @endcond */
