/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerFeatureIds, providing access to .RT5 files.
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
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.11  2006/03/29 00:46:20  fwarmerdam
 * update contact info
 *
 * Revision 1.10  2005/09/21 00:53:19  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.9  2003/01/04 23:21:56  mbp
 * Minor bug fixes and field definition changes.  Cleaned
 * up and commented code written for TIGER 2002 support.
 *
 * Revision 1.8  2002/12/26 00:20:19  mbp
 * re-organized code to hold TIGER-version details in TigerRecordInfo structs;
 * first round implementation of TIGER_2002 support
 *
 * Revision 1.7  2001/07/19 16:05:49  warmerda
 * clear out tabs
 *
 * Revision 1.6  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.5  2001/07/04 23:25:32  warmerda
 * first round implementation of writer
 *
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

CPL_CVSID("$Id$");

#define FILE_CODE "5"

static TigerFieldInfo rt5_2002_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTInteger,    6,  10,   5,       1,   1,     1 },
  { "FEAT",       'R', 'N', OFTInteger,   11,  18,   8,       1,   1,     1 },
  { "FEDIRP",     'L', 'A', OFTString,    19,  20,   2,       1,   1,     1 },
  { "FENAME",     'L', 'A', OFTString,    21,  50,  30,       1,   1,     1 },
  { "FETYPE",     'L', 'A', OFTString,    51,  54,   4,       1,   1,     1 },
  { "FEDIRS",     'L', 'A', OFTString,    55,  56,   2,       1,   1,     1 },
};
static TigerRecordInfo rt5_2002_info =
  {
    rt5_2002_fields,
    sizeof(rt5_2002_fields) / sizeof(TigerFieldInfo),
    56
  };

static TigerFieldInfo rt5_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTString,     2,   6,   5,       1,   1,     1 },
  { "STATE",      'L', 'N', OFTInteger,    2,   3,   2,       1,   1,     1 },
  { "COUNTY",     'L', 'N', OFTInteger,    4,   6,   3,       1,   1,     1 },
  { "FEAT",       'R', 'N', OFTInteger,    7,  14,   8,       1,   1,     1 },
  { "FEDIRP",     'L', 'A', OFTString,    15,  16,   2,       1,   1,     1 },
  { "FENAME",     'L', 'A', OFTString,    17,  46,  30,       1,   1,     1 },
  { "FETYPE",     'L', 'A', OFTString,    47,  50,   4,       1,   1,     1 },
  { "FEDIRS",     'L', 'A', OFTString,    51,  52,   2,       1,   1,     1 }
};

static TigerRecordInfo rt5_info =
  {
    rt5_fields,
    sizeof(rt5_fields) / sizeof(TigerFieldInfo),
    52
  };

/************************************************************************/
/*                            TigerFeatureIds()                         */
/************************************************************************/

TigerFeatureIds::TigerFeatureIds( OGRTigerDataSource * poDSIn,
                                  const char * pszPrototypeModule )

{
  OGRFieldDefn        oField("",OFTInteger);
  poDS = poDSIn;
  poFeatureDefn = new OGRFeatureDefn( "FeatureIds" );
    poFeatureDefn->Reference();
  poFeatureDefn->SetGeomType( wkbNone );

  if (poDS->GetVersion() >= TIGER_2002) {
    psRT5Info = &rt5_2002_info;
  } else {
    psRT5Info = &rt5_info;
  }

  AddFieldDefns( psRT5Info, poFeatureDefn );
}

/************************************************************************/
/*                        ~TigerFeatureIds()                         */
/************************************************************************/

TigerFeatureIds::~TigerFeatureIds()

{
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerFeatureIds::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, FILE_CODE ) )
        return FALSE;

    EstablishFeatureCount();
    
    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerFeatureIds::GetFeature( int nRecordId )

{
    char        achRecord[OGR_TIGER_RECBUF_LEN];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %s5",
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
                  "Failed to seek to %d of %s5",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFRead( achRecord, psRT5Info->nRecordLength, 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %s5",
                  nRecordId, pszModule );
        return NULL;
    }

    OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );

    SetFields( psRT5Info, poFeature, achRecord );

    return poFeature;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr TigerFeatureIds::CreateFeature( OGRFeature *poFeature )

{
    char        szRecord[OGR_TIGER_RECBUF_LEN];

    if( !SetWriteModule( FILE_CODE, psRT5Info->nRecordLength+2, poFeature ) )
        return OGRERR_FAILURE;

    memset( szRecord, ' ', psRT5Info->nRecordLength );

    WriteFields( psRT5Info, poFeature, szRecord);

    WriteRecord( szRecord, psRT5Info->nRecordLength, FILE_CODE );

    return OGRERR_NONE;
}
