/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerZipCodes, providing access to .RT6 files.
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
 * Revision 1.11  2006/03/29 00:46:21  fwarmerdam
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

#define FILE_CODE "6"

static TigerFieldInfo rt6_fields[] = {
  // fieldname    fmt  type OFTType     beg  end  len  bDefine bSet bWrite
  { "MODULE",     ' ', ' ', OFTString,    0,   0,   8,       1,   0,     0 },
  { "TLID",       'R', 'N', OFTInteger,   6,  15,  10,       1,   1,     1 },
  { "RTSQ",       'R', 'N', OFTInteger,  16,  18,   3,       1,   1,     1 },
  { "FRADDL",     'R', 'A', OFTString,   19,  29,  11,       1,   1,     1 },
  { "TOADDL",     'R', 'A', OFTString,   30,  40,  11,       1,   1,     1 },
  { "FRADDR",     'R', 'A', OFTString,   41,  51,  11,       1,   1,     1 },
  { "TOADDR",     'R', 'A', OFTString,   52,  62,  11,       1,   1,     1 },
  { "FRIADDL",    'L', 'A', OFTInteger,  63,  63,   1,       1,   1,     1 },
  { "TOIADDL",    'L', 'A', OFTInteger,  64,  64,   1,       1,   1,     1 },
  { "FRIADDR",    'L', 'A', OFTInteger,  65,  65,   1,       1,   1,     1 },
  { "TOIADDR",    'L', 'A', OFTInteger,  66,  66,   1,       1,   1,     1 },
  { "ZIPL",       'L', 'N', OFTInteger,  67,  71,   5,       1,   1,     1 },
  { "ZIPR",       'L', 'N', OFTInteger,  72,  76,   5,       1,   1,     1 }
};
static TigerRecordInfo rt6_info =
  {
    rt6_fields,
    sizeof(rt6_fields) / sizeof(TigerFieldInfo),
    76
  };

/************************************************************************/
/*                            TigerZipCodes()                           */
/************************************************************************/

TigerZipCodes::TigerZipCodes( OGRTigerDataSource * poDSIn,
                                  const char * pszPrototypeModule )

{
    OGRFieldDefn        oField("",OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn( "ZipCodes" );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    psRT6Info = &rt6_info;

    /* -------------------------------------------------------------------- */
    /*      Fields from type 5 record.                                      */
    /* -------------------------------------------------------------------- */

    AddFieldDefns( psRT6Info, poFeatureDefn );
}

/************************************************************************/
/*                        ~TigerZipCodes()                         */
/************************************************************************/

TigerZipCodes::~TigerZipCodes()

{
}

/************************************************************************/
/*                             SetModule()                              */
/************************************************************************/

int TigerZipCodes::SetModule( const char * pszModule )

{
    if( !OpenFile( pszModule, "6" ) )
        return FALSE;

    EstablishFeatureCount();
    
    return TRUE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerZipCodes::GetFeature( int nRecordId )

{
    char        achRecord[OGR_TIGER_RECBUF_LEN];

    if( nRecordId < 0 || nRecordId >= nFeatures )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Request for out-of-range feature %d of %s6",
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
                  "Failed to seek to %d of %s6",
                  nRecordId * nRecordLength, pszModule );
        return NULL;
    }

    if( VSIFRead( achRecord, psRT6Info->nRecordLength, 1, fpPrimary ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read record %d of %s6",
                  nRecordId, pszModule );
        return NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      Set fields.                                                     */
    /* -------------------------------------------------------------------- */

    OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );

    SetFields( psRT6Info, poFeature, achRecord );

    return poFeature;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr TigerZipCodes::CreateFeature( OGRFeature *poFeature )

{
  char  szRecord[OGR_TIGER_RECBUF_LEN];

    if( !SetWriteModule( FILE_CODE, psRT6Info->nRecordLength+2, poFeature ) )
        return OGRERR_FAILURE;

    memset( szRecord, ' ', psRT6Info->nRecordLength );

    WriteFields( psRT6Info, poFeature, szRecord);

    WriteRecord( szRecord, psRT6Info->nRecordLength, FILE_CODE );

    return OGRERR_NONE;
}
