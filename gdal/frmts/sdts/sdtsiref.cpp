/******************************************************************************
 *
 * Project:  SDTS Translator
 * Purpose:  Implementation of SDTS_IREF class for reading IREF module.
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
 ****************************************************************************/

#include "sdts_al.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                             SDTS_IREF()                              */
/************************************************************************/

SDTS_IREF::SDTS_IREF() :
    nDefaultSADRFormat(0),
    pszXAxisName(CPLStrdup("")),
    pszYAxisName(CPLStrdup("")),
    dfXScale(1.0),
    dfYScale(1.0),
    dfXOffset(0.0),
    dfYOffset(0.0),
    dfXRes(1.0),
    dfYRes(1.0),
    pszCoordinateFormat(CPLStrdup(""))
{}

/************************************************************************/
/*                             ~SDTS_IREF()                             */
/************************************************************************/

SDTS_IREF::~SDTS_IREF()
{
    CPLFree( pszXAxisName );
    CPLFree( pszYAxisName );
    CPLFree( pszCoordinateFormat );
}

/************************************************************************/
/*                                Read()                                */
/*                                                                      */
/*      Read the named file to initialize this structure.               */
/************************************************************************/

int SDTS_IREF::Read( const char * pszFilename )

{
/* -------------------------------------------------------------------- */
/*      Open the file, and read the header.                             */
/* -------------------------------------------------------------------- */
    DDFModule oIREFFile;
    if( !oIREFFile.Open( pszFilename ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Read the first record, and verify that this is an IREF record.  */
/* -------------------------------------------------------------------- */
    DDFRecord *poRecord = oIREFFile.ReadRecord();
    if( poRecord == NULL )
        return FALSE;

    if( poRecord->GetStringSubfield( "IREF", 0, "MODN", 0 ) == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get the labels.                                                 */
/* -------------------------------------------------------------------- */
    CPLFree( pszXAxisName );
    pszXAxisName = CPLStrdup( poRecord->GetStringSubfield( "IREF", 0,
                                                           "XLBL", 0 ) );
    CPLFree( pszYAxisName );
    pszYAxisName = CPLStrdup( poRecord->GetStringSubfield( "IREF", 0,
                                                           "YLBL", 0 ) );

/* -------------------------------------------------------------------- */
/*      Get the coordinate encoding.                                    */
/* -------------------------------------------------------------------- */
    CPLFree( pszCoordinateFormat );
    pszCoordinateFormat =
        CPLStrdup( poRecord->GetStringSubfield( "IREF", 0, "HFMT", 0 ) );

/* -------------------------------------------------------------------- */
/*      Get the transformation information, and resolution.             */
/* -------------------------------------------------------------------- */
    dfXScale = poRecord->GetFloatSubfield( "IREF", 0, "SFAX", 0 );
    dfYScale = poRecord->GetFloatSubfield( "IREF", 0, "SFAY", 0 );

    dfXOffset = poRecord->GetFloatSubfield( "IREF", 0, "XORG", 0 );
    dfYOffset = poRecord->GetFloatSubfield( "IREF", 0, "YORG", 0 );

    dfXRes = poRecord->GetFloatSubfield( "IREF", 0, "XHRS", 0 );
    dfYRes = poRecord->GetFloatSubfield( "IREF", 0, "YHRS", 0 );

    nDefaultSADRFormat = EQUAL(pszCoordinateFormat,"BI32");

    return TRUE;
}

/************************************************************************/
/*                            GetSADRCount()                            */
/*                                                                      */
/*      Return the number of SADR'es in the passed field.               */
/************************************************************************/

int SDTS_IREF::GetSADRCount( DDFField * poField )

{
    if( nDefaultSADRFormat )
        return poField->GetDataSize() / SDTS_SIZEOF_SADR;

    return poField->GetRepeatCount();
}

/************************************************************************/
/*                              GetSADR()                               */
/************************************************************************/

int SDTS_IREF::GetSADR( DDFField * poField, int nVertices,
                        double *padfX, double * padfY, double * padfZ )

{
/* -------------------------------------------------------------------- */
/*      For the sake of efficiency we depend on our knowledge that      */
/*      the SADR field is a series of bigendian int32's and decode      */
/*      them directly.                                                  */
/* -------------------------------------------------------------------- */
    if( nDefaultSADRFormat
        && poField->GetFieldDefn()->GetSubfieldCount() == 2 )
    {
        if( poField->GetDataSize() < nVertices * SDTS_SIZEOF_SADR )
        {
            return FALSE;
        }

        GInt32          anXY[2];
        const char      *pachRawData = poField->GetData();

        for( int iVertex = 0; iVertex < nVertices; iVertex++ )
        {
            // we copy to a temp buffer to ensure it is world aligned.
            memcpy( anXY, pachRawData, 8 );
            pachRawData += 8;

            // possibly byte swap, and always apply scale factor
            padfX[iVertex] = dfXOffset
                + dfXScale * static_cast<int>( CPL_MSBWORD32( anXY[0] ) );
            padfY[iVertex] = dfYOffset
                + dfYScale * static_cast<int>( CPL_MSBWORD32( anXY[1] ) );

            padfZ[iVertex] = 0.0;
        }
    }

/* -------------------------------------------------------------------- */
/*      This is the generic case.  We assume either two or three        */
/*      subfields, and treat these as X, Y and Z regardless of          */
/*      name.                                                           */
/* -------------------------------------------------------------------- */
    else
    {
        DDFFieldDefn    *poFieldDefn = poField->GetFieldDefn();
        int             nBytesRemaining = poField->GetDataSize();
        const char     *pachFieldData = poField->GetData();

        if( poFieldDefn->GetSubfieldCount() != 2 &&
            poFieldDefn->GetSubfieldCount() != 3 )
        {
            return FALSE;
        }

        for( int iVertex = 0; iVertex < nVertices; iVertex++ )
        {
            double adfXYZ[3] = { 0.0, 0.0, 0.0 };

            for( int iEntry = 0;
                 nBytesRemaining > 0 &&
                 iEntry < poFieldDefn->GetSubfieldCount();
                 iEntry++ )
            {
                int nBytesConsumed = 0;
                DDFSubfieldDefn *poSF = poFieldDefn->GetSubfield(iEntry);

                switch( poSF->GetType() )
                {
                  case DDFInt:
                    adfXYZ[iEntry] =
                        poSF->ExtractIntData( pachFieldData,
                                              nBytesRemaining,
                                              &nBytesConsumed );
                    break;

                  case DDFFloat:
                    adfXYZ[iEntry] =
                        poSF->ExtractFloatData( pachFieldData,
                                                nBytesRemaining,
                                                &nBytesConsumed );
                    break;

                  case DDFBinaryString:
                    {
                      GByte *pabyBString = reinterpret_cast<GByte *> (
                          const_cast<char *>(
                              poSF->ExtractStringData( pachFieldData,
                                                       nBytesRemaining,
                                                       &nBytesConsumed ) ) );

                    if( EQUAL(pszCoordinateFormat,"BI32") )
                    {
                        if( nBytesConsumed < 4 )
                            return FALSE;
                        GInt32  nValue;
                        memcpy( &nValue, pabyBString, 4 );
                        adfXYZ[iEntry]
                            = static_cast<int>( CPL_MSBWORD32( nValue ) );
                    }
                    else if( EQUAL(pszCoordinateFormat,"BI16") )
                    {
                        if( nBytesConsumed < 2 )
                            return FALSE;
                        GInt16  nValue;
                        memcpy( &nValue, pabyBString, 2 );
                        adfXYZ[iEntry]
                            = static_cast<int>( CPL_MSBWORD16( nValue ) );
                    }
                    else if( EQUAL(pszCoordinateFormat,"BU32") )
                    {
                        if( nBytesConsumed < 4 )
                            return FALSE;
                        GUInt32 nValue;
                        memcpy( &nValue, pabyBString, 4 );
                        adfXYZ[iEntry]
                            = static_cast<GUInt32>( CPL_MSBWORD32( nValue ) );
                    }
                    else if( EQUAL(pszCoordinateFormat,"BU16") )
                    {
                        if( nBytesConsumed < 2 )
                            return FALSE;
                        GUInt16 nValue;
                        memcpy( &nValue, pabyBString, 2 );
                        adfXYZ[iEntry]
                            = static_cast<GUInt16>( CPL_MSBWORD16( nValue ) );
                    }
                    else if( EQUAL(pszCoordinateFormat,"BFP32") )
                    {
                        if( nBytesConsumed < 4 )
                            return FALSE;
                        float   fValue;

                        memcpy( &fValue, pabyBString, 4 );
                        CPL_MSBPTR32( &fValue );
                        adfXYZ[iEntry] = fValue;
                    }
                    else if( EQUAL(pszCoordinateFormat,"BFP64") )
                    {
                        if( nBytesConsumed < 8 )
                            return FALSE;
                        double  dfValue;

                        memcpy( &dfValue, pabyBString, 8 );
                        CPL_MSBPTR64( &dfValue );
                        adfXYZ[iEntry] = dfValue;
                    }
                    }
                    break;

                  default:
                    adfXYZ[iEntry] = 0.0;
                    break;
                }

                pachFieldData += nBytesConsumed;
                nBytesRemaining -= nBytesConsumed;
            } /* next iEntry */

            padfX[iVertex] = dfXOffset + adfXYZ[0] * dfXScale;
            padfY[iVertex] = dfYOffset + adfXYZ[1] * dfYScale;
            padfZ[iVertex] = adfXYZ[2];
        } /* next iVertex */
    }

    return TRUE;
}
