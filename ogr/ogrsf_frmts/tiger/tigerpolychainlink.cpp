/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerPolyChainLink, providing access to .RTI files.
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
 * Revision 1.12  2006/03/29 00:46:20  fwarmerdam
 * update contact info
 *
 * Revision 1.11  2005/09/21 00:53:19  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.10  2003/01/11 15:29:55  warmerda
 * expanded tabs
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

#define FILE_CODE "I"

static TigerFieldInfo rtI_2002_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "FILE",       'L', 'N', OFTInteger,    6,  10,   5,       1,   1,     1 },
  { "TLID",       'R', 'N', OFTInteger,   11,  20,  10,       1,   1,     1 },
  { "TZIDS",      'R', 'N', OFTInteger,   21,  30,  10,       1,   1,     1 },
  { "TZIDE",      'R', 'N', OFTInteger,   31,  40,  10,       1,   1,     1 },
  { "CENIDL",     'L', 'A', OFTString,    41,  45,   5,       1,   1,     1 },
  { "POLYIDL",    'R', 'N', OFTInteger,   46,  55,  10,       1,   1,     1 },
  { "CENIDR",     'L', 'A', OFTString,    56,  60,   5,       1,   1,     1 },
  { "POLYIDR",    'R', 'N', OFTInteger,   61,  70,  10,       1,   1,     1 },
  { "SOURCE",     'L', 'A', OFTString,    71,  80,  10,       1,   1,     1 },
  { "FTSEG",      'L', 'A', OFTString,    81,  97,  17,       1,   1,     1 },
  { "RS_I1",      'L', 'A', OFTString,    98, 107,  10,       1,   1,     1 },
  { "RS_I2",      'L', 'A', OFTString,   108, 117,  10,       1,   1,     1 },
  { "RS_I3",      'L', 'A', OFTString,   118, 127,  10,       1,   1,     1 },
};
static TigerRecordInfo rtI_2002_info =
  {
    rtI_2002_fields,
    sizeof(rtI_2002_fields) / sizeof(TigerFieldInfo),
    127
  };

static TigerFieldInfo rtI_fields[] = {
  // fieldname    fmt  type OFTType      beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,     0,   0,   8,       1,   0,     0 },
  { "TLID",       'R', 'N', OFTInteger,    6,  15,  10,       1,   1,     1 },
  { "FILE",       'L', 'N', OFTString,    16,  20,   5,       1,   1,     1 },
  { "STATE",      'L', 'N', OFTInteger,   16,  17,   2,       1,   1,     1 },
  { "COUNTY",     'L', 'N', OFTInteger,   18,  20,   3,       1,   1,     1 },
  { "RTLINK",     'L', 'A', OFTString,    21,  21,   1,       1,   1,     1 },
  { "CENIDL",     'L', 'A', OFTString,    22,  26,   5,       1,   1,     1 },
  { "POLYIDL",    'R', 'N', OFTInteger,   27,  36,  10,       1,   1,     1 },
  { "CENIDR",     'L', 'A', OFTString,    37,  41,   5,       1,   1,     1 },
  { "POLYIDR",    'R', 'N', OFTInteger,   42,  51,  10,       1,   1,     1 }
};
static TigerRecordInfo rtI_info =
  {
    rtI_fields,
    sizeof(rtI_fields) / sizeof(TigerFieldInfo),
    52
  };


/************************************************************************/
/*                         TigerPolyChainLink()                         */
/************************************************************************/

TigerPolyChainLink::TigerPolyChainLink( OGRTigerDataSource * poDSIn,
                                  const char * pszPrototypeModule )

{
    OGRFieldDefn        oField("",OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "PolyChainLink" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    if (poDS->GetVersion() >= TIGER_2002) {
      psRTIInfo = &rtI_2002_info;
    } else {
      psRTIInfo = &rtI_info;
    }

    /* -------------------------------------------------------------------- */
    /*      Fields from type I record.                                      */
    /* -------------------------------------------------------------------- */

    AddFieldDefns( psRTIInfo, poFeatureDefn );
}

/************************************************************************/
/*                        ~TigerPolyChainLink()                         */
/************************************************************************/

TigerPolyChainLink::~TigerPolyChainLink()

{
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerPolyChainLink::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, FILE_CODE ) )
        return FALSE;

    EstablishFeatureCount();
    
    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerPolyChainLink::GetFeature( int nRecordId )

{
    char        achRecord[OGR_TIGER_RECBUF_LEN];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %sI",
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
                  "Failed to seek to %d of %sI",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFRead( achRecord, psRTIInfo->nRecordLength, 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %sI",
                  nRecordId, pszModule );
        return NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      Set fields.                                                     */
    /* -------------------------------------------------------------------- */

    OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );

    SetFields( psRTIInfo, poFeature, achRecord );

    return poFeature;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr TigerPolyChainLink::CreateFeature( OGRFeature *poFeature )

{
    char        szRecord[OGR_TIGER_RECBUF_LEN];

    if( !SetWriteModule( FILE_CODE, psRTIInfo->nRecordLength+2, poFeature ) )
        return OGRERR_FAILURE;

    memset( szRecord, ' ', psRTIInfo->nRecordLength );

    WriteFields( psRTIInfo, poFeature, szRecord );

    WriteRecord( szRecord, psRTIInfo->nRecordLength, FILE_CODE );

    return OGRERR_NONE;
}
