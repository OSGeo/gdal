/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerZipPlus4, providing access to .RTZ files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

#include "ogr_tiger.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

#define FILE_CODE       "Z"

static TigerFieldInfo rtZ_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "TLID",       'R', 'N', OFTInteger,    6,  15,  10,       1,   1,     1 },
  { "RTSQ",       'R', 'N', OFTInteger,   16,  18,   3,       1,   1,     1 },
  { "ZIP4L",      'L', 'N', OFTInteger,   19,  22,   4,       1,   1,     1 },
  { "ZIP4R",      'L', 'N', OFTInteger,   23,  26,   4,       1,   1,     1 }
};
static TigerRecordInfo rtZ_info =
  {
    rtZ_fields,
    sizeof(rtZ_fields) / sizeof(TigerFieldInfo),
    26
  };

/************************************************************************/
/*                           TigerZipPlus4()                            */
/************************************************************************/

TigerZipPlus4::TigerZipPlus4( OGRTigerDataSource * poDSIn,
                              const char * pszPrototypeModule )

{
    OGRFieldDefn        oField("",OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "ZipPlus4" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    psRTZInfo = &rtZ_info;

    /* -------------------------------------------------------------------- */
    /*      Fields from type Z record.                                      */
    /* -------------------------------------------------------------------- */

    AddFieldDefns( psRTZInfo, poFeatureDefn );

}

/************************************************************************/
/*                           ~TigerZipPlus4()                           */
/************************************************************************/

TigerZipPlus4::~TigerZipPlus4()

{
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerZipPlus4::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, FILE_CODE ) )
        return FALSE;

    EstablishFeatureCount();
    
    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerZipPlus4::GetFeature( int nRecordId )

{
    char        achRecord[OGR_TIGER_RECBUF_LEN];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %sZ",
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
                  "Failed to seek to %d of %sZ",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFRead( achRecord, psRTZInfo->nRecordLength, 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %sZ",
                  nRecordId, pszModule );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Set fields.                                                     */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );

    SetFields( psRTZInfo, poFeature, achRecord );

    return poFeature;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr TigerZipPlus4::CreateFeature( OGRFeature *poFeature )

{
  char        szRecord[OGR_TIGER_RECBUF_LEN];

    if( !SetWriteModule( FILE_CODE, psRTZInfo->nRecordLength+2, poFeature ) )
        return OGRERR_FAILURE;

    memset( szRecord, ' ', psRTZInfo->nRecordLength );

    WriteFields( psRTZInfo, poFeature, szRecord );

    WriteRecord( szRecord, psRTZInfo->nRecordLength, FILE_CODE );

    return OGRERR_NONE;
}
