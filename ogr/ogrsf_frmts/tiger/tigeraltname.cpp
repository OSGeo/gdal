/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerAltName, providing access to RT4 files.
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
 * Revision 1.7  2001/07/19 16:05:49  warmerda
 * clear out tabs
 *
 * Revision 1.6  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.5  2001/07/04 23:25:32  warmerda
 * first round implementation of writer
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

#define FILE_CODE "4"

/************************************************************************/
/*                            TigerAltName()                            */
/************************************************************************/

TigerAltName::TigerAltName( OGRTigerDataSource * poDSIn,
                            const char * pszPrototypeModule )

{
    OGRFieldDefn        oField("",OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "AltName" );
    poFeatureDefn->SetGeomType( wkbNone );

/* -------------------------------------------------------------------- */
/*      Fields from type 4 record.                                      */
/* -------------------------------------------------------------------- */
    oField.Set( "MODULE", OFTString, 8 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "TLID", OFTInteger, 10 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "RTSQ", OFTInteger, 3 );
    poFeatureDefn->AddFieldDefn( &oField );
    
    oField.Set( "FEAT", OFTIntegerList, 8 );
    poFeatureDefn->AddFieldDefn( &oField );
}

/************************************************************************/
/*                           ~TigerAltName()                            */
/************************************************************************/

TigerAltName::~TigerAltName()

{
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerAltName::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, FILE_CODE ) )
        return FALSE;

    EstablishFeatureCount();
    
    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerAltName::GetFeature( int nRecordId )

{
    char        achRecord[58];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %s4",
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
                  "Failed to seek to %d of %s4",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFRead( achRecord, 58, 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %s4",
                  nRecordId, pszModule );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Set fields.                                                     */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );
    int         anFeatList[5];
    int         nFeatCount=0;

    poFeature->SetField( "TLID", GetField( achRecord, 6, 15 ));
    poFeature->SetField( "RTSQ", GetField( achRecord, 16, 18 ));


    for( int iFeat = 0; iFeat < 5; iFeat++ )
    {
        const char *    pszFieldText;

        pszFieldText = GetField( achRecord, 19 + iFeat*8, 26 + iFeat*8 );

        if( *pszFieldText != '\0' )
            anFeatList[nFeatCount++] = atoi(pszFieldText);
    }

    poFeature->SetField( "FEAT", nFeatCount, anFeatList );

    return poFeature;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

#define WRITE_REC_LEN 58

OGRErr TigerAltName::CreateFeature( OGRFeature *poFeature )

{
    char        szRecord[WRITE_REC_LEN+1];
    const int   *panValue;
    int         nValueCount = 0;

    if( !SetWriteModule( FILE_CODE, WRITE_REC_LEN+2, poFeature ) )
        return OGRERR_FAILURE;

    memset( szRecord, ' ', WRITE_REC_LEN );

    WriteField( poFeature, "TLID", szRecord, 6, 15, 'R', 'N' );
    WriteField( poFeature, "RTSQ", szRecord, 16, 18, 'R', 'N' );

    panValue = poFeature->GetFieldAsIntegerList( "FEAT", &nValueCount );
    for( int i = 0; i < nValueCount; i++ )
    {
        char    szWork[9];
        
        sprintf( szWork, "%8d", panValue[i] );
        strncpy( szRecord + 18 + 8 * i, szWork, 8 );
    }

    WriteRecord( szRecord, WRITE_REC_LEN, FILE_CODE );

    return OGRERR_NONE;
}
