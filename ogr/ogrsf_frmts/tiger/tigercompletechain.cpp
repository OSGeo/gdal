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
 * Revision 1.10  2001/07/19 16:05:49  warmerda
 * clear out tabs
 *
 * Revision 1.9  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.8  2001/07/04 23:25:32  warmerda
 * first round implementation of writer
 *
 * Revision 1.7  2001/07/04 05:40:35  warmerda
 * upgraded to support FILE, and Tiger2000 schema
 *
 * Revision 1.6  2001/07/04 03:08:21  warmerda
 * fixed FRADDL width
 *
 * Revision 1.5  2001/01/19 21:15:20  warmerda
 * expanded tabs
 *
 * Revision 1.4  2000/01/13 05:18:11  warmerda
 * added support for multiple versions
 *
 * Revision 1.3  1999/12/22 15:38:15  warmerda
 * major update
 *
 * Revision 1.2  1999/11/04 21:14:31  warmerda
 * various improvements, and TestCapability()
 *
 * Revision 1.1  1999/10/07 18:19:21  warmerda
 * New
 *
 */

#include "ogr_tiger.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         TigerCompleteChain()                         */
/************************************************************************/

TigerCompleteChain::TigerCompleteChain( OGRTigerDataSource * poDSIn,
                                        const char * pszPrototypeModule )

{
    OGRFieldDefn        oField("",OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "CompleteChain" );
    poFeatureDefn->SetGeomType( wkbLineString );

    fpRT3 = NULL;
    bUsingRT3 = TRUE;

    panShapeRecordId = NULL;
    fpShape = NULL;
    
/* -------------------------------------------------------------------- */
/*      Fields from type 1 record.                                      */
/* -------------------------------------------------------------------- */
    oField.Set( "MODULE", OFTString, 8 );
    poFeatureDefn->AddFieldDefn( &oField );
    
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

/* -------------------------------------------------------------------- */
/*      Fields from type 3 record.  Eventually we should verify that    */
/*      a .RT3 file is available before adding these fields.            */
/* -------------------------------------------------------------------- */
    if( bUsingRT3 )
    {
        oField.Set( "STATE90L", OFTInteger, 2 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "STATE90R", OFTInteger, 2 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "COUN90L", OFTInteger, 3 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "COUN90R", OFTInteger, 3 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "FMCD90L", OFTInteger, 5 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "FMCD90R", OFTInteger, 5 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "FPL90L", OFTInteger, 5 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "FPL90R", OFTInteger, 5 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "CTBNA90L", OFTInteger, 6 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "CTBNA90R", OFTInteger, 6 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "AIR90L", OFTInteger, 4 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "AIR90R", OFTInteger, 4 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "TRUST90L", OFTInteger, 1 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "TRUST90R", OFTInteger, 1 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "BLK90L", OFTString, 4 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "BLK90R", OFTString, 4 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "AIRL", OFTInteger, 4 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "AIRR", OFTInteger, 4 );
        poFeatureDefn->AddFieldDefn( &oField );

        if( poDS->GetVersion() >= TIGER_2000_Redistricting )
        {
            oField.Set( "ANRCL", OFTInteger, 5 );
            poFeatureDefn->AddFieldDefn( &oField );
            
            oField.Set( "ANRCR", OFTInteger, 5 );
            poFeatureDefn->AddFieldDefn( &oField );
            
            oField.Set( "AITSCEL", OFTInteger, 3 );
            poFeatureDefn->AddFieldDefn( &oField );
            
            oField.Set( "AITSCER", OFTInteger, 3 );
            poFeatureDefn->AddFieldDefn( &oField );
            
            oField.Set( "AITL", OFTInteger, 5 );
            poFeatureDefn->AddFieldDefn( &oField );
            
            oField.Set( "AITR", OFTInteger, 5 );
            poFeatureDefn->AddFieldDefn( &oField );
        }
        else
        {
            oField.Set( "VTDL", OFTString, 4 );
            poFeatureDefn->AddFieldDefn( &oField );
            
            oField.Set( "VTDR", OFTString, 4 );
            poFeatureDefn->AddFieldDefn( &oField );
        }
    }
}

/************************************************************************/
/*                        ~TigerCompleteChain()                         */
/************************************************************************/

TigerCompleteChain::~TigerCompleteChain()

{
    if( fpRT3 != NULL )
        VSIFClose( fpRT3 );

    if( fpShape != NULL )
        VSIFClose( fpShape );
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerCompleteChain::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, "1" ) )
        return FALSE;

    EstablishFeatureCount();

/* -------------------------------------------------------------------- */
/*      Open the RT3 file                                               */
/* -------------------------------------------------------------------- */
    if( bUsingRT3 )
    {
        if( fpRT3 != NULL )
        {
            VSIFClose( fpRT3 );
            fpRT3 = NULL;
        }

        if( pszModule )
        {
            char        *pszFilename;
        
            pszFilename = poDS->BuildFilename( pszModule, "3" );

            fpRT3 = VSIFOpen( pszFilename, "rb" );

            CPLFree( pszFilename );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Close the shape point file, if open and free the list of        */
/*      record ids.                                                     */
/* -------------------------------------------------------------------- */
    if( fpShape != NULL )
    {
        VSIFClose( fpShape );
        fpShape = NULL;
    }
    
    CPLFree( panShapeRecordId );
    panShapeRecordId = NULL;

    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerCompleteChain::GetFeature( int nRecordId )

{
    char        achRecord[228];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %s1",
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
                  "Failed to seek to %d of %s1",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFRead( achRecord, 228, 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %s1",
                  nRecordId, pszModule );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Set fields.                                                     */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );

    SetField( poFeature, "TLID", achRecord, 6, 15 );
    SetField( poFeature, "SIDE1", achRecord, 16, 16 );
    SetField( poFeature, "SOURCE", achRecord, 17, 17 );
    SetField( poFeature, "FEDIRP", achRecord, 18, 19 );
    SetField( poFeature, "FENAME", achRecord, 20, 49 );
    SetField( poFeature, "FETYPE", achRecord, 50, 53 );
    SetField( poFeature, "FEDIRS", achRecord, 54, 55 );
    SetField( poFeature, "CFCC", achRecord, 56, 58 );
    SetField( poFeature, "FRADDL", achRecord, 59, 69 );
    SetField( poFeature, "TOADDL", achRecord, 70, 80 );
    SetField( poFeature, "FRADDR", achRecord, 81, 91 );
    SetField( poFeature, "TOADDR", achRecord, 92, 102 );
    SetField( poFeature, "FRIADDL", achRecord, 103, 103 );
    SetField( poFeature, "TOIADDL", achRecord, 104, 104 );
    SetField( poFeature, "FRIADDR", achRecord, 105, 105 );
    SetField( poFeature, "TOIADDR", achRecord, 106, 106 );
    SetField( poFeature, "ZIPL", achRecord, 107, 111 );
    SetField( poFeature, "ZIPR", achRecord, 112, 116 );
    SetField( poFeature, "FAIRL", achRecord, 117, 121 );
    SetField( poFeature, "FAIRR", achRecord, 122, 126 );
    SetField( poFeature, "TRUSTL", achRecord, 127, 127 );
    SetField( poFeature, "TRUSTR", achRecord, 128, 128 );
    SetField( poFeature, "CENSUS1", achRecord, 129, 129 );
    SetField( poFeature, "CENSUS2", achRecord, 130, 130 );
    SetField( poFeature, "STATEL", achRecord, 131, 132 );
    SetField( poFeature, "STATER", achRecord, 133, 134 );
    SetField( poFeature, "COUNTYL", achRecord, 135, 137 );
    SetField( poFeature, "COUNTYR", achRecord, 138, 140 );
    SetField( poFeature, "FMCDL", achRecord, 141, 145 );
    SetField( poFeature, "FMCDR", achRecord, 146, 150 );
    SetField( poFeature, "FSMCDL", achRecord, 151, 155 );
    SetField( poFeature, "FSMCDR", achRecord, 156, 160 );
    SetField( poFeature, "FPLL", achRecord, 161, 165 );
    SetField( poFeature, "FPLR", achRecord, 166, 170 );
    SetField( poFeature, "CTBNAL", achRecord, 171, 176 );
    SetField( poFeature, "CTBNAR", achRecord, 177, 182 );
    SetField( poFeature, "BLKL", achRecord, 183, 186 );
    SetField( poFeature, "BLKR", achRecord, 187, 190 );

/* -------------------------------------------------------------------- */
/*      Read RT3 record, and apply fields.                              */
/* -------------------------------------------------------------------- */
    if( fpRT3 != NULL )
    {
        char    achRT3Rec[111];
        int     nRT3RecLen = 111 + nRecordLength - 228;

        if( VSIFSeek( fpRT3, nRecordId * nRT3RecLen, SEEK_SET ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to seek to %d of %s3",
                      nRecordId * nRT3RecLen, pszModule );
            return NULL;
        }

        if( VSIFRead( achRT3Rec, 111, 1, fpRT3 ) != 1 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to read record %d of %s3",
                      nRecordId, pszModule );
            return NULL;
        }

        SetField( poFeature, "STATE90L", achRT3Rec, 16, 17 );
        SetField( poFeature, "STATE90R", achRT3Rec, 18, 19 );
        SetField( poFeature, "COUN90L", achRT3Rec, 20, 22 );
        SetField( poFeature, "COUN90R", achRT3Rec, 23, 25 );
        SetField( poFeature, "FMCD90L", achRT3Rec, 26, 30 );
        SetField( poFeature, "FMCD90R", achRT3Rec, 31, 35 );
        SetField( poFeature, "FPL90L", achRT3Rec, 36, 40 );
        SetField( poFeature, "FPL90R", achRT3Rec, 41, 45 );
        SetField( poFeature, "CTBNA90L", achRT3Rec, 46, 51 );
        SetField( poFeature, "CTBNA90R", achRT3Rec, 52, 57 );
        SetField( poFeature, "AIR90L", achRT3Rec, 58, 61 );
        SetField( poFeature, "AIR90R", achRT3Rec, 62, 65 );
        SetField( poFeature, "TRUST90L", achRT3Rec, 66, 66 );
        SetField( poFeature, "TRUST90R", achRT3Rec, 67, 67 );
        SetField( poFeature, "BLK90L", achRT3Rec, 70, 73 );
        SetField( poFeature, "BLK90R", achRT3Rec, 74, 77 );
        SetField( poFeature, "AIRL", achRT3Rec, 78, 81 );
        SetField( poFeature, "AIRR", achRT3Rec, 82, 85 );

        if( GetVersion() >= TIGER_2000_Redistricting )
        {
            SetField( poFeature, "ANRCL", achRT3Rec, 86, 90 );
            SetField( poFeature, "ANRCR", achRT3Rec, 91, 95 );
            SetField( poFeature, "AITSCEL", achRT3Rec, 96, 98 );
            SetField( poFeature, "AITSCER", achRT3Rec, 99, 101 );
            SetField( poFeature, "AITSL", achRT3Rec, 102, 106 );
            SetField( poFeature, "AITSR", achRT3Rec, 107, 111 );
        }
        else
        {
            SetField( poFeature, "VTDL", achRT3Rec, 104, 107 );
            SetField( poFeature, "VTDR", achRT3Rec, 108, 111 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Set geometry                                                    */
/* -------------------------------------------------------------------- */
    OGRLineString       *poLine = new OGRLineString();

    poLine->setPoint(0,
                     atoi(GetField(achRecord, 191, 200)) / 1000000.0,
                     atoi(GetField(achRecord, 201, 209)) / 1000000.0 );

    AddShapePoints( poFeature->GetFieldAsInteger("TLID"), nRecordId,
                    poLine, 0 );
    
    poLine->addPoint(atoi(GetField(achRecord, 210, 219)) / 1000000.0,
                     atoi(GetField(achRecord, 220, 228)) / 1000000.0 );

    poFeature->SetGeometryDirectly( poLine );

    return poFeature;
}

/************************************************************************/
/*                           AddShapePoints()                           */
/*                                                                      */
/*      Record zero or more shape records associated with this line     */
/*      and add the points to the passed line geometry.                 */
/************************************************************************/

void TigerCompleteChain::AddShapePoints( int nTLID, int nRecordId,
                                         OGRLineString * poLine, int nSeqNum ) 

{
    int         nShapeRecId;

    nShapeRecId = GetShapeRecordId( nRecordId, nTLID );
    if( nShapeRecId == -1 )
        return;

/* -------------------------------------------------------------------- */
/*      Read all the sequential records with the same TLID.             */
/* -------------------------------------------------------------------- */
    char        achShapeRec[208];
    int         nShapeRecLen = 208 + nRecordLength - 228;

    for( ; TRUE; nShapeRecId++ )
    {
        if( VSIFSeek( fpShape, (nShapeRecId-1) * nShapeRecLen,
                      SEEK_SET ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to seek to %d of %s2",
                      (nShapeRecId-1) * nShapeRecLen, pszModule );
            return;
        }

        if( VSIFRead( achShapeRec, 208, 1, fpShape ) != 1 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to read record %d of %s2",
                      nShapeRecId-1, pszModule );
            return;
        }

        if( atoi(GetField(achShapeRec,6,15)) != nTLID )
            break;

/* -------------------------------------------------------------------- */
/*      Translate the locations into OGRLineString vertices.            */
/* -------------------------------------------------------------------- */
        int     iVertex;

        for( iVertex = 0; iVertex < 10; iVertex++ )
        {
            int         iStart = 19 + 19*iVertex;
            if( EQUALN(achShapeRec+iStart-1,"+000000000+00000000",19) )
                break;

            poLine->addPoint(atoi(GetField(achShapeRec,iStart,iStart+9))
                                                                / 1000000.0,
                             atoi(GetField(achShapeRec,iStart+10,iStart+18))
                                                                / 1000000.0 );
        }

/* -------------------------------------------------------------------- */
/*      Don't get another record if this one was incomplete.            */
/* -------------------------------------------------------------------- */
        if( iVertex < 10 )
            break;
    }
}

/************************************************************************/
/*                          GetShapeRecordId()                          */
/*                                                                      */
/*      Get the record id of the first record of shape points for       */
/*      the provided TLID (complete chain).                             */
/************************************************************************/

int TigerCompleteChain::GetShapeRecordId( int nChainId, int nTLID )

{
/* -------------------------------------------------------------------- */
/*      As a by-product we force the shape point file (RT2) to be       */
/*      opened and allocate the record id array if the file isn't       */
/*      already open.                                                   */
/* -------------------------------------------------------------------- */
    if( fpShape == NULL )
    {
        char    *pszFilename;

        pszFilename = poDS->BuildFilename( pszModule, "2" );

        fpShape = VSIFOpen( pszFilename, "rb" );
        
        if( fpShape == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to open %s.\n",
                      pszFilename );

            CPLFree( pszFilename );
            return -1;
        }
        
        CPLFree( pszFilename );

        panShapeRecordId = (int *) CPLCalloc(sizeof(int),GetFeatureCount());
    }

    CPLAssert( nChainId >= 0 && nChainId < GetFeatureCount() );
    
/* -------------------------------------------------------------------- */
/*      Do we already have the answer?                                  */
/* -------------------------------------------------------------------- */
    if( panShapeRecordId[nChainId] != 0 )
        return panShapeRecordId[nChainId];
    
/* -------------------------------------------------------------------- */
/*      If we don't already have this value, then search from the       */
/*      previous known record.                                          */
/* -------------------------------------------------------------------- */
    int iTestChain, nWorkingRecId;
        
    for( iTestChain = nChainId-1;
         iTestChain >= 0 && panShapeRecordId[iTestChain] <= 0;
         iTestChain-- ) {}

    if( iTestChain < 0 )
    {
        iTestChain = -1;
        nWorkingRecId = 1;
    }
    else
    {
        nWorkingRecId = panShapeRecordId[iTestChain]+1;
    }

/* -------------------------------------------------------------------- */
/*      If we have non existent records following (-1's) we can         */
/*      narrow our search a bit.                                        */
/* -------------------------------------------------------------------- */
    while( panShapeRecordId[iTestChain+1] == -1 )
    {
        iTestChain++;
    }

/* -------------------------------------------------------------------- */
/*      Read records up to the maximum distance that is possibly        */
/*      required, looking for our target TLID.                          */
/* -------------------------------------------------------------------- */
    int         nMaxChainToRead = nChainId - iTestChain;
    int         nChainsRead = 0;
    char        achShapeRec[208];
    int         nShapeRecLen = 208 + nRecordLength - 228;

    while( nChainsRead < nMaxChainToRead )
    {
        if( VSIFSeek( fpShape, (nWorkingRecId-1) * nShapeRecLen,
                      SEEK_SET ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to seek to %d of %s2",
                      (nWorkingRecId-1) * nShapeRecLen, pszModule );
            return -1;
        }

        if( VSIFRead( achShapeRec, 208, 1, fpShape ) != 1 )
        {
            if( !VSIFEof( fpShape ) )
                CPLError( CE_Failure, CPLE_FileIO,
                          "Failed to read record %d of %s2",
                          nWorkingRecId-1, pszModule );
            return -1;
        }

        if( atoi(GetField(achShapeRec,6,15)) == nTLID )
        {
            panShapeRecordId[nChainId] = nWorkingRecId;

            return nWorkingRecId;
        }

        if( atoi(GetField(achShapeRec,16,18)) == 1 )
        {
            nChainsRead++;
        }

        nWorkingRecId++;
    }

    panShapeRecordId[nChainId] = -1;

    return -1;
}

/************************************************************************/
/*                           SetWriteModule()                           */
/************************************************************************/
int TigerCompleteChain::SetWriteModule( const char *pszFileCode, int nRecLen, 
                                        OGRFeature *poFeature )

{
    int bSuccess;

    bSuccess = TigerFileBase::SetWriteModule( pszFileCode, nRecLen, poFeature);
    if( !bSuccess )
        return bSuccess;

/* -------------------------------------------------------------------- */
/*      Open the RT3 file                                               */
/* -------------------------------------------------------------------- */
    if( bUsingRT3 )
    {
        if( fpRT3 != NULL )
        {
            VSIFClose( fpRT3 );
            fpRT3 = NULL;
        }

        if( pszModule )
        {
            char        *pszFilename;
        
            pszFilename = poDS->BuildFilename( pszModule, "3" );

            fpRT3 = VSIFOpen( pszFilename, "ab" );

            CPLFree( pszFilename );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Close the shape point file, if open and free the list of        */
/*      record ids.                                                     */
/* -------------------------------------------------------------------- */
    if( fpShape != NULL )
    {
        VSIFClose( fpShape );
        fpShape = NULL;
    }
    
    if( pszModule )
    {
        char        *pszFilename;
        
        pszFilename = poDS->BuildFilename( pszModule, "2" );
        
        fpShape = VSIFOpen( pszFilename, "ab" );
        
        CPLFree( pszFilename );
    }

    return TRUE;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

#define WRITE_REC_LEN_RT1 228
#define WRITE_REC_LEN_RT2 208
#define WRITE_REC_LEN_RT3 111

/* max of above */
#define WRITE_REC_LEN WRITE_REC_LEN_RT1

OGRErr TigerCompleteChain::CreateFeature( OGRFeature *poFeature )

{
    char        szRecord[WRITE_REC_LEN+1];
    OGRLineString *poLine = (OGRLineString *) poFeature->GetGeometryRef();

    if( poLine == NULL 
        || (poLine->getGeometryType() != wkbLineString
            && poLine->getGeometryType() != wkbLineString25D) )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Write basic data record ("RT1")                                 */
/* -------------------------------------------------------------------- */

    if( !SetWriteModule( "1", WRITE_REC_LEN_RT1+2, poFeature ) )
        return OGRERR_FAILURE;

    memset( szRecord, ' ', WRITE_REC_LEN_RT1 );

    WriteField( poFeature, "TLID", szRecord, 6, 15, 'R', 'N' );
    WriteField( poFeature, "SIDE1", szRecord, 16, 16, 'R', 'N' );
    WriteField( poFeature, "SOURCE", szRecord, 17, 17, 'L', 'A' );
    WriteField( poFeature, "FEDIRP", szRecord, 18, 19, 'L', 'A' );
    WriteField( poFeature, "FENAME", szRecord, 20, 49, 'L', 'A' );
    WriteField( poFeature, "FETYPE", szRecord, 50, 53, 'L', 'A' );
    WriteField( poFeature, "FEDIRS", szRecord, 54, 55, 'L', 'A' );
    WriteField( poFeature, "CFCC", szRecord, 56, 58, 'L', 'A' );
    WriteField( poFeature, "FRADDL", szRecord, 59, 69, 'R', 'A' );
    WriteField( poFeature, "TOADDL", szRecord, 70, 80, 'R', 'A' );
    WriteField( poFeature, "FRADDR", szRecord, 81, 91, 'R', 'A' );
    WriteField( poFeature, "TOADDR", szRecord, 92, 102, 'R', 'A' );
    WriteField( poFeature, "FRIADDL", szRecord, 103, 103, 'L', 'A' );
    WriteField( poFeature, "TOIADDL", szRecord, 104, 104, 'L', 'A' );
    WriteField( poFeature, "FRIADDR", szRecord, 105, 105, 'L', 'A' );
    WriteField( poFeature, "TOIADDR", szRecord, 106, 106, 'L', 'A' );
    WriteField( poFeature, "ZIPL", szRecord, 107, 111, 'L', 'N' );
    WriteField( poFeature, "ZIPR", szRecord, 112, 116, 'L', 'N' );
    WriteField( poFeature, "FAIRL", szRecord, 117, 121, 'L', 'N' );
    WriteField( poFeature, "FAIRR", szRecord, 122, 126, 'L', 'N' );
    WriteField( poFeature, "TRUSTL", szRecord, 127, 127, 'L', 'A' );
    WriteField( poFeature, "TRUSTR", szRecord, 128, 128, 'L', 'A' );
    WriteField( poFeature, "CENSUS1", szRecord, 129, 129, 'L', 'A' );
    WriteField( poFeature, "CENSUS2", szRecord, 130, 130, 'L', 'A' );
    WriteField( poFeature, "STATEL", szRecord, 131, 132, 'L', 'N' );
    WriteField( poFeature, "STATER", szRecord, 133, 134, 'L', 'N' );
    WriteField( poFeature, "COUNTYL", szRecord, 135, 137, 'L', 'N' );
    WriteField( poFeature, "COUNTYR", szRecord, 138, 140, 'L', 'N' );
    WriteField( poFeature, "FMCDL", szRecord, 141, 145, 'L', 'N' );
    WriteField( poFeature, "FMCDR", szRecord, 146, 150, 'L', 'N' );
    WriteField( poFeature, "FSMCDL", szRecord, 151, 155, 'L', 'N' );
    WriteField( poFeature, "FSMCDR", szRecord, 156, 160, 'L', 'N' );
    WriteField( poFeature, "FPLL", szRecord, 161, 165, 'L', 'N' );
    WriteField( poFeature, "FPLR", szRecord, 166, 170, 'L', 'N' );
    WriteField( poFeature, "CTBNAL", szRecord, 171, 176, 'L', 'N' );
    WriteField( poFeature, "CTBNAR", szRecord, 177, 182, 'L', 'N' );
    WriteField( poFeature, "BLKL", szRecord, 183, 186, 'L', 'N' );
    WriteField( poFeature, "BLKR", szRecord, 187, 190, 'L', 'N' );

    WritePoint( szRecord, 191, poLine->getX(0), poLine->getY(0) );
    WritePoint( szRecord, 210, 
                poLine->getX(poLine->getNumPoints()-1), 
                poLine->getY(poLine->getNumPoints()-1) );

    WriteRecord( szRecord, WRITE_REC_LEN_RT1, "1" );

/* -------------------------------------------------------------------- */
/*      Write geographic entity codes (RT3)                             */
/* -------------------------------------------------------------------- */

    memset( szRecord, ' ', WRITE_REC_LEN_RT3 );

    WriteField( poFeature, "TLID", szRecord, 6, 15, 'R', 'N' );
    WriteField( poFeature, "STATE90L", szRecord, 16, 17, 'L', 'N' );
    WriteField( poFeature, "STATE90R", szRecord, 18, 19, 'L', 'N' );
    WriteField( poFeature, "COUN90L", szRecord, 20, 22, 'L', 'N' );
    WriteField( poFeature, "COUN90R", szRecord, 23, 25, 'L', 'N' );
    WriteField( poFeature, "FMCD90L", szRecord, 26, 30, 'L', 'N' );
    WriteField( poFeature, "FMCD90R", szRecord, 31, 35, 'L', 'N' );
    WriteField( poFeature, "FPL90L", szRecord, 36, 40, 'L', 'N' );
    WriteField( poFeature, "FPL90R", szRecord, 41, 45, 'L', 'N' );
    WriteField( poFeature, "CTBNA90L", szRecord, 46, 51, 'L', 'N' );
    WriteField( poFeature, "CTBNA90R", szRecord, 52, 57, 'L', 'N' );
    WriteField( poFeature, "AIR90L", szRecord, 58, 61, 'L', 'N' );
    WriteField( poFeature, "AIR90R", szRecord, 62, 65, 'L', 'N' );
    WriteField( poFeature, "TRUST90L", szRecord, 66, 66, 'L', 'A' );
    WriteField( poFeature, "TRUST90R", szRecord, 67, 67, 'L', 'A' );
    WriteField( poFeature, "BLK90L", szRecord, 70, 73, 'L', 'A' );
    WriteField( poFeature, "BLK90R", szRecord, 74, 77, 'L', 'A' );
    WriteField( poFeature, "AIRL", szRecord, 78, 81, 'L', 'N' );
    WriteField( poFeature, "AIRR", szRecord, 82, 85, 'L', 'N' );

    /* Census 2000 */
    WriteField( poFeature, "ANRCL", szRecord, 86, 90, 'L', 'N' );
    WriteField( poFeature, "ANRCR", szRecord, 91, 95, 'L', 'N' );
    WriteField( poFeature, "AITSCEL", szRecord, 96, 98, 'L', 'N' );
    WriteField( poFeature, "AITSCER", szRecord, 99, 101, 'L', 'N' );
    WriteField( poFeature, "AITSL", szRecord, 102, 106, 'L', 'N' );
    WriteField( poFeature, "AITSR", szRecord, 107, 111, 'L', 'N' );

    /* Pre 2000 */
    WriteField( poFeature, "VTDL", szRecord, 104, 107, 'L', 'A' );
    WriteField( poFeature, "VTDR", szRecord, 108, 111, 'L', 'A' );

    WriteRecord( szRecord, WRITE_REC_LEN_RT3, "3", fpRT3 );

/* -------------------------------------------------------------------- */
/*      Write shapes sections (RT2)                                     */
/* -------------------------------------------------------------------- */
    if( poLine->getNumPoints() > 2 )
    {
        int     nPoints = poLine->getNumPoints();
        int     iPoint, nRTSQ = 1;

        for( iPoint = 1; iPoint < nPoints-1; )
        {
            int         i;
            char        szTemp[5];

            memset( szRecord, ' ', WRITE_REC_LEN_RT2 );

            WriteField( poFeature, "TLID", szRecord, 6, 15, 'R', 'N' );
            
            sprintf( szTemp, "%3d", nRTSQ );
            strncpy( ((char *)szRecord) + 15, szTemp, 4 );

            for( i = 0; i < 10; i++ )
            {
                if( iPoint < nPoints-1 )
                    WritePoint( szRecord, 19+19*i, 
                                poLine->getX(iPoint), poLine->getY(iPoint) );
                else
                    WritePoint( szRecord, 19+19*i, 0.0, 0.0 );

                iPoint++;
            }
            
            WriteRecord( szRecord, WRITE_REC_LEN_RT2, "2", fpShape );

            nRTSQ++;
        }
    }

    return OGRERR_NONE;
}
