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
 * Revision 1.2  1999/11/04 21:14:31  warmerda
 * various improvements, and TestCapability()
 *
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

    fpRT3 = NULL;
    bUsingRT3 = TRUE;

    panShapeRecordId = NULL;
    fpShape = NULL;
    
/* -------------------------------------------------------------------- */
/*      Fields from type 1 record.                                      */
/* -------------------------------------------------------------------- */
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

        oField.Set( "VTDL", OFTString, 4 );
        poFeatureDefn->AddFieldDefn( &oField );

        oField.Set( "VTDR", OFTString, 4 );
        poFeatureDefn->AddFieldDefn( &oField );
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
    if( !OpenFile( pszModule, "RT1" ) )
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
            char	*pszFilename;
        
            pszFilename = poDS->BuildFilename( pszModule, "RT3" );

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
    poFeature->SetField( "STATEL", GetField( achRecord, 131, 132 ));
    poFeature->SetField( "STATER", GetField( achRecord, 133, 134 ));
    poFeature->SetField( "COUNTYL", GetField( achRecord, 135, 137 ));
    poFeature->SetField( "COUNTYR", GetField( achRecord, 138, 140 ));
    poFeature->SetField( "FMCDL", GetField( achRecord, 141, 145 ));
    poFeature->SetField( "FMCDR", GetField( achRecord, 146, 150 ));
    poFeature->SetField( "FSMCDL", GetField( achRecord, 151, 155 ));
    poFeature->SetField( "FSMCDR", GetField( achRecord, 156, 160 ));
    poFeature->SetField( "FPLL", GetField( achRecord, 161, 165 ));
    poFeature->SetField( "FPLR", GetField( achRecord, 166, 170 ));
    poFeature->SetField( "CTBNAL", GetField( achRecord, 171, 176 ));
    poFeature->SetField( "CTBNAR", GetField( achRecord, 177, 182 ));
    poFeature->SetField( "BLKL", GetField( achRecord, 183, 186 ));
    poFeature->SetField( "BLKR", GetField( achRecord, 187, 190 ));

/* -------------------------------------------------------------------- */
/*      Read RT3 record, and apply fields.				*/
/* -------------------------------------------------------------------- */
    if( fpRT3 != NULL )
    {
        char	achRT3Rec[111];
        int	nRT3RecLen = 111 + nRecordLength - 228;

        if( VSIFSeek( fpRT3, nRecordId * nRT3RecLen, SEEK_SET ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to seek to %d of %s.RT3",
                      nRecordId * nRT3RecLen, pszModule );
            return NULL;
        }

        if( VSIFRead( achRT3Rec, 111, 1, fpRT3 ) != 1 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to read record %d of %s.RT3",
                      nRecordId, pszModule );
            return NULL;
        }

        poFeature->SetField( "STATE90L", GetField( achRT3Rec, 16, 17 ));
        poFeature->SetField( "STATE90R", GetField( achRT3Rec, 18, 19 ));
        poFeature->SetField( "COUN90L", GetField( achRT3Rec, 20, 22 ));
        poFeature->SetField( "COUN90R", GetField( achRT3Rec, 23, 25 ));
        poFeature->SetField( "FMCD90L", GetField( achRT3Rec, 26, 30 ));
        poFeature->SetField( "FMCD90R", GetField( achRT3Rec, 31, 35 ));
        poFeature->SetField( "FPL90L", GetField( achRT3Rec, 36, 40 ));
        poFeature->SetField( "FPL90R", GetField( achRT3Rec, 41, 45 ));
        poFeature->SetField( "CTBNA90L", GetField( achRT3Rec, 46, 51 ));
        poFeature->SetField( "CTBNA90R", GetField( achRT3Rec, 52, 57 ));
        poFeature->SetField( "AIR90L", GetField( achRT3Rec, 58, 61 ));
        poFeature->SetField( "AIR90R", GetField( achRT3Rec, 62, 65 ));
        poFeature->SetField( "TRUST90L", GetField( achRT3Rec, 66, 66 ));
        poFeature->SetField( "TRUST90R", GetField( achRT3Rec, 67, 67 ));
        poFeature->SetField( "BLK90L", GetField( achRT3Rec, 70, 73 ));
        poFeature->SetField( "BLK90R", GetField( achRT3Rec, 74, 77 ));
        poFeature->SetField( "AIRL", GetField( achRT3Rec, 78, 81 ));
        poFeature->SetField( "AIRR", GetField( achRT3Rec, 82, 85 ));
        poFeature->SetField( "VTDL", GetField( achRT3Rec, 104, 107 ));
        poFeature->SetField( "VTDR", GetField( achRT3Rec, 108, 111 ));
    }

/* -------------------------------------------------------------------- */
/*      Set geometry                                                    */
/* -------------------------------------------------------------------- */
    OGRLineString	*poLine = new OGRLineString();

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
    int		nShapeRecId;

    nShapeRecId = GetShapeRecordId( nRecordId, nTLID );
    if( nShapeRecId == -1 )
        return;

/* -------------------------------------------------------------------- */
/*      Read all the sequential records with the same TLID.             */
/* -------------------------------------------------------------------- */
    char	achShapeRec[208];
    int		nShapeRecLen = 208 + nRecordLength - 228;

    for( ; TRUE; nShapeRecId++ )
    {
        if( VSIFSeek( fpShape, (nShapeRecId-1) * nShapeRecLen,
                      SEEK_SET ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to seek to %d of %s.RT2",
                      (nShapeRecId-1) * nShapeRecLen, pszModule );
            return;
        }

        if( VSIFRead( achShapeRec, 208, 1, fpShape ) != 1 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to read record %d of %s.RT2",
                      nShapeRecId-1, pszModule );
            return;
        }

        if( atoi(GetField(achShapeRec,6,15)) != nTLID )
            break;

/* -------------------------------------------------------------------- */
/*      Translate the locations into OGRLineString vertices.            */
/* -------------------------------------------------------------------- */
        int	iVertex;

        for( iVertex = 0; iVertex < 10; iVertex++ )
        {
            int		iStart = 19 + 19*iVertex;
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
        char	*pszFilename;

        pszFilename = poDS->BuildFilename( pszModule, "RT2" );

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
    int	iTestChain, nWorkingRecId;
        
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
    int		nMaxChainToRead = nChainId - iTestChain;
    int		nChainsRead = 0;
    char	achShapeRec[208];
    int		nShapeRecLen = 208 + nRecordLength - 228;

    while( nChainsRead < nMaxChainToRead )
    {
        if( VSIFSeek( fpShape, (nWorkingRecId-1) * nShapeRecLen,
                      SEEK_SET ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to seek to %d of %s.RT2",
                      (nWorkingRecId-1) * nShapeRecLen, pszModule );
            return -1;
        }

        if( VSIFRead( achShapeRec, 208, 1, fpShape ) != 1 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to read record %d of %s.RT2",
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




