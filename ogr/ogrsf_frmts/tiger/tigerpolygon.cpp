/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerPolygon, providing access to .RTA files.
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

/************************************************************************/
/*                            TigerPolygon()                            */
/************************************************************************/

TigerPolygon::TigerPolygon( OGRTigerDataSource * poDSIn,
                                  const char * pszPrototypeModule )

{
    OGRFieldDefn        oField("",OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "Polygon" );
    poFeatureDefn->SetGeomType( wkbNone );

    fpRTS = NULL;
    bUsingRTS = TRUE;

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
    
    oField.Set( "FAIR", OFTInteger, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FMCD", OFTInteger, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FPL", OFTInteger, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "CTBNA90", OFTInteger, 6 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "BLK90", OFTInteger, 4 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "CD106", OFTInteger, 2 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "CD108", OFTInteger, 2 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "SDELM", OFTString, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "SDSEC", OFTString, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "SDUNI", OFTString, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "TAZ", OFTString, 6 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "UA", OFTInteger, 4 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "URBFLAG", OFTString, 1 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "CTPP", OFTString, 4 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "STATE90", OFTInteger, 2 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "COUN90", OFTInteger, 3 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "AIR90", OFTInteger, 4 );
    poFeatureDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      Add the RTS records if it is available.                         */
/* -------------------------------------------------------------------- */
    if( bUsingRTS )
    {
        oField.Set( "WATER", OFTString, 1 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "CMSAMSA", OFTInteger, 4 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "PMSA", OFTInteger, 4 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "AIR", OFTInteger, 4 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "TRUST", OFTString, 1 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "ANRC", OFTInteger, 2 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "STATECU", OFTInteger, 2 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "COUNTYCU", OFTInteger, 3 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "FCCITY", OFTInteger, 5 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "FMCD", OFTInteger, 5 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "FSMCD", OFTInteger, 5 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "FPL", OFTInteger, 5 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "CTBNA00", OFTInteger, 6 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "BLK00", OFTString, 4 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "CDCU", OFTInteger, 2 );
        poFeatureDefn->AddFieldDefn( &oField );

        if( poDS->GetVersion() < TIGER_2000_Redistricting )
        {
            oField.Set( "STSENATE", OFTString, 6 );
            poFeatureDefn->AddFieldDefn( &oField );
            
            oField.Set( "STHOUSE", OFTString, 6 );
            poFeatureDefn->AddFieldDefn( &oField );
            
            oField.Set( "VTD00", OFTString, 6 );
            poFeatureDefn->AddFieldDefn( &oField );
        }
        else
        {
            oField.Set( "SLDU", OFTString, 3 );
            poFeatureDefn->AddFieldDefn( &oField );
            
            oField.Set( "SLDL", OFTString, 3 );
            poFeatureDefn->AddFieldDefn( &oField );
            
            oField.Set( "UGA", OFTString, 5 );
            poFeatureDefn->AddFieldDefn( &oField );
            
            oField.Set( "BLKGRP", OFTInteger, 1 );
            poFeatureDefn->AddFieldDefn( &oField );
            
            oField.Set( "VTD", OFTString, 6 );
            poFeatureDefn->AddFieldDefn( &oField );
            
            oField.Set( "STATECOL", OFTInteger, 2 );
            poFeatureDefn->AddFieldDefn( &oField );
            
            oField.Set( "COUNTYCOL", OFTInteger, 3 );
            poFeatureDefn->AddFieldDefn( &oField );
            
            oField.Set( "BLOCKCOL", OFTInteger, 5 );
            poFeatureDefn->AddFieldDefn( &oField );
            
            oField.Set( "BLKSUFCOL", OFTString, 1 );
            poFeatureDefn->AddFieldDefn( &oField );
            
            oField.Set( "ZCTA5", OFTString, 5 );
            poFeatureDefn->AddFieldDefn( &oField );
        }
    }
}

/************************************************************************/
/*                           ~TigerPolygon()                            */
/************************************************************************/

TigerPolygon::~TigerPolygon()

{
    if( fpRTS != NULL )
        VSIFClose( fpRTS );
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerPolygon::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, "A" ) )
        return FALSE;

    EstablishFeatureCount();
    
/* -------------------------------------------------------------------- */
/*      Open the RTS file                                               */
/* -------------------------------------------------------------------- */
    if( bUsingRTS )
    {
        if( fpRTS != NULL )
        {
            VSIFClose( fpRTS );
            fpRTS = NULL;
        }

        if( pszModule )
        {
            char        *pszFilename;
        
            pszFilename = poDS->BuildFilename( pszModule, "S" );

            fpRTS = VSIFOpen( pszFilename, "rb" );

            CPLFree( pszFilename );

            nRTSRecLen = EstablishRecordLength( fpRTS );
        }
    }
    
    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerPolygon::GetFeature( int nRecordId )

{
    char        achRecord[98];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %sA",
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
                  "Failed to seek to %d of %sA",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFRead( achRecord, sizeof(achRecord), 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %sA",
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
    SetField( poFeature, "FAIR", achRecord, 26, 30 );
    SetField( poFeature, "FMCD", achRecord, 31, 35 );
    SetField( poFeature, "FPL", achRecord, 36, 40 );
    SetField( poFeature, "CTBNA90", achRecord, 41, 46 );
    SetField( poFeature, "BLK90", achRecord, 47, 50 );
    SetField( poFeature, "CD106", achRecord, 51, 52 );
    SetField( poFeature, "CD108", achRecord, 53, 54 );
    SetField( poFeature, "SDELM", achRecord, 55, 59 );
    SetField( poFeature, "SDSEC", achRecord, 65, 69 );
    SetField( poFeature, "SDUNI", achRecord, 70, 74 );
    SetField( poFeature, "TAZ", achRecord, 75, 80 );
    SetField( poFeature, "UA", achRecord, 81, 84 );
    SetField( poFeature, "URBFLAG", achRecord, 85, 85 );
    SetField( poFeature, "CTPP", achRecord, 86, 89 );
    SetField( poFeature, "STATE90", achRecord, 90, 91 );
    SetField( poFeature, "COUN90", achRecord, 92, 94 );
    SetField( poFeature, "AIR90", achRecord, 95, 98 );

/* -------------------------------------------------------------------- */
/*      Read RTS record, and apply fields.                              */
/* -------------------------------------------------------------------- */
    if( fpRTS != NULL )
    {
        char    achRTSRec[120];

        if( VSIFSeek( fpRTS, nRecordId * nRTSRecLen, SEEK_SET ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to seek to %d of %sS",
                      nRecordId * nRTSRecLen, pszModule );
            return NULL;
        }

        if( VSIFRead( achRTSRec, 111, 1, fpRTS ) != 1 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to read record %d of %sS",
                      nRecordId, pszModule );
            return NULL;
        }

        SetField( poFeature, "WATER", achRTSRec, 26, 26 );
        SetField( poFeature, "CMSAMSA", achRTSRec, 27, 30 );
        SetField( poFeature, "PMSA", achRTSRec, 31, 34 );
        SetField( poFeature, "AIR", achRTSRec, 40, 43 );
        SetField( poFeature, "TRUST", achRTSRec, 44, 44 );
        SetField( poFeature, "ANRC", achRTSRec, 45, 46 );
        SetField( poFeature, "STATECU", achRTSRec, 47, 48 );
        SetField( poFeature, "COUNTYCU", achRTSRec, 49, 51 );
        SetField( poFeature, "FCCITY", achRTSRec, 52, 56 );
        SetField( poFeature, "FMCD", achRTSRec, 57, 61 );
        SetField( poFeature, "FSMCD", achRTSRec, 62, 66 );
        SetField( poFeature, "FPL", achRTSRec, 67, 71 );
        SetField( poFeature, "CTBNA00", achRTSRec, 72, 77 );
        SetField( poFeature, "BLK00", achRTSRec, 78, 81 );
        SetField( poFeature, "RS10", achRTSRec, 82, 82 );
        SetField( poFeature, "CDCU", achRTSRec, 83, 84 );

        if( GetVersion() < TIGER_2000_Redistricting )
        {
            SetField( poFeature, "STSENATE", achRTSRec, 85, 90 );
            SetField( poFeature, "STHOUSE", achRTSRec, 91, 96 );
            SetField( poFeature, "VTD00", achRTSRec, 97, 102 );
        }
        else
        {
            SetField( poFeature, "SLDU", achRTSRec, 85, 87 );
            SetField( poFeature, "SLDL", achRTSRec, 88, 90 );
            SetField( poFeature, "UGA", achRTSRec, 91, 96 );
            SetField( poFeature, "BLKGRP", achRTSRec, 97, 102 );
            SetField( poFeature, "VTD", achRTSRec, 97, 102 );
            SetField( poFeature, "STATECOL", achRTSRec, 103, 104 );
            SetField( poFeature, "COUNTYCOL", achRTSRec, 105, 107 );
            SetField( poFeature, "BLOCKCOL", achRTSRec, 108, 112 );
            SetField( poFeature, "BLKSUFCOL", achRTSRec, 113, 113 );
            SetField( poFeature, "ZCTA5", achRTSRec, 114, 118 );
        }
    }
    
    return poFeature;
}


