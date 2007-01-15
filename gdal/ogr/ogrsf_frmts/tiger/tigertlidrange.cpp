/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerTLIDRange, providing access to .RTR files.
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

#define FILE_CODE "R"

static TigerFieldInfo rtR_2002_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTInteger,    6,  10,   5,       1,   1,     1 },
  { "CENID",      'L', 'A', OFTString,    11,  15,   5,       1,   1,     1 },
  { "TLMAXID",    'R', 'N', OFTInteger,   16,  25,  10,       1,   1,     1 },
  { "TLMINID",    'R', 'N', OFTInteger,   26,  35,  10,       1,   1,     1 },
  { "TLIGHID",    'R', 'N', OFTInteger,   36,  45,  10,       1,   1,     1 },
  { "TZMAXID",    'R', 'N', OFTInteger,   46,  55,  10,       1,   1,     1 },
  { "TZMINID",    'R', 'N', OFTInteger,   56,  65,  10,       1,   1,     1 },
  { "TZHIGHID",   'R', 'N', OFTInteger,   66,  75,  10,       1,   1,     1 },
  { "FILLER",     'L', 'A', OFTString,    76,  76,   1,       1,   1,     1 },
};
static TigerRecordInfo rtR_2002_info =
  {
    rtR_2002_fields,
    sizeof(rtR_2002_fields) / sizeof(TigerFieldInfo),
    76
  };

static TigerFieldInfo rtR_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTString,     6,  10,   5,       1,   1,     1 },
  { "STATE",      'L', 'N', OFTInteger,    6,   7,   2,       1,   1,     1 },
  { "COUNTY",     'L', 'N', OFTInteger,    8,  10,   3,       1,   1,     1 },
  { "CENID",      'L', 'A', OFTString,    11,  15,   5,       1,   1,     1 },
  { "MAXID",      'R', 'N', OFTInteger,   16,  25,  10,       1,   1,     1 },
  { "MINID",      'R', 'N', OFTInteger,   26,  35,  10,       1,   1,     1 },
  { "HIGHID",     'R', 'N', OFTInteger,   36,  45,  10,       1,   1,     1 }
};

static TigerRecordInfo rtR_info =
  {
    rtR_fields,
    sizeof(rtR_fields) / sizeof(TigerFieldInfo),
    46
  };

/************************************************************************/
/*                           TigerTLIDRange()                           */
/************************************************************************/

TigerTLIDRange::TigerTLIDRange( OGRTigerDataSource * poDSIn,
                            const char * pszPrototypeModule )

{
    OGRFieldDefn        oField("",OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "TLIDRange" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    if (poDS->GetVersion() >= TIGER_2002) {
      psRTRInfo = &rtR_2002_info;
    } else {
      psRTRInfo = &rtR_info;
    }

    /* -------------------------------------------------------------------- */
    /*      Fields from type R record.                                      */
    /* -------------------------------------------------------------------- */

    AddFieldDefns( psRTRInfo, poFeatureDefn );

}

/************************************************************************/
/*                          ~TigerTLIDRange()                           */
/************************************************************************/

TigerTLIDRange::~TigerTLIDRange()

{
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerTLIDRange::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, FILE_CODE ) )
        return FALSE;

    EstablishFeatureCount();
    
    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerTLIDRange::GetFeature( int nRecordId )

{
    char        achRecord[OGR_TIGER_RECBUF_LEN];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %sR",
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
                  "Failed to seek to %d of %sR",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFRead( achRecord, psRTRInfo->nRecordLength, 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %sR",
                  nRecordId, pszModule );
        return NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      Set fields.                                                     */
    /* -------------------------------------------------------------------- */

    OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );

    SetFields( psRTRInfo, poFeature, achRecord );

    return poFeature;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr TigerTLIDRange::CreateFeature( OGRFeature *poFeature )

{
    char        szRecord[OGR_TIGER_RECBUF_LEN];

    if( !SetWriteModule( FILE_CODE, psRTRInfo->nRecordLength+2, poFeature ) )
        return OGRERR_FAILURE;

    memset( szRecord, ' ', psRTRInfo->nRecordLength );

    WriteFields( psRTRInfo, poFeature, szRecord );

    WriteRecord( szRecord, psRTRInfo->nRecordLength, FILE_CODE );

    return OGRERR_NONE;
}
