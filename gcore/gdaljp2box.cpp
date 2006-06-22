/******************************************************************************
 * $Id$
 *
 * Project:  GDAL 
 * Purpose:  GDALJP2Box Implementation - Low level JP2 box reader.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
 *****************************************************************************
 *
 * $Log$
 * Revision 1.7  2006/06/22 01:33:40  fwarmerdam
 * added support for preparing writable gml and geotiff boxes
 *
 * Revision 1.6  2006/06/08 18:47:42  fwarmerdam
 * Fixed nBoxLength sizeof test, should be 8 bytes, not 64 bits.
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=1199
 *
 * Revision 1.5  2006/01/11 06:30:18  fwarmerdam
 * Fixed bug in ReadBox() with boxes where LBox is 1 and the length
 * is read from the XLBox (such as GeoJP2 UUID Boxes from Erdas Imagine).
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=1032
 *
 * Revision 1.4  2005/09/14 19:26:50  fwarmerdam
 * added better debug output
 *
 * Revision 1.3  2005/09/14 13:13:17  dron
 * Avoid warnings in DumpReadable().
 *
 * Revision 1.2  2005/05/09 14:42:33  fwarmerdam
 * Fixed to use VSFReadL() instead of VSIFRead().
 *
 * Revision 1.1  2005/05/03 21:10:59  fwarmerdam
 * New
 *
 */

#include "gdaljp2metadata.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                             GDALJP2Box()                             */
/************************************************************************/

GDALJP2Box::GDALJP2Box( FILE *fpIn )

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
    if( !poSuperBox->IsSuperBox() )
        return FALSE;

    return SetOffset( poSuperBox->nDataOffset ) && ReadBox();
}

/************************************************************************/
/*                           ReadNextChild()                            */
/************************************************************************/

int GDALJP2Box::ReadNextChild( GDALJP2Box *poSuperBox )

{
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
    }

    if( EQUAL(szBoxType,"uuid") )
    {
        VSIFReadL( abyUUID, 16, 1, fpVSIL );
        nDataOffset += 16;
    }

    return TRUE;
}

/************************************************************************/
/*                             IsSuperBox()                             */
/************************************************************************/

int GDALJP2Box::IsSuperBox()

{
    if( EQUAL(GetType(),"asoc") || EQUAL(GetType(),"jp2h") )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                            ReadBoxData()                             */
/************************************************************************/

GByte *GDALJP2Box::ReadBoxData()

{
    char *pszData = (char *) CPLMalloc(GetDataLength() + 1);

    if( VSIFReadL( pszData, 1, GetDataLength(), fpVSIL ) != GetDataLength() )
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

int GDALJP2Box::DumpReadable( FILE *fpOut )

{
    if( fpOut == NULL )
        fpOut = stdout;

    fprintf( fpOut, "  Type=%s, Offset=%d/%d, Data Size=%d",
             szBoxType, (int) nBoxOffset, (int) nDataOffset, 
             (int)(nBoxLength - (nDataOffset - nBoxOffset)) );

    if( IsSuperBox() )
        fprintf( fpOut, " (super)" );

    fprintf( fpOut, "\n" );
    if( EQUAL(GetType(),"uuid") )
    {
        char *pszHex = CPLBinaryToHex( 16, GetUUID() );
        fprintf( fpOut, "    UUID=%s", pszHex );

        if( EQUAL(pszHex,"B14BF8BD083D4B43A5AE8CD7D5A6CE03") )
            fprintf( fpOut, " (GeoTIFF)" );
        if( EQUAL(pszHex,"96A9F1F1DC98402DA7AED68E34451809") )
            fprintf( fpOut, " (MSI Worldfile)" );
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

    szBoxType[0] = pszType[3];
    szBoxType[1] = pszType[2];
    szBoxType[2] = pszType[1];
    szBoxType[3] = pszType[0];
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
/*                           CreateUUIDBox()                            */
/************************************************************************/

GDALJP2Box *GDALJP2Box::CreateUUIDBox( 
    const GByte *pabyUUID, int nDataSize, GByte *pabyData )

{
    GDALJP2Box *poBox;

    poBox = new GDALJP2Box();
    poBox->SetType( "uuid" );
    memcpy( poBox->abyUUID, pabyUUID, 16 );

    GByte *pabyMergedData = (GByte *) CPLMalloc(16+nDataSize);
    memcpy( pabyMergedData, pabyUUID, 16 );
    memcpy( pabyMergedData+16, pabyData, nDataSize );
    
    poBox->SetWritableData( 16 + nDataSize, pabyMergedData );
    
    CPLFree( pabyMergedData );

    return poBox;
}

/************************************************************************/
/*                           CreateAsocBox()                            */
/************************************************************************/

GDALJP2Box *GDALJP2Box::CreateAsocBox( int nCount, GDALJP2Box **papoBoxes )


{
    int nDataSize=0, iBox;
    GByte *pabyCompositeData, *pabyNext;

/* -------------------------------------------------------------------- */
/*      Compute size of data area of asoc box.                          */
/* -------------------------------------------------------------------- */
    for( iBox = 0; iBox < nCount; iBox++ )
        nDataSize += 8 + papoBoxes[iBox]->GetDataLength();

    pabyNext = pabyCompositeData = (GByte *) CPLMalloc(nDataSize);

/* -------------------------------------------------------------------- */
/*      Copy subboxes headers and data into buffer.                     */
/* -------------------------------------------------------------------- */
    for( iBox = 0; iBox < nCount; iBox++ )
    {
        GUInt32   nLBox, nTBox;

        nLBox = CPL_MSBWORD32(papoBoxes[iBox]->nBoxLength);
        memcpy( pabyNext, &nLBox, 4 );
        pabyNext += 4;

        memcpy( &nTBox, papoBoxes[iBox]->szBoxType, 4 );
        nTBox = CPL_MSBWORD32( nTBox );
        memcpy( pabyNext, &nTBox, 4 );
        pabyNext += 4;

        memcpy( pabyNext, papoBoxes[iBox]->pabyData, 
                papoBoxes[iBox]->GetDataLength() );
        pabyNext += papoBoxes[iBox]->GetDataLength();
    }
    
/* -------------------------------------------------------------------- */
/*      Create asoc box.                                                */
/* -------------------------------------------------------------------- */
    GDALJP2Box *poAsoc = new GDALJP2Box();

    poAsoc->SetType( "asoc" );
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

