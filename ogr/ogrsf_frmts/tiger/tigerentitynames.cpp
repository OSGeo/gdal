/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerEntityNames, providing access to .RTC files.
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
 * Revision 1.2  2000/01/13 05:18:11  warmerda
 * added support for multiple versions
 *
 * Revision 1.1  1999/12/22 15:37:59  warmerda
 * New
 *
 */

#include "ogr_tiger.h"
#include "cpl_conv.h"

/************************************************************************/
/*                          TigerEntityNames()                          */
/************************************************************************/

TigerEntityNames::TigerEntityNames( OGRTigerDataSource * poDSIn,
                            const char * pszPrototypeModule )

{
    OGRFieldDefn	oField("",OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "EntityNames" );
    poFeatureDefn->SetGeomType( wkbPoint );

/* -------------------------------------------------------------------- */
/*      Fields from type 9 record.                                      */
/* -------------------------------------------------------------------- */
    oField.Set( "MODULE", OFTString, 8 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "STATE", OFTInteger, 2 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "COUNTY", OFTInteger, 3 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FIPSYR", OFTString, 2 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FIPS", OFTInteger, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FIPSCC", OFTInteger, 2 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "PDC", OFTString, 1 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "LASAD", OFTInteger, 2 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "ENTITY", OFTString, 1 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "MA", OFTInteger, 4 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "SD", OFTInteger, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "AIR", OFTInteger, 4 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "VTD", OFTString, 6 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "UA", OFTInteger, 4 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "NAME", OFTString, 66 );
    poFeatureDefn->AddFieldDefn( &oField );
}

/************************************************************************/
/*                         ~TigerEntityNames()                          */
/************************************************************************/

TigerEntityNames::~TigerEntityNames()

{
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerEntityNames::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, "C" ) )
        return FALSE;

    EstablishFeatureCount();
    
    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerEntityNames::GetFeature( int nRecordId )

{
    char	achRecord[112];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %sC",
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
                  "Failed to seek to %d of %sC",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFRead( achRecord, sizeof(achRecord), 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %sC",
                  nRecordId, pszModule );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Set fields.                                                     */
/* -------------------------------------------------------------------- */
    OGRFeature	*poFeature = new OGRFeature( poFeatureDefn );

    SetField( poFeature, "STATE", achRecord, 6, 7 );
    SetField( poFeature, "COUNTY", achRecord, 8, 10 );
    SetField( poFeature, "FIPSYR", achRecord, 11, 12 );
    SetField( poFeature, "FIPS", achRecord, 13, 17 );
    SetField( poFeature, "FIPSCC", achRecord, 18, 19 );
    SetField( poFeature, "PDC", achRecord, 20, 20 );
    SetField( poFeature, "LASAD", achRecord, 21, 22 );
    SetField( poFeature, "ENTITY", achRecord, 23, 23 );
    SetField( poFeature, "MA", achRecord, 24, 27 );
    SetField( poFeature, "SD", achRecord, 28, 32 );
    SetField( poFeature, "AIR", achRecord, 33, 36 );
    SetField( poFeature, "VTD", achRecord, 37, 42 );
    SetField( poFeature, "UA", achRecord, 43, 46 );
    SetField( poFeature, "NAME", achRecord, 47, 112 );

    return poFeature;
}


