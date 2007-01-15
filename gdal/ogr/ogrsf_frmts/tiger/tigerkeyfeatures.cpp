/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerKeyFeatures, providing access to .RT9 files.
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

#define FILE_CODE "9"

static TigerFieldInfo rt9_fields[] = {
  // fieldname    fmt  type  OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTString,     6,  10,   5,       1,   1,     1 },
  { "STATE",      'L', 'N', OFTInteger,    6,   7,   2,       1,   1,     1 },
  { "COUNTY",     'L', 'N', OFTInteger,    8,  10,   3,       1,   1,     1 },
  { "CENID",      'L', 'A', OFTString,    11,  15,   5,       1,   1,     1 },
  { "POLYID",     'R', 'N', OFTInteger,   16,  25,  10,       1,   1,     1 },
  { "SOURCE",     'L', 'A', OFTString,    26,  26,   1,       1,   1,     1 },
  { "CFCC",       'L', 'A', OFTString,    27,  29,   3,       1,   1,     1 },
  { "KGLNAME",    'L', 'A', OFTString,    30,  59,  30,       1,   1,     1 },
  { "KGLADD",     'R', 'A', OFTString,    60,  70,  11,       1,   1,     1 },
  { "KGLZIP",     'L', 'N', OFTInteger,   71,  75,   5,       1,   1,     1 },
  { "KGLZIP4",    'L', 'N', OFTInteger,   76,  79,   4,       1,   1,     1 },
  { "FEAT",       'R', 'N', OFTInteger,   80,  87,   8,       1,   1,     1 }
};

static TigerRecordInfo rt9_info =
  {
    rt9_fields,
    sizeof(rt9_fields) / sizeof(TigerFieldInfo),
    88
  };

/************************************************************************/
/*                         TigerKeyFeatures()                         */
/************************************************************************/

TigerKeyFeatures::TigerKeyFeatures( OGRTigerDataSource * poDSIn,
                                  const char * pszPrototypeModule )

{
    OGRFieldDefn        oField("",OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "KeyFeatures" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    psRT9Info = &rt9_info;

    /* -------------------------------------------------------------------- */
    /*      Fields from type 9 record.                                      */
    /* -------------------------------------------------------------------- */

    AddFieldDefns( psRT9Info, poFeatureDefn );

}

/************************************************************************/
/*                        ~TigerKeyFeatures()                         */
/************************************************************************/

TigerKeyFeatures::~TigerKeyFeatures()

{
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerKeyFeatures::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, FILE_CODE ) )
        return FALSE;

    EstablishFeatureCount();
    
    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerKeyFeatures::GetFeature( int nRecordId )

{
    char        achRecord[OGR_TIGER_RECBUF_LEN];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %s9",
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
                  "Failed to seek to %d of %s9",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFRead( achRecord, psRT9Info->nRecordLength, 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %s9",
                  nRecordId, pszModule );
        return NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      Set fields.                                                     */
    /* -------------------------------------------------------------------- */

    OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );

    SetFields( psRT9Info, poFeature, achRecord );

    return poFeature;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr TigerKeyFeatures::CreateFeature( OGRFeature *poFeature )

{
    char        szRecord[OGR_TIGER_RECBUF_LEN];

    if( !SetWriteModule( FILE_CODE, psRT9Info->nRecordLength+2, poFeature ) )
        return OGRERR_FAILURE;

    memset( szRecord, ' ', psRT9Info->nRecordLength );

    WriteFields( psRT9Info, poFeature, szRecord );

    WriteRecord( szRecord, psRT9Info->nRecordLength, FILE_CODE );

    return OGRERR_NONE;
}
