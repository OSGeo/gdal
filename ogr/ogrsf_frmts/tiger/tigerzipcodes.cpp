/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerZipCodes, providing access to .RT6 files.
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
 * Revision 1.1  1999/12/15 19:59:17  warmerda
 * New
 *
 */

#include "ogr_tiger.h"
#include "cpl_conv.h"

/************************************************************************/
/*                            TigerZipCodes()                           */
/************************************************************************/

TigerZipCodes::TigerZipCodes( OGRTigerDataSource * poDSIn,
                                  const char * pszPrototypeModule )

{
    OGRFieldDefn	oField("",OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "ZipCodes" );
    poFeatureDefn->SetGeomType( wkbNone );

/* -------------------------------------------------------------------- */
/*      Fields from type 5 record.                                      */
/* -------------------------------------------------------------------- */
    oField.Set( "TLID", OFTInteger, 10 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "RTSQ", OFTInteger, 3 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FRADDL", OFTString, 11 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "TOADDL", OFTString, 11 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FRADDR", OFTString, 11 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "TOADDR", OFTString, 11 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FRIADDL", OFTInteger, 1 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "TOIADDL", OFTInteger, 1 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FRIADDR", OFTInteger, 1 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "TOIADDR", OFTInteger, 1 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "ZIPL", OFTInteger, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "ZIPR", OFTInteger, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
}

/************************************************************************/
/*                        ~TigerZipCodes()                         */
/************************************************************************/

TigerZipCodes::~TigerZipCodes()

{
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerZipCodes::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, "RT6" ) )
        return FALSE;

    EstablishFeatureCount();
    
    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerZipCodes::GetFeature( int nRecordId )

{
    char	achRecord[76];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %s.RT6",
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
                  "Failed to seek to %d of %s.RT6",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFRead( achRecord, 76, 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %s.RT6",
                  nRecordId, pszModule );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Set fields.                                                     */
/* -------------------------------------------------------------------- */
    OGRFeature	*poFeature = new OGRFeature( poFeatureDefn );

    poFeature->SetField( "TLID", GetField( achRecord, 6, 15 ));
    poFeature->SetField( "RTSQ", GetField( achRecord, 16, 18 ));
    poFeature->SetField( "FRADDL", GetField( achRecord, 19, 29 ));
    poFeature->SetField( "TOADDL", GetField( achRecord, 30, 40 ));
    poFeature->SetField( "FRADDR", GetField( achRecord, 41, 51 ));
    poFeature->SetField( "TOADDR", GetField( achRecord, 52, 62 ));
    poFeature->SetField( "FRIADDL", GetField( achRecord, 63, 63 ));
    poFeature->SetField( "TOIADDL", GetField( achRecord, 64, 64 ));
    poFeature->SetField( "FRIADDR", GetField( achRecord, 65, 65 ));
    poFeature->SetField( "TOIADDR", GetField( achRecord, 66, 66 ));
    poFeature->SetField( "ZIPL", GetField( achRecord, 67, 71 ));
    poFeature->SetField( "ZIPR", GetField( achRecord, 72, 76 ));

    return poFeature;
}


