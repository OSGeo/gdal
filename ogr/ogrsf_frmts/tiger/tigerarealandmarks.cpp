/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerAreaLandmarks, providing access to .RT8 files.
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

#define FILE_CODE "8"

/************************************************************************/
/*                         TigerAreaLandmarks()                         */
/************************************************************************/

TigerAreaLandmarks::TigerAreaLandmarks( OGRTigerDataSource * poDSIn,
                                  const char * pszPrototypeModule )

{
    OGRFieldDefn        oField("",OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "AreaLandmarks" );
    poFeatureDefn->SetGeomType( wkbNone );

/* -------------------------------------------------------------------- */
/*      Fields from type 5 record.                                      */
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
    
    oField.Set( "LAND", OFTInteger, 10 );
    poFeatureDefn->AddFieldDefn( &oField );
}

/************************************************************************/
/*                        ~TigerAreaLandmarks()                         */
/************************************************************************/

TigerAreaLandmarks::~TigerAreaLandmarks()

{
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerAreaLandmarks::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, FILE_CODE ) )
        return FALSE;

    EstablishFeatureCount();
    
    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerAreaLandmarks::GetFeature( int nRecordId )

{
    char        achRecord[36];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %s8",
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
                  "Failed to seek to %d of %s8",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFRead( achRecord, sizeof(achRecord), 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %s8",
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
    SetField( poFeature, "LAND", achRecord, 26, 35 );

    return poFeature;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

#define WRITE_REC_LEN 36

OGRErr TigerAreaLandmarks::CreateFeature( OGRFeature *poFeature )

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
    WriteField( poFeature, "LAND", szRecord, 26, 35, 'R', 'N' );

    WriteRecord( szRecord, WRITE_REC_LEN, FILE_CODE );

    return OGRERR_NONE;
}
