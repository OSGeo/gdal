/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerFeatureIds, providing access to .RT5 files.
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
 * Revision 1.5  2001/07/04 23:25:32  warmerda
 * first round implementation of writer
 *
 * Revision 1.4  2001/07/04 05:40:35  warmerda
 * upgraded to support FILE, and Tiger2000 schema
 *
 * Revision 1.3  2001/01/19 21:15:20  warmerda
 * expanded tabs
 *
 * Revision 1.2  2000/01/13 05:18:11  warmerda
 * added support for multiple versions
 *
 * Revision 1.1  1999/12/22 15:37:59  warmerda
 * New
 *
 */

#include "ogr_tiger.h"
#include "cpl_conv.h"

#define FILE_CODE "5"

/************************************************************************/
/*                            TigerFeatureIds()                         */
/************************************************************************/

TigerFeatureIds::TigerFeatureIds( OGRTigerDataSource * poDSIn,
                                  const char * pszPrototypeModule )

{
    OGRFieldDefn        oField("",OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "FeatureIds" );
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
    
    oField.Set( "FEAT", OFTInteger, 8 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FEDIRP", OFTString, 2 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FENAME", OFTString, 30 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FETYPE", OFTString, 4 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FEDIRS", OFTString, 2 );
    poFeatureDefn->AddFieldDefn( &oField );
}

/************************************************************************/
/*                        ~TigerFeatureIds()                         */
/************************************************************************/

TigerFeatureIds::~TigerFeatureIds()

{
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerFeatureIds::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, FILE_CODE ) )
        return FALSE;

    EstablishFeatureCount();
    
    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerFeatureIds::GetFeature( int nRecordId )

{
    char        achRecord[52];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %s5",
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
                  "Failed to seek to %d of %s5",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFRead( achRecord, sizeof(achRecord), 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %s5",
                  nRecordId, pszModule );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Set fields.                                                     */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );

    SetField( poFeature, "FILE", achRecord, 2, 6 );
    SetField( poFeature, "STATE", achRecord, 2, 3 );
    SetField( poFeature, "COUNTY", achRecord, 4, 6 );
    SetField( poFeature, "FEAT", achRecord, 7, 14 );
    SetField( poFeature, "FEDIRP", achRecord, 15, 16 );
    SetField( poFeature, "FENAME", achRecord, 17, 46 );
    SetField( poFeature, "FETYPE", achRecord, 47, 50 );
    SetField( poFeature, "FEDIRS", achRecord, 51, 52 );

    return poFeature;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

#define WRITE_REC_LEN 52

OGRErr TigerFeatureIds::CreateFeature( OGRFeature *poFeature )

{
    char	szRecord[WRITE_REC_LEN+1];

    if( !SetWriteModule( FILE_CODE, WRITE_REC_LEN+2, poFeature ) )
        return OGRERR_FAILURE;

    memset( szRecord, ' ', WRITE_REC_LEN );

    WriteField( poFeature, "FILE", szRecord, 2, 6, 'L', 'N' );
    WriteField( poFeature, "STATE", szRecord, 2, 3, 'L', 'N' );
    WriteField( poFeature, "COUNTY", szRecord, 4, 6, 'L', 'N' );
    WriteField( poFeature, "FEAT", szRecord, 7, 14, 'R', 'N' );
    WriteField( poFeature, "FEDIRP", szRecord, 15, 16, 'L', 'A' );
    WriteField( poFeature, "FENAME", szRecord, 17, 46, 'L', 'A' );
    WriteField( poFeature, "FETYPE", szRecord, 47, 50, 'L', 'A' );
    WriteField( poFeature, "FEDIRS", szRecord, 51, 52, 'L', 'A' );

    WriteRecord( szRecord, WRITE_REC_LEN, FILE_CODE );

    return OGRERR_NONE;
}
