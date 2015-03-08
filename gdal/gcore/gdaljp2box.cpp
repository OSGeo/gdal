/******************************************************************************
 * $Id$
 *
 * Project:  GDAL 
 * Purpose:  GDALJP2Box Implementation - Low level JP2 box reader.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdaljp2metadata.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                             GDALJP2Box()                             */
/************************************************************************/

GDALJP2Box::GDALJP2Box( VSILFILE *fpIn )

{
    fpVSIL = fpIn;
    szBoxType[0] = '\0';
    nBoxOffset = -1;
    nDataOffset = -1;
    nBoxLength = 0;
    
    pabyData = NULL;
}

/************************************************************************/
/*                            ~GDALJP2Box()                             */
/************************************************************************/

GDALJP2Box::~GDALJP2Box()

{
    CPLFree( pabyData );
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

int  GDALJP2Box::SetOffset( GIntBig nNewOffset )

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
    if( poSuperBox == NULL )
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
    if( poSuperBox == NULL )
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
    GUInt32   nLBox;
    GUInt32   nTBox;

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
        GByte abyXLBox[8];
        if( VSIFReadL( abyXLBox, 8, 1, fpVSIL ) != 1 )
            return FALSE;

        
        if( sizeof(nBoxLength) == 8 )
        {
            CPL_MSBPTR64( abyXLBox );
            memcpy( &nBoxLength, abyXLBox, 8 );
        }
        else
        {
            CPL_MSBPTR32( abyXLBox+4 );
            memcpy( &nBoxLength, abyXLBox+4, 4 );
        }

        nDataOffset = nBoxOffset + 16;
    }

    if( nBoxLength == 0 )
    {
        VSIFSeekL( fpVSIL, 0, SEEK_END );
        nBoxLength = VSIFTellL( fpVSIL ) - nBoxOffset;
        VSIFSeekL( fpVSIL, nDataOffset, SEEK_SET );
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
    else
        return FALSE;
}

/************************************************************************/
/*                            ReadBoxData()                             */
/************************************************************************/

GByte *GDALJP2Box::ReadBoxData()

{
    if( GetDataLength() > 100 * 1024 * 1024 )
        return FALSE;

    char *pszData = (char *) CPLMalloc((int)GetDataLength() + 1);

    if( (GIntBig) VSIFReadL( pszData, 1, (int)GetDataLength(), fpVSIL ) 
        != GetDataLength() )
    {
        CPLFree( pszData );
        return NULL;
    }

    pszData[GetDataLength()] = '\0';

    return (GByte *) pszData;
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
    if( fpOut == NULL )
        fpOut = stdout;

    int i;
    for(i=0;i<nIndentLevel;i++)
        fprintf( fpOut, "  " );

    fprintf( fpOut, "  Type=%s, Offset=" CPL_FRMT_GIB "/" CPL_FRMT_GIB", Data Size=" CPL_FRMT_GIB,
             szBoxType, nBoxOffset, nDataOffset, 
             GetDataLength() );

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
        for(i=0;i<nIndentLevel;i++)
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
    
    pabyData = (GByte *) CPLMalloc(nLength);
    memcpy( pabyData, pabyDataIn, nLength );

    nBoxOffset = -9; // virtual offsets for data length computation.
    nDataOffset = -1;
    
    nBoxLength = 8 + nLength;
}

/************************************************************************/
/*                          AppendWritableData()                        */
/************************************************************************/

void GDALJP2Box::AppendWritableData( int nLength, const void *pabyDataIn )

{
    if( pabyData == NULL )
    {
        nBoxOffset = -9; // virtual offsets for data length computation.
        nDataOffset = -1;
        nBoxLength = 8;
    }

    pabyData = (GByte *) CPLRealloc(pabyData, GetDataLength() + nLength);
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
    const GByte *pabyUUID, int nDataSize, const GByte *pabyData )

{
    GDALJP2Box *poBox;

    poBox = new GDALJP2Box();
    poBox->SetType( "uuid" );

    poBox->AppendWritableData( 16, pabyUUID );
    poBox->AppendWritableData( nDataSize, pabyData );

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
    int nDataSize=0, iBox;
    GByte *pabyCompositeData, *pabyNext;

/* -------------------------------------------------------------------- */
/*      Compute size of data area of asoc box.                          */
/* -------------------------------------------------------------------- */
    for( iBox = 0; iBox < nCount; iBox++ )
        nDataSize += 8 + (int) papoBoxes[iBox]->GetDataLength();

    pabyNext = pabyCompositeData = (GByte *) CPLMalloc(nDataSize);

/* -------------------------------------------------------------------- */
/*      Copy subboxes headers and data into buffer.                     */
/* -------------------------------------------------------------------- */
    for( iBox = 0; iBox < nCount; iBox++ )
    {
        GUInt32   nLBox;

        nLBox = CPL_MSBWORD32(papoBoxes[iBox]->nBoxLength);
        memcpy( pabyNext, &nLBox, 4 );
        pabyNext += 4;

        memcpy( pabyNext, papoBoxes[iBox]->szBoxType, 4 );
        pabyNext += 4;

        memcpy( pabyNext, papoBoxes[iBox]->pabyData, 
                (int) papoBoxes[iBox]->GetDataLength() );
        pabyNext += papoBoxes[iBox]->GetDataLength();
    }
    
/* -------------------------------------------------------------------- */
/*      Create asoc box.                                                */
/* -------------------------------------------------------------------- */
    GDALJP2Box *poAsoc = new GDALJP2Box();

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
    GDALJP2Box *poBox;

    poBox = new GDALJP2Box();
    poBox->SetType( "lbl " );
    poBox->SetWritableData( strlen(pszLabel)+1, (const GByte *) pszLabel );

    return poBox;
}

/************************************************************************/
/*                       CreateLabelledXMLAssoc()                       */
/************************************************************************/

GDALJP2Box *GDALJP2Box::CreateLabelledXMLAssoc( const char *pszLabel,
                                                const char *pszXML )

{
    GDALJP2Box oLabel, oXML;
    GDALJP2Box *aoList[2];

    oLabel.SetType( "lbl " );
    oLabel.SetWritableData( strlen(pszLabel)+1, (const GByte *) pszLabel );

    oXML.SetType( "xml " );
    oXML.SetWritableData( strlen(pszXML)+1, (const GByte *) pszXML );

    aoList[0] = &oLabel;
    aoList[1] = &oXML;
    
    return CreateAsocBox( 2, aoList );
}
