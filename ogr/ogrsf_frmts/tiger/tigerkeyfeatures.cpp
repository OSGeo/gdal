/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerKeyFeatures, providing access to .RT9 files.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 * Revision 1.7  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.6  2001/07/04 23:25:32  warmerda
 * first round implementation of writer
 *
 * Revision 1.5  2001/07/04 05:40:35  warmerda
 * upgraded to support FILE, and Tiger2000 schema
 *
 * Revision 1.4  2001/01/19 21:15:20  warmerda
 * expanded tabs
 *
 * Revision 1.3  2000/01/13 05:18:11  warmerda
 * added support for multiple versions
 *
 * Revision 1.2  1999/12/22 15:38:15  warmerda
 * major update
 *
 * Revision 1.1  1999/12/15 19:59:17  warmerda
 * New
 *
 */

#include "ogr_tiger.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

#define FILE_CODE "9"

/************************************************************************/
/*                         TigerKeyFeatures()                         */
/************************************************************************/

TigerKeyFeatures::TigerKeyFeatures( OGRTigerDataSource * poDSIn,
                                  const char * pszPrototypeModule )

{
    OGRFieldDefn        oField("",OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "KeyFeatures" );
    poFeatureDefn->SetGeomType( wkbNone );

/* -------------------------------------------------------------------- */
/*      Fields from type 9 record.                                      */
/* -------------------------------------------------------------------- */
    oField.Set( "MODULE", OFTString, 8 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FILE", OFTString, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "STATE", OFTInteger, 2 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "COUNTY", OFTInteger, 3 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "CENID", OFTString, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "POLYID", OFTInteger, 10 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "SOURCE", OFTString, 1 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "CFCC", OFTString, 3 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "KGLNAME", OFTString, 30 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "KGLADD", OFTString, 11 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "KGLZIP", OFTInteger, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "KGLZIP4", OFTInteger, 4 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FEAT", OFTInteger, 8 );
    poFeatureDefn->AddFieldDefn( &oField );
}

/************************************************************************/
/*                        ~TigerKeyFeatures()                         */
/************************************************************************/

TigerKeyFeatures::~TigerKeyFeatures()

{
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerKeyFeatures::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, FILE_CODE ) )
        return FALSE;

    EstablishFeatureCount();
    
    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerKeyFeatures::GetFeature( int nRecordId )

{
    char        achRecord[88];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %s9",
                  nRecordId, pszModule );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the raw record data from the file.                         */
/* -------------------------------------------------------------------- */
    if( fpPrimary == NULL )
        return NULL;

    if( VSIFSeek( fpPrimary, nRecordId * nRecordLength, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to seek to %d of %s9",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFRead( achRecord, sizeof(achRecord), 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %s9",
                  nRecordId, pszModule );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Set fields.                                                     */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );

    SetField( poFeature, "FILE", achRecord, 6, 10 );
    SetField( poFeature, "STATE", achRecord, 6, 7 );
    SetField( poFeature, "COUNTY", achRecord, 8, 10 );
    SetField( poFeature, "CENID", achRecord, 11, 15 );
    SetField( poFeature, "POLYID", achRecord, 16, 25 );
    SetField( poFeature, "SOURCE", achRecord, 26, 26 );
    SetField( poFeature, "CFCC", achRecord, 27, 29 );
    SetField( poFeature, "KGLNAME", achRecord, 30, 59 );
    SetField( poFeature, "KGLADD", achRecord, 60, 70 );
    SetField( poFeature, "KGLZIP", achRecord, 71, 75 );
    SetField( poFeature, "KGLZIP4", achRecord, 76, 79 );
    SetField( poFeature, "FEAT", achRecord, 80, 87 );

    return poFeature;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

#define WRITE_REC_LEN 88

OGRErr TigerKeyFeatures::CreateFeature( OGRFeature *poFeature )

{
    char	szRecord[WRITE_REC_LEN+1];

    if( !SetWriteModule( FILE_CODE, WRITE_REC_LEN+2, poFeature ) )
        return OGRERR_FAILURE;

    memset( szRecord, ' ', WRITE_REC_LEN );

    WriteField( poFeature, "FILE", szRecord, 6, 10, 'L', 'N' );
    WriteField( poFeature, "STATE", szRecord, 6, 7, 'L', 'N' );
    WriteField( poFeature, "COUNTY", szRecord, 8, 10, 'L', 'N' );
    WriteField( poFeature, "CENID", szRecord, 11, 15, 'L', 'A' );
    WriteField( poFeature, "POLYID", szRecord, 16, 25, 'R', 'N' );
    WriteField( poFeature, "SOURCE", szRecord, 26, 26, 'L', 'A' );
    WriteField( poFeature, "CFCC", szRecord, 27, 29, 'L', 'A' );
    WriteField( poFeature, "KGLNAME", szRecord, 30, 59, 'L', 'A' );
    WriteField( poFeature, "KGLADD", szRecord, 60, 70, 'R', 'A' );
    WriteField( poFeature, "KGLZIP", szRecord, 71, 75, 'L', 'N' );
    WriteField( poFeature, "KGLZIP4", szRecord, 76, 79, 'L', 'N' );
    WriteField( poFeature, "FEAT", szRecord, 80, 87, 'R', 'N' );

    WriteRecord( szRecord, WRITE_REC_LEN, FILE_CODE );

    return OGRERR_NONE;
}
