/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerCompleteChain, providing access to RT1 and
 *           related files.
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
 * Revision 1.1  1999/10/07 18:19:21  warmerda
 * New
 *
 */

#include "ogr_tiger.h"
#include "cpl_conv.h"

/************************************************************************/
/*                         TigerCompleteChain()                         */
/************************************************************************/

TigerCompleteChain::TigerCompleteChain( OGRTigerDataSource * poDSIn,
                                        const char * pszPrototypeModule )

{
    OGRFieldDefn	oField("",OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "CompleteChain" );
    poFeatureDefn->SetGeomType( wkbLineString );

    oField.Set( "TLID", OFTInteger, 10 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "SIDE1", OFTInteger, 1 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "SOURCE", OFTString, 1 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FEDIRP", OFTString, 2 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FENAME", OFTString, 30 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FETYPE", OFTString, 4 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FEDIRS", OFTString, 2 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "CFCC", OFTString, 3 );
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

    oField.Set( "FAIRL", OFTInteger, 5 );
    poFeatureDefn->AddFieldDefn( &oField );

    oField.Set( "FAIRR", OFTInteger, 5 );
    poFeatureDefn->AddFieldDefn( &oField );

    oField.Set( "TRUSTL", OFTString, 1 );
    poFeatureDefn->AddFieldDefn( &oField );

    oField.Set( "TRUSTR", OFTString, 1 );
    poFeatureDefn->AddFieldDefn( &oField );

    oField.Set( "CENSUS1", OFTString, 1 );
    poFeatureDefn->AddFieldDefn( &oField );

    oField.Set( "CENSUS2", OFTString, 1 );
    poFeatureDefn->AddFieldDefn( &oField );

    oField.Set( "STATEL", OFTInteger, 2 );
    poFeatureDefn->AddFieldDefn( &oField );

    oField.Set( "STATER", OFTInteger, 2 );
    poFeatureDefn->AddFieldDefn( &oField );

    oField.Set( "COUNTYL", OFTInteger, 3 );
    poFeatureDefn->AddFieldDefn( &oField );

    oField.Set( "COUNTYR", OFTInteger, 3 );
    poFeatureDefn->AddFieldDefn( &oField );

    oField.Set( "FMCDL", OFTInteger, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FMCDR", OFTInteger, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FSMCDL", OFTInteger, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FSMCDR", OFTInteger, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FPLL", OFTInteger, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FPLR", OFTInteger, 5 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "CTBNAL", OFTInteger, 6 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "CTBNAR", OFTInteger, 6 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "BLKL", OFTString, 4 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "BLKR", OFTString, 4 );
    poFeatureDefn->AddFieldDefn( &oField );
}

/************************************************************************/
/*                        ~TigerCompleteChain()                         */
/************************************************************************/

TigerCompleteChain::~TigerCompleteChain()

{
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerCompleteChain::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, "RT1" ) )
        return FALSE;

    EstablishFeatureCount();

    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerCompleteChain::GetFeature( int nRecordId )

{
    char	achRecord[228];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %s.RT1",
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
                  "Failed to seek to %d of %s.RT1",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFRead( achRecord, 228, 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %s.RT1",
                  nRecordId, pszModule );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Set fields.                                                     */
/* -------------------------------------------------------------------- */
    OGRFeature	*poFeature = new OGRFeature( poFeatureDefn );

    poFeature->SetField( "TLID", GetField( achRecord, 6, 15 ));
    poFeature->SetField( "SIDE1", GetField( achRecord, 16, 16 ));
    poFeature->SetField( "SOURCE", GetField( achRecord, 17, 17 ));
    poFeature->SetField( "FEDIRP", GetField( achRecord, 18, 19 ));
    poFeature->SetField( "FENAME", GetField( achRecord, 20, 49 ));
    poFeature->SetField( "FETYPE", GetField( achRecord, 50, 53 ));
    poFeature->SetField( "FEDIRS", GetField( achRecord, 54, 55 ));
    poFeature->SetField( "CFCC", GetField( achRecord, 56, 58 ));
    poFeature->SetField( "FRADDL", GetField( achRecord, 59, 59 ));
    poFeature->SetField( "TOADDL", GetField( achRecord, 70, 80 ));
    poFeature->SetField( "FRADDR", GetField( achRecord, 81, 91 ));
    poFeature->SetField( "TOADDR", GetField( achRecord, 92, 102 ));
    poFeature->SetField( "FRIADDL", GetField( achRecord, 103, 103 ));
    poFeature->SetField( "TOIADDL", GetField( achRecord, 104, 104 ));
    poFeature->SetField( "FRIADDR", GetField( achRecord, 105, 105 ));
    poFeature->SetField( "TOIADDR", GetField( achRecord, 106, 106 ));
    poFeature->SetField( "ZIPL", GetField( achRecord, 107, 111 ));
    poFeature->SetField( "ZIPR", GetField( achRecord, 112, 116 ));
    poFeature->SetField( "FAIRL", GetField( achRecord, 117, 121 ));
    poFeature->SetField( "FAIRR", GetField( achRecord, 122, 126 ));
    poFeature->SetField( "TRUSTL", GetField( achRecord, 127, 127 ));
    poFeature->SetField( "TRUSTR", GetField( achRecord, 128, 128 ));
    poFeature->SetField( "CENSUS1", GetField( achRecord, 129, 129 ));
    poFeature->SetField( "CENSUS2", GetField( achRecord, 130, 130 ));

/* -------------------------------------------------------------------- */
/*      Set geometry                                                    */
/* -------------------------------------------------------------------- */
    OGRLineString	*poLine = new OGRLineString();

    poLine->setPoint(0,
                     atoi(GetField(achRecord, 191, 200)) / 1000000.0,
                     atoi(GetField(achRecord, 201, 209)) / 1000000.0 );
    poLine->setPoint(1,
                     atoi(GetField(achRecord, 210, 219)) / 1000000.0,
                     atoi(GetField(achRecord, 220, 228)) / 1000000.0 );

    poFeature->SetGeometryDirectly( poLine );

    return poFeature;
}
