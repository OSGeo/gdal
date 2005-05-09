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
 * Revision 1.2  2005/05/09 14:42:33  fwarmerdam
 * Fixed to use VSFReadL() instead of VSIFRead().
 *
 * Revision 1.1  2005/05/03 21:10:59  fwarmerdam
 * New
 *
 */

#include "gdaljp2metadata.h"

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
}

/************************************************************************/
/*                            ~GDALJP2Box()                             */
/************************************************************************/

GDALJP2Box::~GDALJP2Box()

{
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

        if( VSIFReadL( &nLBox, 8, 1, fpVSIL ) != 1 )
            return FALSE;

        
        if( sizeof(nBoxLength) == 64 )
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
    if( EQUAL(GetType(),"asoc") )
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
    fprintf( fpOut, "  Type=%s, Offset=%d/%d, Data Size=%d\n",
             szBoxType, (int) nBoxOffset, (int) nDataOffset, 
             nBoxLength - (nDataOffset - nBoxOffset) );
    return 0;
}
