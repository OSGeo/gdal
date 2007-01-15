/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerSpatialMetadata, providing access to .RTM files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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

#define FILE_CODE "M"

static TigerFieldInfo rtM_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "TLID",       'R', 'N', OFTInteger,    6,  15,  10,       1,   1,     1 },
  { "RTSQ",       'R', 'N', OFTInteger,   16,  18,   3,       1,   1,     1 },
  { "SOURCEID",   'L', 'A', OFTString,    19,  28,  10,       1,   1,     1 },
  { "ID",         'L', 'A', OFTString,    29,  46,  18,       1,   1,     1 },
  { "IDFLAG",     'R', 'A', OFTString,    47,  47,   1,       1,   1,     1 },
  { "RS-M1",      'L', 'A', OFTString,    48,  65,  18,       1,   1,     1 },
  { "RS-M2",      'L', 'A', OFTString,    66,  67,   2,       1,   1,     1 },
  { "RS-M3",      'L', 'A', OFTString,    68,  90,  23,       1,   1,     1 }
};
static TigerRecordInfo rtM_info =
  {
    rtM_fields,
    sizeof(rtM_fields) / sizeof(TigerFieldInfo),
    90
  };

/************************************************************************/
/*                        TigerSpatialMetadata()                        */
/************************************************************************/

TigerSpatialMetadata::TigerSpatialMetadata( OGRTigerDataSource * poDSIn,
                            const char * pszPrototypeModule )

{
    OGRFieldDefn        oField("",OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "SpatialMetadata" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    psRTMInfo = &rtM_info;

    /* -------------------------------------------------------------------- */
    /*      Fields from record type H                                       */
    /* -------------------------------------------------------------------- */

    AddFieldDefns(psRTMInfo, poFeatureDefn);
}

/************************************************************************/
/*                       ~TigerSpatialMetadata()                        */
/************************************************************************/

TigerSpatialMetadata::~TigerSpatialMetadata()

{
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerSpatialMetadata::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, FILE_CODE ) )
        return FALSE;

    EstablishFeatureCount();
    
    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerSpatialMetadata::GetFeature( int nRecordId )

{
    char        achRecord[OGR_TIGER_RECBUF_LEN];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %sH",
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
                  "Failed to seek to %d of %sH",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFRead( achRecord, psRTMInfo->nRecordLength, 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %sM",
                  nRecordId, pszModule );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Set fields.                                                     */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );

    SetFields( psRTMInfo, poFeature, achRecord );

    return poFeature;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr TigerSpatialMetadata::CreateFeature( OGRFeature *poFeature )

{
    char        szRecord[OGR_TIGER_RECBUF_LEN];

    if( !SetWriteModule( FILE_CODE, psRTMInfo->nRecordLength+2, poFeature ) )
        return OGRERR_FAILURE;

    memset( szRecord, ' ', psRTMInfo->nRecordLength );

    WriteFields( psRTMInfo, poFeature, szRecord );

    WriteRecord( szRecord, psRTMInfo->nRecordLength, FILE_CODE );

    return OGRERR_NONE;
}
